# Nolqu Changelog

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
