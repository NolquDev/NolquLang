# Nolqu Feature Gap & Implementation Plan (v1.2.2a9)

This document turns current limitations into an execution plan for upcoming releases.

---

## 1) Priority P0 — Must-have for next stable

### A. Circular import detection
- **Current gap:** circular imports can loop forever.
- **Target:** detect `A -> B -> A` and raise `ImportError` with cycle chain.
- **Acceptance:** cycle detection tests pass; no infinite compile-time recursion.

### B. Transpiler parity (minimum)
- **Current gap:** codegen lacks support for arrays, `try/catch`, and `import`.
- **Target:** parity for common language constructs used in examples/tests.
- **Acceptance:** `nq compile` supports core examples without manual rewrites.

### C. Test strategy upgrade
- **Current gap:** `make test` still focuses on smoke examples.
- **Target:** add regression suites for parser/compiler/VM/JIT edge cases.
- **Acceptance:** CI-like test matrix with clear pass/fail by subsystem.

---

## 2) Priority P1 — Language power

### A. Closures
- Add lexical capture for outer-scope locals.
- Define capture lifetime and GC semantics.
- Add parser/compiler/runtime tests for nested closures and upvalues.

### B. Hash maps
- Add map literal + indexing semantics.
- Define hashing/equality behavior for supported key types.
- Add stdlib helpers for map keys/values/merge operations.

---

## 3) Priority P2 — Performance

### A. Bytecode cache (`.nqc`)
- Save/load compiled chunks with version guards and compatibility checks.
- Reduce startup time for repeated script execution.

### B. Optimizer pass
- Add constant folding and simple peephole optimizations in compiler pipeline.

### C. JIT roadmap (post-a9)
- Better cache policy and eviction metrics.
- Optional hot-loop threshold mode (`auto` vs forced).
- Benchmark harness with reproducible perf snapshots.

---

## 4) Priority P3 — Ecosystem

- `stdlib/regex.nq`
- `stdlib/csv.nq`
- `stdlib/base64.nq`
- `stdlib/http.nq`

Each module should ship with:
1. docs page,
2. examples,
3. tests,
4. error semantics.

---

## 5) Governance checklist per release

- Keep `README.md`, `docs/README.md`, and `ROADMAP.md` version labels aligned.
- Keep stdlib module/function counts aligned across docs.
- Require changelog entry + tests for every language/runtime feature.
