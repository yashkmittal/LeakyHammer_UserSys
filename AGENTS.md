# AGENTS.md — LeakyHammer + DREAM-C User-Sys Workspace

This file gives a fresh LLM session everything it needs to be productive in
this repository. Read this first.

---

## 1. What this project is

This repo extends the official artifact for the MICRO 2025 paper
**"Understanding and Mitigating Covert and Side Channel Vulnerabilities
Introduced by RowHammer Defenses"** (Bostancı et al., ETH Zürich /
[arXiv:2503.17891](https://arxiv.org/pdf/2503.17891)). The paper introduces
**LeakyHammer**, a class of attacks that turn DRAM RowHammer *defenses* into
timing covert channels and side channels.

We have:

1. The original LeakyHammer artifact (gem5 + Ramulator2, two RowHammer
   defenses: PRAC and RFM, plus a website-fingerprinting side channel).
2. **A new defense, DREAM-C, that we ported in from a separate paper
   (`DREAM.pdf` in the repo root) and integrated into the same
   evaluation harness.**

The point of the work is to evaluate whether DREAM-C — a more recent,
counter-based RowHammer mitigation — is more or less leaky than the
industry-standard PRAC/RFM defenses, using the LeakyHammer methodology.

### 1.1 The LeakyHammer paper in one paragraph

RowHammer is a DRAM bug where repeatedly activating a row causes bit flips
in nearby rows. Defenses like **PRAC** (Per-Row Activation Counting) and
**RFM** (Refresh Management) prevent flips by issuing extra refresh commands
when a row gets hit too often. These extra refreshes block memory accesses,
making them slower. LeakyHammer's insight: a **sender** process can
deliberately hammer a row to trigger one of these refreshes, and a
**receiver** process on the same machine can detect the resulting latency
spike. Sender and receiver both run on a simulated multi-core gem5 system
sharing one DRAM. Send a `1` by hammering, send a `0` by sleeping; receiver
probes a few rows every ~µs and decodes bits from the latency it sees. The
paper measures channel capacity (Kbps) at various noise levels.

### 1.2 The three defenses in this repo

| Preset | Source | What it does | Status in our repo |
|--------|--------|--------------|---------------------|
| **PRAC** | LeakyHammer artifact | Tracks per-row ACT count; triggers a "back-off" refresh once a counter saturates | Untouched original |
| **RFM**  | LeakyHammer artifact | Memory controller issues periodic RFM commands proportional to recent ACT count | Receiver patched (drift fix, see §6) |
| **DREAM-C** | Ported from `DREAM.pdf` | Counts ACTs at *gang* (group of rows that map to one shared counter via random per-bank XOR mask) granularity; issues `DRFMab` (Directed Refresh Management, all banks) when a gang counter saturates | New addition: Ramulator plugin + new sender/receiver binaries + receiver patched. **Plugin rewritten 2026-04-25 — see §5.1.** |

DREAM-C uses the JEDEC-standard DRFMab command path that already existed in
the Ramulator2 DDR5-VRR model.

---

## 2. Repository layout

```
LeakyHammer_UserSys/
├── README.md                       # Original artifact README (paper authors')
├── LeakyHammer.pdf                 # The paper
├── DREAM.pdf                       # Paper for the DREAM mitigation we ported
├── RECEIVER_DRIFT_FIX.md           # Detailed writeup of our receiver bug + fix
├── DREAM_INTEGRATION_REPORT.md     # **OUTDATED — do not trust.**
├── AGENTS.md                       # This file
├── leakyhammer_artifact.tar        # Pre-built Docker image (~452 MB)
├── Dockerfile                      # If you ever need to rebuild the image
├── container_setup.sh              # First-time environment setup
└── gem5/                           # Everything that actually runs
    ├── attack-scripts/             # C++ source for senders/receivers
    │   ├── rowhammer-side.cc/.hh   # Shared library: probe loops, hammer routines
    │   ├── rowhammer-prac-{sender,receiver}.cc
    │   ├── rowhammer-rfm-{sender,receiver}.cc
    │   ├── rowhammer-dream-{sender,receiver}.cc   # Our addition
    │   └── rowhammer-mr-noise.cc                  # The "noise generator" 3rd process
    ├── attack-binaries/            # Compiled outputs of attack-scripts/
    ├── compile-scripts/            # Per-target recompile shells
    │   ├── recompile_rfm.sh
    │   ├── recompile_dream.sh      # Our addition
    │   └── ...
    ├── compile_attack_scripts.sh   # Compiles all attack binaries at once
    ├── configs/rhsc/ramulator/     # Ramulator2 YAML configs per defense
    │   ├── prac.yaml
    │   ├── rfm.yaml
    │   └── dream.yaml              # Our addition
    ├── ext/ramulator2/             # Ramulator2 source (vendored)
    │   └── ramulator2/src/dram_controller/impl/plugin/dream.cpp  # Our DREAM-C plugin
    ├── result-scripts/             # Python: orchestrate experiments + parse results
    │   ├── run_config.py           # **Edit this to change what runs.**
    │   ├── setup_test.py           # Generates run_scripts/*.sh and run.sh
    │   ├── parse_and_print.py      # Parses log txts → CSVs, prints summary, calls plotters
    │   └── execute_run_script.py   # Wrapper that local runners invoke
    ├── plot-scripts/               # Python: generate the paper's PDF figures
    │   ├── figure4_plotter.py      # PRAC noiseless results
    │   ├── figure7_plotter.py      # RFM (and our DREAM) results
    │   └── figure{2,3,6}_plotter.py  # Other paper figures (less relevant for us)
    ├── run_scripts/                # **Generated** per-experiment shell scripts
    ├── run.sh                      # **Generated** master list (one `sh run_scripts/...` per line)
    ├── results/                    # Per-experiment output txts + final CSVs
    │   ├── prac/{baseline,noise}/
    │   ├── rfm/{baseline,noise}/
    │   ├── dream/{baseline,noise}/
    │   └── ber_*.csv, noise_ber_*.csv     # Aggregated results
    ├── figures/                    # PDF outputs of the plotters
    └── build/X86/gem5.opt          # Built simulator (inside Docker)
```

---

## 3. The experiment matrix

`gem5/result-scripts/run_config.py` defines what runs. Current settings:

```python
PERSONAL_RUN_THREADS = 4              # parallel docker containers (DON'T crank past 4 — OOM risk)
MSG_BYTES = 100                       # 800 bits per message
DATA_PATTERNS = ["0x00", "0x55", "0xAA", "0xFF"]  # alternating + constant bit patterns
```

Per-preset parameters (in the `get_preset_variables()` function):

| Preset | TXN_PERIOD (ns) | Sender binary  | Receiver binary  | Noise rates (ACTs/window) |
|--------|-----------------|----------------|------------------|----------------------------|
| PRAC   | 25 000          | `prac_sender`  | `prac_receiver`  | `[275, 475, 1075, 1975]`   |
| RFM    | 20 000          | `rfm_sender`   | `rfm_receiver`   | `[200, 263, 325]`          |
| DREAM  | 20 000          | `dream_sender` | `dream_receiver` | `[200, 263, 325]`          |

A single experiment looks like:
- 2 (or 3, with noise) gem5 processes share one simulated DDR5 channel.
- Sender hammers a row to send `1`, idles for `0`, one bit per `TXN_PERIOD` window.
- Receiver probes rows every `PROBE_INTERVAL_NS` and counts latency spikes.
- Optional noise generator hits memory at `access_rate` ACTs/window.
- Each run is one process tree inside a `docker run`, takes ~100–150s.

Total matrix = 4 patterns × 4 access rates × 3 presets ≈ 32 baseline + 36 noise
runs (we currently only re-run RFM and DREAM since PRAC source is untouched).

---

## 4. Top-level workflow (the "just run it" path)

All commands assume you are at the repo root.

### 4.1 First time only — load Docker image

```bash
docker load -i leakyhammer_artifact.tar
```

### 4.2 (Re)compile inside the container

There are **two independent build artifacts**, and which one you need to
rebuild depends on what you changed:

| You edited... | Then rebuild...                  | Command |
|---|---|---|
| C++ in `gem5/attack-scripts/` | the userspace sender/receiver binaries | `./compile_attack_scripts.sh` (all) or `./compile-scripts/recompile_<preset>.sh` (one) |
| C++ in `gem5/ext/ramulator2/...` (e.g. `DDR5-VRR.cpp`, `dream.cpp`, any plugin) | `libramulator.so` only | `./rebuild.sh --ramulator` |
| Public ABI of `libramulator.so` (function signature on an exported symbol, or a header included from gem5) | `libramulator.so` **and** relink `gem5.opt` | `./rebuild.sh --all` (or `--ramulator` then `--gem5`) |
| YAML in `gem5/configs/` | nothing — configs are read at runtime | (no rebuild) |

`gem5.opt` dynamically links `libramulator.so` (`gem5/ext/ramulator2/SConscript`
sets `LIBS=['ramulator']` + `RPATH=[ramulator2_path]`), so for any change that
*only* touches Ramulator's internal source files (DRAM model timing values,
plugin internals, scheduler internals, etc.) `--ramulator` is enough — the
next `gem5.opt` invocation picks up the rebuilt `.so`. You only need `--all`
when you change something gem5 itself compiles against.

So a typical "I changed the DREAM plugin" cycle is:

```bash
# 1. Rebuild just Ramulator (~1-2 min):
docker run --rm --user "$(id -u):$(id -g)" \
    -v "$PWD/gem5":/app leakyhammer_artifact \
    "./rebuild.sh --ramulator"

# 2. Optionally rebuild attack binaries too if you also touched attack-scripts/:
docker run --rm --user "$(id -u):$(id -g)" \
    -v "$PWD/gem5":/app leakyhammer_artifact \
    "./compile_attack_scripts.sh"
```

You can sanity-check that the rebuild actually happened by comparing
`stat -c '%y' gem5/ext/ramulator2/ramulator2/libramulator.so` against the
mtime of the file you edited — `libramulator.so` should be *newer* than your
source change.

> **Build flavor note:** `rebuild.sh` always builds Ramulator with
> `-DCMAKE_BUILD_TYPE=Debug`. The `libramulator.so` originally baked into the
> Docker image (`leakyhammer_artifact.tar`) was almost certainly Release. This
> doesn't change *correctness* (DRAM timing is in mem-cycles, fully
> deterministic), but it can shift per-window event counts vs. the originally
> shipped figures (e.g. exact `[RECV] (N RFMs)` counts in the RFM POC differ
> slightly from `figures/figure6bak.pdf` even though the decoded bits match
> 1:1). If you need exact-count parity, change `-DCMAKE_BUILD_TYPE=Debug` to
> `Release` in `rebuild.sh` and nuke `gem5/ext/ramulator2/ramulator2/build/`
> before rebuilding.

### 4.3 Generate the run scripts

```bash
cd gem5
uv run python3 ./result-scripts/setup_test.py -c -cc docker
```

This (re)creates `run_scripts/*.sh` (one per experiment) and rewrites
`run.sh` with one `sh run_scripts/...` line per script. The `-cc docker`
flag makes each generated script wrap its gem5 invocation in
`docker run --rm --user 1000:1000 -v $PWD/gem5:/app leakyhammer_artifact 'gem5.opt ...'`.

### 4.4 Execute the matrix in parallel

```bash
cat run.sh | sed 's/^sh //' | xargs -I{} -P4 sh {}
```

`-P4` matches `PERSONAL_RUN_THREADS = 4`. Wall time ~25 min for the full RFM
+ DREAM matrix on a beefy desktop. **Do not raise to 16 — we OOM'd.**

Per-experiment output goes to `results/<preset>/<baseline|noise>/<stat>.txt`.

### 4.5 Parse, summarize, and plot

```bash
uv run --with pandas --with numpy --with matplotlib --with seaborn \
    python3 result-scripts/parse_and_print.py
```

This walks `results/`, builds `results/ber_*.csv` and `results/noise_ber_*.csv`,
prints a summary to stdout, and calls the plotters in `plot-scripts/` to
write `figures/figure*.pdf`.

Expected summary on a healthy run:

```
Results for baseline PRAC, RFM, and DREAM:
PRAC  Raw Bit Rate (Kbps): 35.668  mean BER: 0.0356
RFM   Raw Bit Rate (Kbps): 48.774  mean BER: 0.0000
DREAM Raw Bit Rate (Kbps): 48.769  mean BER: 0.0484

Noise channel capacity:
PRAC: 12.41 Kbps   RFM: 5.23 Kbps   DREAM: 20.38 Kbps
```

---

## 5. The DREAM-C addition (what we built)

### 5.1 Ramulator plugin (paper-accurate rewrite, 2026-04)

`gem5/ext/ramulator2/ramulator2/src/dram_controller/impl/plugin/dream.cpp`
implements an `IControllerPlugin` named `DREAM`. **The plugin was rewritten
on 2026-04-25 to match the design in `DREAM.pdf` §5. Results from before
that date are not directly comparable** — see the changelog at the end of
this section.

The new design:

- One **DREAM Counter Table (DCT) per sub-channel**, *shared across all
  banks of that sub-channel*. We map each rank in the gem5/Ramulator
  config to one DDR5 sub-channel (JEDEC DDR5: 32 banks per sub-channel;
  our `DDR5_16Gb_x8` preset gives BG=8 × BA=4 = 32 banks per rank, so
  rank ≡ sub-channel cleanly).
- **DCT size**: `num_rows_per_bank / vertical_sharing` entries per
  sub-channel (e.g. 65 536 entries with `vertical_sharing=1` for our
  DDR5_16Gb_x8). Stored as a flat `std::vector<int>` (≈260 KB of host
  RAM total — tiny).
- **Grouping function** is selectable via the `grouping` YAML key:
  - `random` (default, paper §5.2): `idx = (RowID XOR mask[bank]) mod
    num_entries`. Each bank within the sub-channel has a randomly
    initialized `mask[bank]`. Breaks the spatial correlation between
    RowIDs across banks (which under MOP4 mapping otherwise produces hot
    counters; see paper Fig. 13a).
  - `set_assoc` (paper §5.2 baseline): `idx = RowID mod num_entries`.
    Hot-counter prone — only useful as a comparison.
- **On every ACT**: increment the addressed counter; if it reaches the
  `threshold`, issue `vertical_sharing` `DRFMab` commands back-to-back
  (rank-scope, ~280 ns stall each) and reset the counter to 1.
- **DCT reset**: gradual round-robin — one entry per sub-channel reset
  every `reset_period_clk / num_entries` cycles (paper §5.4: "16 entries
  per REF" for a 128 K-entry table). Avoids the cliff in DRFM rate that
  a bulk 64 ms wipe would create.
- We do **not** issue the explicit 32×(ACT + Pre+S) chain that the paper
  describes before each `DRFMab` — the DDR5-VRR DRAM model already accounts
  for the 280 ns rank-wide stall, which is the only thing a LeakyHammer
  attacker can observe. If you ever want to study the DAR-populating
  overhead itself, you'd need to inject those transactions through
  `priority_send` before the `DRFMab`.

Configured via `gem5/configs/rhsc/ramulator/dream.yaml`:

```yaml
plugins:
  - ControllerPlugin:
      impl: DREAM
      threshold: 40             # T_TH; paper uses T_RH/2 (e.g. 250 for T_RH=500)
      vertical_sharing: 1       # 1 -> gang of 32 rows, 2 -> 64, 4 -> 128, 8 -> 256
      grouping: random          # 'random' (default, paper) or 'set_assoc'
      reset_period_ns: 64000000 # tREFW
      seed: 42                  # determinism for the per-bank masks
      debug: false
      gang_size: 32             # back-compat alias; sanity-checked vs vertical_sharing
```

### 5.1.1 Implications for our LeakyHammer experiments

The previous implementation kept **independent counters per (bank, gang)**;
the rewrite makes counters **shared across all 32 banks of the
sub-channel**. Concretely, this means:

- **The attacker's gang counter is now coupled to noise from other
  banks.** Background traffic that hits *any* bank in the same rank can
  now bump the same DCT entry the receiver is watching. Expect the
  noise-vs-capacity curve for DREAM to shift downward; how much depends
  on noise-to-attacker-bank locality (the existing `dream_sender` pins
  to BG=0/Bank=0).
- **The receiver still works without modification.** It probes 1024
  rows in the *same bank*, so it doesn't self-trigger the gang counter.
  But its sensitivity is now controlled by the rank-wide ACT rate, not
  the per-bank rate. Treat the noise-rate axis on `figure7_dream.pdf`
  with appropriate suspicion until a re-run confirms the new shape.
- **`dream_th=40` is no longer a "per-bank" threshold.** Under the
  shared DCT, a uniform random ACT load will hit each counter ~32×
  faster than before. If you want to faithfully model the paper's
  T_RH=500 case, set `threshold: 250` (= T_TH = T_RH/2). The current
  `threshold: 40` is what we had been using; whether it still produces
  a useful attacker SNR under the new design is an empirical question
  that needs a re-run.

### 5.1.2 Changelog

| Date       | Change |
|------------|--------|
| (initial)  | Per-bank gang counters (`m_tusc[bank][gang]`), bulk reset every 64 ms. Deviated from the paper's cross-bank shared DCT design. |
| 2026-04-25 | Rewrote to the paper's cross-bank shared DCT with randomized grouping, vertical sharing, and gradual reset. |
| 2026-04-25 (later) | **Reverted simulator-side timing inflation introduced in commit `e462c3e`.** That commit had bumped `nDRFMab`/`nDRFMsb`/`nRFM*` in `DDR5-VRR.cpp` to 5000 mem cycles (~3 µs each) to make DRFMab events conspicuous from userspace, and inflated three companion latency-band constants in `rowhammer-side.hh` (`PERIODIC_CAP_NS` 1300→8000, `PERIODIC_CAP_NS_RFM` 550→6000, `ACCESS_CAP_NS` 250→2000) plus dropped `rowhammer-rfm-receiver.cc` `ASSERT_THRESH` from 3→1. **Side effect:** broke the RFM POC (decoded all-zeros — see `figures/figure6.broken.pdf` vs `figure6bak.pdf`) because the inflated band missed the larger-but-rarer RFM stalls. Revert restored `nDRFMab` to JEDEC formula `2*BRC*tRRFsb` (~150 ns) and the userspace bands/threshold to the artifact defaults; RFM POC now decodes `MICRO` cleanly. **DREAM is currently broken under this revert** (its detection band `(DREAM_DRFMAB_CAP_NS=2000, PERIODIC_CAP_NS=1300)` is now empty — see §7 step 1 for the re-tuning plan). |

### 5.2 Sender + receiver

`rowhammer-dream-{sender,receiver}.cc` are new C++ files that mimic the
RFM sender/receiver pattern but are tuned for DREAM-C's spread-row
detection: the receiver probes 1024 rows arranged across 1024 DCT gangs
to maximise the chance of being on the same gang as a sender hammer.

`run_config.py` already wires them up:

```python
SENDER   = ".../attack-binaries/dream_sender"
RECEIVER = ".../attack-binaries/dream_receiver"
```

> **Watch out:** the file `DREAM_INTEGRATION_REPORT.md` claims DREAM uses
> the `rfm_sender/rfm_receiver` binaries. **That document is stale.**
> DREAM has its own dedicated binaries — use them.

### 5.3 Receiver drift fix

Both the **DREAM** and **RFM** receivers had a per-window timing bug
(catastrophic BER on `0x55`/`0xAA` patterns; ~0% BER on `0x00`/`0xFF`).
We diagnosed and fixed it with a tightened per-window timeout
(`txn_period - 8000`) plus a phase-resync block that snaps `next_window`
forward by an integer number of periods if the receive loop overruns.

Read **`RECEIVER_DRIFT_FIX.md`** for the full root-cause analysis,
before/after BER tables, and reproduction recipe. PRAC was not affected
and was not modified.

---

## 6. Current results vs. paper

| Preset | Noiseless capacity (ours) | Paper claim | Notes |
|--------|----------------------------|-------------|-------|
| RFM    | **48.77 Kbps** (matrix, pre-`e462c3e`-revert); RFM POC decodes `MICRO` cleanly post-revert | 48.7 Kbps   | Matches paper exactly. Matrix CSVs/figures dated before 2026-04-25 are consistent. |
| PRAC   | ~27.6 Kbps                 | 39.0 Kbps   | Below paper — not yet investigated. PRAC has ~3.6% baseline BER from receiver self-noise (separate issue from drift). |
| DREAM  | **STALE / BROKEN** — old numbers (~33 Kbps noiseless, 20.4 Kbps under noise) were generated against the inflated `nDRFMab=5000` simulator state. After the 2026-04-25 timing revert (§5.1.2) DREAM decoders see no events; both POC and matrix produce all-zeros until the detection band is re-tuned. | (new) | See §7 step 1 for re-tuning plan. |

Figures live in `gem5/figures/`:
- `figure4.pdf` — PRAC channel capacity vs noise intensity
- `figure6.pdf` — RFM POC (current; matches `figure6bak.pdf` at the bit level, per-window RFM counts differ — Debug-build artifact, see §4.2)
- `figure6.broken.pdf` — RFM POC against the inflated-timing state, all-zero decode (kept for reference)
- `figure6bak.pdf` — pre-`e462c3e` working RFM POC reference (Apr 3, original Release `libramulator.so`)
- `figure7.pdf` — RFM channel capacity vs noise intensity
- `figure7_dream.pdf` — DREAM, same axes as figure7 (**stale, regenerate after re-tune**)

---

## 7. What I'm currently working on

**Goal:** Get clean, paper-comparable LeakyHammer covert-channel results
for **DREAM-C**, alongside the original PRAC/RFM, so we can argue about
whether DREAM is more or less leaky than the industry-standard defenses.

**Done:**
- Ported DREAM-C to Ramulator2 as a plugin.
- Wrote DREAM-specific sender/receiver attack binaries.
- Wired DREAM into the LeakyHammer experiment harness (configs, run_config, plotters).
- Diagnosed and fixed a long-standing receiver phase-drift bug that broke
  alternating-bit patterns (`0x55`/`0xAA`) for both DREAM and RFM.
  See `RECEIVER_DRIFT_FIX.md`.
- Reproduced the paper's RFM noiseless capacity (48.77 vs 48.7 Kbps).

**Open / likely next steps:**
1. **Re-tune the DREAM detection band against the now-restored ~150 ns
   DRFMab.** After the 2026-04-25 timing revert (§5.1.2), `nDRFMab` is back
   to the JEDEC formula `2*BRC*tRRFsb` ≈ 240 mem-cycles ≈ 150 ns DRAM-side.
   The userspace probe will see this amplified by the controller's queue
   stall to roughly the same `(250, 550)` ns band as RFM events — meaning
   *latency magnitude alone no longer distinguishes a DRFMab from an RFM*.
   Concrete sub-tasks (see also "Plan for §7 step 1" below):
   - Instrument `dream_receive_count` to print a latency histogram per
     window across a few representative configs (DREAM only, DREAM+noise)
     so we know the actual post-revert spike distribution.
   - Decide on a new discriminator: most likely **rank-scope** (DRFMab
     stalls *every* bank in the rank, RFM stalls only the addressed bank)
     by probing two banks in different bank-groups and requiring
     simultaneous spikes; or **rate** (DRFMab fires once per `threshold`
     ACTs into the gang, much rarer than per-bank RFM under the same load).
   - Update `DREAM_DRFMAB_CAP_NS`/`DREAM_DRFMAB_UPPER_NS` in
     `rowhammer-side.hh` and the `dream_send`/`dream_receive_count`
     bodies in `rowhammer-side.cc`.
   - Wipe stale DREAM results, re-run the matrix (`grep dream`), re-plot.
2. **Re-run the full DREAM matrix** once the detection band is fixed.
   Recommended sequence:
   ```bash
   docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/gem5":/app \
       leakyhammer_artifact "./rebuild.sh --ramulator"
   docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/gem5":/app \
       leakyhammer_artifact "./compile_attack_scripts.sh"

   # Wipe stale DREAM results.
   docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/gem5":/app \
       leakyhammer_artifact "rm -rf /app/results/dream/*"

   # Regenerate run scripts and run the DREAM matrix.
   cd gem5 && uv run python3 ./result-scripts/setup_test.py -c -cc docker
   cat run.sh | grep dream | sed 's/^sh //' | \
       xargs -I{} -P4 sh -c 'echo "[$(date +%H:%M:%S)] START {}"; sh {}; echo "[$(date +%H:%M:%S)] DONE  {}"'

   uv run --with pandas --with numpy --with matplotlib --with seaborn \
       python3 result-scripts/parse_and_print.py
   ```
3. Investigate the PRAC noiseless gap (we get ~28 Kbps, paper gets 39 Kbps).
   This is *not* the drift bug; it looks like receiver self-noise inside
   `prac_receive`. Out of scope for the recent work but the obvious next
   target.
4. Sweep DREAM's `threshold` and `vertical_sharing` parameters in
   `dream.yaml` to characterise how DREAM's detection sensitivity trades
   off against channel capacity. The paper studies T_RH ∈ {125, 250, 500,
   1000} which corresponds to `threshold` ∈ {62, 125, 250, 500} and
   `vertical_sharing` ∈ {1, 2, 4, 8}.
5. Cross-validate the `figure7_dream.pdf` numbers by hand-computing
   capacity from `noise_ber_dream.csv` (after step 2 regenerates them).
6. Optionally: run `setup_recommended_subset.py` instead of `setup_test.py`
   to do the full paper-style 16-rate sweep (it currently uses a 3- or
   4-rate subset to keep wall time down).

---

## 8. Important quirks and gotchas

- **Use Docker, not Podman.** Earlier sessions tried podman by mistake and
  it broke ownership on result files. The image name is
  `leakyhammer_artifact:latest`, loaded from `leakyhammer_artifact.tar`.
- **Cap parallelism at 4.** Each docker container peaks at ~3 GB RSS. The
  paper authors used 16 on a slurm cluster; on a single dev box that
  OOM-kills the host.
- **Files written by gem5 inside docker are owned by uid 1000.** If you
  see permission errors writing CSVs/PDFs, the cause is almost always
  stale root-owned files from an earlier (mis-launched) docker run. Fix:
  ```bash
  docker run --rm -v "$PWD/gem5":/app leakyhammer_artifact \
      "chown -R 1000:1000 /app/results /app/figures"
  ```
- **`pd.read_csv` on the BER CSVs needs `dtype={'sent': str, 'received': str}`**
  because the columns are 800-character binary strings that overflow when
  pandas tries to convert them to floats. The repo's `parse_and_print.py`
  and figure plotters already do this; if you write a new analysis
  script, do it too.
- **`DREAM_INTEGRATION_REPORT.md` is outdated.** It claims DREAM uses RFM's
  sender/receiver binaries — wrong. Use `RECEIVER_DRIFT_FIX.md` and this
  file as the sources of truth.
- **`compile-scripts/recompile_*.sh` only rebuild attack binaries, not the
  Ramulator plugin.** If you edited a `.cpp` under
  `gem5/ext/ramulator2/` (e.g. `dream.cpp`, `DDR5-VRR.cpp`), you must run
  `./rebuild.sh --ramulator` to relink `libramulator.so`. `gem5.opt` does
  *not* need to be rebuilt for internal Ramulator changes — it dynamically
  links the `.so` via `RPATH`. Use `--all` only when changing Ramulator's
  public ABI or a header gem5 itself includes. See §4.2.
  Symptom of running with a stale `.so`: result CSVs come out *bit-identical*
  to a previous run despite source changes.
- **`txt.txt` at the repo root is from a very old run.** Ignore it.
  Always read the latest data from `gem5/results/*.csv` and the latest
  figures from `gem5/figures/`.
- **Per-experiment receiver logs include diagnostic lines:**
  ```
  [RECV] MinSleepAssert: 5692         <-- positive == healthy
  [RECV] Resyncs: 0 (0 bits skipped)  <-- 0 == clean run
  ```
  If `MinSleepAssert` goes negative or `Resyncs` > 0 on any run, the
  drift fix isn't holding — investigate before trusting the resulting BER.

---

## 9. Quick reference — common one-liners

```bash
# Rebuild Ramulator only (after editing any .cpp under ext/ramulator2/)
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/gem5":/app leakyhammer_artifact \
    "./rebuild.sh --ramulator"

# Recompile DREAM attack binaries only (after editing dream-*.cc in attack-scripts/)
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD/gem5":/app leakyhammer_artifact \
    "./compile-scripts/recompile_dream.sh"

# Regenerate run_scripts/ + run.sh
cd gem5 && uv run python3 ./result-scripts/setup_test.py -c -cc docker

# Run the whole matrix, 4 in parallel
cat gem5/run.sh | sed 's/^sh //' | xargs -I{} -P4 sh {}

# Re-parse results into CSVs and rebuild figures
uv run --with pandas --with numpy --with matplotlib --with seaborn \
    python3 gem5/result-scripts/parse_and_print.py

# Inspect one result
less gem5/results/dream/baseline/t20000_100b_p0x55_r0d_dream.txt

# Sanity-check that DREAM/RFM receivers aren't drifting
grep -E "MinSleepAssert|Resyncs" gem5/results/{rfm,dream}/baseline/*.txt
```

---

## 10. References

- `LeakyHammer.pdf` — the source paper.
- `DREAM.pdf` — the DREAM-C paper we ported into the harness.
- `RECEIVER_DRIFT_FIX.md` — full writeup of the receiver phase-drift bug,
  the fix, and before/after numbers.
- `README.md` — the artifact authors' original README. Useful for the
  Slurm-based execution path; locally we use the simpler `xargs -P4`
  recipe in §4.4 above.
