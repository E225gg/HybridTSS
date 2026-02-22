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

## Lint

```bash
uvx cpp-linter --style file --lines-changed-only true --file-annotations false --tidy-checks="*"
```
