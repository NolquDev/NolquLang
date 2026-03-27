# Nolqu Documentation

This folder contains two parallel documentation tracks:

---

## `stable/` — Nolqu v1.2.1 (Stable)

Documentation for the **v1.2.1 stable release**.

| Document | Contents |
|---|---|
| [stable/language.md](stable/language.md) | Language reference — syntax, types, all built-ins |
| [stable/grammar.md](stable/grammar.md) | Formal EBNF grammar |
| [stable/stdlib.md](stable/stdlib.md) | Standard library (3 modules: math, array, file) |
| [stable/embedding.md](stable/embedding.md) | C/C++ embed API |
| [stable/vm_design.md](stable/vm_design.md) | VM internals and bytecode reference |

---

## `dev/` — Current development docs (Primary)

The main documentation for in-progress and newest language features.

| Document | Contents |
|---|---|
| [dev/language.md](dev/language.md) | Language reference — includes `for/in`, `+=`, `break/continue` |
| [dev/grammar.md](dev/grammar.md) | Formal EBNF grammar — includes all new syntax |
| [dev/stdlib.md](dev/stdlib.md) | Standard library (11 modules, 80+ functions) |
| [dev/embedding.md](dev/embedding.md) | C/C++ embed API (unchanged from v1.0.0) |
| [dev/vm_design.md](dev/vm_design.md) | VM internals — 16-bit const pool, for-in desugaring |
| [dev/feature_gap.md](dev/feature_gap.md) | Prioritized implementation plan for missing features |

---

## Which should I read?

```
Are you using the latest stable release (v1.2.1)?
  └─ Yes → read docs/stable/
  └─ No, I want current/latest features → read docs/dev/
```

---

## Version comparison

| Feature | v1.0.0 | v1.2.x |
|---|---|---|
| `break` / `continue` | ✗ | ✓ |
| `for item in array` | ✗ | ✓ |
| Compound assign `+=` `-=` `*=` `/=` `..=` | ✗ | ✓ |
| `ord()` / `chr()` | ✗ | ✓ |
| Constant pool limit | 256 | 65535 |
| Import deduplication | ✗ (re-executes) | ✓ |
| Undefined var catchable | ✗ | ✓ |
| stdlib modules | 3 | 11 |
| stdlib functions | ~30 | 80+ |
| `str()` precision | 6 digits (`%g`) | 14 digits (`%.14g`) |
| `null` keyword | ✗ | ✓ (alias for `nil`) |
| Extended falsy (`0`, `""`) | ✗ | ✓ |
| `const` variables | ✗ | ✓ (compile-time immutable) |
| Default parameters | ✗ | ✓ |
| Slice `arr[1:3]` | ✗ | ✓ (arrays and strings) |
| Comparison chaining | ✗ | ✓ (`1 < x < 10`) |
