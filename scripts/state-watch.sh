#!/usr/bin/env bash
#
# One-line consistency probe for cron/systemd timers.
# Exit 0 when archive/cascade/tree/MMR agree, 1 otherwise.
#
#   MINIMA_ENV_FILE=/etc/minima-node.env MINIMA_RPC_PORT=9005 state-watch.sh
#
set -euo pipefail
ENV_FILE="${MINIMA_ENV_FILE:-/etc/minima-node.env}"
RPC_PORT="${MINIMA_RPC_PORT:-9005}"

# shellcheck disable=SC1090
set -a; . "$ENV_FILE"; set +a
: "${RPC_PASS:?RPC_PASS missing from $ENV_FILE}"

resp="$(curl -fsS --max-time 10 -u "minima:${RPC_PASS}" \
  -H 'Content-Type: text/plain' --data 'state action:verify' \
  "http://127.0.0.1:${RPC_PORT}/")"

python3 - "$resp" <<'PY'
import json, sys
d = json.loads(sys.argv[1])
print("state ok={} cascade={} archive={} gap={} tree_root={} mmr_coins={} issues={}".format(
    d.get("ok"), d.get("cascade_tip"), d.get("archive_last"),
    d.get("gap_blocks"), d.get("tree_root"), d.get("mmr_coins"),
    d.get("issues", "")))
sys.exit(0 if d.get("ok") else 1)
PY
