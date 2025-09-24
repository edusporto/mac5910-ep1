#!/bin/sh

declare -a PIDS

# Function to kill all background processes
cleanup() {
    echo "Cleaning up background processes..."
    for pid in "${PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then # Check if process is still running
            kill "$pid"
            echo "Killed process $pid"
        fi
    done
}

# Set traps for EXIT and INT signals
trap cleanup EXIT
trap cleanup INT

echo "Sending subs..."
# Start some background processes
mosquitto_sub -V 5 -t topico1 -t topico2 & PIDS+=($!)
mosquitto_sub -V 5 -t topico1 -t topico2 & PIDS+=($!)
mosquitto_sub -V 5 -t topico1 -t topico2 & PIDS+=($!)
mosquitto_sub -V 5 -t topico1 -t topico2 & PIDS+=($!)
mosquitto_sub -V 5 -t topico1 -t topico2 & PIDS+=($!)

sleep 1

echo "Sending pubs..."
mosquitto_pub -t topico2 -m "salve 1" -V 5 & PIDS+=($!)
mosquitto_pub -t topico2 -m "salve 2" -V 5 & PIDS+=($!)
mosquitto_pub -t topico2 -m "salve 3" -V 5 & PIDS+=($!)
mosquitto_pub -t topico2 -m "salve 4" -V 5 & PIDS+=($!)
mosquitto_pub -t topico2 -m "salve 5" -V 5 & PIDS+=($!)
mosquitto_pub -t topico2 -m "salve 6" -V 5 & PIDS+=($!)
mosquitto_pub -t topico2 -m "salve 7" -V 5 & PIDS+=($!)
mosquitto_pub -t topico2 -m "salve 8" -V 5 & PIDS+=($!)
mosquitto_pub -t topico2 -m "salve 9" -V 5 & PIDS+=($!)
mosquitto_pub -t topico2 -m "salve 10" -V 5 & PIDS+=($!)

read
