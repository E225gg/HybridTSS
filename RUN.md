## compile
for x86
```bash
make -f x86/makefile
make clean -f x86/makefile
```

for arm
```bash
make -f arm/makefile
make clean -f arm/makefile
```

## run
```bash
./main -r ./Data/acl1_1k -p ./Data/acl1_1k_trace
```

## lint

```bash
uvx cpp-linter --style file --lines-changed-only true --file-annotations false --tidy-checks="*"
```