# Nolqu Documentation

This folder contains two parallel documentation tracks:

---

## `stable/` — Nolqu v1.0.0 (Stable Release)

Documentation for the **current stable release**.
Use this if you are running the `v1.0.0` binary or following along with the stable tag.

| Document | Contents |
|---|---|
| [stable/language.md](stable/language.md) | Language reference — syntax, types, all built-ins |
| [stable/grammar.md](stable/grammar.md) | Formal EBNF grammar |
| [stable/stdlib.md](stable/stdlib.md) | Standard library (3 modules: math, array, file) |
| [stable/embedding.md](stable/embedding.md) | C/C++ embed API |
| [stable/vm_design.md](stable/vm_design.md) | VM internals and bytecode reference |

---

## `dev/` — Nolqu v1.1.x (Alpha Development)

Documentation for the **current development series**.
These describe features that may not be in v1.0.0 stable.
Updated with every alpha release.

| Document | Contents |
|---|---|
| [dev/language.md](dev/language.md) | Language reference — includes `for/in`, `+=`, `break/continue` |
| [dev/grammar.md](dev/grammar.md) | Formal EBNF grammar — includes all new syntax |
| [dev/stdlib.md](dev/stdlib.md) | Standard library (10 modules, 80+ functions) |
| [dev/embedding.md](dev/embedding.md) | C/C++ embed API (unchanged from v1.0.0) |
| [dev/vm_design.md](dev/vm_design.md) | VM internals — 16-bit const pool, for-in desugaring |

---

## Which should I read?

```
Are you using a v1.0.0 stable binary?
  └─ Yes → read docs/stable/
  └─ No, I cloned the repo / using an alpha build → read docs/dev/
```

---

## Version comparison

| Feature | v1.0.0 stable | v1.1.x alpha |
|---|---|---|
| `break` / `continue` | ✗ | ✓ |
| `for item in array` | ✗ | ✓ |
| Compound assign `+=` `-=` `*=` `/=` `..=` | ✗ | ✓ |
| `ord()` / `chr()` | ✗ | ✓ |
| Constant pool limit | 256 | 65535 |
| Import deduplication | ✗ (re-executes) | ✓ |
| Undefined var catchable | ✗ | ✓ |
| stdlib modules | 3 | 10 |
| stdlib functions | ~30 | 80+ |
| `str()` precision | 6 digits (`%g`) | 14 digits (`%.14g`) |
