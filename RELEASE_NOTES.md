# Nolqu v1.2.0-rc2 — Release Notes

> **Release Candidate.**
> All planned features for v1.2.0 are implemented and tested.
> This release is ready for broader testing before stable promotion.
> See [CHANGELOG.md](CHANGELOG.md) for the full version history.

---

## What's in v1.2.0

### Language

- `for item in array ... end` — range-based loop
- `break` and `continue` in all loop types
- `+=` `-=` `*=` `/=` `..=` — compound assignment
- `const NAME = value` — compile-time immutable binding
- `function f(x = default)` — default parameters
- `arr[1:3]` / `s[1:3]` — slice expressions (arrays and strings, negative indices)
- `null` keyword — alias for `nil`; extended falsy (`0`, `""` are now falsy)
- Comparison chaining: `1 < x < 10` desugars to `(1 < x) and (x < 10)`
- `and` / `or` return the actual value, not a boolean coerced result
- `ord(ch)` / `chr(n)` — character ↔ code point
- `bool(v)` — coerce any value to boolean
- `error_type(e)` — extract `"TypeError"`, `"NameError"`, etc. from an error

### Error System

All runtime errors carry a typed prefix:

| Prefix | Cause |
|---|---|
| `TypeError` | Wrong type for an operation |
| `NameError` | Undefined or undeclared variable |
| `IndexError` | Array / string index out of bounds |
| `ValueError` | Invalid value (division by zero, log of negative) |

All errors are catchable via `try/catch`.

### Import System

```nolqu
import "stdlib/math"          # quoted string
import stdlib/math            # unquoted path
import stdlib/math as math    # alias (accepted; no namespace effect)
from stdlib/math import PI    # selective (runs whole module)
```

### Standard Library — 11 modules, 90+ functions

| Module | Key additions vs v1.0.0 |
|---|---|
| `stdlib/math` | `PI` `TAU` `E` · trig · log · `gcd` `lcm` `hypot` `is_nan` |
| `stdlib/array` | `sum` `flatten` `zip` `chunk` `unique` `any` `all` `min_arr` `max_arr` |
| `stdlib/string` | `replace_all` `title_case` `char_at` `contains_str` `pad_*` `center` |
| `stdlib/path` | Full path manipulation without filesystem calls |
| `stdlib/json` | Recursive descent parser, object API |
| `stdlib/time` | `elapsed` `format_duration` `benchmark` |
| `stdlib/io` | `read_file` `write_file` `copy_file` `ensure_file` |
| `stdlib/os` | `env_or` `path_exists` `read_lines` `write_lines` `file_size` |
| `stdlib/fmt` | `fmt` `printf` `fmt_num` `fmt_pad` `fmt_table` |
| `stdlib/test` | Full test framework: `suite` `expect` `expect_eq` `expect_err` `done` |

### VM / Runtime Improvements

- Constant pool raised from 256 to **65,535** (16-bit index)
- Import deduplication — each module runs at most once
- All errors catchable — undefined variable, wrong arg count, type errors
- `str()` precision: `%.14g` (was `%g`, 6 significant digits)

---

## Upgrade from v1.0.0

### Breaking changes

- `nil` now prints as `"null"` (type is also `"null"`)
- `0` and `""` (empty string) are now **falsy** — previously truthy
- Error messages now carry a type prefix: `"NameError: undefined variable 'x'"`

### Compatibility

All v1.0.0 programs run unchanged unless they:
- Relied on `0` or `""` being truthy in conditions
- Pattern-matched on the exact text of runtime error messages
- Used `print nil` and expected the output `"nil"`

---

## Known Limitations (v1.2.0-rc2)

| Limitation | Notes |
|---|---|
| No closures | Functions cannot capture outer-scope variables |
| No hash maps | Only arrays are collection types |
| No integer type | All numbers are 64-bit IEEE 754 doubles |
| Circular imports | Will loop forever — no cycle detection |
| No string mutation | All string ops return new strings |
| Single-pass compiler | No forward references to functions |
| No comparison chaining | `1 < x < 10` is not supported; use `1 < x and x < 10` |
| `chr()` ASCII only | Code points 0–127 only |
| No `import as` | Not supported — Nolqu has no module namespaces |
| Transpiler experimental | `codegen` does not support all features |

---

*Nolqu™ · nadzil123 · nolquteam@gmail.com*
