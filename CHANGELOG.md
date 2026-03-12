# Changelog

> ⚠️ **Warning:** Nolqu is still in **Alpha** stage. Syntax, behavior, and features may change at any time without notice. Not recommended for production use.

---

## [0.4.0] — 2026-03-13 (Alpha)
[Compare v0.3.0...v0.4.0](https://github.com/Nadzil123/Nolqu/compare/v0.3.0-alpha...v0.4.0-alpha)

### Added

**Block Scoping**
- `if` and `loop` blocks now create their own scope
- Variables declared with `let` inside a block are destroyed when the block ends
- Inner `let` with the same name as an outer variable shadows it for the duration of the block
- Outer variable is unchanged after the block exits

**Scope examples:**
```nolqu
let x = 1
if true
  let x = 99   # shadows outer x
  print x       # → 99
end
print x         # → 1  (outer x unchanged)
```

### Changed

- `let` inside `if`/`loop` is now always block-local — it no longer leaks to the surrounding scope

### Bug Fixes

- Fixed slot-0 reservation missing for top-level script context. Variables declared inside `if`/`loop` at the top level were resolving to the wrong stack slot, causing them to print `<script>` instead of their value.

### Breaking Changes

- Variables declared with `let` inside `if` or `loop` blocks are **no longer accessible** after the block ends. Code that relied on this leak will need to declare the variable before the block.

---

## [0.3.0] — 2026-03-13 (Alpha)
[Compare v0.2.0...v0.3.0](https://github.com/Nadzil123/Nolqu/compare/v0.2.0-alpha...v0.3.0-alpha)

### Added

**Array / List**
- Array literal syntax: `let arr = [1, 2, 3]`
- Index access (0-based): `arr[0]`
- Negative indexing: `arr[-1]` returns the last element
- Index assignment: `arr[0] = "new"`
- String character indexing: `"hello"[0]` returns `"h"`
- Built-ins: `len`, `push`, `pop`, `remove`, `contains`

**Standard Library — Math**
- `sqrt(n)` — square root
- `floor(n)` — round down
- `ceil(n)` — round up
- `round(n)` — round to nearest
- `abs(n)` — absolute value
- `pow(base, exp)` — exponentiation
- `max(a, b)` — larger of two values
- `min(a, b)` — smaller of two values

**Standard Library — String**
- `upper(s)` — convert to uppercase
- `lower(s)` — convert to lowercase
- `slice(s, start, [end])` — substring with optional end index
- `trim(s)` — remove leading and trailing whitespace
- `replace(s, old, new)` — replace first occurrence
- `split(s, sep)` — split into array by separator
- `startswith(s, prefix)` — boolean prefix check
- `endswith(s, suffix)` — boolean suffix check

**Module System**
- `import "path"` — load and execute a `.nq` file, making its globals available
- Paths are resolved relative to the importing file, then relative to CWD
- `.nq` extension is added automatically if omitted

**Reusable Stdlib Modules**
- `stdlib/math.nq` — `clamp`, `lerp`, `sign`
- `stdlib/array.nq` — `map`, `filter`, `reduce`, `reverse`

**New Examples**
- `examples/arrays.nq` — array operations demo
- `examples/stdlib.nq` — standard library demo
- `examples/import_demo.nq` — module import demo

**New Opcodes**
- `OP_BUILD_ARRAY` — build array from N stack values
- `OP_GET_INDEX` — get value at index
- `OP_SET_INDEX` — set value at index

### Changed

- `type()` now returns `"array"` for array values
- Version bumped to `0.3.0`

### Bug Fixes

- Fixed stack corruption in `if` blocks without `else` inside functions. The false-branch `OP_POP` was being executed even on the true path, silently corrupting local variables. This caused incorrect behavior in functions using `if` inside `loop` (e.g. `filter`, `map`).

### Breaking Changes

- (None)

---

## [0.2.0] — 2026-03-12 (Alpha)
[Compare v0.1.0...v0.2.0](https://github.com/Nadzil123/Nolqu/compare/v0.1.0-alpha...v0.2.0-alpha)

### Added

**Built-in Functions**
- `input([prompt])` — read a line from stdin. Accepts an optional prompt string.
- `str(value)` — convert any value to a string.
- `num(value)` — convert a string to a number. Returns `nil` if conversion fails.
- `type(value)` — return the type name of a value (`"number"`, `"string"`, `"bool"`, `"nil"`, `"function"`).

**Native Function System**
- Added `ObjNative` object type for C-level built-in functions.
- Native functions are registered in the VM globals table at startup.
- `OP_CALL` now handles both `ObjFunction` (user-defined) and `ObjNative` (built-in).

**New Example**
- `examples/input.nq` — demonstrates `input()`, `num()`, `str()`, and `type()`.

### Changed

- All source code, error messages, and comments translated from Indonesian to English.
- REPL quit command changed from `keluar` to `exit`.
- REPL banner updated to English.
- All example programs translated to English.
- Version bumped to `0.2.0`.

### Bug Fixes

- Fixed help text showing `nq repl repl` instead of `nq repl`.

### Breaking Changes

- REPL: `keluar` no longer works as a quit command. Use `exit` or `quit`.

---

## [0.1.0] — 2026-03-12 (Alpha)

Initial release of Nolqu. The language foundation runs end-to-end from source code to execution.

---

### Breaking Changes

- (None — initial release)

---

### Architecture

- Compilation pipeline: Source → Lexer → Parser → AST → Bytecode → VM
- Stack-based Virtual Machine with instruction dispatch loop
- Fixed bytecode format with constant pool
- Call frames for function stack management
- String interning for memory efficiency and O(1) comparison
- Open-addressing hash table for global variables

---

### Build & Platform

- Build system: Makefile
- Compiler: GCC / Clang (C99)
- Runtime binary: `nq`
- Tested on:
  - Linux (x86_64)
  - Termux Android (ARM64)

```bash
# Install on Termux
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu
make
cp nq $PREFIX/bin/
```

---

### Compatibility

- Target platform: POSIX-like systems
- Tested architectures: x86_64, ARM64 (Termux)

---

### Dependencies

- Standard C library (`libc`)
- `libm` — math functions
- GCC / Clang (C99 compatible)

---

### Added

**Data Types**
- `number` — 64-bit floating point
- `string` — immutable Unicode string with string interning
- `boolean` — `true` and `false`
- `nil` — empty value

**Syntax & Keywords**
- `let` — variable declaration
- `print` — print value to output
- `if` / `else` / `end` — conditional branching
- `loop` — condition-based loop (while-style)
- `function` / `return` / `end` — function declaration and calls
- `and`, `or`, `not` — logical operators with short-circuit evaluation
- `..` — string concatenation
- `import` — keyword available (not yet active)

**Operators**
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Negation: `-` (unary), `not`

**Runtime**
- Lexer with line-number error messages
- Recursive descent parser
- Bytecode compiler emitting 31 opcodes
- Stack-based Virtual Machine with call frames
- Interactive REPL (`nq repl`) with multi-line support
- CLI: `nq <file>`, `nq run <file>`, `nq repl`, `nq version`, `nq help`

**Error Messages**
- Error messages with line numbers
- Fix hints for common errors (undefined variable, division by zero, wrong argument count, etc.)

---

### Example Programs

The `examples/` folder contains:

- `hello.nq` — variables, basic operations, conditionals
- `fibonacci.nq` — recursion
- `counter.nq` — loops and FizzBuzz
- `functions.nq` — functions, return values, factorial

---

### Testing

- Programs in `examples/` are used as integration tests
- Run all tests: `make test`

---

### Performance

- Execution via bytecode interpreter
- Stack-based execution model
- Runtime binary size: ~61KB
- No compiler optimizations yet (planned for v0.3+)

---

### Language Limitations

- All numbers use 64-bit floating point
- No separate integer type
- No static type system
- `if` and `loop` blocks do not create a new scope
- Maximum 256 local variables per function
- Maximum 64 call frames (recursion depth)

---

### Not Yet Available

- `input()` — read user input *(added in v0.2.0)*
- Array / list
- Standard library (math, string)
- Module system (`import` not yet active)
- File I/O
- In-program error handling

---

### Known Issues

- Deep recursion can cause stack overflow
- No garbage collector (memory is not freed during runtime)
- `if` and `loop` blocks do not create a new scope

---

### Security

- No sandbox
- Runtime has no memory usage limits
- Programs can consume CPU without limit (infinite loops cannot be interrupted externally)

---

### Roadmap

| Version | Target |
|---------|--------|
| v0.2    | `input()`, `str()`, `num()`, `type()` builtins ✅ |
| v0.3    | Array / list |
| v0.4    | Math standard library (`sqrt`, `floor`, `abs`, `pow`) |
| v0.5    | String standard library (`length`, `slice`, `contains`) |
| v0.6    | Module system (`import` active) |
| v0.7    | Local scope for `if` and `loop` blocks |
| v0.8    | Basic garbage collector |
| v1.0    | Stable release |

---

### Contributors

- **Nadzil** — Creator of Nolqu

---

### License

Released under the MIT License.
See [LICENSE](./LICENSE) for details.
