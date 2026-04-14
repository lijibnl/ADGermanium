#!/bin/bash
# auto-update-binary.sh
#   Continuously monitor the changes to local files, and on changes:
#   - Rebuilds both the module and the IOC
#   - Kill the IOC if the build succeeds or if not rebuilt (uses `pkill -f germaniumDetector.*st\.cmd`)
#

set -u

### ── Project Configuration ───────────────────────────────────────────────────
# Edit this section for each module / IOC.

MOD_DIR="/home/liji/data/prj/germanium/ADGermanium"
IOC_DIR=$(pwd)
GIT_BRANCH="async-zmq"

# Directories and file extensions to watch (associative array)
# Keys: directories to watch
# Values: regex alternation of extensions (without leading dot)
declare -A WATCH_EXTS=(
  ["$MOD_DIR/germaniumApp/src"]="c|h|cpp|hpp|tpp|dbd|Makefile"
  ["$MOD_DIR/germaniumApp/Db"]="db|template|substitutions|Makefile"
)

### ── General Settings ────────────────────────────────────────────────────────
# These rarely need changing.

BUILD_CMD=( make )
DEBOUNCE_SECONDS=0.8
GIT_POLL_INTERVAL=10
EVENTS="close_write,moved_to,delete"
EXCLUDE_REGEX='(^|/)\.|(~$)|(\.sw[pxon]$)|(^#.*#$)'
RECURSIVE=0
LOCKFILE="/tmp/auto-update.$(basename "$MOD_DIR").$$.lock"

echo "This script must be run from IOC directory @ $IOC_DIR"
echo

### ── GitHub Polling ──────────────────────────────────────────────────────────

# Pull from GitHub if upstream has new commits.
# The pulled files trigger inotify events, which drive the rebuild.
git_poll_loop() {
  echo "[GitPoll] Monitoring $MOD_DIR (branch=$GIT_BRANCH, every ${GIT_POLL_INTERVAL}s)"
  while true; do
    sleep "$GIT_POLL_INTERVAL"

    local_sha=$(git -C "$MOD_DIR" rev-parse "$GIT_BRANCH" 2>/dev/null) || continue
    git -C "$MOD_DIR" fetch origin "$GIT_BRANCH" --quiet 2>/dev/null || continue
    remote_sha=$(git -C "$MOD_DIR" rev-parse "origin/$GIT_BRANCH" 2>/dev/null) || continue

    if [[ "$local_sha" != "$remote_sha" ]]; then
      echo "[GitPoll] $(date '+%Y-%m-%d %H:%M:%S')  New commits: ${local_sha:0:7} -> ${remote_sha:0:7}"
      # Stash any local/untracked changes that would block the merge
      if ! git -C "$MOD_DIR" diff --quiet 2>/dev/null || \
         [[ -n "$(git -C "$MOD_DIR" ls-files --others --exclude-standard 2>/dev/null)" ]]; then
        git -C "$MOD_DIR" stash push --include-untracked --quiet -m "auto-update stash" 2>/dev/null
        echo "[GitPoll]   Stashed local changes"
      fi
      git -C "$MOD_DIR" pull --ff-only origin "$GIT_BRANCH" --quiet 2>&1 | \
        while IFS= read -r line; do echo "[GitPoll]   $line"; done
      echo "[GitPoll] Pull complete — inotify will trigger rebuild"
    fi
  done
}

# Clean shutdown: kill all child processes on exit/signal
cleanup() {
  trap - EXIT INT TERM   # prevent re-entry
  echo "[auto-update] Shutting down..."
  kill -- -$$ 2>/dev/null   # kill process group by PGID
}
trap cleanup INT TERM

# Start GitHub poller in background
git_poll_loop &

### ── Inotify Watcher ────────────────────────────────────────────────────────

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
      IFS='|' read -ra extarr <<< "$exts"
      for ext in "${extarr[@]}"; do
        if [[ "$ext" == "Makefile" || "$ext" == "Makefile" ]]; then
          # Match files named exactly 'Makefile' (or any extensionless name in the list)
          if [[ "$file" == "$ext" ]]; then
            MATCH_DIR="$d"
            MATCH_REASON="$(basename "$d") change: $file ($event)"
            return 0
          fi
        fi
        # Match files with extension
        if [[ "$file" =~ \.${ext}$ ]]; then
          MATCH_DIR="$d"
          MATCH_REASON="$(basename "$d") change: $file ($event)"
          return 0
        fi
      done
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

    # Debounce/coalesce: drain further events for a short window
    while read -r -t "$DEBOUNCE_SECONDS" _path _file _event; do :; done

    # Lock to prevent overlapping builds
    {
      if flock -n 9; then
        clear
        echo "[Watcher] Trigger: $reason"
        echo

        echo "Entering module directory @ $MOD_DIR..."
        cd "$MOD_DIR"
        echo "[Watcher] Starting module build at $(date '+%Y-%m-%d %H:%M:%S')"
        echo

        if "${BUILD_CMD[@]}"; then
          echo
          echo "[Watcher] ✅ Module build succeeded."
          MODULE_OK=1
        else
          rc=$?
          echo
          echo "[Watcher] ❌ Module build failed (exit $rc)."
          MODULE_OK=0
        fi

        echo

        echo "Entering IOC directory @ $IOC_DIR..."
        echo
        cd -
        echo "[Watcher] Starting IOC build at $(date '+%Y-%m-%d %H:%M:%S')"
        echo

        if [[ $MODULE_OK -eq 1 ]]; then
          if "${BUILD_CMD[@]}"; then
            echo
            echo "[Watcher] ✅ IOC build succeeded."
            echo "[Watcher] Restarting IOC..."
            pkill -f germaniumDetector.*st\.cmd
          else
            rc=$?
            echo
            echo "[Watcher] ❌ IOC build failed (exit $rc)."
          fi
        else
          echo "[Watcher] Skipping IOC build due to module build failure."
        fi

        echo "[Watcher] Build finished at $(date '+%Y-%m-%d %H:%M:%S')"
        echo "Now @ $(pwd)"
        echo
      else
        echo "[Watcher] Skipping: build already in progress (trigger was: $reason)"
      fi
    } 9>"$LOCKFILE"
  fi
done
