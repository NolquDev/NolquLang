# Nolqu Documentation

This folder contains two parallel documentation tracks:

---

## `stable/` — Nolqu v1.2.0 (Stable)

Documentation for the **v1.2.0 stable release**.

| Document | Contents |
|---|---|
| [stable/language.md](stable/language.md) | Language reference — syntax, types, all built-ins |
| [stable/grammar.md](stable/grammar.md) | Formal EBNF grammar |
| [stable/stdlib.md](stable/stdlib.md) | Standard library (3 modules: math, array, file) |
| [stable/embedding.md](stable/embedding.md) | C/C++ embed API |
| [stable/vm_design.md](stable/vm_design.md) | VM internals and bytecode reference |

---

## `dev/` — Nolqu v1.2.0 (Primary)

The main documentation. Reflects v1.2.0 stable.

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
  └─ No, I want the full/latest docs → read docs/dev/
```

---

## Version comparison

| Feature | v1.0.0 | v1.2.0 |
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
| `null` keyword | ✗ | ✓ (alias for `nil`) |
| Extended falsy (`0`, `""`) | ✗ | ✓ |
| `const` variables | ✗ | ✓ (compile-time immutable) |
| Default parameters | ✗ | ✓ |
| Slice `arr[1:3]` | ✗ | ✓ (arrays and strings) |
| Comparison chaining | ✗ | ✓ (`1 < x < 10`) |
