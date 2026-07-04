#!/usr/bin/env bash
# Convenience wrapper around specrun.py.
#
#   ./run.sh                 # run the whole pruned suite against /bin/dash
#   ./run.sh smoke loop      # run named files
#   ./run.sh --shell ./busybox-sh smoke
#   ./run.sh -v -r 3-5 smoke # verbose diff for cases 3..5
#
# Any args are passed through to specrun.py.  With no file args, runs spec/*.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)

have_file_arg=0
for a in "$@"; do
  case "$a" in
    -*) ;;                       # flag
    *) [[ -e "$a" || -e "$here/spec/$a.test.sh" ]] && have_file_arg=1 ;;
  esac
done

if [[ $have_file_arg -eq 0 ]]; then
  exec python3 "$here/specrun.py" "$@" "$here"/spec/*.test.sh
else
  exec python3 "$here/specrun.py" "$@"
fi
