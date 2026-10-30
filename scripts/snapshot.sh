#!/usr/bin/env bash
#
# minima-snapshot: quiesce the node/analytics, copy the coupled stores with
# SQLite's online backup, record a manifest, and optionally validate the
# snapshot by starting the node on it with -nop2p and running "state verify".
#
# Host-specific by design (systemd, /srv paths). Run as root.
#
# Usage:
#   snapshot.sh --out DIR [options]
#
#   --out DIR              snapshot directory (required)
#   --validate             start the node on the snapshot and run state verify
#   --keep N               keep the newest N snapshot-* dirs in the parent (default 3)
#   --force                snapshot even if the live preflight verify fails
#   --no-restart           leave the live node/analytics stopped afterwards
#
# Environment overrides:
#   MINIMA_DATA=/srv/minimanode
#   MINIMA_ANALYTICS_DIR=/srv/minima-analytics
#   MINIMA_NODE_SERVICE=minima-node
#   MINIMA_ANALYTICS_SERVICE=minima-analytics
#   MINIMA_ENV_FILE=/etc/minima-node.env     (RPC_PASS / DB_PASS)
#   MINIMA_NODE_BINARY=/opt/minima-more-v058/minima-more-v0.5.9-linux-aarch64-static
#   MINIMA_RPC_PORT=9005
#
set -euo pipefail

DATA="${MINIMA_DATA:-/srv/minimanode}"
ANALYTICS="${MINIMA_ANALYTICS_DIR:-/srv/minima-analytics}"
NODE_SERVICE="${MINIMA_NODE_SERVICE:-minima-node}"
ANALYTICS_SERVICE="${MINIMA_ANALYTICS_SERVICE:-minima-analytics}"
ENV_FILE="${MINIMA_ENV_FILE:-/etc/minima-node.env}"
RPC_PORT="${MINIMA_RPC_PORT:-9005}"
NODE_BINARY="${MINIMA_NODE_BINARY:-}"

OUT=""
VALIDATE=0
FORCE=0
RESTART=1
KEEP=3

usage() { sed -n '2,30p' "$0"; exit 1; }

while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT="${2:-}"; shift 2 ;;
    --validate) VALIDATE=1; shift ;;
    --force) FORCE=1; shift ;;
    --no-restart) RESTART=0; shift ;;
    --keep) KEEP="${2:-3}"; shift 2 ;;
    -h|--help) usage ;;
    *) echo "unknown option: $1" >&2; usage ;;
  esac
done

[ -n "$OUT" ] || usage
[ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }

# One snapshot at a time.
exec 9>/run/minima-snapshot.lock
flock -n 9 || { echo "another snapshot is running" >&2; exit 1; }

# shellcheck disable=SC1090
set -a; . "$ENV_FILE"; set +a
: "${RPC_PASS:?RPC_PASS missing from $ENV_FILE}"
: "${DB_PASS:?DB_PASS missing from $ENV_FILE}"

if [ -z "$NODE_BINARY" ]; then
  NODE_BINARY="$(systemctl show -p ExecStart --value "$NODE_SERVICE" | awk '{print $1}')"
fi
[ -x "$NODE_BINARY" ] || { echo "node binary not found: $NODE_BINARY" >&2; exit 1; }

rpc() { # rpc <port> <command>
  curl -fsS --max-time 15 -u "minima:${RPC_PASS}" \
    -H 'Content-Type: text/plain' --data "$2" "http://127.0.0.1:$1/"
}

mkdir -p "$OUT"
mkdir -p "$OUT/node" "$OUT/analytics"

# ---------------------------------------------------------------- preflight
echo "[*] preflight state verify"
rpc "$RPC_PORT" "state action:verify" > "$OUT/preflight.json" || true
rpc "$RPC_PORT" "status" > "$OUT/status.json" 2>/dev/null || true
if [ -s "$OUT/preflight.json" ]; then
  PREFLIGHT_OK="$(python3 -c 'import json,sys;print(str(json.load(open(sys.argv[1])).get("ok",False)).lower())' "$OUT/preflight.json")"
  PREFLIGHT_ISSUES="$(python3 -c 'import json,sys;d=json.load(open(sys.argv[1]));print(d.get("issues",""))' "$OUT/preflight.json")"
else
  PREFLIGHT_OK=false
  PREFLIGHT_ISSUES="state verify unavailable"
fi
if [ "$PREFLIGHT_OK" != "true" ] && [ "$FORCE" -ne 1 ]; then
  echo "preflight verify failed: $PREFLIGHT_ISSUES" >&2
  echo "use --force to snapshot anyway" >&2
  exit 2
fi

# ------------------------------------------------------------- quiesce/copy
echo "[*] stopping services"
systemctl stop "$ANALYTICS_SERVICE" 2>/dev/null || true
systemctl stop "$NODE_SERVICE"

cleanup() {
  if [ "$RESTART" -eq 1 ]; then
    systemctl start "$NODE_SERVICE" || true
    systemctl start "$ANALYTICS_SERVICE" 2>/dev/null || true
  fi
}
trap cleanup EXIT

echo "[*] copying node data tree"
cp -a "$DATA/1.0" "$OUT/node/1.0"
rm -f "$OUT/node/1.0/databases/archivesql/archive"*
echo "[*] copying archive (sqlite .backup)"
sqlite3 "$DATA/1.0/databases/archivesql/archive" \
  ".backup '$OUT/node/1.0/databases/archivesql/archive'"

echo "[*] copying analytics"
for f in index.duckdb holders.duckdb entitlements.json; do
  [ -e "$ANALYTICS/$f" ] && cp -a "$ANALYTICS/$f" "$OUT/analytics/$f"
done

echo "[*] hashing"
( cd "$OUT" && find . -type f ! -name SHA256SUMS ! -name validate.log -print0 \
    | sort -z | xargs -0 sha256sum > SHA256SUMS )

# ------------------------------------------------------------- validation
VALIDATED="not_requested"
if [ "$VALIDATE" -eq 1 ]; then
  echo "[*] validating snapshot with -nop2p on ports 19001/19005"
  VALIDATED="unknown"
  "$NODE_BINARY" -daemon -nop2p -archive -megammr -server \
    -data "$OUT/node" -port 19001 -rpcenable true -rpc 19005 \
    -rpcpassword "$RPC_PASS" -dbpassword "$DB_PASS" \
    > "$OUT/validate.log" 2>&1 &
  VPID=$!
  for _ in $(seq 1 60); do
    grep -q "Server started on port" "$OUT/validate.log" 2>/dev/null && break
    kill -0 "$VPID" 2>/dev/null || break
    sleep 1
  done
  if kill -0 "$VPID" 2>/dev/null; then
    if rpc 19005 "state action:verify" > "$OUT/validate.json" 2>/dev/null; then
      if python3 -c 'import json,sys;raise SystemExit(0 if json.load(open(sys.argv[1])).get("ok") else 1)' "$OUT/validate.json"; then
        VALIDATED="true"
      else
        VALIDATED="false"
      fi
    else
      VALIDATED="false"
    fi
    kill "$VPID" 2>/dev/null || true
    wait "$VPID" 2>/dev/null || true
  else
    VALIDATED="false"
  fi
  if [ "$VALIDATED" = "false" ]; then
    echo "snapshot failed validation - do not use for rollback" >&2
  fi
fi

# --------------------------------------------------------------- manifest
python3 - "$OUT" "$VALIDATED" "$PREFLIGHT_OK" <<'PY'
import json, os, sys, time
out, validated, preflight_ok = sys.argv[1], sys.argv[2], sys.argv[3]
man = {
    "created_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    "host": os.uname().nodename,
    "validated": validated,
    "preflight_ok": preflight_ok.lower() == "true",
}
for name in ("preflight.json", "status.json", "validate.json"):
    p = os.path.join(out, name)
    if os.path.exists(p):
        try:
            man[name[:-5]] = json.load(open(p))
        except Exception:
            pass
json.dump(man, open(os.path.join(out, "manifest.json"), "w"), indent=2, sort_keys=True)
PY

echo "[*] restarting services"
cleanup
trap - EXIT

# ------------------------------------------------------------------ prune
if [ "$KEEP" -gt 0 ]; then
  PARENT="$(cd "$OUT/.." && pwd)"
  BASE="$(basename "$OUT")"
  case "$BASE" in
    snapshot-*)
      ls -1d "$PARENT"/snapshot-* 2>/dev/null | sort | head -n "-$KEEP" | while read -r old; do
        [ "$old" = "$OUT" ] && continue
        echo "[*] pruning $old"
        rm -rf "$old"
      done
      ;;
  esac
fi

echo "[*] done: $OUT (validated=$VALIDATED)"
