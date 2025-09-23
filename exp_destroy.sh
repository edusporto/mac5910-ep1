#!/bin/bash

# =================================================================
# A very simple bash script to load-test an MQTT broker.
#
# Usage:
# 1. Save this file as simple_load_test.sh
# 2. Make it executable: chmod +x simple_load_test.sh
# 3. Run it, passing the broker's IP address as an argument:
#    ./simple_load_test.sh <BROKER_IP_ADDRESS>
#
# If no IP is given, it defaults to localhost.
# =================================================================

# --- Configuration ---
BROKER_HOST=${1:-"localhost"} # Use the first argument or default to localhost
NUM_CLIENTS=50
PUBLISHER_LOOPS=50
TOPIC="simple.bash.test"
MESSAGE="hello from bash script"

# --- Cleanup Function ---
# This function will be called automatically when the script exits
# (either normally or via Ctrl+C) to kill all background subscribers.
cleanup() {
    echo ""
    echo "---"
    echo "🧹 Cleaning up background subscriber processes..."
    # Check if the pids array is not empty
    if [ ${#pids[@]} -ne 0 ]; then
        kill ${pids[@]} > /dev/null 2>&1
        echo "✅ Done. All subscribers terminated."
    else
        echo "No subscribers to clean up."
    fi
}

# --- Main Script ---

# The 'trap' command ensures the 'cleanup' function runs when the script exits.
trap cleanup EXIT

# An array to store the Process IDs (PIDs) of our background subscribers
pids=()

echo "---"
echo "🚀 Starting Load Test on Broker: $BROKER_HOST"
echo "---"

# 1. Start 50 subscribers in the background
echo "1. Launching $NUM_CLIENTS subscribers..."
for i in $(seq 1 $NUM_CLIENTS); do
    # Run mosquitto_sub in the background (&) and hide its output.
    # Store its PID ($!) in our array for later cleanup.
    mosquitto_sub -V 5 -h "$BROKER_HOST" -t "$TOPIC" > /dev/null 2>&1 &
    pids+=($!)
done
echo "   All subscribers are running in the background."
sleep 2 # Give them a moment to connect

# 2. Loop and start 50 publishers each time
echo "2. Starting publisher loops..."
for i in $(seq 1 $PUBLISHER_LOOPS); do
    echo "   -> Publisher Loop $i of $PUBLISHER_LOOPS: Sending $NUM_CLIENTS messages..."
    for j in $(seq 1 $NUM_CLIENTS); do
        # Run publishers in the background to send messages concurrently
        mosquitto_pub -V 5 -h "$BROKER_HOST" -t "$TOPIC" -m "$MESSAGE $j" &
    done
    # 'wait' tells the script to pause here until all background jobs
    # from the inner loop have finished before starting the next loop.
    # wait
done

echo "---"
echo "✅ Test complete. Publishers have finished."
# The script will now exit, and the 'trap' will automatically call the cleanup function.