#!/bin/bash
# LogMind Performance Stress Test
# Tests high-concurrency log injection via D-Bus InjectLog
set -e

DAEMON="$1"
CONFIG="$2"
RESULTS_DIR="/tmp/logmind-stress-results"
mkdir -p "$RESULTS_DIR"

if [ -z "$DAEMON" ] || [ -z "$CONFIG" ]; then
    echo "Usage: $0 <daemon-binary> <config-file>"
    exit 1
fi

echo "=== LogMind Performance Stress Test ==="
echo "Time: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""

# Start daemon
"$DAEMON" --config "$CONFIG" &
DAEMON_PID=$!
sleep 2

# Verify daemon is running
if ! kill -0 $DAEMON_PID 2>/dev/null; then
    echo "FAIL: Daemon failed to start"
    exit 1
fi
echo "Daemon started (PID=$DAEMON_PID)"

# ── Test 1: Single-threaded throughput ──
echo ""
echo "--- Test 1: Single-threaded throughput (10000 logs) ---"
START=$(date +%s%N)
for i in $(seq 1 10000); do
    gdbus call --session --dest com.logmind.Daemon \
        --object-path /com/logmind/Daemon \
        --method com.logmind.Daemon1.InjectLog \
        "test-source" "2026-05-31T10:00:00Z INFO [module] Test log entry #$i: user login from 192.168.1.$((i % 256)) action=purchase amount=\$${i}.00" \
        > /dev/null 2>&1
done
END=$(date +%s%N)
ELAPSED_MS=$(( (END - START) / 1000000 ))
TPS_SINGLE=$(( 10000 * 1000 / (ELAPSED_MS + 1) ))
echo "Single-thread: 10000 logs in ${ELAPSED_MS}ms (≈${TPS_SINGLE} logs/sec)"

# ── Test 2: Multi-threaded throughput ──
echo ""
echo "--- Test 2: Multi-threaded throughput (4 threads × 2500 logs) ---"
START=$(date +%s%N)
for t in $(seq 1 4); do
    (
        for i in $(seq 1 2500); do
            gdbus call --session --dest com.logmind.Daemon \
                --object-path /com/logmind/Daemon \
                --method com.logmind.Daemon1.InjectLog \
                "thread-$t" "2026-05-31T10:00:00Z WARN [module$t] Thread-$t entry #$i: disk usage at $(( 50 + i % 50 ))% on /dev/sda1" \
                > /dev/null 2>&1
        done
    ) &
done
wait
END=$(date +%s%N)
ELAPSED_MS=$(( (END - START) / 1000000 ))
TPS_MULTI=$(( 10000 * 1000 / (ELAPSED_MS + 1) ))
echo "Multi-thread (4×2500): 10000 logs in ${ELAPSED_MS}ms (≈${TPS_MULTI} logs/sec)"

# ── Test 3: Large log line (1MB) ──
echo ""
echo "--- Test 3: Large log line (1MB payload) ---"
LARGE_PAYLOAD=$(python3 -c "print('A' * 1048576)")
START=$(date +%s%N)
gdbus call --session --dest com.logmind.Daemon \
    --object-path /com/logmind/Daemon \
    --method com.logmind.Daemon1.InjectLog \
    "large-test" "$LARGE_PAYLOAD" > /dev/null 2>&1
END=$(date +%s%N)
ELAPSED_MS=$(( (END - START) / 1000000 ))
echo "1MB log line: ${ELAPSED_MS}ms"

# ── Test 4: Query performance ──
echo ""
echo "--- Test 4: Query performance (1000 queries) ---"
START=$(date +%s%N)
for i in $(seq 1 1000); do
    gdbus call --session --dest com.logmind.Daemon \
        --object-path /com/logmind/Daemon \
        --method com.logmind.Daemon1.QueryLogs \
        "error" 100 > /dev/null 2>&1
done
END=$(date +%s%N)
ELAPSED_MS=$(( (END - START) / 1000000 ))
QPS=$(( 1000 * 1000 / (ELAPSED_MS + 1) ))
echo "Query performance: 1000 queries in ${ELAPSED_MS}ms (≈${QPS} queries/sec)"

# ── Collect stats ──
echo ""
echo "--- Final Stats ---"
gdbus call --session --dest com.logmind.Daemon \
    --object-path /com/logmind/Daemon \
    --method com.logmind.Daemon1.GetStats 2>&1
gdbus call --session --dest com.logmind.Daemon \
    --object-path /com/logmind/Daemon \
    --method com.logmind.Daemon1.GetStatus 2>&1

# ── Memory snapshot ──
echo ""
echo "--- Memory Snapshot ---"
ps -o pid,rss,vsz,comm -p $DAEMON_PID 2>/dev/null || true

# Stop daemon
kill $DAEMON_PID 2>/dev/null
wait $DAEMON_PID 2>/dev/null || true

echo ""
echo "=== Performance Test Complete ==="
echo "Single-thread TPS: $TPS_SINGLE"
echo "Multi-thread TPS:  $TPS_MULTI"
echo "Query QPS:         $QPS"
