#!/usr/bin/env bash
# Append a manual note entry to the evolution journal with a correct UTC
# timestamp. Manual notes written by hand have ended up carrying local
# wall-clock time with a "Z" suffix, breaking the journal's chronology;
# always append notes through this script instead.
#
# Usage: tools/journal_note.sh "<excerpt>" [tag ...]
set -euo pipefail

excerpt="${1:?usage: journal_note.sh \"<excerpt>\" [tag ...]}"
shift

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
journal="${script_dir}/../evolution/journal.ndjson"

EXCERPT="$excerpt" python3 - "$@" >> "$journal" <<'PY'
import json
import os
import sys
import time

print(json.dumps({
    "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "type": "note",
    "excerpt": os.environ["EXCERPT"],
    "tags": sys.argv[1:],
}, ensure_ascii=False))
PY
