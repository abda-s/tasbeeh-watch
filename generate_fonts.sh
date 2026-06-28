#!/bin/bash
# generate_fonts.sh — Full pipeline: TTF → remapped TTF → LVGL C files
set -e
TTF="$1"
PREFIX="$2"
[ -z "$TTF" ] || [ -z "$PREFIX" ] && { echo "Usage: bash generate_fonts.sh <font.ttf> <name_prefix>"; exit 1; }
OUT_DIR="${3:-$(dirname "$0")/src}"
REMAP_TTF="/tmp/${PREFIX}-remapped.ttf"

echo "=== Step 1: Remap Arabic Presentation Forms ==="
python3 "$(dirname "$0")/remap_arabic.py" "$TTF" "$REMAP_TTF"

echo ""
echo "=== Step 2: Generate LVGL font files ==="
RANGES="0x0020-0x007F,0x0600-0x06FF,0xFB50-0xFDFF,0xFE70-0xFEFF"
CTRL_CHARS="$(python3 -c 'print(chr(0x200C)+chr(0x200D)+chr(0x200B)+chr(0x00A0))')"

for size in 12 16 28; do
    bpp=2
    [ $size -ge 28 ] && bpp=4
    echo "  ${size}px ${bpp}bpp -> font_${PREFIX}_${size}.c"
    lv_font_conv --no-compress --format lvgl \
        --font "$REMAP_TTF" --size $size --bpp $bpp \
        --range "$RANGES" --symbols "$CTRL_CHARS" \
        -o "${OUT_DIR}/font_${PREFIX}_${size}.c"
    sed -i 's|#include "lvgl/lvgl.h"|#include <lvgl.h>|' "${OUT_DIR}/font_${PREFIX}_${size}.c"
    echo "    -> $(grep -c 'U+' ${OUT_DIR}/font_${PREFIX}_${size}.c) glyphs, $(stat -c%s ${OUT_DIR}/font_${PREFIX}_${size}.c) bytes"
done

rm -f "$REMAP_TTF"
echo ""
echo "=== Done ==="
ls -lh "${OUT_DIR}"/font_${PREFIX}_*.c
