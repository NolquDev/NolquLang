# Nolqu Changelog

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

Promoted from v1.2.2a2. No code changes.

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

### Performance: zero-copy loop variable + branch prediction hints

**for-range: zero-copy loop variable alias**

Previously `for i = 0 to N` emitted two extra opcodes **every iteration**:
- `OP_GET_LOCAL __i` → push onto new slot (visible var copy)
- `OP_POP` on `endScope` when the iteration ends

Fix: the user's variable name is registered as an alias pointing directly
to the `__i` slot. No copy, no extra push/pop. The body reads and writes
the counter directly.

Result:
| | a2 | a3 |
|---|---|---|
| for-range empty 100M | 3279ms | 2711ms (-17%) |
| for-range ≈ loop | — | ✅ Same speed |
| 1B iterations (local) | — | ~39s |

**NQ_LIKELY / NQ_UNLIKELY branch hints**

Added `__builtin_expect` hints on the most-executed paths:
- `BINARY_NUM` type check: the number case is almost always taken
- `JUMP_IF_FALSE`: the loop-continue case (condition true) is dominant
- `tableGet`: successful lookup is the common path; miss is unlikely
- `findEntry`: key pointer equality is the expected outcome

**`NQ_INLINE` + `NQ_RESTRICT` on `findEntry`**

`findEntry` is called on every global variable access. Marking it
`always_inline` and the `entries` pointer as `restrict` allows the
compiler to avoid reload/alias checks inside the lookup loop.

**Compiler hints defined in `common.h`:**
```c
NQ_INLINE      // __attribute__((always_inline)) static inline
NQ_RESTRICT    // __restrict__
NQ_LIKELY(x)   // __builtin_expect(!!(x), 1)
NQ_UNLIKELY(x) // __builtin_expect(!!(x), 0)
```
All fall back to no-ops on non-GCC/Clang compilers.

---

## v1.2.2a2 — Alpha 2 (2026)

### Performance optimization

**Compiler: O3 + march=native + flto** (was O2)

`-O3` enables loop vectorization, more aggressive inlining, and strength
reduction. `-march=native` lets the compiler use all CPU instructions available
on the build machine. `-flto` enables link-time optimization across translation
units — particularly effective for the VM dispatch loop which calls helpers
defined in separate .c files.

**for-range: step-direction check hoisted out of the loop**

Previously, `for i = 0 to N` emitted a `step >= 0` branch **every iteration**
to decide whether to use `i < stop` or `i > stop`. Since the step is almost
always a compile-time constant, this was pure overhead.

Now:
- If step is a literal (default `1`, or e.g. `step -1`, `step 2`) → the
  compiler chooses the comparison at compile time. The loop body is just
  `GET_LOCAL; GET_LOCAL; LT; JUMP_IF_FALSE` — one fewer comparison per iter.
- If step is a runtime expression → direction is checked **once before the
  loop**, then two specialized loops are emitted (positive and negative).
  No per-iteration branching.
- Zero step → `ValueError` at compile time.

**Benchmark results (1 billion iterations, `for i = 0 to 1_000_000_000`):**

| Version | Time |
|---|---|
| v1.2.2a1 | ~61s |
| v1.2.2a2 | ~43s |
| Improvement | **~29% faster** |

---

## v1.2.2a1 — Alpha 1 (2026)

### New: Numeric `for` loop (`for i = start to stop`)

Nolqu now has a pure counter-based loop with no array allocation:

```nolqu
# Equivalent to Python:  for i in range(1_000_000_000): number += 1
let number = 0
for i = 0 to 1000000000
  number += 1
end
print number   # 1000000000
```

Syntax:
```nolqu
for i = start to stop            # step defaults to +1
for i = start to stop step s     # custom step (any number, including negative)
```

Features:
- `break` and `continue` work correctly
- Nested loops work correctly
- `step` can be negative: `for i = 10 to 0 step -1`
- `to` and `step` are contextual — still usable as variable names
- Compiles to a pure counter loop: no array created, no `len()` call
- Compatible with existing `for item in array` syntax

Benchmark (1 billion iterations):
- Nolqu v1.2.2a1: ~61s on this machine
- The loop overhead is minimal — two GET_LOCAL + ADD + SET_LOCAL per iteration

Implementation:
- `NODE_FOR_RANGE` added to ast.h / ast.cpp
- Parser: detects `for x = ...` vs `for x in ...` by checking for `=` after ident
- Compiler: emits step-direction check (positive vs negative), correct break/continue

---

## v1.2.1 — Stable Release (2026)

Promoted from v1.2.1-rc1 with no code changes.

### What's new since v1.2.0

- **REPL auto-print** — `1 + 2` → `3`, identifiers, all binary operators
- **Circular import detection** — `A → B → A` → `[ImportError]` with cycle chain
- **`from X import a, b`** — compile-time name verification, `[ImportError]` on typo
- **CLI command suggestion** — `nq verdion` → `Did you mean: version ?`
- **Typed error brackets** — `[TypeError]` `[NameError]` `[IndexError]` `[ValueError]` `[ImportError]` `[SyntaxError]` `[IOError]` `[UsageError]`
- **`range()` `first()` `last()` `find()` `count_if()`** in `stdlib/array`
- **`str_contains()` `starts_with()` `ends_with()`** aliases in `stdlib/string`
- **Parser refactor** — `parseExprWithLeft()`, no duplication
- **Better error messages** — arithmetic shows operand types, import shows searched paths
- **CLI usage errors** — `nq check` without filename shows clear usage
- **License year updated to 2026**

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

Promoted from v1.2.2a2. No code changes.

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

### Performance: zero-copy loop variable + branch prediction hints

**for-range: zero-copy loop variable alias**

Previously `for i = 0 to N` emitted two extra opcodes **every iteration**:
- `OP_GET_LOCAL __i` → push onto new slot (visible var copy)
- `OP_POP` on `endScope` when the iteration ends

Fix: the user's variable name is registered as an alias pointing directly
to the `__i` slot. No copy, no extra push/pop. The body reads and writes
the counter directly.

Result:
| | a2 | a3 |
|---|---|---|
| for-range empty 100M | 3279ms | 2711ms (-17%) |
| for-range ≈ loop | — | ✅ Same speed |
| 1B iterations (local) | — | ~39s |

**NQ_LIKELY / NQ_UNLIKELY branch hints**

Added `__builtin_expect` hints on the most-executed paths:
- `BINARY_NUM` type check: the number case is almost always taken
- `JUMP_IF_FALSE`: the loop-continue case (condition true) is dominant
- `tableGet`: successful lookup is the common path; miss is unlikely
- `findEntry`: key pointer equality is the expected outcome

**`NQ_INLINE` + `NQ_RESTRICT` on `findEntry`**

`findEntry` is called on every global variable access. Marking it
`always_inline` and the `entries` pointer as `restrict` allows the
compiler to avoid reload/alias checks inside the lookup loop.

**Compiler hints defined in `common.h`:**
```c
NQ_INLINE      // __attribute__((always_inline)) static inline
NQ_RESTRICT    // __restrict__
NQ_LIKELY(x)   // __builtin_expect(!!(x), 1)
NQ_UNLIKELY(x) // __builtin_expect(!!(x), 0)
```
All fall back to no-ops on non-GCC/Clang compilers.

---

## v1.2.2a2 — Alpha 2 (2026)

### Performance optimization

**Compiler: O3 + march=native + flto** (was O2)

`-O3` enables loop vectorization, more aggressive inlining, and strength
reduction. `-march=native` lets the compiler use all CPU instructions available
on the build machine. `-flto` enables link-time optimization across translation
units — particularly effective for the VM dispatch loop which calls helpers
defined in separate .c files.

**for-range: step-direction check hoisted out of the loop**

Previously, `for i = 0 to N` emitted a `step >= 0` branch **every iteration**
to decide whether to use `i < stop` or `i > stop`. Since the step is almost
always a compile-time constant, this was pure overhead.

Now:
- If step is a literal (default `1`, or e.g. `step -1`, `step 2`) → the
  compiler chooses the comparison at compile time. The loop body is just
  `GET_LOCAL; GET_LOCAL; LT; JUMP_IF_FALSE` — one fewer comparison per iter.
- If step is a runtime expression → direction is checked **once before the
  loop**, then two specialized loops are emitted (positive and negative).
  No per-iteration branching.
- Zero step → `ValueError` at compile time.

**Benchmark results (1 billion iterations, `for i = 0 to 1_000_000_000`):**

| Version | Time |
|---|---|
| v1.2.2a1 | ~61s |
| v1.2.2a2 | ~43s |
| Improvement | **~29% faster** |

---

## v1.2.2a1 — Alpha 1 (2026)

### New: Numeric `for` loop (`for i = start to stop`)

Nolqu now has a pure counter-based loop with no array allocation:

```nolqu
# Equivalent to Python:  for i in range(1_000_000_000): number += 1
let number = 0
for i = 0 to 1000000000
  number += 1
end
print number   # 1000000000
```

Syntax:
```nolqu
for i = start to stop            # step defaults to +1
for i = start to stop step s     # custom step (any number, including negative)
```

Features:
- `break` and `continue` work correctly
- Nested loops work correctly
- `step` can be negative: `for i = 10 to 0 step -1`
- `to` and `step` are contextual — still usable as variable names
- Compiles to a pure counter loop: no array created, no `len()` call
- Compatible with existing `for item in array` syntax

Benchmark (1 billion iterations):
- Nolqu v1.2.2a1: ~61s on this machine
- The loop overhead is minimal — two GET_LOCAL + ADD + SET_LOCAL per iteration

Implementation:
- `NODE_FOR_RANGE` added to ast.h / ast.cpp
- Parser: detects `for x = ...` vs `for x in ...` by checking for `=` after ident
- Compiler: emits step-direction check (positive vs negative), correct break/continue

---

## v1.2.1 — Stable Release (2026)

Promoted from v1.2.1-rc1 with no code changes.

### What's new since v1.2.0

- **REPL auto-print** — expressions evaluated and printed automatically
- **REPL binary operators** — `x + 1`, `a < b and b < 10` all work
- **Circular import detection** — `A → B → A` → `[ImportError]`
- **`from X import a, b`** — compile-time name verification
- **CLI command suggestion** — `nq verdion` → `Did you mean: version?`
- **Typed error brackets** — `[TypeError]` `[NameError]` `[IndexError]` `[ValueError]` `[ImportError]` `[SyntaxError]` `[IOError]` `[UsageError]`
- **`range()` `first()` `last()` `find()` `count_if()`** in stdlib/array
- **`str_contains()` `starts_with()` `ends_with()`** in stdlib/string
- **`parseExprWithLeft`** — clean REPL parser, no duplication
- **License year updated** to 2026

### No breaking changes from v1.2.0

---

## v1.2.1-rc1 — Release Candidate (2026)

Promoted from v1.2.1b3. No code changes.

All planned features for v1.2.1 are implemented, tested, and stable.
This release is ready for broader testing before stable promotion.

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for the full feature summary.

---

## v1.2.1b3 — Beta 4 (2026)

### All typed errors now show correct bracket header

Previously `vmRuntimeError` always printed `[RuntimeError]` regardless of what
error was inside. Now both `vmRuntimeError` (runtime) and `compileError`
(compile-time) extract the type prefix from the message and use it as the
bracket label:

```
[TypeError]   /tmp/e.nq:2
  TypeError: arithmetic requires numbers, got string and number.

[NameError]   /tmp/e.nq:5
  NameError: 'x' is not defined. Did you mean 'y'?

[IndexError]  /tmp/e.nq:3
  IndexError: array index 99 is out of bounds (length 3).

[ValueError]  /tmp/e.nq:1
  ValueError: division by zero. Check the divisor before dividing.

[ImportError] /tmp/e.nq:1
  ImportError: module 'mymod' not found.
  Searched:
      ./mymod.nq

[SyntaxError] /tmp/e.nq:4
  Invalid expression. Expected a number...

[RuntimeError] /tmp/e.nq:7
  something broke         ← plain error("message") with no prefix
```

Implementation: format the message string first, scan for `Word: ` prefix
using the same logic as `error_type()`, then use it as the bracket label.
Falls back to `[RuntimeError]` / `[SyntaxError]` if no recognized prefix.
Zero duplication — the same extraction logic works in both the VM and compiler.

Full error type map now consistent end-to-end:

| Bracket header | Cause |
|---|---|
| `[TypeError]` | Wrong type for operation |
| `[NameError]` | Undefined/undeclared variable |
| `[IndexError]` | Array/string out of bounds |
| `[ValueError]` | Division by zero, invalid value |
| `[ImportError]` | Module not found, bad name, circular import |
| `[SyntaxError]` | Parse error, compile error |
| `[RuntimeError]` | Uncaught `error("plain message")` |
| `[IOError]` | File not found, read failure |
| `[UsageError]` | Unknown command, missing filename |
| `[Warning]` | Unused variable |

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

Promoted from v1.2.2a2. No code changes.

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

### Performance: zero-copy loop variable + branch prediction hints

**for-range: zero-copy loop variable alias**

Previously `for i = 0 to N` emitted two extra opcodes **every iteration**:
- `OP_GET_LOCAL __i` → push onto new slot (visible var copy)
- `OP_POP` on `endScope` when the iteration ends

Fix: the user's variable name is registered as an alias pointing directly
to the `__i` slot. No copy, no extra push/pop. The body reads and writes
the counter directly.

Result:
| | a2 | a3 |
|---|---|---|
| for-range empty 100M | 3279ms | 2711ms (-17%) |
| for-range ≈ loop | — | ✅ Same speed |
| 1B iterations (local) | — | ~39s |

**NQ_LIKELY / NQ_UNLIKELY branch hints**

Added `__builtin_expect` hints on the most-executed paths:
- `BINARY_NUM` type check: the number case is almost always taken
- `JUMP_IF_FALSE`: the loop-continue case (condition true) is dominant
- `tableGet`: successful lookup is the common path; miss is unlikely
- `findEntry`: key pointer equality is the expected outcome

**`NQ_INLINE` + `NQ_RESTRICT` on `findEntry`**

`findEntry` is called on every global variable access. Marking it
`always_inline` and the `entries` pointer as `restrict` allows the
compiler to avoid reload/alias checks inside the lookup loop.

**Compiler hints defined in `common.h`:**
```c
NQ_INLINE      // __attribute__((always_inline)) static inline
NQ_RESTRICT    // __restrict__
NQ_LIKELY(x)   // __builtin_expect(!!(x), 1)
NQ_UNLIKELY(x) // __builtin_expect(!!(x), 0)
```
All fall back to no-ops on non-GCC/Clang compilers.

---

## v1.2.2a2 — Alpha 2 (2026)

### Performance optimization

**Compiler: O3 + march=native + flto** (was O2)

`-O3` enables loop vectorization, more aggressive inlining, and strength
reduction. `-march=native` lets the compiler use all CPU instructions available
on the build machine. `-flto` enables link-time optimization across translation
units — particularly effective for the VM dispatch loop which calls helpers
defined in separate .c files.

**for-range: step-direction check hoisted out of the loop**

Previously, `for i = 0 to N` emitted a `step >= 0` branch **every iteration**
to decide whether to use `i < stop` or `i > stop`. Since the step is almost
always a compile-time constant, this was pure overhead.

Now:
- If step is a literal (default `1`, or e.g. `step -1`, `step 2`) → the
  compiler chooses the comparison at compile time. The loop body is just
  `GET_LOCAL; GET_LOCAL; LT; JUMP_IF_FALSE` — one fewer comparison per iter.
- If step is a runtime expression → direction is checked **once before the
  loop**, then two specialized loops are emitted (positive and negative).
  No per-iteration branching.
- Zero step → `ValueError` at compile time.

**Benchmark results (1 billion iterations, `for i = 0 to 1_000_000_000`):**

| Version | Time |
|---|---|
| v1.2.2a1 | ~61s |
| v1.2.2a2 | ~43s |
| Improvement | **~29% faster** |

---

## v1.2.2a1 — Alpha 1 (2026)

### New: Numeric `for` loop (`for i = start to stop`)

Nolqu now has a pure counter-based loop with no array allocation:

```nolqu
# Equivalent to Python:  for i in range(1_000_000_000): number += 1
let number = 0
for i = 0 to 1000000000
  number += 1
end
print number   # 1000000000
```

Syntax:
```nolqu
for i = start to stop            # step defaults to +1
for i = start to stop step s     # custom step (any number, including negative)
```

Features:
- `break` and `continue` work correctly
- Nested loops work correctly
- `step` can be negative: `for i = 10 to 0 step -1`
- `to` and `step` are contextual — still usable as variable names
- Compiles to a pure counter loop: no array created, no `len()` call
- Compatible with existing `for item in array` syntax

Benchmark (1 billion iterations):
- Nolqu v1.2.2a1: ~61s on this machine
- The loop overhead is minimal — two GET_LOCAL + ADD + SET_LOCAL per iteration

Implementation:
- `NODE_FOR_RANGE` added to ast.h / ast.cpp
- Parser: detects `for x = ...` vs `for x in ...` by checking for `=` after ident
- Compiler: emits step-direction check (positive vs negative), correct break/continue

---

## v1.2.1 — Stable Release (2026)

Promoted from v1.2.1-rc1 with no code changes.

### What's new since v1.2.0

- **REPL auto-print** — `1 + 2` → `3`, identifiers, all binary operators
- **Circular import detection** — `A → B → A` → `[ImportError]` with cycle chain
- **`from X import a, b`** — compile-time name verification, `[ImportError]` on typo
- **CLI command suggestion** — `nq verdion` → `Did you mean: version ?`
- **Typed error brackets** — `[TypeError]` `[NameError]` `[IndexError]` `[ValueError]` `[ImportError]` `[SyntaxError]` `[IOError]` `[UsageError]`
- **`range()` `first()` `last()` `find()` `count_if()`** in `stdlib/array`
- **`str_contains()` `starts_with()` `ends_with()`** aliases in `stdlib/string`
- **Parser refactor** — `parseExprWithLeft()`, no duplication
- **Better error messages** — arithmetic shows operand types, import shows searched paths
- **CLI usage errors** — `nq check` without filename shows clear usage
- **License year updated to 2026**

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

Promoted from v1.2.2a2. No code changes.

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

### Performance: zero-copy loop variable + branch prediction hints

**for-range: zero-copy loop variable alias**

Previously `for i = 0 to N` emitted two extra opcodes **every iteration**:
- `OP_GET_LOCAL __i` → push onto new slot (visible var copy)
- `OP_POP` on `endScope` when the iteration ends

Fix: the user's variable name is registered as an alias pointing directly
to the `__i` slot. No copy, no extra push/pop. The body reads and writes
the counter directly.

Result:
| | a2 | a3 |
|---|---|---|
| for-range empty 100M | 3279ms | 2711ms (-17%) |
| for-range ≈ loop | — | ✅ Same speed |
| 1B iterations (local) | — | ~39s |

**NQ_LIKELY / NQ_UNLIKELY branch hints**

Added `__builtin_expect` hints on the most-executed paths:
- `BINARY_NUM` type check: the number case is almost always taken
- `JUMP_IF_FALSE`: the loop-continue case (condition true) is dominant
- `tableGet`: successful lookup is the common path; miss is unlikely
- `findEntry`: key pointer equality is the expected outcome

**`NQ_INLINE` + `NQ_RESTRICT` on `findEntry`**

`findEntry` is called on every global variable access. Marking it
`always_inline` and the `entries` pointer as `restrict` allows the
compiler to avoid reload/alias checks inside the lookup loop.

**Compiler hints defined in `common.h`:**
```c
NQ_INLINE      // __attribute__((always_inline)) static inline
NQ_RESTRICT    // __restrict__
NQ_LIKELY(x)   // __builtin_expect(!!(x), 1)
NQ_UNLIKELY(x) // __builtin_expect(!!(x), 0)
```
All fall back to no-ops on non-GCC/Clang compilers.

---

## v1.2.2a2 — Alpha 2 (2026)

### Performance optimization

**Compiler: O3 + march=native + flto** (was O2)

`-O3` enables loop vectorization, more aggressive inlining, and strength
reduction. `-march=native` lets the compiler use all CPU instructions available
on the build machine. `-flto` enables link-time optimization across translation
units — particularly effective for the VM dispatch loop which calls helpers
defined in separate .c files.

**for-range: step-direction check hoisted out of the loop**

Previously, `for i = 0 to N` emitted a `step >= 0` branch **every iteration**
to decide whether to use `i < stop` or `i > stop`. Since the step is almost
always a compile-time constant, this was pure overhead.

Now:
- If step is a literal (default `1`, or e.g. `step -1`, `step 2`) → the
  compiler chooses the comparison at compile time. The loop body is just
  `GET_LOCAL; GET_LOCAL; LT; JUMP_IF_FALSE` — one fewer comparison per iter.
- If step is a runtime expression → direction is checked **once before the
  loop**, then two specialized loops are emitted (positive and negative).
  No per-iteration branching.
- Zero step → `ValueError` at compile time.

**Benchmark results (1 billion iterations, `for i = 0 to 1_000_000_000`):**

| Version | Time |
|---|---|
| v1.2.2a1 | ~61s |
| v1.2.2a2 | ~43s |
| Improvement | **~29% faster** |

---

## v1.2.2a1 — Alpha 1 (2026)

### New: Numeric `for` loop (`for i = start to stop`)

Nolqu now has a pure counter-based loop with no array allocation:

```nolqu
# Equivalent to Python:  for i in range(1_000_000_000): number += 1
let number = 0
for i = 0 to 1000000000
  number += 1
end
print number   # 1000000000
```

Syntax:
```nolqu
for i = start to stop            # step defaults to +1
for i = start to stop step s     # custom step (any number, including negative)
```

Features:
- `break` and `continue` work correctly
- Nested loops work correctly
- `step` can be negative: `for i = 10 to 0 step -1`
- `to` and `step` are contextual — still usable as variable names
- Compiles to a pure counter loop: no array created, no `len()` call
- Compatible with existing `for item in array` syntax

Benchmark (1 billion iterations):
- Nolqu v1.2.2a1: ~61s on this machine
- The loop overhead is minimal — two GET_LOCAL + ADD + SET_LOCAL per iteration

Implementation:
- `NODE_FOR_RANGE` added to ast.h / ast.cpp
- Parser: detects `for x = ...` vs `for x in ...` by checking for `=` after ident
- Compiler: emits step-direction check (positive vs negative), correct break/continue

---

## v1.2.1 — Stable Release (2026)

Promoted from v1.2.1-rc1 with no code changes.

### What's new since v1.2.0

- **REPL auto-print** — expressions evaluated and printed automatically
- **REPL binary operators** — `x + 1`, `a < b and b < 10` all work
- **Circular import detection** — `A → B → A` → `[ImportError]`
- **`from X import a, b`** — compile-time name verification
- **CLI command suggestion** — `nq verdion` → `Did you mean: version?`
- **Typed error brackets** — `[TypeError]` `[NameError]` `[IndexError]` `[ValueError]` `[ImportError]` `[SyntaxError]` `[IOError]` `[UsageError]`
- **`range()` `first()` `last()` `find()` `count_if()`** in stdlib/array
- **`str_contains()` `starts_with()` `ends_with()`** in stdlib/string
- **`parseExprWithLeft`** — clean REPL parser, no duplication
- **License year updated** to 2026

### No breaking changes from v1.2.0

---

## v1.2.1-rc1 — Release Candidate (2026)

Promoted from v1.2.1b3. No code changes.

All planned features for v1.2.1 are implemented, tested, and stable.
This release is ready for broader testing before stable promotion.

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for the full feature summary.

---

## v1.2.1b3 — Beta 3 (2026)

### CLI: Unknown command suggestion

When you mistype a command, Nolqu now detects it and suggests the closest
valid command using Levenshtein distance:

```
$ nq verdion
[UsageError] Unknown command: verdion
  Did you mean: version ?

$ nq chekc myfile.nq
[UsageError] Unknown command: chekc
  Did you mean: check ?
```

Rules: only triggers when the input has no `.nq` extension, the file does not
exist, and the edit distance to a valid command is ≤ 2. Long or unrelated
inputs fall through to the normal `[IOError]` (file not found) path.

### Error prefixes: typed and consistent

All error output now uses typed prefixes instead of the generic `[ Error ]`:

| Old | New | When |
|---|---|---|
| `[ Error ]` (file) | `[IOError]` | File not found, read failure |
| `[ Error ]` (usage) | `[UsageError]` | Unknown command, missing filename |
| `[ Compile Error ]` | `[SyntaxError]` | Parse error, compile error |
| `[ Warning ]` | `[Warning]` | Unused variable etc. |
| `[ Runtime Error ]` | `[RuntimeError]` | Uncaught runtime error |
| `[ check ] ok` | `[OK]` | nq check pass |
| `[ check ] error` | `[SyntaxError]` | nq check fail |

Typed prefixes in `try/catch` error values remain unchanged
(`TypeError:`, `NameError:`, `IndexError:`, `ValueError:`, `ImportError:`).

---

## v1.2.1b2 — Beta 2 (2026)

### Refactor: REPL expression parser (no duplication)

`b1` introduced `parseExprWithLeft()` as a concept but still had a 60-line
duplicated precedence tower in the `TK_IDENT` case. `b2` completes the
refactor:

- Added `parseExprWithLeft(Parser* p, ASTNode* left)` — a proper function
  that accepts an already-built left-hand node and continues through the
  full precedence chain (index → multiply → add → comparison → equality →
  and → or).
- The `TK_IDENT` repl_mode block is now 5 lines: detect operator, call
  `parseExprWithLeft`.
- Added forward declaration so the function can be called from `TK_IDENT`
  case before it is defined.

Behavior is identical to `b1`. This is purely a code quality fix — one place
to maintain, no duplication, future operators automatically handled.

---

## v1.2.1b1 — Beta 1 (2026)

**First beta release.** Feature freeze. Only bug fixes until v1.2.1 stable.

### Bug Fix: REPL expression evaluation with binary operators

In alpha 4, `x + 1` worked but only because the pre-advance literal check caught
number tokens. Identifier-based expressions like `a + b`, `x > 3`, `x == 5`,
`a < b and b < 10` all failed with "Expected a new line after statement."

Root cause: the `TK_IDENT` case in `parseStmt` parsed the identifier and any
following call `(...)`, then immediately called `expectNewline`. In repl_mode,
binary operators after the identifier were not consumed.

Fix: added an operator-continuation block in the `TK_IDENT` case for repl_mode
that walks through multiply → addition → comparison → equality → and → or,
building the binary expression tree correctly. The `and`/`or` rhs now uses
`parseComparison` (not `parseUnary`) so `a < b and b < 10` works as expected.

All REPL expression forms now work:
```
nq > let x = 5
nq > x + 1
6
nq > x > 3
true
nq > x == 5
true
nq > let a = 3
nq > let b = 7
nq > a < b and b < 10
true
nq > [1, 2, 3][0]
1
```

---

## v1.2.1a4 — Alpha 4 (2026)

### Features

**REPL auto-print expressions**

The REPL now evaluates and prints expression results automatically — no need
to wrap everything in `print`. Non-null values are displayed; null is silent.

```
nq > 1 + 2
3
nq > "hello"
hello
nq > [1, 2, 3]
[1, 2, 3]
nq > let x = 99
nq > x
99
nq > null
nq >
```

Supported in REPL: number/string/bool/array literals, arithmetic, comparisons,
function calls, and identifier lookups. Implemented via `repl_mode` flag in
the parser and compiler; normal scripts are unaffected.

**Circular import detection**

`A → B → A` now produces a clear compile error:

```
ImportError: circular import detected.
  Cycle: /path/to/a.nq -> /path/to/b.nq -> /path/to/a.nq
```

Previously silent (dedup set caused B→A to silently do nothing).
Fixed by checking the `import_stack` before the `already_imported` dedup skip.

### Bug Fixes

- **Circular check order**: the `already_imported` dedup check was firing before
  the import_stack cycle check, suppressing cycle detection. Fixed by reordering.

- **strncat warnings** in circular chain builder replaced with `snprintf`.

---

## v1.2.1a3 — Alpha 3 (2026)

### Focus: import, CLI, errors, stdlib

**1. Import: searched paths in error message**

When a module is not found, the error now shows exactly which paths were tried:
```
ImportError: module 'mymod' not found.
  Searched:
      /path/to/script/mymod.nq
      mymod.nq
```

**2. CLI: proper usage errors**

`nq check`, `nq run`, `nq test`, and `nq compile` without a filename
previously crashed or showed a confusing "Cannot open file: check" error.
Now they show a clear usage message:
```
[ Error ] Missing filename.
  Usage:  nq check <file.nq>
  Run  nq help  for all commands.
```

**3. Error messages: more human, more context**

- **Arithmetic**: `TypeError: arithmetic requires numbers, got string and number.`
  (previously just "arithmetic requires numbers")
- **Undefined variable**: `NameError: 'x' is not defined.`
  (previously "undefined variable 'x'")
- **Cannot assign**: `NameError: cannot assign to 'x' — it has not been declared.`
- **Negate**: `TypeError: cannot apply '-' to a string value.`
- **Division by zero**: `ValueError: division by zero. Check the divisor before dividing.`

**4. stdlib/array: new utilities**

- `range(n)` — `[0, 1, ..., n-1]`
- `range(start, stop)` — `[start, ..., stop-1]`
- `range(start, stop, step)` — supports negative step
- `first(arr)` — first element or null
- `last(arr)` — last element or null
- `find(arr, fn)` — first matching element or null
- `count_if(arr, fn)` — count matching elements

**5. stdlib/string: naming aliases**

- `str_contains(s, sub)` — readable alternative to `contains_str`
- `starts_with(s, prefix)` — consistent with snake_case convention
- `ends_with(s, suffix)`
- `to_upper(s)`, `to_lower(s)`, `repeat_str(s, n)`

---

## v1.2.1a2 — Alpha 2 (2026)

### Bug Fixes

**`as` and `from` incorrectly reserved as keywords**

`let as = "x"` and `let from = "y"` produced errors ("Expected a variable name
after 'let', Found: 'as'"). Fixed by removing `as` and `from` from the keyword
table. They are now **contextual identifiers** — they get special meaning only
in import syntax and are freely usable as variable, function, or parameter names.

```nolqu
let as   = "alias"      # now works
let from = "source"     # now works
function route(as, from)
  return from .. " -> " .. as
end
```

**`from X import Y` dispatch broken when `from` is not a keyword**

When `as`/`from` were removed from the keyword table, `from` became `TK_IDENT`
and the statement dispatcher's `case TK_FROM:` was never reached. Fixed by
checking for the contextual keyword `"from"` at the start of the `TK_IDENT`
case in `parseStmt`.

**Runtime error output: redundant `Error:` prefix**

Errors were printed as `Error: IndexError: array index out of bounds` — the
word "Error:" was redundant since the typed prefix already identifies the
error type. Now prints: `IndexError: array index out of bounds`.

**Old `v1.0.0-rc1` comments in `object.c` and `gc.c`**

Two inline comments still referenced `v1.0.0-rc1`. Removed.

---

## v1.2.1a1 — Alpha 1 (2026)

### Re-introduced features (fully implemented)

**Comparison chaining** — `1 < x < 10`, `a < b <= c`

Safely re-implemented using AST node cloning. The v1.2.0 crash (heap double-free)
was caused by sharing a pointer between two binary nodes. Fixed by cloning the
middle operand — only identifiers and literals can be cloned; complex expressions
(calls, arithmetic) produce a clear compile error directing the user to `let`.

Guarantees:
- Each operand evaluated **exactly once**
- Short-circuit via AND: if first comparison is false, second is skipped
- No shared AST pointers — no double-free
- `freeNode` is always safe

**`from module import name1, name2`**

Fully implemented with compile-time name verification:
- Reads and parses the module's AST at compile time
- Verifies each requested name is declared at the module's top level
- Reports `ImportError: 'X' not found in module 'Y'` if a name is missing
- Module caching respected — module runs at most once even with mixed import/from-import
- Supports both quoted (`from "stdlib/math"`) and unquoted (`from stdlib/math`) paths

Limitation (documented): all module globals become accessible after import
(Nolqu has no namespace isolation). The `from` form provides documentation
clarity and early typo detection.

**`ImportError` prefix** — module not found and invalid name errors now carry
`ImportError:` prefix, consistent with the typed error system.

### Still not supported (with clear compile errors)
- `import X as Y` — no module namespaces in Nolqu

### New test files
- `examples/tests/chain_test.nq` — 14 tests for chaining correctness
- `examples/tests/import_test.nq` — 10 tests for import system

### Documentation
- `docs/dev/language.md` — chaining section + from-import section updated
- `docs/dev/grammar.md` — import rule updated, chaining note added
- `docs/dev/semantics.md` — chaining row updated in summary

---

## v1.2.0 — Stable Release (2026)

**First stable release of the v1.2 series.**

Promoted from v1.2.0-rc2 with no code changes.
All rc2 fixes are included. See rc1 and rc2 entries below for details.

### Summary of what's new since v1.0.0

- `for item in array` — range-based loop with `break`/`continue`
- `+=` `-=` `*=` `/=` `..=` — compound assignment operators
- `const NAME = value` — compile-time immutable binding
- `function f(x = default)` — default parameters
- `arr[1:3]` / `s[1:3]` — slice expressions for arrays and strings
- `null` keyword — alias for `nil`
- `bool(v)`, `ord(ch)`, `chr(n)`, `error_type(e)` — new builtins
- Typed errors: `TypeError:`, `NameError:`, `IndexError:`, `ValueError:`
- Import: `import stdlib/math` (unquoted path supported)
- Constant pool raised 256 → 65535
- All runtime errors now catchable via `try/catch`
- `str()` precision: 14 significant digits (was 6)
- 11 stdlib modules, 90+ functions

### Breaking changes from v1.0.0

- `0` and `""` (empty string) are now **falsy**
- `nil` prints as `"null"`; `type(nil)` returns `"null"`
- Error messages carry type prefix: `"NameError: ..."` not `"Undefined variable..."`

---

## v1.2.0-rc2 — Release Candidate 2 (2026)

Stability-only pass. No new features.

### Critical Bug Fixes

**Comparison chaining caused heap crash (memory corruption)**

`1 < x < 10` and even `a < x < b` with simple identifiers caused a
`free(): double free detected` crash at runtime. Root cause: the middle
operand AST node was shared (same pointer) between two binary nodes in the
generated AND-chain. When `freeNode` freed the left subtree, then the right
subtree, the shared node was freed twice → heap corruption.

Fix: comparison chaining is **removed**. `parseComparison` reverted to a
simple left-to-right loop with no chaining. Use `a < x and x < 10` instead.

**`import X as Y` was a silent no-op**

The alias was never created. `import stdlib/math as m` would run the module
(PI, sin etc. became global) but `m` itself was undefined. Now produces a
clear compile error:
```
'import X as Y' is not supported — Nolqu has no module namespaces.
  Use:  import "stdlib/math"
  Then access its functions directly: PI, sin, cos, ...
```

**`from X import Y` imported everything, not just Y**

`from stdlib/math import sin` imported the entire module (PI, cos, etc all
became global), ignoring the names listed. Now produces a clear compile error:
```
'from X import Y' is not supported.
  Use:  import "stdlib/math"
  All module definitions become available globally after import.
```

### Documentation

- `README.md` — added prominent `[!IMPORTANT]` falsy/truthy warning before
  the feature list. Version updated to rc2.
- `docs/dev/language.md` — import section updated (removed `as`/`from`
  examples, added error note); const clarified with array mutation example;
  comparison chaining limitation updated to say "not supported".
- `docs/dev/semantics.md` — falsy section upgraded to `[!IMPORTANT]` callout;
  const/array mutation section expanded with explicit note about lack of
  freeze mechanism; comparison chaining marked "Not supported" in summary.
- `docs/dev/grammar.md` — import rule simplified; `as`/`from` removed from
  keywords list with note they are reserved.

### Also fixed

- `import stdlib/path as X` path-parsing bug: the unquoted path scanner was
  consuming `as` as part of the module path (resulting in `stdlib/pathas` or
  similar). Fixed by stopping at the first token that isn't preceded by `/`.

---

## v1.2.0-rc1 — Release Candidate (2026)

Stabilization pass before the v1.2.0 stable release.
No new language features. All changes are fixes and cleanup.

### Fixes

- **`nq help` accuracy** — stdlib list now correct: `stdlib/os` shows actual
  functions (`env_or`, `path_exists`, etc.); `stdlib/fmt` shows `fmt`/`printf`;
  `stdlib/io` added; `bool()` and `error_type()` added to built-in list;
  duplicate `ord`/`chr` entry removed; syntax quick-ref updated with `const`
  and slice examples.

- **Remaining `"nil"` in error messages** — four error message format strings
  in `vm.c` still used `"nil"` as a type name. All updated to `"null"`.

- **All `docs/dev/` files** — headers, banners, and internal version references
  updated from `v1.1.1a4`/`a5`/`a6` to `v1.2.0-rc1 (Release Candidate)`.
  Per-version tags in `stdlib.md` (e.g. `*(v1.1.1a4)*`) removed — the docs
  now describe the current state, not the history.

- **`RELEASE_NOTES.md`** — fully rewritten for v1.2.0. Covers all new
  language features, error system, import syntax, stdlib summary, upgrade
  guide, and known limitations.

- **`README.md`** — banner updated from "alpha" to "release candidate";
  version references updated.

- **`ROADMAP.md`** — v1.2.0-rc1 added; stable promotion criteria updated
  to reflect current state (most boxes already checked).

---

## v1.1.1a6 — Alpha 6 (2026)

### Language / Runtime

- **Typed errors** — all runtime errors now carry a type prefix:
  `TypeError:`, `NameError:`, `IndexError:`, `ValueError:`.
  Stdlib errors updated to match. Use `error_type(e)` to dispatch.

- **`bool(v)` builtin** — coerce any value to `true`/`false` using
  the language's truthiness rules. `bool(0) = false`, `bool("hi") = true`.

- **`error_type(e)` builtin** — extract the type prefix from a typed
  error string. `error_type("TypeError: foo") = "TypeError"`.
  Falls back to `"RuntimeError"` for plain error strings.

- **Import syntax extended:**
  - `import stdlib/math` — unquoted slash-separated path
  - `import "stdlib/math" as math` — alias (parsed; no namespace effect)
  - `from stdlib/math import PI, sin` — selective import (runs whole module)

### Standard Library

- **`stdlib/io.nq`** — new unified file I/O module:
  `read_file` · `write_file` · `append_file` · `read_lines` · `write_lines` ·
  `append_line` · `file_size` · `ensure_file` · `copy_file`

### Examples

- **`examples/real/cli_tool.nq`** — interactive task manager (input, file I/O)
- **`examples/real/file_reader.nq`** — file stats and word frequency
- **`examples/real/json_tool.nq`** — JSON build, encode, decode, validate, merge
- **`examples/real/text_processor.nq`** — text transformation pipeline

### Documentation

- `docs/dev/semantics.md` — new section: Typed Errors with dispatch examples
- `docs/dev/language.md` — updated: typed error table, `bool()`/`error_type()`, import forms
- `docs/dev/grammar.md` — added `module_path`, extended `import_stmt`, `from`/`as` keywords

---

## v1.1.1a5 — Alpha 5 (2026)

### Language — New Features

- **`null` keyword** — alias for `nil`. Both print as `"null"`, `type(null)` returns `"null"`.

- **Extended falsy values** — `0` and `""` (empty string) are now falsy in addition
  to `nil`/`null` and `false`. Breaking change from v1.0.0 where only `nil` and `false` were falsy.

- **`const` declarations** — `const NAME = value` creates an immutable binding.
  Reassignment and compound assignment (`+=` etc.) are **compile-time errors**.
  Works at global and local scope. The binding is immutable; array contents can still be mutated.

- **Default parameters** — `function greet(name = "world")`.
  If a parameter is passed as `null` or omitted, the default expression is used.
  Default parameters must follow non-default parameters.

- **Slice expressions** — `arr[start:end]`, `arr[:end]`, `arr[start:]`, `arr[:]`.
  Works on arrays and strings. Negative indices supported. Returns a new value (copy).

- **`and`/`or` return actual values** — already worked since v1.1.1a2, now explicitly
  documented. `1 and 2 = 2`, `nil or 5 = 5`.

### Bug Fixes

- **Default param stack corruption** — the initial implementation omitted a `JUMP`
  to skip the false-branch `OP_POP` in the default injection pattern, causing
  the return value to be concatenated into the string. Fixed.

- **`printValue` now prints `null` for nil** — `print nil` and `print null`
  both output `null`. `str(nil)`, `type(nil)`, and `type(null)` all return `"null"`.

- **Memory leak in `parseFunctionDecl`** — `defaults` array and its AST nodes
  were not freed in `freeNode` for `NODE_FUNCTION`. Fixed; ASan/LSan clean.

### Documentation

- **`docs/dev/semantics.md`** — new comprehensive semantics reference covering:
  truthiness rules, null/nil behavior, function return, logical operator value
  semantics, const rules, and slice edge cases.

- **`docs/dev/language.md`** — updated: falsy rules, null keyword, Known Limitations
  updated with comparison-chaining caveat.

- **`docs/dev/grammar.md`** — added `const_stmt`, `slice_or_index`, `null` keyword,
  `const` keyword, `param_decl` with optional default.

- **`docs/README.md`** — comparison table updated with all a5 additions.

---

## v1.1.1a4 — Alpha 4 (2026)

### Standard Library — Expanded

**`stdlib/array.nq`** — 9 new functions:
- `sum(arr)` — sum all numbers
- `min_arr(arr)` · `max_arr(arr)` — smallest / largest element
- `any(arr, fn)` · `all(arr, fn)` — predicate tests across all elements
- `flatten(arr)` — one-level deep flatten of nested arrays
- `unique(arr)` — remove duplicates, preserve insertion order
- `zip(a, b)` — pair two arrays element-wise into `[[a0,b0], ...]`
- `chunk(arr, size)` — split into fixed-size sub-arrays

**`stdlib/string.nq`** — 3 new functions:
- `char_at(s, i)` — character at index with bounds-checking error
- `contains_str(s, sub)` — readable alias for `index(s,sub) != -1`
- `title_case(s)` — capitalise the first letter of each word

**`stdlib/math.nq`** — 5 new functions:
- `is_nan(n)` — IEEE 754 NaN check via self-inequality
- `is_inf(n)` — infinity check
- `hypot(a, b)` — Euclidean distance `sqrt(a²+b²)`
- `gcd(a, b)` — greatest common divisor (Euclidean algorithm)
- `lcm(a, b)` — least common multiple

### README — Alpha Warning

Added a prominent warning banner to `README.md` so readers immediately know
they are looking at an alpha version, with a link to the stable v1.0.0 release:

```
⚠️ This README describes the 1.1.x development series.
   For production use, stick with v1.0.0 stable.
```

### Documentation

- `docs/dev/stdlib.md` — all new functions documented with examples and version tags
- `docs/dev/language.md` — built-in reference tables updated with new stdlib entries
- `ROADMAP.md` — a4 added to history, criteria updated

### Documentation Structure Reorganised

The `docs/` folder is now split into two tracks:

- **`docs/stable/`** — snapshot of v1.0.0 documentation.
  Accurate for the stable release. Includes a Known Limitations table
  that links each limitation to the version that fixed it.

- **`docs/dev/`** — current development docs (v1.1.x).
  Updated with every alpha release. Each file carries a banner
  noting it describes an alpha version.

- **`docs/README.md`** — index with a version comparison table and
  guidance on which track to read.

---

## v1.1.1a3 — Alpha 3 (2026)

### Bug Fixes

- **Undefined variable access is now catchable** — `OP_GET_GLOBAL` on an
  undefined name previously called `vmRuntimeError` which bypassed
  `try/catch`. Now uses `THROW_ERROR` and can be caught.
  Affects: `let x = undefined_name`, compound assign on undeclared variable.

- **Undeclared variable assignment is now catchable** — `OP_SET_GLOBAL` on a
  name that was never declared with `let` now throws a catchable error instead
  of aborting.

### Documentation — Accuracy Pass

All documents updated to reflect the current state of the language:

- **`README.md`** — quick start uses `for-in` example; features list includes
  `for item in array` and compound assignment; stdlib count updated to 10.

- **`docs/dev/grammar.md`** — added `for_stmt` and `compound_assign_stmt` to the
  statement grammar; `for` and `in` added to keyword list; compound assignment
  operators documented; import deduplication note corrected (was "re-executes",
  now "no-op after first load").

- **`docs/dev/language.md`** — removed stale limitation "import re-executes" (fixed
  in v1.1.1a2); added "accessing undefined variable" and "assigning to undeclared
  variable" to the "What throws" table; `replace()` limitation clarified to
  point to `replace_all()` in `stdlib/string`.

- **`docs/dev/vm_design.md`** — `OP_CONST` updated to show 16-bit index `[hi, lo]`;
  constant pool limit updated 256 → 65535; new section documenting the `for-in`
  desugaring and `break`/`continue` invariants.

- **`ROADMAP.md`** — all released versions listed with accurate status;
  criteria for promoting a3 → stable v1.1.1 documented.

---

## v1.1.1a2 — Alpha 2 (2026)

> **Alpha release** — all features are functional and tested.
> Syntax is stable. Not yet recommended for production.

### Language — New Features

- **`for item in array ... end`** — range-based loop over arrays.
  `break` and `continue` work correctly inside `for`, including after
  nested `if` blocks and across sequential `for` loops.
- **Compound assignment: `+=` `-=` `*=` `/=` `..=`** — modify a variable
  in place. Works on both local and global variables.
- **`ord(ch)`** — return the ASCII code point (0–127) of the first character.
- **`chr(n)`** — return a one-character string for code point `n` (0–127).

### Bug Fixes

- **Non-function call is now catchable** — calling a string/number/nil as
  a function previously aborted with an uncatchable runtime error. Now
  it goes through `THROW_ERROR` and can be caught with `try/catch`.
- **Builtin arity error is now catchable** — `sqrt()` with wrong number of
  arguments previously aborted. Now catchable.
- **Import deduplication** — `import "stdlib/math"` twice no longer
  re-executes the module. Tracked per `compile()` call; REPL-safe.
- **Constant pool limit raised from 256 → 65535** — the pool index is now
  16-bit. Files with >256 global variables or string constants no longer
  crash with "Too many constants". Disassembler and VM reader updated.
- **`str()` precision** — `str(3.14159265358979)` now returns
  `"3.14159265358979"` (14 significant digits) instead of `"3.14159"`.
  Uses `%.14g` format; integers are still formatted without decimal point.
- **`for-in` break/continue stack corruption** — `break` inside a `for`
  loop now correctly cleans up the hidden `__for_arr__` and `__for_idx__`
  locals before jumping, preventing stack corruption in subsequent loops.
  `continue` inside `for` correctly increments the index before looping.
- **`loop_stack` reset per `compile()` call** — a leftover loop context
  from one REPL input could bleed into the next. Fixed.

### Standard Library — New Modules

- **`stdlib/os.nq`** — file-system utilities: `env_or`, `home_dir`,
  `path_exists`, `read_lines`, `write_lines`, `append_line`,
  `file_size`, `touch`, `is_empty_file`.
- **`stdlib/fmt.nq`** — printf-style formatting: `fmt(template, args)`,
  `printf`, `fmt_num`, `fmt_pad`, `fmt_table`.

### Internal

- `LoopCtx` in `compiler.cpp` extended with `continue_target`,
  `outer_local_count`, `scope_depth`, `continue_patches` fields.
- `include/nolqu.h` version macros guarded with `#ifndef` to prevent
  redefinition warnings when included alongside `src/common.h`.

---

## v1.1.0 — Extended Standard Library (2026)

### Language
- **`break`** — exit the nearest enclosing loop immediately
- **`continue`** — skip to the next loop iteration (re-evaluates condition)
- Both are compile-time errors when used outside a loop
- Nested loops: `break`/`continue` only affect the innermost loop

### Standard Library — New Modules
- **`stdlib/time.nq`** — `now`, `millis`, `elapsed`, `sleep`, `format_duration`, `benchmark`
- **`stdlib/string.nq`** — `is_empty`, `lines`, `words`, `count`, `replace_all`,
  `lstrip`, `rstrip`, `pad_left`, `pad_right`, `center`, `truncate`
- **`stdlib/path.nq`** — `path_join`, `basename`, `dirname`, `ext`, `strip_ext`,
  `is_absolute`, `normalize`
- **`stdlib/json.nq`** — `json_object`, `json_set`, `json_get`, `json_has`,
  `json_keys`, `json_is_object`, `json_stringify`, `json_parse`, `json_parse_error`
- **`stdlib/test.nq`** — `suite`, `expect`, `expect_eq`, `expect_err`, `done`

### Standard Library — Extended
- **`stdlib/math.nq`** — added `PI`, `TAU`, `E`, `sin`, `cos`, `tan`,
  `degrees`, `radians`, `log`, `log2`, `log10`

### Bug Fixes
- `stdlib/string.nq` — `replace_all` double-appended the tail after the last match
- `stdlib/json.nq` — scientific notation (`1e15`) not supported by Nolqu lexer,
  replaced with plain literals
- `stdlib/json.nq` — multi-line `or` chain in `_jp_is_numchar` caused parse error,
  split into separate `if` blocks
- `stdlib/test.nq` — `\xNN` Unicode escapes not supported by Nolqu string literals,
  replaced with ASCII `[PASS]` / `[FAIL]`

### Project Health
- **`CONTRIBUTING.md`** — code style, commit format, PR process, development workflow
- **`SECURITY.md`** — vulnerability reporting (nolqucontact@gmail.com), scope, disclosure policy
- **`CODE_OF_CONDUCT.md`** — community standards (nolquteam@gmail.com)
- **`.gitignore`** — C/C++, editor, OS artefacts
- **`.github/ISSUE_TEMPLATE/bug_report.md`**
- **`.github/ISSUE_TEMPLATE/feature_request.md`**
- **`.github/PULL_REQUEST_TEMPLATE.md`**

### Documentation
- `docs/dev/language.md` — added `break`/`continue` section, removed from Known Limitations
- `docs/dev/grammar.md` — `break_stmt`, `continue_stmt` added to grammar, keywords updated
- `docs/dev/stdlib.md` — all 5 new modules documented with examples

---

## v1.0.0 — Stable Release (2026)

**First stable release of Nolqu.** Language and bytecode format are now frozen.
Programs written for v1.0.0 will continue to run on all future 1.x versions.

### Bug Fixes

- **`vm.c` — memory tracker underflow (critical):**
  Seven native functions (`nativeUpper`, `nativeLower`, `nativeRepeat`,
  `nativeReplace`, `nativeJoin`, `nativeFileRead`, `concatStrings`) used
  `malloc()` to allocate string buffers that were then handed to `takeString()`.
  When `takeString` deduplicates against the intern table it frees the buffer
  via `nq_realloc()` — but since the buffer was allocated with `malloc`, the
  byte counter `nq_bytes_allocated` underflowed, causing the GC threshold to
  never be reached (GC would not fire).
  Fixed: all native buffers now use `nq_realloc(NULL, 0, size)`.

- **`vm.c` — `nativeFileLines` free mismatch:**
  The temporary read buffer was allocated via `nq_realloc` but freed with
  bare `free()`. Fixed: `free(buf)` → `nq_realloc(buf, size + 1, 0)`.

### Code Improvements

- **`nq_embed.cpp` — native registration refactored:**
  Replaced the 16-slot `MAKE_WRAPPER` macro table with a clean C++ template
  trampoline system (`TrampolineTable<Is...>` + `MakeSeq<N>`).
  Limit raised from 16 to 64 registered host-native functions.
  Dead `dispatch_wrapper` stub removed.

- **Documentation completed:**
  - `README.md` — full language tour, all builtins, embed API, project structure
  - `docs/dev/grammar.md` — complete EBNF, type system, scoping rules, limitations
  - `docs/dev/vm_design.md` — full instruction set reference, GC phases, error flow

- **Test suite expanded:**
  All four test files (`math_test.nq`, `string_test.nq`, `array_test.nq`,
  `error_test.nq`) rewritten with correct syntax and broader coverage.
  `array_test.nq` previously used `!contains()` (unsupported prefix `!` on calls);
  fixed to `not contains()`.

### Stability

- 17/17 tests passing
- Zero warnings in release build
- ASan + UBSan clean (debug build)
- Version string updated: `1.0.0-rc1` → `1.0.0`

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

Promoted from v1.2.2a2. No code changes.

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

### Performance: zero-copy loop variable + branch prediction hints

**for-range: zero-copy loop variable alias**

Previously `for i = 0 to N` emitted two extra opcodes **every iteration**:
- `OP_GET_LOCAL __i` → push onto new slot (visible var copy)
- `OP_POP` on `endScope` when the iteration ends

Fix: the user's variable name is registered as an alias pointing directly
to the `__i` slot. No copy, no extra push/pop. The body reads and writes
the counter directly.

Result:
| | a2 | a3 |
|---|---|---|
| for-range empty 100M | 3279ms | 2711ms (-17%) |
| for-range ≈ loop | — | ✅ Same speed |
| 1B iterations (local) | — | ~39s |

**NQ_LIKELY / NQ_UNLIKELY branch hints**

Added `__builtin_expect` hints on the most-executed paths:
- `BINARY_NUM` type check: the number case is almost always taken
- `JUMP_IF_FALSE`: the loop-continue case (condition true) is dominant
- `tableGet`: successful lookup is the common path; miss is unlikely
- `findEntry`: key pointer equality is the expected outcome

**`NQ_INLINE` + `NQ_RESTRICT` on `findEntry`**

`findEntry` is called on every global variable access. Marking it
`always_inline` and the `entries` pointer as `restrict` allows the
compiler to avoid reload/alias checks inside the lookup loop.

**Compiler hints defined in `common.h`:**
```c
NQ_INLINE      // __attribute__((always_inline)) static inline
NQ_RESTRICT    // __restrict__
NQ_LIKELY(x)   // __builtin_expect(!!(x), 1)
NQ_UNLIKELY(x) // __builtin_expect(!!(x), 0)
```
All fall back to no-ops on non-GCC/Clang compilers.

---

## v1.2.2a2 — Alpha 2 (2026)

### Performance optimization

**Compiler: O3 + march=native + flto** (was O2)

`-O3` enables loop vectorization, more aggressive inlining, and strength
reduction. `-march=native` lets the compiler use all CPU instructions available
on the build machine. `-flto` enables link-time optimization across translation
units — particularly effective for the VM dispatch loop which calls helpers
defined in separate .c files.

**for-range: step-direction check hoisted out of the loop**

Previously, `for i = 0 to N` emitted a `step >= 0` branch **every iteration**
to decide whether to use `i < stop` or `i > stop`. Since the step is almost
always a compile-time constant, this was pure overhead.

Now:
- If step is a literal (default `1`, or e.g. `step -1`, `step 2`) → the
  compiler chooses the comparison at compile time. The loop body is just
  `GET_LOCAL; GET_LOCAL; LT; JUMP_IF_FALSE` — one fewer comparison per iter.
- If step is a runtime expression → direction is checked **once before the
  loop**, then two specialized loops are emitted (positive and negative).
  No per-iteration branching.
- Zero step → `ValueError` at compile time.

**Benchmark results (1 billion iterations, `for i = 0 to 1_000_000_000`):**

| Version | Time |
|---|---|
| v1.2.2a1 | ~61s |
| v1.2.2a2 | ~43s |
| Improvement | **~29% faster** |

---

## v1.2.2a1 — Alpha 1 (2026)

### New: Numeric `for` loop (`for i = start to stop`)

Nolqu now has a pure counter-based loop with no array allocation:

```nolqu
# Equivalent to Python:  for i in range(1_000_000_000): number += 1
let number = 0
for i = 0 to 1000000000
  number += 1
end
print number   # 1000000000
```

Syntax:
```nolqu
for i = start to stop            # step defaults to +1
for i = start to stop step s     # custom step (any number, including negative)
```

Features:
- `break` and `continue` work correctly
- Nested loops work correctly
- `step` can be negative: `for i = 10 to 0 step -1`
- `to` and `step` are contextual — still usable as variable names
- Compiles to a pure counter loop: no array created, no `len()` call
- Compatible with existing `for item in array` syntax

Benchmark (1 billion iterations):
- Nolqu v1.2.2a1: ~61s on this machine
- The loop overhead is minimal — two GET_LOCAL + ADD + SET_LOCAL per iteration

Implementation:
- `NODE_FOR_RANGE` added to ast.h / ast.cpp
- Parser: detects `for x = ...` vs `for x in ...` by checking for `=` after ident
- Compiler: emits step-direction check (positive vs negative), correct break/continue

---

## v1.2.1 — Stable Release (2026)

Promoted from v1.2.1-rc1 with no code changes.

### What's new since v1.2.0

- **REPL auto-print** — `1 + 2` → `3`, identifiers, all binary operators
- **Circular import detection** — `A → B → A` → `[ImportError]` with cycle chain
- **`from X import a, b`** — compile-time name verification, `[ImportError]` on typo
- **CLI command suggestion** — `nq verdion` → `Did you mean: version ?`
- **Typed error brackets** — `[TypeError]` `[NameError]` `[IndexError]` `[ValueError]` `[ImportError]` `[SyntaxError]` `[IOError]` `[UsageError]`
- **`range()` `first()` `last()` `find()` `count_if()`** in `stdlib/array`
- **`str_contains()` `starts_with()` `ends_with()`** aliases in `stdlib/string`
- **Parser refactor** — `parseExprWithLeft()`, no duplication
- **Better error messages** — arithmetic shows operand types, import shows searched paths
- **CLI usage errors** — `nq check` without filename shows clear usage
- **License year updated to 2026**

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

Promoted from v1.2.2a2. No code changes.

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

### Performance: zero-copy loop variable + branch prediction hints

**for-range: zero-copy loop variable alias**

Previously `for i = 0 to N` emitted two extra opcodes **every iteration**:
- `OP_GET_LOCAL __i` → push onto new slot (visible var copy)
- `OP_POP` on `endScope` when the iteration ends

Fix: the user's variable name is registered as an alias pointing directly
to the `__i` slot. No copy, no extra push/pop. The body reads and writes
the counter directly.

Result:
| | a2 | a3 |
|---|---|---|
| for-range empty 100M | 3279ms | 2711ms (-17%) |
| for-range ≈ loop | — | ✅ Same speed |
| 1B iterations (local) | — | ~39s |

**NQ_LIKELY / NQ_UNLIKELY branch hints**

Added `__builtin_expect` hints on the most-executed paths:
- `BINARY_NUM` type check: the number case is almost always taken
- `JUMP_IF_FALSE`: the loop-continue case (condition true) is dominant
- `tableGet`: successful lookup is the common path; miss is unlikely
- `findEntry`: key pointer equality is the expected outcome

**`NQ_INLINE` + `NQ_RESTRICT` on `findEntry`**

`findEntry` is called on every global variable access. Marking it
`always_inline` and the `entries` pointer as `restrict` allows the
compiler to avoid reload/alias checks inside the lookup loop.

**Compiler hints defined in `common.h`:**
```c
NQ_INLINE      // __attribute__((always_inline)) static inline
NQ_RESTRICT    // __restrict__
NQ_LIKELY(x)   // __builtin_expect(!!(x), 1)
NQ_UNLIKELY(x) // __builtin_expect(!!(x), 0)
```
All fall back to no-ops on non-GCC/Clang compilers.

---

## v1.2.2a2 — Alpha 2 (2026)

### Performance optimization

**Compiler: O3 + march=native + flto** (was O2)

`-O3` enables loop vectorization, more aggressive inlining, and strength
reduction. `-march=native` lets the compiler use all CPU instructions available
on the build machine. `-flto` enables link-time optimization across translation
units — particularly effective for the VM dispatch loop which calls helpers
defined in separate .c files.

**for-range: step-direction check hoisted out of the loop**

Previously, `for i = 0 to N` emitted a `step >= 0` branch **every iteration**
to decide whether to use `i < stop` or `i > stop`. Since the step is almost
always a compile-time constant, this was pure overhead.

Now:
- If step is a literal (default `1`, or e.g. `step -1`, `step 2`) → the
  compiler chooses the comparison at compile time. The loop body is just
  `GET_LOCAL; GET_LOCAL; LT; JUMP_IF_FALSE` — one fewer comparison per iter.
- If step is a runtime expression → direction is checked **once before the
  loop**, then two specialized loops are emitted (positive and negative).
  No per-iteration branching.
- Zero step → `ValueError` at compile time.

**Benchmark results (1 billion iterations, `for i = 0 to 1_000_000_000`):**

| Version | Time |
|---|---|
| v1.2.2a1 | ~61s |
| v1.2.2a2 | ~43s |
| Improvement | **~29% faster** |

---

## v1.2.2a1 — Alpha 1 (2026)

### New: Numeric `for` loop (`for i = start to stop`)

Nolqu now has a pure counter-based loop with no array allocation:

```nolqu
# Equivalent to Python:  for i in range(1_000_000_000): number += 1
let number = 0
for i = 0 to 1000000000
  number += 1
end
print number   # 1000000000
```

Syntax:
```nolqu
for i = start to stop            # step defaults to +1
for i = start to stop step s     # custom step (any number, including negative)
```

Features:
- `break` and `continue` work correctly
- Nested loops work correctly
- `step` can be negative: `for i = 10 to 0 step -1`
- `to` and `step` are contextual — still usable as variable names
- Compiles to a pure counter loop: no array created, no `len()` call
- Compatible with existing `for item in array` syntax

Benchmark (1 billion iterations):
- Nolqu v1.2.2a1: ~61s on this machine
- The loop overhead is minimal — two GET_LOCAL + ADD + SET_LOCAL per iteration

Implementation:
- `NODE_FOR_RANGE` added to ast.h / ast.cpp
- Parser: detects `for x = ...` vs `for x in ...` by checking for `=` after ident
- Compiler: emits step-direction check (positive vs negative), correct break/continue

---

## v1.2.1 — Stable Release (2026)

Promoted from v1.2.1-rc1 with no code changes.

### What's new since v1.2.0

- **REPL auto-print** — expressions evaluated and printed automatically
- **REPL binary operators** — `x + 1`, `a < b and b < 10` all work
- **Circular import detection** — `A → B → A` → `[ImportError]`
- **`from X import a, b`** — compile-time name verification
- **CLI command suggestion** — `nq verdion` → `Did you mean: version?`
- **Typed error brackets** — `[TypeError]` `[NameError]` `[IndexError]` `[ValueError]` `[ImportError]` `[SyntaxError]` `[IOError]` `[UsageError]`
- **`range()` `first()` `last()` `find()` `count_if()`** in stdlib/array
- **`str_contains()` `starts_with()` `ends_with()`** in stdlib/string
- **`parseExprWithLeft`** — clean REPL parser, no duplication
- **License year updated** to 2026

### No breaking changes from v1.2.0

---

## v1.2.1-rc1 — Release Candidate (2026)

Promoted from v1.2.1b3. No code changes.

All planned features for v1.2.1 are implemented, tested, and stable.
This release is ready for broader testing before stable promotion.

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for the full feature summary.

---

## v1.2.1b3 — Beta 4 (2026)

### All typed errors now show correct bracket header

Previously `vmRuntimeError` always printed `[RuntimeError]` regardless of what
error was inside. Now both `vmRuntimeError` (runtime) and `compileError`
(compile-time) extract the type prefix from the message and use it as the
bracket label:

```
[TypeError]   /tmp/e.nq:2
  TypeError: arithmetic requires numbers, got string and number.

[NameError]   /tmp/e.nq:5
  NameError: 'x' is not defined. Did you mean 'y'?

[IndexError]  /tmp/e.nq:3
  IndexError: array index 99 is out of bounds (length 3).

[ValueError]  /tmp/e.nq:1
  ValueError: division by zero. Check the divisor before dividing.

[ImportError] /tmp/e.nq:1
  ImportError: module 'mymod' not found.
  Searched:
      ./mymod.nq

[SyntaxError] /tmp/e.nq:4
  Invalid expression. Expected a number...

[RuntimeError] /tmp/e.nq:7
  something broke         ← plain error("message") with no prefix
```

Implementation: format the message string first, scan for `Word: ` prefix
using the same logic as `error_type()`, then use it as the bracket label.
Falls back to `[RuntimeError]` / `[SyntaxError]` if no recognized prefix.
Zero duplication — the same extraction logic works in both the VM and compiler.

Full error type map now consistent end-to-end:

| Bracket header | Cause |
|---|---|
| `[TypeError]` | Wrong type for operation |
| `[NameError]` | Undefined/undeclared variable |
| `[IndexError]` | Array/string out of bounds |
| `[ValueError]` | Division by zero, invalid value |
| `[ImportError]` | Module not found, bad name, circular import |
| `[SyntaxError]` | Parse error, compile error |
| `[RuntimeError]` | Uncaught `error("plain message")` |
| `[IOError]` | File not found, read failure |
| `[UsageError]` | Unknown command, missing filename |
| `[Warning]` | Unused variable |

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

Promoted from v1.2.2a2. No code changes.

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

### Performance: zero-copy loop variable + branch prediction hints

**for-range: zero-copy loop variable alias**

Previously `for i = 0 to N` emitted two extra opcodes **every iteration**:
- `OP_GET_LOCAL __i` → push onto new slot (visible var copy)
- `OP_POP` on `endScope` when the iteration ends

Fix: the user's variable name is registered as an alias pointing directly
to the `__i` slot. No copy, no extra push/pop. The body reads and writes
the counter directly.

Result:
| | a2 | a3 |
|---|---|---|
| for-range empty 100M | 3279ms | 2711ms (-17%) |
| for-range ≈ loop | — | ✅ Same speed |
| 1B iterations (local) | — | ~39s |

**NQ_LIKELY / NQ_UNLIKELY branch hints**

Added `__builtin_expect` hints on the most-executed paths:
- `BINARY_NUM` type check: the number case is almost always taken
- `JUMP_IF_FALSE`: the loop-continue case (condition true) is dominant
- `tableGet`: successful lookup is the common path; miss is unlikely
- `findEntry`: key pointer equality is the expected outcome

**`NQ_INLINE` + `NQ_RESTRICT` on `findEntry`**

`findEntry` is called on every global variable access. Marking it
`always_inline` and the `entries` pointer as `restrict` allows the
compiler to avoid reload/alias checks inside the lookup loop.

**Compiler hints defined in `common.h`:**
```c
NQ_INLINE      // __attribute__((always_inline)) static inline
NQ_RESTRICT    // __restrict__
NQ_LIKELY(x)   // __builtin_expect(!!(x), 1)
NQ_UNLIKELY(x) // __builtin_expect(!!(x), 0)
```
All fall back to no-ops on non-GCC/Clang compilers.

---

## v1.2.2a2 — Alpha 2 (2026)

### Performance optimization

**Compiler: O3 + march=native + flto** (was O2)

`-O3` enables loop vectorization, more aggressive inlining, and strength
reduction. `-march=native` lets the compiler use all CPU instructions available
on the build machine. `-flto` enables link-time optimization across translation
units — particularly effective for the VM dispatch loop which calls helpers
defined in separate .c files.

**for-range: step-direction check hoisted out of the loop**

Previously, `for i = 0 to N` emitted a `step >= 0` branch **every iteration**
to decide whether to use `i < stop` or `i > stop`. Since the step is almost
always a compile-time constant, this was pure overhead.

Now:
- If step is a literal (default `1`, or e.g. `step -1`, `step 2`) → the
  compiler chooses the comparison at compile time. The loop body is just
  `GET_LOCAL; GET_LOCAL; LT; JUMP_IF_FALSE` — one fewer comparison per iter.
- If step is a runtime expression → direction is checked **once before the
  loop**, then two specialized loops are emitted (positive and negative).
  No per-iteration branching.
- Zero step → `ValueError` at compile time.

**Benchmark results (1 billion iterations, `for i = 0 to 1_000_000_000`):**

| Version | Time |
|---|---|
| v1.2.2a1 | ~61s |
| v1.2.2a2 | ~43s |
| Improvement | **~29% faster** |

---

## v1.2.2a1 — Alpha 1 (2026)

### New: Numeric `for` loop (`for i = start to stop`)

Nolqu now has a pure counter-based loop with no array allocation:

```nolqu
# Equivalent to Python:  for i in range(1_000_000_000): number += 1
let number = 0
for i = 0 to 1000000000
  number += 1
end
print number   # 1000000000
```

Syntax:
```nolqu
for i = start to stop            # step defaults to +1
for i = start to stop step s     # custom step (any number, including negative)
```

Features:
- `break` and `continue` work correctly
- Nested loops work correctly
- `step` can be negative: `for i = 10 to 0 step -1`
- `to` and `step` are contextual — still usable as variable names
- Compiles to a pure counter loop: no array created, no `len()` call
- Compatible with existing `for item in array` syntax

Benchmark (1 billion iterations):
- Nolqu v1.2.2a1: ~61s on this machine
- The loop overhead is minimal — two GET_LOCAL + ADD + SET_LOCAL per iteration

Implementation:
- `NODE_FOR_RANGE` added to ast.h / ast.cpp
- Parser: detects `for x = ...` vs `for x in ...` by checking for `=` after ident
- Compiler: emits step-direction check (positive vs negative), correct break/continue

---

## v1.2.1 — Stable Release (2026)

Promoted from v1.2.1-rc1 with no code changes.

### What's new since v1.2.0

- **REPL auto-print** — `1 + 2` → `3`, identifiers, all binary operators
- **Circular import detection** — `A → B → A` → `[ImportError]` with cycle chain
- **`from X import a, b`** — compile-time name verification, `[ImportError]` on typo
- **CLI command suggestion** — `nq verdion` → `Did you mean: version ?`
- **Typed error brackets** — `[TypeError]` `[NameError]` `[IndexError]` `[ValueError]` `[ImportError]` `[SyntaxError]` `[IOError]` `[UsageError]`
- **`range()` `first()` `last()` `find()` `count_if()`** in `stdlib/array`
- **`str_contains()` `starts_with()` `ends_with()`** aliases in `stdlib/string`
- **Parser refactor** — `parseExprWithLeft()`, no duplication
- **Better error messages** — arithmetic shows operand types, import shows searched paths
- **CLI usage errors** — `nq check` without filename shows clear usage
- **License year updated to 2026**

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

Promoted from v1.2.2a2. No code changes.

---

## v1.2.2a4 — Alpha 4 (2026)

### Performance: superinstructions (69% faster than v1.2.1)

**`OP_ADD_LOCAL_CONST` — fused increment**

The pattern `local += number` (and `-=`) is the most common operation in
loops. Previously it emitted 5 opcodes per execution:
```
GET_LOCAL slot    # push value
CONST step        # push step
ADD               # pop 2, push result
SET_LOCAL slot    # peek → slot (value stays on stack)
POP               # discard
```
Now it emits 1 opcode:
```
ADD_LOCAL_CONST slot [const_idx]   # slots[slot] += const, no stack traffic
```
Applied automatically to:
- `for i = 0 to N` increment (always)
- `n += 1`, `n -= 2`, any `local op= literal_number` (compound assign)

**`OP_ADD_LOCAL_CONST` applied in two places:**
1. `for-range` increment path — replaces the 5-opcode sequence unconditionally
2. `NODE_COMPOUND_ASSIGN` — detects `local += number_literal` at compile time

**Benchmark results:**

| Benchmark | v1.2.1 | v1.2.2a2 | v1.2.2a4 | Total gain |
|---|---|---|---|---|
| 1B iters (local `+=`) | 61s | 43s | **19s** | **-69%** |
| for-range empty 100M | — | 2711ms | 1726ms | — |
| local `+= 1` 1B | — | 39s | 19s | — |

**Updated benchmark file** (`benchmark.nq`) now uses a local variable inside
a function — the correct way to avoid global hash-table overhead:
```nolqu
function run()
  let number = 0
  for i = 0 to 1000000000
    number += 1
  end
  return number
end
print run()
```

---

## v1.2.2a3 — Alpha 3 (2026)

### Performance: zero-copy loop variable + branch prediction hints

**for-range: zero-copy loop variable alias**

Previously `for i = 0 to N` emitted two extra opcodes **every iteration**:
- `OP_GET_LOCAL __i` → push onto new slot (visible var copy)
- `OP_POP` on `endScope` when the iteration ends

Fix: the user's variable name is registered as an alias pointing directly
to the `__i` slot. No copy, no extra push/pop. The body reads and writes
the counter directly.

Result:
| | a2 | a3 |
|---|---|---|
| for-range empty 100M | 3279ms | 2711ms (-17%) |
| for-range ≈ loop | — | ✅ Same speed |
| 1B iterations (local) | — | ~39s |

**NQ_LIKELY / NQ_UNLIKELY branch hints**

Added `__builtin_expect` hints on the most-executed paths:
- `BINARY_NUM` type check: the number case is almost always taken
- `JUMP_IF_FALSE`: the loop-continue case (condition true) is dominant
- `tableGet`: successful lookup is the common path; miss is unlikely
- `findEntry`: key pointer equality is the expected outcome

**`NQ_INLINE` + `NQ_RESTRICT` on `findEntry`**

`findEntry` is called on every global variable access. Marking it
`always_inline` and the `entries` pointer as `restrict` allows the
compiler to avoid reload/alias checks inside the lookup loop.

**Compiler hints defined in `common.h`:**
```c
NQ_INLINE      // __attribute__((always_inline)) static inline
NQ_RESTRICT    // __restrict__
NQ_LIKELY(x)   // __builtin_expect(!!(x), 1)
NQ_UNLIKELY(x) // __builtin_expect(!!(x), 0)
```
All fall back to no-ops on non-GCC/Clang compilers.

---

## v1.2.2a2 — Alpha 2 (2026)

### Performance optimization

**Compiler: O3 + march=native + flto** (was O2)

`-O3` enables loop vectorization, more aggressive inlining, and strength
reduction. `-march=native` lets the compiler use all CPU instructions available
on the build machine. `-flto` enables link-time optimization across translation
units — particularly effective for the VM dispatch loop which calls helpers
defined in separate .c files.

**for-range: step-direction check hoisted out of the loop**

Previously, `for i = 0 to N` emitted a `step >= 0` branch **every iteration**
to decide whether to use `i < stop` or `i > stop`. Since the step is almost
always a compile-time constant, this was pure overhead.

Now:
- If step is a literal (default `1`, or e.g. `step -1`, `step 2`) → the
  compiler chooses the comparison at compile time. The loop body is just
  `GET_LOCAL; GET_LOCAL; LT; JUMP_IF_FALSE` — one fewer comparison per iter.
- If step is a runtime expression → direction is checked **once before the
  loop**, then two specialized loops are emitted (positive and negative).
  No per-iteration branching.
- Zero step → `ValueError` at compile time.

**Benchmark results (1 billion iterations, `for i = 0 to 1_000_000_000`):**

| Version | Time |
|---|---|
| v1.2.2a1 | ~61s |
| v1.2.2a2 | ~43s |
| Improvement | **~29% faster** |

---

## v1.2.2a1 — Alpha 1 (2026)

### New: Numeric `for` loop (`for i = start to stop`)

Nolqu now has a pure counter-based loop with no array allocation:

```nolqu
# Equivalent to Python:  for i in range(1_000_000_000): number += 1
let number = 0
for i = 0 to 1000000000
  number += 1
end
print number   # 1000000000
```

Syntax:
```nolqu
for i = start to stop            # step defaults to +1
for i = start to stop step s     # custom step (any number, including negative)
```

Features:
- `break` and `continue` work correctly
- Nested loops work correctly
- `step` can be negative: `for i = 10 to 0 step -1`
- `to` and `step` are contextual — still usable as variable names
- Compiles to a pure counter loop: no array created, no `len()` call
- Compatible with existing `for item in array` syntax

Benchmark (1 billion iterations):
- Nolqu v1.2.2a1: ~61s on this machine
- The loop overhead is minimal — two GET_LOCAL + ADD + SET_LOCAL per iteration

Implementation:
- `NODE_FOR_RANGE` added to ast.h / ast.cpp
- Parser: detects `for x = ...` vs `for x in ...` by checking for `=` after ident
- Compiler: emits step-direction check (positive vs negative), correct break/continue

---

## v1.2.1 — Stable Release (2026)

Promoted from v1.2.1-rc1 with no code changes.

### What's new since v1.2.0

- **REPL auto-print** — expressions evaluated and printed automatically
- **REPL binary operators** — `x + 1`, `a < b and b < 10` all work
- **Circular import detection** — `A → B → A` → `[ImportError]`
- **`from X import a, b`** — compile-time name verification
- **CLI command suggestion** — `nq verdion` → `Did you mean: version?`
- **Typed error brackets** — `[TypeError]` `[NameError]` `[IndexError]` `[ValueError]` `[ImportError]` `[SyntaxError]` `[IOError]` `[UsageError]`
- **`range()` `first()` `last()` `find()` `count_if()`** in stdlib/array
- **`str_contains()` `starts_with()` `ends_with()`** in stdlib/string
- **`parseExprWithLeft`** — clean REPL parser, no duplication
- **License year updated** to 2026

### No breaking changes from v1.2.0

---

## v1.2.1-rc1 — Release Candidate (2026)

Promoted from v1.2.1b3. No code changes.

All planned features for v1.2.1 are implemented, tested, and stable.
This release is ready for broader testing before stable promotion.

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for the full feature summary.

---

## v1.2.1b3 — Beta 3 (2026)

### CLI: Unknown command suggestion

When you mistype a command, Nolqu now detects it and suggests the closest
valid command using Levenshtein distance:

```
$ nq verdion
[UsageError] Unknown command: verdion
  Did you mean: version ?

$ nq chekc myfile.nq
[UsageError] Unknown command: chekc
  Did you mean: check ?
```

Rules: only triggers when the input has no `.nq` extension, the file does not
exist, and the edit distance to a valid command is ≤ 2. Long or unrelated
inputs fall through to the normal `[IOError]` (file not found) path.

### Error prefixes: typed and consistent

All error output now uses typed prefixes instead of the generic `[ Error ]`:

| Old | New | When |
|---|---|---|
| `[ Error ]` (file) | `[IOError]` | File not found, read failure |
| `[ Error ]` (usage) | `[UsageError]` | Unknown command, missing filename |
| `[ Compile Error ]` | `[SyntaxError]` | Parse error, compile error |
| `[ Warning ]` | `[Warning]` | Unused variable etc. |
| `[ Runtime Error ]` | `[RuntimeError]` | Uncaught runtime error |
| `[ check ] ok` | `[OK]` | nq check pass |
| `[ check ] error` | `[SyntaxError]` | nq check fail |

Typed prefixes in `try/catch` error values remain unchanged
(`TypeError:`, `NameError:`, `IndexError:`, `ValueError:`, `ImportError:`).

---

## v1.2.1b2 — Beta 2 (2026)

### Refactor: REPL expression parser (no duplication)

`b1` introduced `parseExprWithLeft()` as a concept but still had a 60-line
duplicated precedence tower in the `TK_IDENT` case. `b2` completes the
refactor:

- Added `parseExprWithLeft(Parser* p, ASTNode* left)` — a proper function
  that accepts an already-built left-hand node and continues through the
  full precedence chain (index → multiply → add → comparison → equality →
  and → or).
- The `TK_IDENT` repl_mode block is now 5 lines: detect operator, call
  `parseExprWithLeft`.
- Added forward declaration so the function can be called from `TK_IDENT`
  case before it is defined.

Behavior is identical to `b1`. This is purely a code quality fix — one place
to maintain, no duplication, future operators automatically handled.

---

## v1.2.1b1 — Beta 1 (2026)

**First beta release.** Feature freeze. Only bug fixes until v1.2.1 stable.

### Bug Fix: REPL expression evaluation with binary operators

In alpha 4, `x + 1` worked but only because the pre-advance literal check caught
number tokens. Identifier-based expressions like `a + b`, `x > 3`, `x == 5`,
`a < b and b < 10` all failed with "Expected a new line after statement."

Root cause: the `TK_IDENT` case in `parseStmt` parsed the identifier and any
following call `(...)`, then immediately called `expectNewline`. In repl_mode,
binary operators after the identifier were not consumed.

Fix: added an operator-continuation block in the `TK_IDENT` case for repl_mode
that walks through multiply → addition → comparison → equality → and → or,
building the binary expression tree correctly. The `and`/`or` rhs now uses
`parseComparison` (not `parseUnary`) so `a < b and b < 10` works as expected.

All REPL expression forms now work:
```
nq > let x = 5
nq > x + 1
6
nq > x > 3
true
nq > x == 5
true
nq > let a = 3
nq > let b = 7
nq > a < b and b < 10
true
nq > [1, 2, 3][0]
1
```

---

## v1.2.1a4 — Alpha 4 (2026)

### Features

**REPL auto-print expressions**

The REPL now evaluates and prints expression results automatically — no need
to wrap everything in `print`. Non-null values are displayed; null is silent.

```
nq > 1 + 2
3
nq > "hello"
hello
nq > [1, 2, 3]
[1, 2, 3]
nq > let x = 99
nq > x
99
nq > null
nq >
```

Supported in REPL: number/string/bool/array literals, arithmetic, comparisons,
function calls, and identifier lookups. Implemented via `repl_mode` flag in
the parser and compiler; normal scripts are unaffected.

**Circular import detection**

`A → B → A` now produces a clear compile error:

```
ImportError: circular import detected.
  Cycle: /path/to/a.nq -> /path/to/b.nq -> /path/to/a.nq
```

Previously silent (dedup set caused B→A to silently do nothing).
Fixed by checking the `import_stack` before the `already_imported` dedup skip.

### Bug Fixes

- **Circular check order**: the `already_imported` dedup check was firing before
  the import_stack cycle check, suppressing cycle detection. Fixed by reordering.

- **strncat warnings** in circular chain builder replaced with `snprintf`.

---

## v1.2.1a3 — Alpha 3 (2026)

### Focus: import, CLI, errors, stdlib

**1. Import: searched paths in error message**

When a module is not found, the error now shows exactly which paths were tried:
```
ImportError: module 'mymod' not found.
  Searched:
      /path/to/script/mymod.nq
      mymod.nq
```

**2. CLI: proper usage errors**

`nq check`, `nq run`, `nq test`, and `nq compile` without a filename
previously crashed or showed a confusing "Cannot open file: check" error.
Now they show a clear usage message:
```
[ Error ] Missing filename.
  Usage:  nq check <file.nq>
  Run  nq help  for all commands.
```

**3. Error messages: more human, more context**

- **Arithmetic**: `TypeError: arithmetic requires numbers, got string and number.`
  (previously just "arithmetic requires numbers")
- **Undefined variable**: `NameError: 'x' is not defined.`
  (previously "undefined variable 'x'")
- **Cannot assign**: `NameError: cannot assign to 'x' — it has not been declared.`
- **Negate**: `TypeError: cannot apply '-' to a string value.`
- **Division by zero**: `ValueError: division by zero. Check the divisor before dividing.`

**4. stdlib/array: new utilities**

- `range(n)` — `[0, 1, ..., n-1]`
- `range(start, stop)` — `[start, ..., stop-1]`
- `range(start, stop, step)` — supports negative step
- `first(arr)` — first element or null
- `last(arr)` — last element or null
- `find(arr, fn)` — first matching element or null
- `count_if(arr, fn)` — count matching elements

**5. stdlib/string: naming aliases**

- `str_contains(s, sub)` — readable alternative to `contains_str`
- `starts_with(s, prefix)` — consistent with snake_case convention
- `ends_with(s, suffix)`
- `to_upper(s)`, `to_lower(s)`, `repeat_str(s, n)`

---

## v1.2.1a2 — Alpha 2 (2026)

### Bug Fixes

**`as` and `from` incorrectly reserved as keywords**

`let as = "x"` and `let from = "y"` produced errors ("Expected a variable name
after 'let', Found: 'as'"). Fixed by removing `as` and `from` from the keyword
table. They are now **contextual identifiers** — they get special meaning only
in import syntax and are freely usable as variable, function, or parameter names.

```nolqu
let as   = "alias"      # now works
let from = "source"     # now works
function route(as, from)
  return from .. " -> " .. as
end
```

**`from X import Y` dispatch broken when `from` is not a keyword**

When `as`/`from` were removed from the keyword table, `from` became `TK_IDENT`
and the statement dispatcher's `case TK_FROM:` was never reached. Fixed by
checking for the contextual keyword `"from"` at the start of the `TK_IDENT`
case in `parseStmt`.

**Runtime error output: redundant `Error:` prefix**

Errors were printed as `Error: IndexError: array index out of bounds` — the
word "Error:" was redundant since the typed prefix already identifies the
error type. Now prints: `IndexError: array index out of bounds`.

**Old `v1.0.0-rc1` comments in `object.c` and `gc.c`**

Two inline comments still referenced `v1.0.0-rc1`. Removed.

---

## v1.2.1a1 — Alpha 1 (2026)

### Re-introduced features (fully implemented)

**Comparison chaining** — `1 < x < 10`, `a < b <= c`

Safely re-implemented using AST node cloning. The v1.2.0 crash (heap double-free)
was caused by sharing a pointer between two binary nodes. Fixed by cloning the
middle operand — only identifiers and literals can be cloned; complex expressions
(calls, arithmetic) produce a clear compile error directing the user to `let`.

Guarantees:
- Each operand evaluated **exactly once**
- Short-circuit via AND: if first comparison is false, second is skipped
- No shared AST pointers — no double-free
- `freeNode` is always safe

**`from module import name1, name2`**

Fully implemented with compile-time name verification:
- Reads and parses the module's AST at compile time
- Verifies each requested name is declared at the module's top level
- Reports `ImportError: 'X' not found in module 'Y'` if a name is missing
- Module caching respected — module runs at most once even with mixed import/from-import
- Supports both quoted (`from "stdlib/math"`) and unquoted (`from stdlib/math`) paths

Limitation (documented): all module globals become accessible after import
(Nolqu has no namespace isolation). The `from` form provides documentation
clarity and early typo detection.

**`ImportError` prefix** — module not found and invalid name errors now carry
`ImportError:` prefix, consistent with the typed error system.

### Still not supported (with clear compile errors)
- `import X as Y` — no module namespaces in Nolqu

### New test files
- `examples/tests/chain_test.nq` — 14 tests for chaining correctness
- `examples/tests/import_test.nq` — 10 tests for import system

### Documentation
- `docs/dev/language.md` — chaining section + from-import section updated
- `docs/dev/grammar.md` — import rule updated, chaining note added
- `docs/dev/semantics.md` — chaining row updated in summary

---

## v1.2.0 — Stable Release (2026)

**First stable release of the v1.2 series.**

Promoted from v1.2.0-rc2 with no code changes.
All rc2 fixes are included. See rc1 and rc2 entries below for details.

### Summary of what's new since v1.0.0

- `for item in array` — range-based loop with `break`/`continue`
- `+=` `-=` `*=` `/=` `..=` — compound assignment operators
- `const NAME = value` — compile-time immutable binding
- `function f(x = default)` — default parameters
- `arr[1:3]` / `s[1:3]` — slice expressions for arrays and strings
- `null` keyword — alias for `nil`
- `bool(v)`, `ord(ch)`, `chr(n)`, `error_type(e)` — new builtins
- Typed errors: `TypeError:`, `NameError:`, `IndexError:`, `ValueError:`
- Import: `import stdlib/math` (unquoted path supported)
- Constant pool raised 256 → 65535
- All runtime errors now catchable via `try/catch`
- `str()` precision: 14 significant digits (was 6)
- 11 stdlib modules, 90+ functions

### Breaking changes from v1.0.0

- `0` and `""` (empty string) are now **falsy**
- `nil` prints as `"null"`; `type(nil)` returns `"null"`
- Error messages carry type prefix: `"NameError: ..."` not `"Undefined variable..."`

---

## v1.2.0-rc2 — Release Candidate 2 (2026)

Stability-only pass. No new features.

### Critical Bug Fixes

**Comparison chaining caused heap crash (memory corruption)**

`1 < x < 10` and even `a < x < b` with simple identifiers caused a
`free(): double free detected` crash at runtime. Root cause: the middle
operand AST node was shared (same pointer) between two binary nodes in the
generated AND-chain. When `freeNode` freed the left subtree, then the right
subtree, the shared node was freed twice → heap corruption.

Fix: comparison chaining is **removed**. `parseComparison` reverted to a
simple left-to-right loop with no chaining. Use `a < x and x < 10` instead.

**`import X as Y` was a silent no-op**

The alias was never created. `import stdlib/math as m` would run the module
(PI, sin etc. became global) but `m` itself was undefined. Now produces a
clear compile error:
```
'import X as Y' is not supported — Nolqu has no module namespaces.
  Use:  import "stdlib/math"
  Then access its functions directly: PI, sin, cos, ...
```

**`from X import Y` imported everything, not just Y**

`from stdlib/math import sin` imported the entire module (PI, cos, etc all
became global), ignoring the names listed. Now produces a clear compile error:
```
'from X import Y' is not supported.
  Use:  import "stdlib/math"
  All module definitions become available globally after import.
```

### Documentation

- `README.md` — added prominent `[!IMPORTANT]` falsy/truthy warning before
  the feature list. Version updated to rc2.
- `docs/dev/language.md` — import section updated (removed `as`/`from`
  examples, added error note); const clarified with array mutation example;
  comparison chaining limitation updated to say "not supported".
- `docs/dev/semantics.md` — falsy section upgraded to `[!IMPORTANT]` callout;
  const/array mutation section expanded with explicit note about lack of
  freeze mechanism; comparison chaining marked "Not supported" in summary.
- `docs/dev/grammar.md` — import rule simplified; `as`/`from` removed from
  keywords list with note they are reserved.

### Also fixed

- `import stdlib/path as X` path-parsing bug: the unquoted path scanner was
  consuming `as` as part of the module path (resulting in `stdlib/pathas` or
  similar). Fixed by stopping at the first token that isn't preceded by `/`.

---

## v1.2.0-rc1 — Release Candidate (2026)

Stabilization pass before the v1.2.0 stable release.
No new language features. All changes are fixes and cleanup.

### Fixes

- **`nq help` accuracy** — stdlib list now correct: `stdlib/os` shows actual
  functions (`env_or`, `path_exists`, etc.); `stdlib/fmt` shows `fmt`/`printf`;
  `stdlib/io` added; `bool()` and `error_type()` added to built-in list;
  duplicate `ord`/`chr` entry removed; syntax quick-ref updated with `const`
  and slice examples.

- **Remaining `"nil"` in error messages** — four error message format strings
  in `vm.c` still used `"nil"` as a type name. All updated to `"null"`.

- **All `docs/dev/` files** — headers, banners, and internal version references
  updated from `v1.1.1a4`/`a5`/`a6` to `v1.2.0-rc1 (Release Candidate)`.
  Per-version tags in `stdlib.md` (e.g. `*(v1.1.1a4)*`) removed — the docs
  now describe the current state, not the history.

- **`RELEASE_NOTES.md`** — fully rewritten for v1.2.0. Covers all new
  language features, error system, import syntax, stdlib summary, upgrade
  guide, and known limitations.

- **`README.md`** — banner updated from "alpha" to "release candidate";
  version references updated.

- **`ROADMAP.md`** — v1.2.0-rc1 added; stable promotion criteria updated
  to reflect current state (most boxes already checked).

---

## v1.1.1a6 — Alpha 6 (2026)

### Language / Runtime

- **Typed errors** — all runtime errors now carry a type prefix:
  `TypeError:`, `NameError:`, `IndexError:`, `ValueError:`.
  Stdlib errors updated to match. Use `error_type(e)` to dispatch.

- **`bool(v)` builtin** — coerce any value to `true`/`false` using
  the language's truthiness rules. `bool(0) = false`, `bool("hi") = true`.

- **`error_type(e)` builtin** — extract the type prefix from a typed
  error string. `error_type("TypeError: foo") = "TypeError"`.
  Falls back to `"RuntimeError"` for plain error strings.

- **Import syntax extended:**
  - `import stdlib/math` — unquoted slash-separated path
  - `import "stdlib/math" as math` — alias (parsed; no namespace effect)
  - `from stdlib/math import PI, sin` — selective import (runs whole module)

### Standard Library

- **`stdlib/io.nq`** — new unified file I/O module:
  `read_file` · `write_file` · `append_file` · `read_lines` · `write_lines` ·
  `append_line` · `file_size` · `ensure_file` · `copy_file`

### Examples

- **`examples/real/cli_tool.nq`** — interactive task manager (input, file I/O)
- **`examples/real/file_reader.nq`** — file stats and word frequency
- **`examples/real/json_tool.nq`** — JSON build, encode, decode, validate, merge
- **`examples/real/text_processor.nq`** — text transformation pipeline

### Documentation

- `docs/dev/semantics.md` — new section: Typed Errors with dispatch examples
- `docs/dev/language.md` — updated: typed error table, `bool()`/`error_type()`, import forms
- `docs/dev/grammar.md` — added `module_path`, extended `import_stmt`, `from`/`as` keywords

---

## v1.1.1a5 — Alpha 5 (2026)

### Language — New Features

- **`null` keyword** — alias for `nil`. Both print as `"null"`, `type(null)` returns `"null"`.

- **Extended falsy values** — `0` and `""` (empty string) are now falsy in addition
  to `nil`/`null` and `false`. Breaking change from v1.0.0 where only `nil` and `false` were falsy.

- **`const` declarations** — `const NAME = value` creates an immutable binding.
  Reassignment and compound assignment (`+=` etc.) are **compile-time errors**.
  Works at global and local scope. The binding is immutable; array contents can still be mutated.

- **Default parameters** — `function greet(name = "world")`.
  If a parameter is passed as `null` or omitted, the default expression is used.
  Default parameters must follow non-default parameters.

- **Slice expressions** — `arr[start:end]`, `arr[:end]`, `arr[start:]`, `arr[:]`.
  Works on arrays and strings. Negative indices supported. Returns a new value (copy).

- **`and`/`or` return actual values** — already worked since v1.1.1a2, now explicitly
  documented. `1 and 2 = 2`, `nil or 5 = 5`.

### Bug Fixes

- **Default param stack corruption** — the initial implementation omitted a `JUMP`
  to skip the false-branch `OP_POP` in the default injection pattern, causing
  the return value to be concatenated into the string. Fixed.

- **`printValue` now prints `null` for nil** — `print nil` and `print null`
  both output `null`. `str(nil)`, `type(nil)`, and `type(null)` all return `"null"`.

- **Memory leak in `parseFunctionDecl`** — `defaults` array and its AST nodes
  were not freed in `freeNode` for `NODE_FUNCTION`. Fixed; ASan/LSan clean.

### Documentation

- **`docs/dev/semantics.md`** — new comprehensive semantics reference covering:
  truthiness rules, null/nil behavior, function return, logical operator value
  semantics, const rules, and slice edge cases.

- **`docs/dev/language.md`** — updated: falsy rules, null keyword, Known Limitations
  updated with comparison-chaining caveat.

- **`docs/dev/grammar.md`** — added `const_stmt`, `slice_or_index`, `null` keyword,
  `const` keyword, `param_decl` with optional default.

- **`docs/README.md`** — comparison table updated with all a5 additions.

---

## v1.1.1a4 — Alpha 4 (2026)

### Standard Library — Expanded

**`stdlib/array.nq`** — 9 new functions:
- `sum(arr)` — sum all numbers
- `min_arr(arr)` · `max_arr(arr)` — smallest / largest element
- `any(arr, fn)` · `all(arr, fn)` — predicate tests across all elements
- `flatten(arr)` — one-level deep flatten of nested arrays
- `unique(arr)` — remove duplicates, preserve insertion order
- `zip(a, b)` — pair two arrays element-wise into `[[a0,b0], ...]`
- `chunk(arr, size)` — split into fixed-size sub-arrays

**`stdlib/string.nq`** — 3 new functions:
- `char_at(s, i)` — character at index with bounds-checking error
- `contains_str(s, sub)` — readable alias for `index(s,sub) != -1`
- `title_case(s)` — capitalise the first letter of each word

**`stdlib/math.nq`** — 5 new functions:
- `is_nan(n)` — IEEE 754 NaN check via self-inequality
- `is_inf(n)` — infinity check
- `hypot(a, b)` — Euclidean distance `sqrt(a²+b²)`
- `gcd(a, b)` — greatest common divisor (Euclidean algorithm)
- `lcm(a, b)` — least common multiple

### README — Alpha Warning

Added a prominent warning banner to `README.md` so readers immediately know
they are looking at an alpha version, with a link to the stable v1.0.0 release:

```
⚠️ This README describes the 1.1.x development series.
   For production use, stick with v1.0.0 stable.
```

### Documentation

- `docs/dev/stdlib.md` — all new functions documented with examples and version tags
- `docs/dev/language.md` — built-in reference tables updated with new stdlib entries
- `ROADMAP.md` — a4 added to history, criteria updated

### Documentation Structure Reorganised

The `docs/` folder is now split into two tracks:

- **`docs/stable/`** — snapshot of v1.0.0 documentation.
  Accurate for the stable release. Includes a Known Limitations table
  that links each limitation to the version that fixed it.

- **`docs/dev/`** — current development docs (v1.1.x).
  Updated with every alpha release. Each file carries a banner
  noting it describes an alpha version.

- **`docs/README.md`** — index with a version comparison table and
  guidance on which track to read.

---

## v1.1.1a3 — Alpha 3 (2026)

### Bug Fixes

- **Undefined variable access is now catchable** — `OP_GET_GLOBAL` on an
  undefined name previously called `vmRuntimeError` which bypassed
  `try/catch`. Now uses `THROW_ERROR` and can be caught.
  Affects: `let x = undefined_name`, compound assign on undeclared variable.

- **Undeclared variable assignment is now catchable** — `OP_SET_GLOBAL` on a
  name that was never declared with `let` now throws a catchable error instead
  of aborting.

### Documentation — Accuracy Pass

All documents updated to reflect the current state of the language:

- **`README.md`** — quick start uses `for-in` example; features list includes
  `for item in array` and compound assignment; stdlib count updated to 10.

- **`docs/dev/grammar.md`** — added `for_stmt` and `compound_assign_stmt` to the
  statement grammar; `for` and `in` added to keyword list; compound assignment
  operators documented; import deduplication note corrected (was "re-executes",
  now "no-op after first load").

- **`docs/dev/language.md`** — removed stale limitation "import re-executes" (fixed
  in v1.1.1a2); added "accessing undefined variable" and "assigning to undeclared
  variable" to the "What throws" table; `replace()` limitation clarified to
  point to `replace_all()` in `stdlib/string`.

- **`docs/dev/vm_design.md`** — `OP_CONST` updated to show 16-bit index `[hi, lo]`;
  constant pool limit updated 256 → 65535; new section documenting the `for-in`
  desugaring and `break`/`continue` invariants.

- **`ROADMAP.md`** — all released versions listed with accurate status;
  criteria for promoting a3 → stable v1.1.1 documented.

---

## v1.1.1a2 — Alpha 2 (2026)

> **Alpha release** — all features are functional and tested.
> Syntax is stable. Not yet recommended for production.

### Language — New Features

- **`for item in array ... end`** — range-based loop over arrays.
  `break` and `continue` work correctly inside `for`, including after
  nested `if` blocks and across sequential `for` loops.
- **Compound assignment: `+=` `-=` `*=` `/=` `..=`** — modify a variable
  in place. Works on both local and global variables.
- **`ord(ch)`** — return the ASCII code point (0–127) of the first character.
- **`chr(n)`** — return a one-character string for code point `n` (0–127).

### Bug Fixes

- **Non-function call is now catchable** — calling a string/number/nil as
  a function previously aborted with an uncatchable runtime error. Now
  it goes through `THROW_ERROR` and can be caught with `try/catch`.
- **Builtin arity error is now catchable** — `sqrt()` with wrong number of
  arguments previously aborted. Now catchable.
- **Import deduplication** — `import "stdlib/math"` twice no longer
  re-executes the module. Tracked per `compile()` call; REPL-safe.
- **Constant pool limit raised from 256 → 65535** — the pool index is now
  16-bit. Files with >256 global variables or string constants no longer
  crash with "Too many constants". Disassembler and VM reader updated.
- **`str()` precision** — `str(3.14159265358979)` now returns
  `"3.14159265358979"` (14 significant digits) instead of `"3.14159"`.
  Uses `%.14g` format; integers are still formatted without decimal point.
- **`for-in` break/continue stack corruption** — `break` inside a `for`
  loop now correctly cleans up the hidden `__for_arr__` and `__for_idx__`
  locals before jumping, preventing stack corruption in subsequent loops.
  `continue` inside `for` correctly increments the index before looping.
- **`loop_stack` reset per `compile()` call** — a leftover loop context
  from one REPL input could bleed into the next. Fixed.

### Standard Library — New Modules

- **`stdlib/os.nq`** — file-system utilities: `env_or`, `home_dir`,
  `path_exists`, `read_lines`, `write_lines`, `append_line`,
  `file_size`, `touch`, `is_empty_file`.
- **`stdlib/fmt.nq`** — printf-style formatting: `fmt(template, args)`,
  `printf`, `fmt_num`, `fmt_pad`, `fmt_table`.

### Internal

- `LoopCtx` in `compiler.cpp` extended with `continue_target`,
  `outer_local_count`, `scope_depth`, `continue_patches` fields.
- `include/nolqu.h` version macros guarded with `#ifndef` to prevent
  redefinition warnings when included alongside `src/common.h`.

---

## v1.1.0 — Extended Standard Library (2026)

### Language
- **`break`** — exit the nearest enclosing loop immediately
- **`continue`** — skip to the next loop iteration (re-evaluates condition)
- Both are compile-time errors when used outside a loop
- Nested loops: `break`/`continue` only affect the innermost loop

### Standard Library — New Modules
- **`stdlib/time.nq`** — `now`, `millis`, `elapsed`, `sleep`, `format_duration`, `benchmark`
- **`stdlib/string.nq`** — `is_empty`, `lines`, `words`, `count`, `replace_all`,
  `lstrip`, `rstrip`, `pad_left`, `pad_right`, `center`, `truncate`
- **`stdlib/path.nq`** — `path_join`, `basename`, `dirname`, `ext`, `strip_ext`,
  `is_absolute`, `normalize`
- **`stdlib/json.nq`** — `json_object`, `json_set`, `json_get`, `json_has`,
  `json_keys`, `json_is_object`, `json_stringify`, `json_parse`, `json_parse_error`
- **`stdlib/test.nq`** — `suite`, `expect`, `expect_eq`, `expect_err`, `done`

### Standard Library — Extended
- **`stdlib/math.nq`** — added `PI`, `TAU`, `E`, `sin`, `cos`, `tan`,
  `degrees`, `radians`, `log`, `log2`, `log10`

### Bug Fixes
- `stdlib/string.nq` — `replace_all` double-appended the tail after the last match
- `stdlib/json.nq` — scientific notation (`1e15`) not supported by Nolqu lexer,
  replaced with plain literals
- `stdlib/json.nq` — multi-line `or` chain in `_jp_is_numchar` caused parse error,
  split into separate `if` blocks
- `stdlib/test.nq` — `\xNN` Unicode escapes not supported by Nolqu string literals,
  replaced with ASCII `[PASS]` / `[FAIL]`

### Project Health
- **`CONTRIBUTING.md`** — code style, commit format, PR process, development workflow
- **`SECURITY.md`** — vulnerability reporting (nolqucontact@gmail.com), scope, disclosure policy
- **`CODE_OF_CONDUCT.md`** — community standards (nolquteam@gmail.com)
- **`.gitignore`** — C/C++, editor, OS artefacts
- **`.github/ISSUE_TEMPLATE/bug_report.md`**
- **`.github/ISSUE_TEMPLATE/feature_request.md`**
- **`.github/PULL_REQUEST_TEMPLATE.md`**

### Documentation
- `docs/dev/language.md` — added `break`/`continue` section, removed from Known Limitations
- `docs/dev/grammar.md` — `break_stmt`, `continue_stmt` added to grammar, keywords updated
- `docs/dev/stdlib.md` — all 5 new modules documented with examples

---

## v1.0.0-rc1 — Release Candidate (2026)

### Architecture: C / C++ Split
- Runtime core (`memory`, `value`, `chunk`, `object`, `table`, `gc`, `vm`) → **C11**
- Tooling layer (`lexer`, `parser`, `ast`, `compiler`, `codegen`, `repl`, `main`,
  `nq_embed`) → **C++17**
- All public headers updated with `extern "C"` guards
- `value.h` box macros (`NIL_VAL`, `BOOL_VAL`, etc.) use inline functions under C++;
  C path keeps compound literals

### Bug Fixes (rc1)
- `gc.c` — `removeWhiteStrings()` used `calloc`/`free` bypassing tracker → fixed
- `object.c` — `allocString()` called `free(chars)` instead of `nq_realloc` → fixed
- `memory.c` — counter add-before-subtract ordering → fixed

### Build System
- New Makefile: mixed C/C++ build, platform notes for Linux/macOS/Windows/Termux
- Debug build: AddressSanitizer + UndefinedBehaviourSanitizer

---

## v0.9.0-beta — Stabilization

- New builtins: `assert`, `clock`, `mem_usage`, `is_nil/num/str/bool/array`
- "Did you mean?" on undefined variable (Levenshtein distance)
- Compiler warning for unused locals (`_` prefix suppresses)
- Improved REPL: `help`, `clear`, depth-proportional prompt, heap buffer
- Improved error format: `file:line`, coloured stack trace
- All comparison operators now throw catchable errors
- Zero compiler warnings

## v0.8.0-beta — Garbage Collector

- Mark-and-sweep GC in `gc.c` / `gc.h`
- Auto-trigger when `nq_bytes_allocated > nq_gc_threshold` (initial 1 MB)
- After collection: threshold = max(2× surviving bytes, 1 MB)
- `gc_collect()` builtin — manual trigger, returns bytes freed

## v0.7.0 — File I/O

- `file_read` `file_write` `file_append` `file_exists` `file_lines`
- `stdlib/file.nq`: `read_or_default`, `write_lines`, `count_lines`

## v0.6.0 — Error Handling

- `try` / `catch` / `end` (nestable, up to 64 levels)
- `error(msg)` builtin
- Catchable: division/modulo by zero, type mismatch, index out of bounds
- Opcodes: `OP_TRY`, `OP_TRY_END`, `OP_THROW`

## v0.5.0 — Extended Stdlib

- String: `index`, `repeat`, `join`
- Random: `random()`, `rand_int(lo, hi)`
- `nq check` command — syntax check without execution
- `nq test` command — run `*_test.nq` files

## v0.4.0 — Scope Improvements

- Block-scoped locals in `if`, `loop`, `function`
- Unused-variable warning framework

## v0.3.0 — Arrays and Modules

- Array literals `[a, b, c]`, indexing, `push`, `pop`, `remove`, `contains`, `sort`
- `import "path"` module system
- `stdlib/math.nq`: `clamp`, `lerp`, `sign`
- `stdlib/array.nq`: `map`, `filter`, `reduce`, `reverse`

## v0.2.0 — User Input and Type Builtins

- `input(prompt?)` — read from stdin
- `str()`, `num()`, `type()` — type conversion
- `upper`, `lower`, `trim`, `slice`, `replace`, `split`, `startswith`, `endswith`

## v0.1.0 — Foundation

- Lexer → Parser → AST → Compiler → Stack VM pipeline
- Variables (`let`), arithmetic, string concatenation (`..`)
- `if` / `else` / `end` conditionals
- `loop` / `end` loops
- `function` / `return` / `end`
- `print` keyword
- `sqrt`, `floor`, `ceil`, `round`, `abs`, `pow`, `min`, `max`
