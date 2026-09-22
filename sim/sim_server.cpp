// Tiny HTTP server (plain POSIX sockets, loopback only, no dependencies) that
// lets a browser tab be the watch's screen + touch panel + control panel.
#include <Arduino.h>
#undef gettimeofday
#undef settimeofday
#include <lvgl.h>
#include "ui/screens.h"
#include "sim_hal.h"
#include "sim_state.h"

#include <vector>
#include <map>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct Conn { int fd; std::string in; uint64_t t0; };
static int lfd = -1;
static std::vector<Conn> conns;

static uint64_t mono_ms() { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }

bool sim_server_start(int port) {
    lfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);   // CLOEXEC: freed on deep-sleep exec()
    if (lfd < 0) return false;
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    if (bind(lfd, (sockaddr *)&a, sizeof a) < 0) { perror("bind"); return false; }
    return listen(lfd, 16) == 0;
}

// ── helpers ─────────────────────────────────────────────────────────────
static std::string url_decode(const std::string &s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) { r += (char)strtol(s.substr(i + 1, 2).c_str(), 0, 16); i += 2; }
        else if (s[i] == '+') r += ' ';
        else r += s[i];
    }
    return r;
}
static std::map<std::string, std::string> parse_query(const std::string &q) {
    std::map<std::string, std::string> m;
    size_t i = 0;
    while (i < q.size()) {
        size_t amp = q.find('&', i);
        if (amp == std::string::npos) amp = q.size();
        std::string kv = q.substr(i, amp - i);
        size_t eq = kv.find('=');
        if (eq == std::string::npos) m[url_decode(kv)] = "";
        else m[url_decode(kv.substr(0, eq))] = url_decode(kv.substr(eq + 1));
        i = amp + 1;
    }
    return m;
}
static bool send_all(int fd, const char *p, size_t n) {
    uint64_t t0 = mono_ms();
    while (n) {
        ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
        if (w > 0) { p += w; n -= w; continue; }
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (mono_ms() - t0 > 300) return false;
            pollfd pf{fd, POLLOUT, 0}; poll(&pf, 1, 20); continue;
        }
        return false;
    }
    return true;
}
static void respond(int fd, int code, const char *ctype, const std::string &body, const char *extra = "") {
    char h[256];
    const char *msg = code == 200 ? "OK" : code == 204 ? "No Content" : code == 404 ? "Not Found" : "Error";
    int n = snprintf(h, sizeof h, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n%sConnection: close\r\n\r\n",
                     code, msg, ctype, body.size(), extra);
    send_all(fd, h, n);
    if (!body.empty()) send_all(fd, body.data(), body.size());
}

static inline void rgb565(uint16_t v, uint8_t &r, uint8_t &g, uint8_t &b) {
    r = (v >> 11) & 31; g = (v >> 5) & 63; b = v & 31;
    r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2);
}

static void handle(int fd, const std::string &req) {
    size_t sp1 = req.find(' '), sp2 = req.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) { respond(fd, 400, "text/plain", "bad request"); return; }
    std::string target = req.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t qm = target.find('?');
    std::string path = target.substr(0, qm), query = qm == std::string::npos ? "" : target.substr(qm + 1);
    auto q = parse_query(query);

    if (path == "/" || path == "/index.html") {
        FILE *f = fopen((sim_root() + "/sim/web/index.html").c_str(), "rb");
        if (!f) { respond(fd, 404, "text/plain", "sim/web/index.html not found"); return; }
        std::string body; char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) body.append(buf, n);
        fclose(f);
        respond(fd, 200, "text/html; charset=utf-8", body);
    } else if (path == "/state") {
        respond(fd, 200, "application/json", sim_build_state_json(atoi(q["edges"].c_str()), atoi(q["log"].c_str())));
    } else if (path == "/frame") {
        if ((uint32_t)atoi(q["since"].c_str()) == sim.frame) { respond(fd, 204, "application/octet-stream", ""); return; }
        char extra[64]; snprintf(extra, sizeof extra, "X-Frame: %u\r\n", sim.frame);
        respond(fd, 200, "application/octet-stream", std::string((const char *)sim.fb, sizeof sim.fb), extra);
    } else if (path == "/shot.ppm") {                       // for scripted screenshots
        std::string body = "P6\n240 240\n255\n";
        for (int i = 0; i < 240 * 240; i++) { uint8_t r, g, b; rgb565(sim.fb[i], r, g, b); body += (char)r; body += (char)g; body += (char)b; }
        respond(fd, 200, "image/x-portable-pixmap", body);
    } else if (path == "/touch") {
        sim.touch_down = q["d"] == "1";
        sim.touch_x = atoi(q["x"].c_str());
        sim.touch_y = atoi(q["y"].c_str());
        respond(fd, 200, "application/json", "{\"ok\":1}");
    } else if (path == "/ctl") {
        if (q.count("battery")) sim.battery_pct = constrain(atoi(q["battery"].c_str()), 0, 100);
        if (q["wake"] == "timer") sim.wake_request = 1;
        if (q["wake"] == "touch") sim.wake_request = 2;
        for (const char *k : {"fire", "buzz", "reboot", "factory"})
            if (q.count(k)) sim_queue_action(std::string(k) + "=" + q[k]);
        respond(fd, 200, "application/json", "{\"ok\":1}");
    } else {
        respond(fd, 404, "text/plain", "not found");
    }
}

void sim_server_poll() {
    if (lfd < 0) return;
    for (;;) {
        int c = accept4(lfd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (c < 0) break;
        conns.push_back({c, "", mono_ms()});
    }
    for (size_t i = 0; i < conns.size();) {
        Conn &c = conns[i];
        char buf[2048];
        ssize_t n = recv(c.fd, buf, sizeof buf, 0);
        bool done = false;
        if (n > 0) {
            c.in.append(buf, n);
            if (c.in.find("\r\n\r\n") != std::string::npos) { handle(c.fd, c.in); done = true; }
        } else if (n == 0) done = true;                                     // peer closed
        else if (errno != EAGAIN && errno != EWOULDBLOCK) done = true;
        if (!done && mono_ms() - c.t0 > 3000) done = true;                  // idle browser preconnect
        if (done) { close(c.fd); conns.erase(conns.begin() + i); } else i++;
    }
}

void sim_pump() { sim_server_poll(); }
