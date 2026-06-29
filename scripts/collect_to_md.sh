#!/bin/bash
# Collects all project source files into a single markdown file.
# Usage: ./scripts/collect_to_md.sh [output_file]
# Output: /tmp/project_sources.md (or specified path)

set -euo pipefail
OUTPUT="${1:-/tmp/project_sources.md}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TMPFILE="/tmp/_collect_md_$$.md"

echo "# Project Sources — $(date +%Y-%m-%d)" > "$TMPFILE"
echo "" >> "$TMPFILE"

# Collect all non-font, non-build files
find "$PROJECT_DIR" -type f \
  ! -path "*/.pio/*" \
  ! -path "*/.git/*" \
  ! -path "*/boards/*" \
  ! -path "*/lib/*" \
  ! -path "*/scripts/*" \
  ! -name "*.o" \
  ! -name "font_*.c" \
  | sort | while read -r f; do

  rel="${f#$PROJECT_DIR/}"
  echo "## \`$rel\`" >> "$TMPFILE"
  echo "" >> "$TMPFILE"

  ext="${rel##*.}"
  case "$ext" in
    c|cpp|h|hpp|ino) lang="cpp" ;;
    py)               lang="python" ;;
    sh)               lang="bash" ;;
    ini|cfg|conf)     lang="ini" ;;
    json)             lang="json" ;;
    md)               lang="" ;;
    *)                lang="" ;;
  esac

  [ -n "$lang" ] && echo '```'"$lang" >> "$TMPFILE" || echo '```' >> "$TMPFILE"
  cat "$f" >> "$TMPFILE"
  echo '```' >> "$TMPFILE"
  echo "" >> "$TMPFILE"
done

mv "$TMPFILE" "$OUTPUT"
echo "Done → $OUTPUT ($(wc -l < "$OUTPUT") lines)"
