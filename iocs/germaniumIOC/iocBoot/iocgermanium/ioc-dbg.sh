#!/bin/bash

# Path to the binary and arguments
TARGET="../../bin/linux-x86_64/germaniumDetector"
ARGS="st.cmd"

while true; do
    # Check if a gdb session exists for this user
    if ! pgrep -u "$USER" -x gdb > /dev/null; then
        clear
        echo "[Watcher] gdb exited at $(date)"
        echo "Restarting in 1 seconds..."
        sleep 1
        echo "[Watcher] Starting gdb for $TARGET $ARGS"
        gdb -ex run --args "$TARGET" "$ARGS"
    else
        # If gdb is running, wait a bit before checking again
        sleep 5
    fi
done

