# HTQ1 Model Artifact Contract

This document freezes the v1 HybridTSS model artifact contract (`HTQ1`) so
training and inference can evolve independently.

## Scope

- Recommended artifact file extension: `.qtable` (binary; extension is
  conventional only and not enforced by the runtime loader)
- Producer: HybridTSS training path (`train_online=true` + `qtable_out_path`)
- Consumer: HybridTSS inference path (`train_online=false` + `qtable_in_path`)
- Current writer/reader implementation: `HybridTSS/HybridTSS.cpp`

## Binary Format (HTQ1)

All integer fields are little-endian `uint32_t`.

Header:

1. `magic[4]` = ASCII `HTQ1`
2. `version` = `1`
3. `state_bits`
4. `action_bits`
5. `rows`
6. `cols`

Payload:

- `rows * cols` values, row-major, each value is IEEE-754 `double` (8 bytes)

Canonical sizes:

- `rows == (1 << state_bits)`
- `cols == (1 << action_bits)`

## Compatibility Rules

Load is accepted only if all checks pass:

1. File is readable and header is complete
2. `magic == HTQ1`
3. `version == 1`
4. `state_bits` and `action_bits` exactly match current `HybridOptions`
5. `rows`/`cols` match the bit-derived dimensions
6. Payload length is sufficient for all doubles

Implication:

- A model trained with one `(state_bits, action_bits)` pair is not loadable into
  runtime configured with a different pair.

## Failure Reasons (Current Runtime Messages)

The loader (`LoadQTable()` / `ConstructClassifierSafe()`) reports reasons via
`LastError()` and optional output `err`. The writer (`SaveQTable()`) reports
reasons via output `err` only (it does not update `LastError()` directly unless
the caller propagates the message).

Current messages:

- save path:
  - `QTable is empty; nothing to save`
  - `QTable rows have inconsistent sizes`
  - `failed to open QTable output file: <path>`
  - `failed while writing QTable file: <path>`
- load path:
  - `failed to open QTable input file: <path>`
  - `invalid QTable header: <path>`
  - `QTable magic mismatch: <path>`
  - `QTable version mismatch`
  - `QTable bits mismatch with current HybridOptions`
  - `QTable dimensions mismatch with state/action bits`
  - `QTable payload truncated: <path>`
- precondition path:
  - runtime API (`HybridTSS`): `inference-only mode requires --ht-qtable-in`
  - CLI validator (`./main`): `--ht-train-online 0 requires --ht-qtable-in <path>`
  - `ht-state-bits must be in [1, 30]`
  - `ht-action-bits must be in [1, 30]`

## Train / Use Separation Workflow

Train and export once:

```bash
./main -r ./Data/acl1_1k -p ./Data/acl1_1k_trace \
  --classifier hybrid \
  --ht-train-online 1 \
  --ht-qtable-out ./Data/hybrid_acl1_1k.qtable
```

Inference only:

```bash
./main -r ./Data/acl1_1k -p ./Data/acl1_1k_trace \
  --classifier hybrid \
  --ht-train-online 0 \
  --ht-qtable-in ./Data/hybrid_acl1_1k.qtable
```

## Reproducibility Notes

- Use a fixed `--ht-seed` during training for deterministic runs.
- Keep `(state_bits, action_bits)` identical between training and inference.
- Keep the same architecture assumptions for binary portability
  (little-endian + IEEE-754 double).

## Versioning Policy

- `HTQ1` is frozen as v1 contract.
- Future format changes must use a new magic/version contract (for example
  `HTQ2`) while preserving backward read support where practical.
