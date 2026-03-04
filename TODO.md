# HybridTSS — TODO

## Completed

### Stability & Correctness
- [x] Fix null pointer dereferences in `SubHybridTSS` — added null checks for `bigClassifier` in `ClassifyAPacket`, `DeleteRule`, `InsertRule`, `FindRule`, `FindPacket`
- [x] Fix uninitialized pointers in all 4 `SubHybridTSS` constructors — `TMO`, `pstss`, `par` now initialized to `nullptr` (was causing SEGV during RL training)
- [x] Fix `Rule` constructor member initializer order mismatch (`prefix_length` before `range`)
- [x] Fix C++17 structured bindings in `HybridTSS.cpp` `train()` — replaced with C++14-compatible `.first`/`.second`
- [x] Fix `ANDBITS1`/`ANDBITS2` macros — added missing parentheses for operator precedence

### Memory Leaks
- [x] Fix leaks in `main.cpp` — `delete` classifiers (PSTSS, CT, HT), `fclose` file handles (fpr, fpt)
- [x] Fix leaks in `SubHybridTSS::recurDelete()` — now frees `TMO` and `pstss`
- [x] Add `~SubHybridTSS()` destructor — calls `recurDelete()`, fixes training loop leak when stack-allocated root goes out of scope
- [x] Simplify `~HybridTSS()` — now just `delete root;` (SubHybridTSS destructor handles cleanup)
- [x] Fix leaks in `CutTSS` — added `~CutTSSNode()` destructor (recursive child + PSTSS cleanup), fixed `~CutTSS()` to delete nodeSet entries and PSbig
- [x] Fix leaks in `PriorityTupleSpaceSearch` — added destructor, fixed `DeleteRule` to properly find/erase/delete empty tuples
- [x] Add `virtual ~PacketClassifier() = default;` — undefined behavior when deleting derived objects through base pointer
- [x] Fix `Tuple::Deletion` — `delete found_node` after `cmap_remove` (node was unlinked but never freed)
- [x] Fix `SlottedTable::Deletion` — `delete found_node` after `cmap_remove` (same pattern)
- [x] Fix `Tuple::Destroy()` — walk all `cmap_node` objects with `cmap_cursor` and delete before `cmap_destroy`
- [x] Fix `SlottedTable::~SlottedTable()` — walk all `cmap_node` objects with `cmap_cursor` and delete before `cmap_destroy`
- [x] Fix `Tuple` copy semantics / double-free — enabled `~Tuple()` destructor, deleted copy ctor/assignment, implemented move ctor/assignment (transfers `cmap_impl*` ownership), made `Destroy()` idempotent, added null guard to `cmap_destroy`, removed all by-value `Tuple` copies in `TupleSpaceSearch`
- [x] Delete copy/move on `SubHybridTSS`, `HybridTSS`, `SlottedTable` — prevents accidental shallow-copy double-free
- [x] Simplify `SubHybridTSS::recurDelete()` — removed redundant manual recursion before `delete` (destructor handles it)

### API & Code Quality
- [x] Remove `using namespace std` from 4 headers (`SubHybridTSS.h`, `HybridTSS.h`, `CutTSS.h`, `random.h`) — fully qualified `std::` types in headers
- [x] Convert 16 `#define` constants in `ElementaryClasses.h` to `constexpr`
- [x] Add I/O validation — null checks in `loadrule`/`loadpacket`, CLI `-r`/`-p` argument validation, `fopen` failure checks, usage message
- [x] Change `getRules()` to return `const vector<Rule>&` — eliminates O(N) rule copies per `getAction()` call during training BFS
- [x] Change `loadrule`/`loadpacket` from `static` to `inline` — eliminates `-Wunused-function` warnings in every TU except `main.cpp`

### Build & Warnings
- [x] Improve makefile — `-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare`, `DEBUG=1`/`ASAN=1` options, separated .o rules, `.PHONY`
- [x] Fix all third-party code warnings — unused `delta` in `TupleMergeOnline.cpp:43`, unused-but-set `ok` in `cmap.cpp:974`, unused `key` in `cmap.cpp:1125`, unchecked `fscanf` return in `cmap.cpp:72`, unused `t` in `SlottedTable.cpp:131,144`, unused `ignore` in `TupleMergeOnline.cpp:247`

### Observability
- [x] Add structured metrics output — `testPerformance()` writes CSV rows to `results.csv` (classifier, ruleset, num_rules, num_packets, construction_time_ms, avg_classify_us, classify_mpps, misclassified, avg_update_us, update_mpps)
  - [x] Extend metrics with HybridTSS configuration values for reproducibility

### Testing
- [x] Create `scripts/gen_testdata.py` — generates ClassBench-format rules + matching packet traces
- [x] Generate test data — `Data/test_100` (100 rules), `Data/test_100_trace` (1000 packets)
- [x] Verify all 3 classifiers (PSTSS, CutTSS, HybridTSS) produce correct results with generated data

### Documentation
- [x] Rewrite `README.md` — project overview, architecture, build instructions, input/output format, project structure
- [x] Update `RUN.md` — current makefile options and usage

---

## Planned / Next

### Research Track (PR-based, OVS + OpenFlow first)
- [x] PR0: Freeze model artifact contract (HTQ1)
  - Scope: document `.qtable` header/payload layout, compatibility rules, and load failure reasons
  - Acceptance: same artifact loads reproducibly across machines; mismatch errors are explicit
- [ ] PR1: Add OVS-like userspace backend shim in this repo (feature-flagged) [IN PROGRESS]
  - Scope: add `build/lookup/insert/delete/stats` shim API; hybrid/native backend toggle
  - Acceptance: backend can be enabled/disabled safely; disabled path matches native behavior
  - Decisions: default to auto-fallback to native on hybrid init/load unsupported/mismatch
- [ ] PR2: Implement rule translation + fallback policy
  - Scope: support IPv4 5-tuple + proto mask (0/0xFF); unsupported matches fallback to native dpcls
  - Acceptance: OpenFlow rules classify correctly; fallback counter increments on unsupported rules
- [ ] PR3: Add end-to-end research harness (parity + perf)
  - Scope: fixed-seed dataset/flows, HybridTSS vs native dpcls correctness parity, throughput/p99 scripts
  - Acceptance: reproducible report with correctness and performance numbers
- [ ] PR4: Add reload/rollback lite (optional, recommended)
  - Scope: build new instance then swap; keep old instance on failure; basic rollback command
  - Acceptance: reload does not break forwarding; failed reload keeps previous model active
- [ ] PR5: Add minimal observability (optional)
  - Scope: `show-model`, `active_model_id`, `fallback_count`, `reload_success/fail`
  - Acceptance: enough runtime visibility for debugging without hot-path logging

### Milestones (research scope)
- [ ] M1 (Week 1): PR1 complete; userspace OVS-like backend compiles and can be toggled
- [ ] M2 (Week 2): PR2 + PR3 complete; OpenFlow-facing functional flow + parity/perf results available
- [ ] M3 (Week 3, optional): PR4 (+PR5 if time); basic safe reload and minimal runtime visibility

### Out of Scope for research MVP
- [ ] Kernel datapath integration
- [ ] Full production-grade observability/counters per PMD
- [ ] Broad match-field support beyond the defined IPv4 5-tuple subset

### OVS Integration (dpcls backend)
- [ ] Define dpcls backend shim: miniflow/minimask → `Rule`/`Packet`, build/lookup/insert/delete/stats API
- [ ] Decide update model: RCU swap (new instance build + pointer flip) and/or per-PMD instances; keep EMC behavior unchanged
- [ ] Scope support matrix: IPv4 5-tuple + proto mask 0/0xFF only; unknown fields fall back to native dpcls

### Dataplane Readiness (OVS netdev userspace)
- [x] Split training from inference path: `ConstructClassifierSafe()` now supports inference-only mode (`train_online=false`) with fail-fast behavior when no model is provided
- [x] Add model artifact workflow: `SaveQTable`/`LoadQTable` with versioned binary format (`HTQ1`)
- [x] Add immutable runtime engine API hooks for fast path (`ConstructClassifierSafe`, `IsReady`, `LastError`) and explicit not-ready behavior
- [ ] Replace packet hot-path `std::vector` representation with fixed-size `PacketKey` struct for cache-friendly lookups
- [ ] Define OVS PoC acceptance criteria: correctness parity vs native dpcls, throughput, p99 latency, update stall time
- [ ] Add fallback policy: unsupported match fields automatically route to native dpcls backend

### HybridTSS Configurability
- [x] Introduce `HybridOptions` to hold tunables (binth, rtssleaf, loop_num, lr, decay, epsilons, state/action bits, seed, inflation)
- [x] Plumb options from CLI/config into HybridTSS; keep current hardcoded defaults when unspecified
- [x] Record options into metrics (config id + JSON dump) for reproducibility; plan for optional pre-trained QTable load

### Benchmark Driver (main.cpp)
- [x] Add CLI flags: classifiers selection, trials, run/skip updates, seed, metrics path (append/overwrite)
- [ ] Isolate state: rebuild or copy before updates so runs don’t contaminate each other
- [x] Replace global randomness with mt19937_64 seeded from CLI; generate update sequence once per run
- [x] Add HybridTSS model-mode CLI flags and validation: `--ht-train-online`, `--ht-qtable-in`, `--ht-qtable-out`

### Testing
- [x] Add inference-only failure-path test (no QTable input)
- [x] Add train-save-load QTable roundtrip test for HybridTSS

### Observability
- [ ] Define stats to expose (hit/miss, tuple/table counts, rules, build/training time, memory footprint, update latency)
- [ ] Add lightweight per-PMD counters and unixctl/ovs-appctl dump format (no hot-path logging)

### Dataset Toolchain
- [ ] Evaluate and fork maintained dataset generator (`classbench-ng`) for long-term reproducibility
- [ ] Add dataset manifest (seed, count, generator commit, checksum) and enforce it in CI
- [ ] Keep local `scripts/gen_testdata.py` as a quick smoke-data generator; use ClassBench-ng datasets for reported benchmarks

## Not Addressed — Known Pre-existing Issues

### UB in cmap_rehash (risk: too high to fix)
The OVS `cmap` code uses C-style `malloc`/`free`/`memset` to manage memory containing embedded `cmap_node` objects that hold `shared_ptr<Rule>`. This skips constructors/destructors, which is technically undefined behavior. Currently "accidentally correct" because the embedded nodes' `shared_ptr`s happen to be null when they're freed. Fixing this requires a deep structural rewrite of third-party OVS code.

### QCount memory usage in train() (optimization)
`QCount` in `HybridTSS::train()` allocates a 256MB dense array. Could use a sparse `unordered_map` instead. Not a correctness issue.

---

## Branch & File Summary

**Branch:** `feature/improvements` (from `master` @ `7bdd933`)

**Modified files:**
- `main.cpp`, `ElementaryClasses.h`, `makefile`
- `HybridTSS/SubHybridTSS.h`, `HybridTSS/SubHybridTSS.cpp`
- `HybridTSS/HybridTSS.h`, `HybridTSS/HybridTSS.cpp`
- `CutTSS/CutTSS.h`, `CutTSS/CutTSS.cpp`
- `OVS/TupleSpaceSearch.h`, `OVS/TupleSpaceSearch.cpp`, `OVS/cmap.cpp`, `OVS/random.h`
- `TupleMerge/SlottedTable.h`, `TupleMerge/SlottedTable.cpp`, `TupleMerge/TupleMergeOnline.cpp`
- `README.md`, `RUN.md`

**Created files:**
- `TODO.md`, `scripts/gen_testdata.py`
- `Data/test_100`, `Data/test_100_trace`
- `results.csv` (benchmark output)
