// Desktop entry point: runs the firmware's own setup()/loop() unmodified.
#include <Arduino.h>
#undef gettimeofday
#undef settimeofday
#include "sim_hal.h"
#include <stdlib.h>

void setup();
void loop();

int main(int argc, char **argv) {
    int port = 8080;
    if (const char *p = getenv("SIM_PORT")) port = atoi(p);
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);

    sim_init(argc, argv);
    if (!sim_server_start(port)) { fprintf(stderr, "could not listen on port %d (set SIM_PORT or --port)\n", port); return 1; }
    if (sim.boot_id == 1) printf("\n  Watch simulator running  ->  http://localhost:%d\n\n", port);

    setup();
    for (;;) {
        sim_apply_actions();
        loop();
        sim_pump();
    }
}
