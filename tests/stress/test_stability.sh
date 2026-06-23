#!/bin/bash
# LogMind Long-Running Stability Test
# Runs daemon for extended period with periodic operations
set -e

DAEMON="$1"
CONFIG="$2"
DURATION_MIN="${3:-5}"  # Default 5 minutes

if [ -z "$DAEMON" ] || [ -z "$CONFIG" ]; then
    echo "Usage: $0 <daemon-binary> <config-file> [duration-minutes]"
    exit 1
fi

echo "=== LogMind Long-Running Stability Test ==="
echo "Time: $(date '+%Y-%m-%d %H:%M:%S')"
echo "Duration: ${DURATION_MIN} minutes"
echo ""

# Start daemon
"$DAEMON" --config "$CONFIG" &
DAEMON_PID=$!
sleep 2

if ! kill -0 $DAEMON_PID 2>/dev/null; then
    echo "FAIL: Daemon failed to start"
    exit 1
fi
echo "Daemon started (PID=$DAEMON_PID)"
echo ""

# Track results
CHECKS=0
FAILURES=0
MEMORY_SAMPLES=""
END_TIME=$(($(date +%s) + DURATION_MIN * 60))

echo "--- Starting stability monitoring ---"
echo ""

while [ $(date +%s) -lt $END_TIME ]; do
    CHECKS=$((CHECKS + 1))
    ELAPSED=$(( $(date +%s) - (END_TIME - DURATION_MIN * 60) ))
    ELAPSED_MIN=$(( ELAPSED / 60 ))
    ELAPSED_SEC=$(( ELAPSED % 60 ))

    # Check daemon is alive
    if ! kill -0 $DAEMON_PID 2>/dev/null; then
        echo "[${ELAPSED_MIN}m${ELAPSED_SEC}s] FAIL: Daemon died!"
        FAILURES=$((FAILURES + 1))
        break
    fi

    # Get status
    STATUS=$(gdbus call --session --dest com.logmind.Daemon \
        --object-path /com/logmind/Daemon \
        --method com.logmind.Daemon1.GetStatus 2>&1)

    if [ $? -ne 0 ]; then
        echo "[${ELAPSED_MIN}m${ELAPSED_SEC}s] FAIL: D-Bus call failed"
        FAILURES=$((FAILURES + 1))
    else
        # Extract buffer size
        BUF_SIZE=$(echo "$STATUS" | grep -oP "buffer_size.*?<int64 \K[0-9]+" || echo "0")
        BUF_TOTAL=$(echo "$STATUS" | grep -oP "buffer_total.*?<int64 \K[0-9]+" || echo "0")
        echo "[${ELAPSED_MIN}m${ELAPSED_SEC}s] OK - buffer_size=$BUF_SIZE, total_pushed=$BUF_TOTAL"
    fi

    # Inject some logs periodically
    for i in $(seq 1 10); do
        gdbus call --session --dest com.logmind.Daemon \
            --object-path /com/logmind/Daemon \
            --method com.logmind.Daemon1.InjectLog \
            "stability-test" "2026-05-31T10:00:${ELAPSED_SEC}Z INFO [stability] Heartbeat #$CHECKS entry #$i" \
            > /dev/null 2>&1
    done

    # Query periodically
    if [ $((CHECKS % 5)) -eq 0 ]; then
        gdbus call --session --dest com.logmind.Daemon \
            --object-path /com/logmind/Daemon \
            --method com.logmind.Daemon1.QueryLogs \
            "error" 10 > /dev/null 2>&1
    fi

    # Memory snapshot
    RSS=$(ps -o rss= -p $DAEMON_PID 2>/dev/null | tr -d ' ')
    if [ -n "$RSS" ]; then
        MEMORY_SAMPLES="$MEMORY_SAMPLES $RSS"
    fi

    sleep 10
done

echo ""
echo "--- Stability Test Summary ---"
echo "Checks performed: $CHECKS"
echo "Failures: $FAILURES"

if [ -n "$MEMORY_SAMPLES" ]; then
    MIN_RSS=$(echo $MEMORY_SAMPLES | tr ' ' '\n' | sort -n | head -1)
    MAX_RSS=$(echo $MEMORY_SAMPLES | tr ' ' '\n' | sort -n | tail -1)
    AVG_RSS=$(echo $MEMORY_SAMPLES | tr ' ' '\n' | awk '{sum+=$1} END {printf "%.0f", sum/NR}')
    echo "Memory (RSS KB): min=$MIN_RSS, max=$MAX_RSS, avg=$AVG_RSS"
    
    # Check for memory growth (>20% increase)
    if [ "$MIN_RSS" -gt 0 ]; then
        GROWTH=$(( (MAX_RSS - MIN_RSS) * 100 / MIN_RSS ))
        echo "Memory growth: ${GROWTH}%"
        if [ $GROWTH -gt 20 ]; then
            echo "WARNING: Memory growth > 20% - possible leak"
        fi
    fi
fi

# Final status
echo ""
echo "--- Final D-Bus Status ---"
gdbus call --session --dest com.logmind.Daemon \
    --object-path /com/logmind/Daemon \
    --method com.logmind.Daemon1.GetStats 2>&1
gdbus call --session --dest com.logmind.Daemon \
    --object-path /com/logmind/Daemon \
    --method com.logmind.Daemon1.GetStatus 2>&1

# Stop daemon
kill $DAEMON_PID 2>/dev/null
wait $DAEMON_PID 2>/dev/null || true

echo ""
if [ $FAILURES -eq 0 ]; then
    echo "=== Stability Test PASSED ==="
else
    echo "=== Stability Test FAILED ($FAILURES failures) ==="
fi
