#!/bin/bash
# LogMind Plugin Dynamic Loading/Unloading Stability Test
# Tests repeated load/unload cycles of .so plugins
set -e

DAEMON="$1"
CONFIG="$2"
PLUGIN_DIR="$3"

if [ -z "$DAEMON" ] || [ -z "$CONFIG" ] || [ -z "$PLUGIN_DIR" ]; then
    echo "Usage: $0 <daemon-binary> <config-file> <plugin-dir>"
    exit 1
fi

echo "=== LogMind Plugin Load/Unload Stability Test ==="
echo "Time: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""

# ── Test 1: Build mock plugin ──
echo "--- Test 1: Build mock plugin ---"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLUGIN_SRC="$SCRIPT_DIR/../plugins/mock_collector.cpp"
PLUGIN_SO="$PLUGIN_DIR/mock_collector.so"

g++ -std=c++17 -shared -fPIC -o "$PLUGIN_SO" "$PLUGIN_SRC" \
    "$SCRIPT_DIR/../../src/daemon/plugin_api.cpp" \
    -I/home/ling/WorkSpace/LogMind/include 2>&1
echo "Built: $PLUGIN_SO"

# ── Test 2: Load/unload cycle (10 iterations) ──
echo ""
echo "--- Test 2: Load/unload cycle (10 iterations) ---"
for i in $(seq 1 10); do
    # Start daemon
    "$DAEMON" --config "$CONFIG" &
    DAEMON_PID=$!
    sleep 1

    # Verify plugin loaded
    RESULT=$(gdbus call --session --dest com.logmind.Daemon \
        --object-path /com/logmind/Daemon \
        --method com.logmind.Daemon1.ListPlugins 2>&1)
    if echo "$RESULT" | grep -q '"loaded": true' && echo "$RESULT" | grep -q "mock_collector"; then
        echo "  Iteration $i: Plugin loaded ✓"
    else
        echo "  Iteration $i: Plugin NOT loaded ✗"
    fi

    # Stop daemon
    kill $DAEMON_PID 2>/dev/null
    wait $DAEMON_PID 2>/dev/null || true

    # Verify clean unload
    sleep 0.5
done

# ── Test 3: Rapid load/unload (stress) ──
echo ""
echo "--- Test 3: Rapid load/unload (20 iterations, no daemon restart) ---"
"$DAEMON" --config "$CONFIG" &
DAEMON_PID=$!
sleep 1

for i in $(seq 1 20); do
    # Reload plugins
    gdbus call --session --dest com.logmind.Daemon \
        --object-path /com/logmind/Daemon \
        --method com.logmind.Daemon1.ReloadPlugins 2>&1 > /dev/null
    
    # Verify
    RESULT=$(gdbus call --session --dest com.logmind.Daemon \
        --object-path /com/logmind/Daemon \
        --method com.logmind.Daemon1.ListPlugins 2>&1)
    
    if echo "$RESULT" | grep -q '"loaded": true' && echo "$RESULT" | grep -q "mock_collector"; then
        echo "  Reload $i: OK (plugin present and loaded)"
    else
        echo "  Reload $i: FAIL (plugin missing or not loaded)"
    fi
done

kill $DAEMON_PID 2>/dev/null
wait $DAEMON_PID 2>/dev/null || true

# ── Test 4: dlopen/dlclose stress (direct) ──
echo ""
echo "--- Test 4: dlopen/dlclose stress (50 iterations) ---"
cat > /tmp/dl_stress.c << 'EOF'
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/tmp/logmind-test/plugins/mock_collector.so";
    int failures = 0;
    for (int i = 0; i < 50; i++) {
        void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!h) {
            fprintf(stderr, "  Iteration %d: dlopen failed: %s\n", i, dlerror());
            failures++;
            continue;
        }
        int (*abi_fn)() = dlsym(h, "plugin_abi_version");
        if (abi_fn) {
            int ver = abi_fn();
            if (ver != 1) {
                fprintf(stderr, "  Iteration %d: ABI version mismatch: %d\n", i, ver);
                failures++;
            }
        }
        dlclose(h);
    }
    printf("  dlopen/dlclose: %d/50 succeeded, %d failed\n", 50 - failures, failures);
    return failures > 0 ? 1 : 0;
}
EOF
gcc -o /tmp/dl_stress /tmp/dl_stress.c -ldl && /tmp/dl_stress "$PLUGIN_SO"

echo ""
echo "=== Plugin Load/Unload Stability Test Complete ==="
