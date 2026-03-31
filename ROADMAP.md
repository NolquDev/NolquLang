# Nolqu Roadmap

## Released ✅

| Version | Focus |
|---------|-------|
| v0.1 – v0.5 | Foundation through extended stdlib |
| v0.6 | Error handling (`try`/`catch`/`end`) |
| v0.7 | File I/O |
| v0.8-beta | Mark-and-sweep garbage collector |
| v0.9-beta | Stabilization (assert, clock, is_*, "did you mean?") |
| v1.0.0-rc1 | C/C++ split, memory bug fixes |
| **v1.0.0** | **First stable release** |
| v1.1.0 | break/continue, 5 stdlib modules, project health files |
| v1.1.1a2 | for-in loop, compound assign +=, bug fixes, stdlib/os+fmt |
| v1.1.1a3 | Undefined var catchable, docs accuracy pass |
| v1.1.1a4 | Stdlib expanded, alpha banner, docs updated |
| v1.1.1a5 | null, const, default params, slice, semantics.md |
| v1.1.1a6 | Typed errors, bool(), import syntax, stdlib/io, real examples |
| v1.2.0-rc1 | Stabilization, help fix, docs cleanup |
| v1.2.0-rc2 | Critical crash fix, import/chaining removed, docs |
| v1.2.0 | Stable release |
| v1.2.1a1 | Comparison chaining, from-import, ImportError |
| **v1.2.2a9** | **ARGS, JIT improvements, stdlib expansion ← current dev** |

---

## In Progress

### v1.2.x hardening (active)

Focus: stabilize current dev line before next stable tag.

- [ ] Close open correctness gaps in docs/dev Known Limitations
  - [ ] Cycle detection for circular imports
  - [ ] Forward-reference story (or explicit compiler diagnostics)
  - [ ] Transpiler parity for arrays / try-catch / import
- [ ] Expand automated tests beyond smoke examples
  - [ ] Dedicated regression suite for parser/compiler/JIT edge cases
  - [ ] Platform matrix checks (Linux/macOS/Windows/Termux)
- [ ] Documentation consistency pass
  - [ ] Keep version tags aligned across README/docs/roadmap
  - [ ] Keep module/function counts in sync with stdlib

---

## Future

### v1.3 — Language Completeness
- Closures — functions capturing outer variables
- Hash maps as first-class value type (`{"key": value}`)
- `for i in range(n)` — numeric range loop sugar
- Multi-line strings (triple-quote or heredoc)
- String interpolation: `"Hello, {name}!"`
- `not in` operator for arrays
- Better module ergonomics (clear namespace behavior and import diagnostics)

### v1.4 — Standard Library
- `stdlib/regex.nq` — basic pattern matching
- `stdlib/csv.nq` — CSV read/write
- `stdlib/base64.nq` — encode/decode
- `stdlib/http.nq` — simple HTTP client (requires native extension)

### v1.5 — Performance
- Bytecode serialisation (`.nqc` compiled cache)
- Constant folding in compiler
- JIT tiering improvements (profile-guided thresholds, cache policy)
- NaN-boxing value representation

### v2.0 — Major (breaking changes allowed)
- Type annotations (optional, gradual)
- Module namespacing (no global pollution on import)
- WASM compilation target
