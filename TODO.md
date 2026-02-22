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

### Testing
- [x] Create `gen_testdata.py` — generates ClassBench-format rules + matching packet traces
- [x] Generate test data — `Data/test_100` (100 rules), `Data/test_100_trace` (1000 packets)
- [x] Verify all 3 classifiers (PSTSS, CutTSS, HybridTSS) produce correct results with generated data

### Documentation
- [x] Rewrite `README.md` — project overview, architecture, build instructions, input/output format, project structure
- [x] Update `RUN.md` — current makefile options and usage

---

## Not Addressed — Known Pre-existing Issues

### UB in cmap_rehash (risk: too high to fix)
The OVS `cmap` code uses C-style `malloc`/`free`/`memset` to manage memory containing embedded `cmap_node` objects that hold `shared_ptr<Rule>`. This skips constructors/destructors, which is technically undefined behavior. Currently "accidentally correct" because the embedded nodes' `shared_ptr`s happen to be null when they're freed. Fixing this requires a deep structural rewrite of third-party OVS code.

### Tuple copy semantics (risk: too high to fix)
`Tuple` is stored in `std::unordered_map` but has no proper copy/move semantics for its `cmap` member (a C struct with internal pointers). The commented-out destructor `//~Tuple() { Destroy(); }` exists because enabling it causes double-free. Fixing this requires refactoring to pointer storage or making `Tuple` non-copyable.

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
- `TODO.md`, `gen_testdata.py`
- `Data/test_100`, `Data/test_100_trace`
- `results.csv` (benchmark output)
