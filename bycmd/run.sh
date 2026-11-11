#!/system/bin/sh
# run.sh — Standoff2 ESP
# MT Manager: Long-press → Run as root
# Terminal:   su -c "sh /data/local/tmp/run.sh"

BINARY=/data/local/tmp/standoff-esp
LOCK=/data/local/tmp/bycmd-1.0.0.lock
LOG=/data/local/tmp/standoff-esp.log

id | grep -q "uid=0" || { echo "[-] Need root"; exit 1; }

[ -f "$LOCK" ] && rm -f "$LOCK"
[ -f "$BINARY" ] || { echo "[-] Binary not found: $BINARY"; exit 1; }

# Выдаём права и запускаем
chmod 777 "$BINARY"
su -c "$BINARY" > "$LOG" 2>&1 &
echo "[+] Started. PID=$!  log=$LOG"
