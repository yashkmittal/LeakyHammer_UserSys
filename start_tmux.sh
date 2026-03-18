#!/bin/bash

# Launches a tmux session for LeakyHammer experiments.
# If a session already exists, reattaches to it.
#
# Usage:
#   ./start_tmux.sh                          # interactive session
#   ./start_tmux.sh setup <container_cmd>    # run container_setup.sh
#   ./start_tmux.sh local <container_cmd>    # run run_parallel_local_container.sh
#   ./start_tmux.sh slurm <container_cmd>    # run run_parallel_slurm_container.sh

SESSION="leakyhammer"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TMUX_CONF="$SCRIPT_DIR/.tmux.conf"

tmux_start() {
    if tmux has-session -t "$SESSION" 2>/dev/null; then
        echo "Session '$SESSION' already exists. Reattaching..."
        tmux attach-session -t "$SESSION"
        return
    fi

    local cmd="${1:-}"
    local container_cmd="${2:-}"

    case "$cmd" in
        setup)
            tmux new-session -d -s "$SESSION" -c "$SCRIPT_DIR" -n "setup"
            tmux send-keys -t "$SESSION:setup" \
                "bash container_setup.sh $container_cmd 2>&1 | tee setup_\$(date +%Y%m%d_%H%M%S).log" Enter
            ;;
        local)
            tmux new-session -d -s "$SESSION" -c "$SCRIPT_DIR" -n "experiments"
            tmux send-keys -t "$SESSION:experiments" \
                "bash gem5/run_parallel_local_container.sh $container_cmd 2>&1 | tee experiments_\$(date +%Y%m%d_%H%M%S).log" Enter
            ;;
        slurm)
            tmux new-session -d -s "$SESSION" -c "$SCRIPT_DIR" -n "experiments"
            tmux send-keys -t "$SESSION:experiments" \
                "bash gem5/run_parallel_slurm_container.sh $container_cmd 2>&1 | tee experiments_\$(date +%Y%m%d_%H%M%S).log" Enter
            ;;
        *)
            tmux new-session -d -s "$SESSION" -c "$SCRIPT_DIR" -n "main"
            ;;
    esac

    tmux attach-session -t "$SESSION"
}

# Load the project-local tmux config if tmux is available
if command -v tmux &>/dev/null; then
    export TMUX_CONF
    alias tmux="tmux -f $TMUX_CONF"
fi

tmux_start "$@"
