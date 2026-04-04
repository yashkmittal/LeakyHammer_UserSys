#!/bin/bash

# run_recommended_subset.sh
# Executes a diverse yet non-exhaustive subset of LeakyHammer tests.

# Defaults
CONTAINER_CMD=""
USE_CONTAINER=false

# Simple argument parsing
while [[ "$#" -gt 0 ]]; do
    case $1 in
        docker|podman) CONTAINER_CMD="$1"; USE_CONTAINER=true ;;
        *) echo "Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

echo "--- STEP 1: Running Small POC/Latency Tests (Figures 2, 3, 6) ---"
if [ "$USE_CONTAINER" = true ]; then
    "$CONTAINER_CMD" run --rm --user "$(id -u):$(id -g)" -v "$PWD":/app leakyhammer_artifact ./get_all_results_local.sh
else
    sh get_all_results_local.sh
fi

echo "--- STEP 2: Setting up Recommended Noise/Baseline Subset ---"
SETUP_OPTS=""
if [ "$USE_CONTAINER" = true ]; then
    SETUP_OPTS="-c -cc $CONTAINER_CMD"
fi

uv run python3 result-scripts/setup_recommended_subset.py $SETUP_OPTS

echo "--- STEP 3: Executing Subset Simulations ---"
# We can use the existing execute_run_script.py but we need it to point to our subset file
# Or simply run the generated run_subset.sh which contains the specific subset.

# To keep parallelism, we use the project's executor but tell it to use our run_subset.sh
# Actually, execute_run_script.py is hardcoded to read 'run.sh'. 
# Let's temporarily link run_subset.sh to run.sh for the executor.

mv run.sh run.sh.bak 2>/dev/null
cp run_subset.sh run.sh

uv run python3 result-scripts/execute_run_script.py

# Restore run.sh
mv run.sh.bak run.sh 2>/dev/null

echo "--- Subset Execution Completed ---"
echo "You can now run 'sh plot_figures.sh' (native) or 'sh plot_figures_container.sh' (container) to view results."
