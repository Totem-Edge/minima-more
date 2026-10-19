#!/bin/sh
# Exercise core mainnet commands against a running node via RPC.
# Usage: run_mainnet_commands.sh <rpcport> <user:pass> [sendto_address] [amount]
set -u
RPC=${1:-25205}
AUTH=${2:-minima:minimamore-rpc}
SENDTO=${3:-}
AMOUNT=${4:-1}
R="http://127.0.0.1:$RPC"

r() { echo "### $1"; curl -s -m 30 -u "$AUTH" "$R/$1"; echo; echo; }

echo "==== MAINNET CORE COMMAND SUITE  $(date -u) ===="
echo

r status
r block
r getaddress
r balance
r tokens
r coins
r keys
r history
r peers
r network
r vault
r megammr

echo "### KISSVM runscript (RETURN TRUE)"
curl -s -m 30 -u "$AUTH" "$R/runscript%20script:RETURN%20TRUE"; echo; echo

if [ -n "$SENDTO" ]; then
  echo "### send microspend -> $SENDTO amount $AMOUNT"
  curl -s -m 120 -u "$AUTH" "$R/send%20address:$SENDTO%20amount:$AMOUNT"; echo; echo
  sleep 8
  r balance
  r coins
  r history
fi

echo "==== SUITE DONE $(date -u) ===="
