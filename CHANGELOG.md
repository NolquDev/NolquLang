# Nolqu Changelog

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
  - `docs/grammar.md` — complete EBNF, type system, scoping rules, limitations
  - `docs/vm_design.md` — full instruction set reference, GC phases, error flow

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
