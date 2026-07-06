#!/bin/bash
# Send commands to the ESP32-C3 I2C eye controller over USB serial
# Usage: ./eye.sh <port> <command> [args]
#        ./eye.sh <port> interactive

PORT="${1:?Usage: $0 <port> <command> [args]}"
shift
CMD="$*"

stty -F "$PORT" 115200 cs8 -cstopb -parenb raw

send() {
  echo "$1" > "$PORT"
  sleep 0.15
  while read -t 0.05 -r line < "$PORT" 2>/dev/null; do
    echo "$line"
  done
}

case "$CMD" in
  ""|interactive)
    exec 3<>"$PORT"
    stty -F "$PORT" 115200 raw -echo
    echo "Interactive mode. Type commands or Ctrl-C to quit."
    while read -r -p "eye> " line; do
      echo "$line" >&3
      sleep 0.1
      while read -t 0.05 -r response <&3 2>/dev/null; do
        echo "$response"
      done
    done
    exec 3>&-
    ;;
  watch)
    exec 3<>"$PORT"
    stty -F "$PORT" 115200 raw -echo
    echo "Watching connection. Ctrl-C to quit."
    while true; do
      echo "probe" >&3
      sleep 0.1
      while read -t 0.05 -r response <&3 2>/dev/null; do
        echo "[$(date +%H:%M:%S)] $response"
      done
      sleep 2
    done
    exec 3>&-
    ;;
  scan)
    send "i2cscan"
    ;;
  *)
    send "$CMD"
    ;;
esac
