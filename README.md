# HybridTSS (NeuroTSS)

A packet classification system that combines **reinforcement learning** with **tuple space search** to build adaptive, high-performance multi-field packet classifiers.

## Overview

HybridTSS trains a Q-table via reinforcement learning to decide, per tree node, whether to:
- **Hash** on a chosen dimension/bit combination,
- Delegate to **TupleMerge** (a tuple-space merge structure), or
- Use **linear** search (for small rule sets).

The result is a hybrid tree where each internal node uses the action that maximizes classification throughput for its local rule subset.

Three classifiers are benchmarked:
| Classifier | Description |
|---|---|
| `PriorityTupleSpaceSearch` | Baseline priority-aware tuple space search (OVS-style) |
| `CutTSS` | Decision-tree (HiCuts/HyperCuts-style) + TSS leaf nodes |
| `HybridTSS` | RL-trained hybrid of hash, TupleMerge, and linear |

## Project Structure

```
HybridTSS/
├── main.cpp                  # Entry point, benchmarking, CSV metrics output
├── ElementaryClasses.h       # Rule/Packet types, loadrule/loadpacket, PacketClassifier base
├── makefile                  # Build system (x86, supports DEBUG/ASAN modes)
├── HybridTSS/
│   ├── HybridTSS.h/cpp      # Top-level RL classifier (Q-table training + tree construction)
│   └── SubHybridTSS.h/cpp   # Tree node: hash/TM/linear dispatch, recursive build
├── CutTSS/
│   └── CutTSS.h/cpp         # Decision-tree classifier with TSS leaves
├── OVS/
│   ├── TupleSpaceSearch.h/cpp # PriorityTupleSpaceSearch (PSTSS)
│   ├── cmap.h/cpp            # Concurrent hash map (from Open vSwitch)
│   ├── hash.h                # Hash utilities
│   ├── MapExtensions.h/cpp   # Map helper functions
│   └── random.h              # Random number utilities
├── TupleMerge/
│   ├── TupleMergeOnline.h/cpp # Online tuple-merge classifier
│   └── SlottedTable.h/cpp    # Slotted hash table for tuple merge
├── Data/                     # Rule sets and packet traces (ClassBench format)
└── results.csv               # Benchmark output (auto-generated)
```

## Dependencies

- **C++14** compatible compiler (GCC 5+ or Clang 3.4+)
- **OpenMP** (for parallel Q-table training)
- **POSIX** environment (Linux/macOS)

## Building

### Standard build

```bash
make
```

### Debug build (unoptimized, with `-DDEBUG`)

```bash
make DEBUG=1
```

### AddressSanitizer build (memory error detection)

```bash
make ASAN=1
```

### Tests (GoogleTest)

GoogleTest is fetched automatically via CMake FetchContent. From the repo root:

```bash
make test          # configure + build (Release) + run ctest
make test-verbose  # same, verbose ctest output
make clean-test    # remove CMake build directory
```

Direct CMake usage:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

## Usage

```bash
./main -r <rule_file> -p <packet_file>
```

### Arguments

| Flag | Description |
|---|---|
| `-r <path>` | Path to rule file (ClassBench format) |
| `-p <path>` | Path to packet trace file |
| `--classifier <names>` | Comma-separated classifiers to run (`pstss,cuttss,hybrid`; default all) |
| `--trials <N>` | Classification trials per classifier (default 10) |
| `--skip-updates` / `--run-updates` | Disable/enable update benchmark (default on) |
| `--seed <u64>` | RNG seed for deterministic update sequence (default 1) |
| `--metrics <path>` | CSV output path (default `results.csv`) |
| `--append-metrics` | Append to metrics file instead of overwriting |
| `--ht-binth <int>` | HybridTSS linear search threshold |
| `--ht-rtssleaf <float>` | HybridTSS tuple-merge vs hash threshold |
| `--ht-loop <int>` | HybridTSS training episodes |
| `--ht-lr <float>` | HybridTSS learning rate |
| `--ht-decay <float>` | HybridTSS learning rate decay |
| `--ht-epsilon0 <float>` | HybridTSS initial epsilon |
| `--ht-epsilon-min <float>` | HybridTSS minimum epsilon |
| `--ht-epsilon-decay <float>` | HybridTSS epsilon decay factor |
| `--ht-state-bits <int>` | HybridTSS state bits (Q-table size) |
| `--ht-action-bits <int>` | HybridTSS action bits |
| `--ht-hash-inflation <int>` | HybridTSS hash inflation factor |
| `--ht-seed <u64>` | HybridTSS training seed (0 = time-based) |

### Example

```bash
./main -r ./Data/acl1_1k -p ./Data/acl1_1k_trace --classifier hybrid --ht-loop 40 --ht-hash-inflation 8 --ht-seed 123
```

### Model Artifact Contract

- HybridTSS uses a versioned binary model artifact (`.qtable`) for train/use
  separation.
- Contract details (layout, compatibility, and failure reasons) are documented
  in `docs/htq1-model-contract.md`.

## Input Formats

### Rule file (ClassBench format)

Each line defines a 5-tuple rule:

```
@<sip1>.<sip2>.<sip3>.<sip4>/<smask>  <dip1>.<dip2>.<dip3>.<dip4>/<dmask>  <sport_lo> : <sport_hi>  <dport_lo> : <dport_hi>  <protocol>/<proto_mask>  <ht>/<htmask>
```

Fields:
- Source IP / prefix length
- Destination IP / prefix length
- Source port range
- Destination port range
- Protocol / protocol mask
- Hash table / hash table mask (unused by classifier, parsed for compatibility)

### Packet trace file

Each line is a space-separated packet header:

```
<src_ip> <dst_ip> <src_port> <dst_port> <protocol> <proto_mask> <expected_fid>
```

The last field (`fid`) is the expected matching rule index, used for correctness checking.

## Output

### Console output

For each classifier, the program prints:
- Construction time (ms)
- Classification throughput (Mpps) and average latency (us)
- Misclassified packet count
- Update throughput (Mpps) for random insert/delete mix

### CSV metrics (`results.csv`)

Auto-generated with columns:

```
classifier,ruleset,num_rules,num_packets,construction_time_ms,avg_classify_us,classify_mpps,misclassified,avg_update_us,update_mpps,ht_binth,ht_rtssleaf,ht_loop,ht_lr,ht_decay,ht_epsilon0,ht_epsilon_min,ht_epsilon_decay,ht_state_bits,ht_action_bits,ht_hash_inflation,ht_seed
```

## Architecture Notes

### Reinforcement Learning Training

- **State encoding**: 20-bit bitmask tracking which dimension/bit combinations have been selected
- **Action encoding**: 6-bit value = (dimension << 4) | bit_position
- **Training**: Parallel episodes using thread-local Q-tables with OpenMP, merged after training
- **Reward**: Based on classification structure quality (normalized via tanh)

### Memory Management

All classifiers properly clean up via destructors:
- `HybridTSS::~HybridTSS()` calls `root->recurDelete()` to free the entire tree
- `CutTSSNode::~CutTSSNode()` recursively deletes children and PSTSS leaves
- `PriorityTupleSpaceSearch::~PriorityTupleSpaceSearch()` deletes all priority tuples
- Base class `PacketClassifier` has a virtual destructor for safe polymorphic deletion
