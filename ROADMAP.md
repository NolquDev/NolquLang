# Nolqu Roadmap

## Released ✅

| Version | Focus |
|---------|-------|
| v0.1 | Foundation: VM, variables, arithmetic, control flow, functions |
| v0.2 | User input, type builtins, extended string stdlib |
| v0.3 | Arrays, modules (`import`), stdlib/math, stdlib/array |
| v0.4 | Block-scoped locals |
| v0.5 | Extended stdlib (index, repeat, join, random) |
| v0.6 | Error handling (`try`/`catch`/`end`, `error()`) |
| v0.7 | File I/O (read, write, append, lines) |
| v0.8-beta | Mark-and-sweep garbage collector |
| v0.9-beta | Stabilization (assert, clock, is_*, "did you mean?", warnings) |
| v1.0.0-rc1 | C/C++ split, memory bug fixes, nq_embed refactor |
| **v1.0.0** | **Stable release ✅ ← current** |

---

## Future (Post-1.0)

These are ideas for future versions. Nothing is committed.

### v1.1 — Developer Experience
- `nq fmt` — auto-format source files
- `nq doc` — generate documentation from `#` comments
- Coloured REPL output (syntax highlighting)
- Improved stack trace with local variable values

### v1.2 — Language Features
- Closures — functions capturing outer variables
- `for item in array` — range-based loop sugar
- Multi-line strings (heredoc or triple-quote)
- String interpolation: `"Hello, {name}!"`

### v1.3 — Standard Library
- `stdlib/string` — format, pad_left, pad_right, count
- `stdlib/json` — basic JSON encode/decode
- `stdlib/os` — argv, env, getcwd, exit
- `stdlib/time` — timestamp, sleep, date format

### v1.4 — Performance
- Bytecode serialisation (`.nqc` compiled cache)
- Constant folding in compiler
- NaN-boxing value representation

### v2.0 — Major (breaking allowed)
- Type annotations (optional, gradual)
- Closures and upvalues
- Hash maps as a first-class type
- Module namespacing (prevent global pollution on import)
- WASM compilation target
