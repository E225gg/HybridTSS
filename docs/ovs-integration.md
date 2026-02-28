# OVS Integration Plan for HybridTSS

## Goals
- Integrate HybridTSS as an optional classifier backend in Open vSwitch (OVS) for multi-field packet classification.
- Preserve OVS correctness and fallbacks; feature-flagged rollout with easy disable/enable.
- Expose runtime observability (stats, options) via `ovs-appctl`/unixctl.
- Support x86_64 and arm64; keep CI coverage for both.

## Scope and Touch Points
- **Datapath hook**: replace/augment tuple space search path with HybridTSS for flow classification (userspace datapath first; kernel datapath later/optional).
- **Rule translation**: map OVS flows (match fields) into HybridTSS rules; ensure priority ordering is preserved.
- **Lifecycle**: construct HybridTSS tree on flow-table updates; support incremental updates (insert/delete) as in current HybridTSS API.
- **Configuration**: pass HybridOptions at init; allow runtime changes where safe (e.g., training seed/loop for rebuilds) via unixctl.
- **Fallback**: on errors or unsupported configurations, fall back to existing OVS classifier and surface warnings.

## Support Matrix
- Architectures: x86_64 and arm64 (parity with current CI matrix).
- Build: GCC/Clang with OpenMP; C++14 baseline (matches repo). OVS build flag to enable HybridTSS (e.g., `--enable-hybridtss`).
- Modes: userspace datapath initially; kernel datapath is future work.

## unixctl / ovs-appctl hooks
- `hybridtss/show` – print status, options in effect, rule/packet counts, misclass count, and last build time.
- `hybridtss/reload [opts...]` – rebuild with provided HybridOptions (ht-* flags), preserving flows.
- `hybridtss/enable` / `hybridtss/disable` – toggle HybridTSS backend; disable reverts to stock classifier.
- `hybridtss/stats` – emit metrics snapshot (throughput, update timings, misclassifications, memory sizes) and optionally append to a CSV/JSON sink.

## Metrics & Observability
- Record HybridOptions alongside metrics (already in benchmark driver) and surface via unixctl `stats`.
- Export counters: constructions, inserts, deletes, avg classify/update times, misclassifications, Q-table sizes, hash sizes.
- Provide a lightweight debug log level for training progress (gated by build flag or runtime toggle).

## Rollout Plan
1. Add OVS build flag and stub backend that wraps existing HybridTSS API; wire rule translation and lifecycle.
2. Implement unixctl commands above; include a safe reload path.
3. Add userspace datapath integration tests and a smoketest with a small ruleset (x64/arm64).
4. Add documentation to OVS tree (INSTALL/README) and this repo; keep CI matrix parity.

## Open Questions / Later Work
- Kernel datapath support feasibility and performance.
- Sharing Q-table/training across tables vs per-table instances.
- Persistence of trained state across restarts.
