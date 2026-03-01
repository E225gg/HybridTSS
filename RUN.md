## Compile

```bash
make            # standard optimized build
make DEBUG=1    # debug build (unoptimized, -DDEBUG)
make ASAN=1     # AddressSanitizer build (memory error detection)
make clean      # remove build artifacts

# Tests (GoogleTest via CMake)
make test          # configure + build (Release) + run ctest
make test-verbose  # verbose ctest output
make clean-test    # remove CMake build directory
```

## Run

```bash
./main -r <rule_file> -p <packet_file>
```

Example:

```bash
./main -r ./Data/acl1_1k -p ./Data/acl1_1k_trace
```

## Generate test data

```bash
uv run scripts/gen_testdata.py --rules 1000 --packets 10000 --seed 42 --out-dir Data
```

## HybridTSS model workflow (train once, infer many)

### 1) Train and export QTable

```bash
./main -r ./Data/acl1_1k -p ./Data/acl1_1k_trace \
  --classifier hybrid \
  --ht-train-online 1 \
  --ht-qtable-out ./Data/hybrid_acl1_1k.qtable
```

### 2) Inference-only (no online training)

```bash
./main -r ./Data/acl1_1k -p ./Data/acl1_1k_trace \
  --classifier hybrid \
  --ht-train-online 0 \
  --ht-qtable-in ./Data/hybrid_acl1_1k.qtable
```

If `--ht-train-online 0` is used without `--ht-qtable-in`, the run fails fast with an explicit error.

## Lint

```bash
uvx cpp-linter --style file --lines-changed-only true --file-annotations false --tidy-checks="*"
```
