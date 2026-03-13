# Nolqu Changelog

## v1.0.0-rc1 — Release Candidate (2026)

### Architecture: C / C++ Split
- **Runtime core** (VM, GC, memory, value system, objects, table, chunk)
  remains pure **C11** — for performance and ABI stability.
- **Tooling layer** (lexer, parser, AST, compiler, code-generator, REPL,
  CLI, embed API) migrated to **C++17** — for maintainability.
- All public headers updated with `extern "C"` guards for correct
  C/C++ interoperability.
- `value.h` — `NIL_VAL`, `BOOL_VAL`, `NUMBER_VAL`, `OBJ_VAL` box macros
  now use **inline functions** under C++ (C99 compound literals are not
  valid C++); C path unchanged.

### Bug Fixes
- **`gc.c` — memory tracker bypass** (critical):  
  `removeWhiteStrings()` previously used `calloc` / `free` directly,
  bypassing `nq_realloc`. This caused `nq_bytes_allocated` to drift,
  making the GC trigger unpredictably.  
  Fixed: all allocations now go through `NQ_ALLOC` / `FREE_ARRAY`.

- **`object.c` — memory tracker bypass** (critical):  
  `allocString()` called `free(chars)` when an identical interned string
  already existed. Because the buffer was allocated via `nq_realloc`,
  the free must also go through `nq_realloc`.  
  Fixed: `free(chars)` → `nq_realloc(chars, length + 1, 0)`.

- **`memory.c` — counter ordering**:  
  `nq_bytes_allocated += new_size` was executed before the subtraction of
  `old_size`, which could temporarily inflate the counter and trigger a
  spurious GC.  
  Fixed: subtract first, then add.

### Build System
- New **Makefile** supports mixed C/C++ compilation:
  - `.c` files → `$(CC)` with `-std=c11 -Wall -Wextra -Wpedantic`
  - `.cpp` files → `$(CXX)` with `-std=c++17 -Wall -Wextra`
  - Linker step uses `$(CXX)` to satisfy C++ runtime requirements
- Platform notes added for **Linux**, **macOS**, **Windows (MinGW)**,
  and **Termux (Android)**.
- Debug build: AddressSanitizer + UndefinedBehaviourSanitizer.

### Code Quality
- All headers fully documented with section banners and field comments.
- `extern "C"` guards on every public header.
- `NQ_UNUSED(x)` macro applied consistently.
- Removed all implicit `void*` → `type*` casts that are valid C but
  invalid C++ (now explicit in the few places needed).

### Compatibility
- **No syntax changes.**  All existing `.nq` programs run unchanged.
- **No bytecode changes.**  Instruction set identical to v0.9.0-beta.
- **No API changes.**  `nolqu.h` embed API unchanged.

---

## v0.9.0-beta — Stabilization

- New builtins: `assert`, `clock`, `mem_usage`, `is_nil/num/str/bool/array`
- "Did you mean?" suggestions on undefined variable errors (Levenshtein)
- Compiler warning for unused local variables (`_` prefix suppresses)
- Improved REPL: `help`, `clear`, depth-proportional prompt
- Improved error format: `file:line`, coloured call stack
- Fixed: GC string table cleanup (`removeWhiteStrings` wired correctly)
- Zero compiler warnings

## v0.8.0-beta — Garbage Collector

- Mark-and-sweep GC in `gc.c` / `gc.h`
- Auto-trigger when `nq_bytes_allocated > nq_gc_threshold` (initial: 1 MB)
- `gc_collect()` builtin — manual trigger, returns bytes freed
- `mem_usage()` builtin

## v0.7.0 — File I/O

- `file_read`, `file_write`, `file_append`, `file_exists`, `file_lines`
- `stdlib/file.nq`: `read_or_default`, `write_lines`, `count_lines`

## v0.6.0 — Error Handling

- `try` / `catch` / `end` blocks (nestable, up to 64 levels)
- `error(msg)` builtin
- Catchable runtime errors: division by zero, type mismatch, index errors
- Opcodes: `OP_TRY`, `OP_TRY_END`, `OP_THROW`

## v0.1 – v0.5 — Foundation

- v0.1: core VM, variables, arithmetic, control flow
- v0.2: user input, type builtins
- v0.3: arrays, stdlib modules, `import`
- v0.4: scope improvements, closures
- v0.5: extended stdlib (string, math, array, random)
