#!/usr/bin/env python3
"""
remap_arabic.py — Add Unicode Presentation Form mappings to Arabic TTF fonts.

Modern Arabic TTF fonts (Reem Kufi, Amiri, Alexandria, Cairo, etc.) store
contextual glyph shapes (initial, medial, final, isolated) in OpenType GSUB
tables — NOT at Unicode Arabic Presentation Forms codepoints.

This means lv_font_conv (which extracts glyphs by codepoint) can't find them,
and LVGL's Arabic shaper shows empty boxes for connected Arabic text.

This script fixes that by:
  1. Opening the TTF with fonttools
  2. Detecting all contextual glyphs by name (e.g., "beh-ar.init", "lam-ar.medi")
  3. Adding cmap entries that map the standard Unicode PF codepoints to those glyphs
  4. Saving a remapped TTF

Usage:
  python3 remap_arabic.py ReemKufi-Regular.ttf
  # → writes ReemKufi-Regular-remapped.ttf

Then use lv_font_conv on the remapped TTF:
  lv_font_conv --font remapped.ttf --size 16 --bpp 2 \
    --range 0x0020-0x007F,0x0600-0x06FF,0xFB50-0xFDFF,0xFE70-0xFEFF \
    --format lvgl -o font_output.c

Requirements:
  pip install fonttools freetype-py
"""

import sys
import os
import shutil
from fontTools.ttLib import TTFont, newTable
from fontTools.ttLib.tables._c_m_a_p import cmap_format_4


# ── Mapping: glyph name pattern → presentation form Unicode codepoints ──
# Arabic letters share glyph shapes (beh/teh/theh look identical but for dots).
# Each entry maps every contextual-form glyph to ALL codepoints it covers.
GLYPH_TO_UNICODE = {
    # alef (ا) — doesn't connect to left, so only isol/fina
    'alef-ar.isol':      [0xFE8D, 0xFEF9],
    'alef-ar.fina':      [0xFE8E],
    'alef-ar.fina.Lam':  [0xFEF6],
    'alef-ar.isol.1':    [0xFE8D],
    'alef-ar.isol.2':    [0xFE8D],
    'alef-ar.isol.3':    [0xFE8D],
    'alef-ar.fina.Lam.1': [0xFEF6],

    # beh/teh/theh (ب/ت/ث) — share glyphs, differ by dots
    'behDotless-ar.init': [0xFE91, 0xFE97, 0xFE9B],
    'behDotless-ar.medi': [0xFE92, 0xFE98, 0xFE9C],
    'behDotless-ar.isol': [0xFE8F, 0xFE95, 0xFE99],
    'behDotless-ar.fina': [0xFE90, 0xFE96, 0xFE9A],
    'behDotless-ar.medi.high':   [0xFE92],
    'behDotless-ar.medi.high.wide': [0xFE92],
    'behDotless-ar.medi.high.wider': [0xFE92],
    'behDotless-ar.medi.wide':  [0xFE92],
    'behDotless-ar.medi.wider': [0xFE92],
    'behDotless-ar.init.Hah':   [0xFE91],
    'behDotless-ar.init.wide':  [0xFE91],

    # hah/jeem (ح/ج) — share glyphs
    'hah-ar.init': [0xFEA3, 0xFE9F],
    'hah-ar.medi': [0xFEA4, 0xFEA0],
    'hah-ar.isol': [0xFEA1, 0xFE9D],
    'hah-ar.fina': [0xFEA2, 0xFE9E],
    'hah-ar.isol.alt':  [0xFEA1],
    'hah-ar.fina.1':    [0xFEA2],
    'hah-ar.fina.alt':  [0xFEA2],
    'hah-ar.fina.alt.1': [0xFEA2],
    'hah-ar.medi.1':    [0xFEA4],
    'hah-ar.medi.2':    [0xFEA4],
    'hah-ar.init.2':    [0xFEA3],

    # dal/thal (د/ذ)
    'dal-ar.isol': [0xFEA9, 0xFEAB],
    'dal-ar.fina': [0xFEAA, 0xFEAC],

    # reh/zain (ر/ز)
    'reh-ar.isol': [0xFEAD, 0xFEAF],
    'reh-ar.fina': [0xFEAE, 0xFEB0],

    # seen/sheen (س/ش)
    'seen-ar.init': [0xFEB3, 0xFEB7],
    'seen-ar.medi': [0xFEB4, 0xFEB8],
    'seen-ar.isol': [0xFEB1, 0xFEB5],
    'seen-ar.fina': [0xFEB2, 0xFEB6],

    # sad/dad (ص/ض)
    'sad-ar.init': [0xFEBB, 0xFEBF],
    'sad-ar.medi': [0xFEBC, 0xFEC0],
    'sad-ar.isol': [0xFEB9, 0xFEBD],
    'sad-ar.fina': [0xFEBA, 0xFEBE],

    # tah/zah (ط/ظ)
    'tah-ar.init': [0xFEC3, 0xFEC7],
    'tah-ar.medi': [0xFEC4, 0xFEC8],
    'tah-ar.isol': [0xFEC1, 0xFEC5],
    'tah-ar.fina': [0xFEC2, 0xFEC6],

    # ain/ghain (ع/غ)
    'ain-ar.init': [0xFECB, 0xFECF],
    'ain-ar.medi': [0xFECC, 0xFED0],
    'ain-ar.isol': [0xFEC9, 0xFECD],
    'ain-ar.fina': [0xFECA, 0xFECE],
    'ain-ar.fina.1': [0xFECA],
    'ain-ar.medi.1': [0xFECC],

    # feh/qaf (ف/ق)
    'fehDotless-ar.init': [0xFED3, 0xFED7],
    'fehDotless-ar.medi': [0xFED4, 0xFED8],
    'fehDotless-ar.isol': [0xFED1, 0xFED5],
    'fehDotless-ar.fina': [0xFED2, 0xFED6],

    # kaf (ك)
    'kaf-ar.init': [0xFEDB],
    'kaf-ar.medi': [0xFEDC],
    'kaf-ar.isol': [0xFED9],
    'kaf-ar.fina': [0xFEDA],
    'kaf-ar.isol.1': [0xFED9],
    'kaf-ar.isol.2': [0xFED9],
    'kaf-ar.fina.1': [0xFEDA],
    'kaf-ar.fina.2': [0xFEDA],
    'kaf-ar.medi.2': [0xFEDC],
    'kaf-ar.init.1': [0xFEDB],
    'kaf-ar.init.2': [0xFEDB],

    # lam (ل)
    'lam-ar.init':  [0xFEDF],
    'lam-ar.medi':  [0xFEE0],
    'lam-ar.isol':  [0xFEDD],
    'lam-ar.fina':  [0xFEDE],
    'lam-ar.init.Alef':  [0xFEF7],
    'lam-ar.init.Alef.1': [0xFEF7],
    'lam-ar.medi.Alef':  [0xFEF8],
    'lam-ar.init.Hah':   [0xFEDF],

    # meem (م)
    'meem-ar.init': [0xFEE3],
    'meem-ar.medi': [0xFEE4],
    'meem-ar.isol': [0xFEE1],
    'meem-ar.fina': [0xFEE2],
    'meem-ar.isol.1': [0xFEE1],
    'meem-ar.isol.2': [0xFEE1],
    'meem-ar.fina.1': [0xFEE2],
    'meem-ar.fina.2': [0xFEE2],
    'meem-ar.medi.1': [0xFEE4],

    # noon (ن)
    'noonghunna-ar.init': [0xFEE7],
    'noonghunna-ar.medi': [0xFEE8],
    'noonghunna-ar.isol': [0xFEE5],
    'noonghunna-ar.fina': [0xFEE6],

    # heh (ه)
    'heh-ar.init':  [0xFEEB],
    'heh-ar.medi':  [0xFEEC],
    'heh-ar.isol':  [0xFEE9],
    'heh-ar.fina':  [0xFEEA],
    'heh-ar.isol2': [0xFEE9],
    'heh-ar.isol.1': [0xFEE9],
    'heh-ar.fina.1': [0xFEEA],
    'heh-ar.medi.1': [0xFEEC],
    'heh-ar.medi.2': [0xFEEC],
    'heh-ar.medi.3': [0xFEEC],
    'heh-ar.init.1': [0xFEEB],
    'heh-ar.init.2': [0xFEEB],
    'heh-ar.init.3': [0xFEEB],

    # waw (و)
    'waw-ar.isol': [0xFEED],
    'waw-ar.fina': [0xFEEE],
    'waw-ar.fina.1': [0xFEEE],

    # alef maksura (ى)
    'alefMaksura-ar.isol': [0xFEEF],
    'alefMaksura-ar.fina': [0xFEF0],
    'alefMaksura-ar.isol.1': [0xFEEF],
    'alefMaksura-ar.fina.1': [0xFEF0],

    # yeh (ي)
    'yehbarree-ar.isol': [0xFEF1],
    'yehbarree-ar.fina': [0xFEF2],

    # special
    'hehDoachashmee-ar.isol': [0xFBAA],
    'hehDoachashmee-ar.fina': [0xFBAB],
    'ae-ar.isol': [0xFBAC],
}

# Base-form letters that lack separate isol/fina glyphs — map to base form
BASE_FALLBACK = {
    'tehMarbuta-ar':     [0xFE93, 0xFE94],   # ة
    'alefHamzaabove-ar': [0xFE83, 0xFE84],   # أ
    'alef-ar.isol':      [0xFEF9],            # alef extended isol
}

# Missing isol forms — share glyph with fina form
ISOL_FALLBACK = {
    0xFE80: 0xFE82,  # hamza isol → hamza fina
    0xFE83: 0xFE84,  # alefHamza isol → alefHamza fina
    0xFE8D: 0xFE8E,  # alef isol → alef fina
    0xFE93: 0xFE94,  # tehMarbuta isol → tehMarbuta fina
    0xFE95: 0xFE96,  # teh isol → teh fina
    0xFEA9: 0xFEAA,  # dal isol → dal fina
    0xFEAD: 0xFEAE,  # reh isol → reh fina
     0xFEB9: 0xFEBA,  # sad isol → sad fina
     0xFEE1: 0xFEE2,  # meem isol → meem fina
     0xFEE5: 0xFEE6,  # noon isol → noon fina
     0xFEF4: 0xFEF2,  # yeh medi → yeh fina
     0xFEF9: 0xFE8E,  # alef isol extended → alef fina
}


def remap_font(input_path, output_path=None):
    """Add Arabic Presentation Form Unicode mappings to a TTF font."""
    if output_path is None:
        base, ext = os.path.splitext(input_path)
        output_path = f"{base}-remapped{ext}"

    print(f"Loading: {input_path}")
    font = TTFont(input_path)

    # Collect existing cmap entries
    best_cmap = font.getBestCmap()
    print(f"  {len(best_cmap)} existing codepoint mappings")

    # Build new cmap dict
    new_cmap = dict(best_cmap)
    mapped = 0

    # 1. Map contextual-form glyphs to Unicode PF codepoints
    glyph_order = font.getGlyphOrder()
    for idx, name in enumerate(glyph_order):
        if name in GLYPH_TO_UNICODE:
            for cp in GLYPH_TO_UNICODE[name]:
                if cp not in new_cmap:
                    new_cmap[cp] = name
                    mapped += 1

    # 2. Map base-form fallback glyphs
    for glyph_name, codepoints in BASE_FALLBACK.items():
        if glyph_name in [best_cmap.get(cp) for cp in best_cmap if glyph_name in str(best_cmap)] or glyph_name in glyph_order:
            for cp in codepoints:
                if cp not in new_cmap:
                    new_cmap[cp] = glyph_name
                    mapped += 1

    # 3. Fill missing isol forms using fina form glyphs
    for missing_cp, source_cp in ISOL_FALLBACK.items():
        if source_cp in best_cmap and missing_cp not in new_cmap:
            new_cmap[missing_cp] = best_cmap[source_cp]
            mapped += 1

    print(f"  Added {mapped} new Presentation Form mappings")

    # 4. Map ZWJ / ZWNJ → zero-width space (needed by LVGL Arabic shaper)
    zwsp_name = best_cmap.get(0x200B)  # zero-width space
    if zwsp_name:
        for cp in [0x200C, 0x200D]:    # ZWJ, ZWNJ
            if cp not in new_cmap:
                new_cmap[cp] = zwsp_name
                mapped += 1

    # Rebuild the format-4 cmap subtable
    cmap_table = font['cmap']
    # Remove old Windows Unicode subtable
    cmap_table.tables = [
        t for t in cmap_table.tables
        if not (t.platformID == 3 and t.platEncID == 1)
    ]

    new_table = cmap_format_4(4)
    new_table.platformID = 3
    new_table.platEncID = 1
    new_table.format = 4
    new_table.language = 0
    new_table.cmap = dict(sorted(new_cmap.items()))
    cmap_table.tables.append(new_table)

    font.save(output_path)
    print(f"Saved: {output_path}")
    print(f"  Total cmap entries: {len(new_cmap)}")
    return output_path


def verify_font(font_path):
    """Quick check: do key presentation form glyphs exist?"""
    import freetype

    face = freetype.Face(font_path)
    test_chars = [
        (0xFE8D, "alef isol"), (0xFE91, "beh init"), (0xFEA3, "hah init"),
        (0xFEB1, "seen isol"), (0xFEB9, "sad isol"), (0xFEDF, "lam init"), (0xFEE1, "meem isol"), (0xFEE9, "heh isol"),
        (0x200C, "ZWJ"), (0x200D, "ZWNJ"), (0x0020, "space"),
    ]

    print("\nVerification:")
    all_ok = True
    for cp, name in test_chars:
        gi = face.get_char_index(cp)
        ok = gi > 0
        if not ok:
            all_ok = False
        print(f"  U+{cp:04X} {name}: {'✓' if ok else '✗ MISSING! ' + str(gi)}")

    if all_ok:
        print("\n✓ All key glyphs present — font ready for lv_font_conv")
    else:
        print("\n⚠ Some glyphs still missing — may need manual mapping")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 remap_arabic.py <font.ttf> [output.ttf]")
        print("Example: python3 remap_arabic.py ReemKufi-Regular.ttf")
        sys.exit(1)

    input_ttf = sys.argv[1]
    output_ttf = sys.argv[2] if len(sys.argv) > 2 else None

    result = remap_font(input_ttf, output_ttf)
    verify_font(result)
