#!/bin/bash
# General inotify-based build watcher
# - Watches multiple directories, each with its own list of file extensions
# - Coalesces bursts of events so one save => one build
# - Serializes builds with a lock
# - Clears screen only when a build actually starts

set -u

### ── Configuration ────────────────────────────────────────────────────────────

# Command to run when a change is detected (use an array for safety)
BUILD_CMD=( make )

# Debounce window (seconds) to coalesce multiple events from a single save
DEBOUNCE_SECONDS=0.8

# Inotify events to react to (finalizing events reduce duplicates)
EVENTS="close_write,moved_to,delete"

# Exclude editor temp/hidden files
EXCLUDE_REGEX='(^|/)\.|(~$)|(\.sw[pxon]$)|(^#.*#$)'

# Set to 1 to watch subdirectories of all watched dirs; 0 for top-level only
RECURSIVE=0

# Lockfile path to ensure only one build at a time
LOCKFILE="/tmp/build.watcher.lock"

# --- Define your directories and file types here ---
# Use regex ORs (e.g., "c|cpp|hpp|dbd") for the extensions (without leading dot)
# Example for ADGermanium:
MODULE_DIR="/epics/base/base-7.0.9/synApps_6_3/support/areaDetector-R3-12-1/ADGermanium"
LIB_DIR="$MODULE_DIR/lib/linux-x86_64"
DB_DIR="$MODULE_DIR/db"
DBD_DIR="$MODULE_DIR/dbd"
echo $DBD_DIR
# Map: directory => "ext1|ext2|ext3"
declare -A WATCH_EXTS=(
  ["$LIB_DIR"]="c|cpp|hpp|dbd"
  ["$DB_DIR"]="db|template"
  ["$DBD_DIR"]="dbd"
)

# ── Add more:
# OTHER_DIR="/path/to/other"
# WATCH_EXTS["$OTHER_DIR"]="py|sh|yaml"

### ── End Configuration ───────────────────────────────────────────────────────

### ── Functions ───────────────────────────────────────────────────────────────
# Kill all gdb processes owned by this user
kill_gdb() {
    local pids
    # get PIDs of gdb for this user
    pids=$(pgrep -u "$USER" -x gdb)

    if [[ -z "$pids" ]]; then
        echo "No gdb sessions found."
        return 0
    fi

    echo "Killing gdb sessions: $pids"
    # first try normal termination
    kill $pids 2>/dev/null

    # wait a moment, then force kill any survivors
    sleep 1
    local still_alive
    still_alive=$(pgrep -u "$USER" -x gdb)
    if [[ -n "$still_alive" ]]; then
        echo "Force killing stubborn gdb sessions: $still_alive"
        kill -9 $still_alive
    fi
}

### ── End functions ───────────────────────────────────────────────────────────

# Build inotify args & directory list
INOTIFY_ARGS=( -m --format '%w %f %e' --event "$EVENTS" --exclude "$EXCLUDE_REGEX" )
(( RECURSIVE == 1 )) && INOTIFY_ARGS+=( -r )

WATCH_DIRS=()
for d in "${!WATCH_EXTS[@]}"; do
  WATCH_DIRS+=( "$d" )
done

# Normalize a path to ensure it ends with a trailing slash (like %w from inotifywait)
norm_with_slash() {
  local p="$1"
  [[ "$p" == */ ]] || p="$p/"
  printf '%s' "$p"
}

# Check if an event applies to any configured dir/ext rule; sets globals MATCH_DIR/MATCH_REASON
match_event() {
  local path="$1" file="$2" event="$3"
  MATCH_DIR=""
  MATCH_REASON=""

  for d in "${!WATCH_EXTS[@]}"; do
    local nd; nd="$(norm_with_slash "$d")"
    local hit=1

    if (( RECURSIVE == 1 )); then
      [[ "$path" == "$nd"* ]] || hit=0
    else
      [[ "$path" == "$nd"   ]] || hit=0
    fi

    if (( hit == 1 )); then
      local exts="${WATCH_EXTS[$d]}"
      if [[ "$file" =~ \.($exts)$ ]]; then
        MATCH_DIR="$d"
        MATCH_REASON="$(basename "$d") change: $file ($event)"
        return 0
      fi
    fi
  done

  return 1
}

# Pretty print current config on start
echo "[Watcher] Monitoring the following:"
for d in "${!WATCH_EXTS[@]}"; do
  echo "  - $d  [exts: ${WATCH_EXTS[$d]}]"
done
echo "[Watcher] Recursive: $RECURSIVE  |  Events: $EVENTS"
echo

# Start the watcher
inotifywait "${INOTIFY_ARGS[@]}" "${WATCH_DIRS[@]}" | \
while read -r path file event; do
  if match_event "$path" "$file" "$event"; then
    reason="$MATCH_REASON"

    # Drain subsequent events for a short window (debounce/coalesce)
    while read -r -t "$DEBOUNCE_SECONDS" _path _file _event; do
      # keep draining; we only need one build per burst
      :
    done

    # Acquire non-blocking lock to prevent overlapping builds
    {
      if flock -n 9; then
        clear
        echo "[Watcher] Trigger: $reason"
        echo "[Watcher] Starting build at $(date '+%Y-%m-%d %H:%M:%S')"
        echo

        if "${BUILD_CMD[@]}"; then
          echo
          echo "[Watcher] ✅ Build succeeded."
          echo "[Watcher] Kill gdb sessions."
          kill_gdb
        else
          rc=$?
          echo
          echo "[Watcher] ❌ Build failed (exit $rc)."
        fi

        echo "[Watcher] Build finished at $(date '+%Y-%m-%d %H:%M:%S')"
        echo
      else
        echo "[Watcher] Skipping: build already in progress (trigger was: $reason)"
      fi
    } 9>"$LOCKFILE"
  fi
done

