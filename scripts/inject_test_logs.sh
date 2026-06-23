#!/bin/bash
# LogMind Test Log Injector - uses dbus-send
DBUS="dbus-send --session --dest=com.logmind.Daemon --type=method_call /com/logmind/Daemon com.logmind.Daemon1.InjectLog"
sources=("nginx" "system" "app-server" "auth" "database")
echo "Injecting test logs..."
for i in $(seq 1 100); do
    idx=$((i % ${#sources[@]}))
    src="${sources[$idx]}"
    case "$src" in
        nginx)
            line="192.168.1.$i - - [$(date +%d/%b/%Y:%H:%M:%S) +0000] GET /api/users HTTP/1.1 200 $((RANDOM % 5000))"
            ;;
        system)
            line="kernel: memory usage at $((60 + RANDOM % 40))% on node-$((i % 8))"
            ;;
        app-server)
            line="[INFO] req=$i user=$((RANDOM % 1000)) path=/api/items ms=$((RANDOM % 800)) status=200"
            ;;
        auth)
            line="[WARN] Failed login attempt from 10.0.0.$((RANDOM % 255))"
            ;;
        database)
            line="[ERROR] connection timeout to db-$((i % 4)) after $((RANDOM % 30000))ms"
            ;;
    esac
    $DBUS string:"$src" string:"$line" >/dev/null 2>&1
done
echo "Done: injected 100 logs"
