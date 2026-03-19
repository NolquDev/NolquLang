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
| **v1.2.0-rc1** | **Stabilization, help fix, docs cleanup — release candidate ← current** |

---

## In Progress

### v1.2.0 — Stable release 🎯

Criteria to promote rc1 → stable v1.2.0:
- [ ] All known bugs closed
- [x] Full test suite green (33/33 ✅)
- [x] ASan + UBSan clean ✅
- [x] Documentation accurate and complete ✅
- [ ] Tested on Linux, macOS, Windows (MinGW), Termux
- [x] No regressions vs v1.0.0

---

## Future (Post-1.1.1)

### v1.2 — Language Completeness
- Closures — functions capturing outer variables
- `for i in range(n)` — numeric range loop sugar
- Multi-line strings (triple-quote or heredoc)
- String interpolation: `"Hello, {name}!"`
- `not in` operator for arrays

### v1.3 — Standard Library
- `stdlib/regex.nq` — basic pattern matching
- `stdlib/csv.nq` — CSV read/write
- `stdlib/base64.nq` — encode/decode
- `stdlib/http.nq` — simple HTTP client (requires native extension)

### v1.4 — Performance
- Bytecode serialisation (`.nqc` compiled cache)
- Constant folding in compiler
- NaN-boxing value representation

### v2.0 — Major (breaking changes allowed)
- Type annotations (optional, gradual)
- Hash maps as a first-class type: `{"key": value}`
- Module namespacing (no global pollution on import)
- WASM compilation target
