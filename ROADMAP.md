# Nolqu Roadmap

> This roadmap reflects the current state of Nolqu and planned direction toward a stable release.
> Features may shift between versions as development progresses.

---

## ✅ v0.1.0 — Foundation
Variables, conditionals, loops, functions, recursion, REPL, CLI, bytecode VM.

## ✅ v0.2.0 — User Input & Builtins
`input()`, `str()`, `num()`, `type()`. Full English translation.

## ✅ v0.3.0 — Arrays, Stdlib, Modules
Array / list with `[]` syntax, index access, index assignment.  
Math stdlib: `sqrt`, `pow`, `abs`, `floor`, `ceil`, `round`, `max`, `min`.  
String stdlib: `upper`, `lower`, `slice`, `trim`, `replace`, `split`, `startswith`, `endswith`.  
Module system: `import "file"` — active and working.  
Bugfix: stack corruption in `if` blocks without `else` inside functions.

---

## ✅ v0.4.0 — Scope Improvements
`if` and `loop` blocks now create their own scope. Variables declared inside a block are destroyed when the block ends. Shadowing works correctly — inner `let` doesn't affect the outer variable.  
Bugfix: slot-0 reservation missing for top-level script, causing block-local variables to resolve incorrectly.

---

## v0.5 — Extended Stdlib
**Focus:** Fill in common missing utilities.

- `math.random()` — random number between 0 and 1
- `string.index(s, sub)` — find position of substring
- `string.repeat(s, n)` — repeat string n times
- `array.sort(arr)` — sort array in-place
- `array.join(arr, sep)` — join array into string

---

## v0.6 — Error Handling
**Focus:** Let programs handle errors gracefully.

- `try` / `catch` / `end` blocks
- Catchable runtime errors (division by zero, index out of bounds, etc.)
- `error(message)` — throw a custom error from user code

---

## v0.7 — File I/O
**Focus:** Read and write files.

- `file.read(path)` — read file as string
- `file.write(path, content)` — write string to file
- `file.exists(path)` — check if file exists
- `file.lines(path)` — read file as array of lines

---

## v0.8 — Garbage Collector
**Focus:** Automatic memory management.

- Mark-and-sweep garbage collector
- Memory freed automatically during runtime
- No more memory leaks on long-running programs

---

## v0.9 — Stabilization
**Focus:** Make the runtime solid before 1.0.

- VM performance optimizations
- Improved error messages across all edge cases
- Compiler refactor for cleaner codegen
- Final stdlib review and consistency pass

---

## v1.0.0-rc1 — Release Candidate
**Focus:** Final testing and polish.

- Full test suite across all features
- Documentation review
- No new features — bugfixes only

---

## v1.0.0 — Stable Release 🎉
- Stable syntax (no breaking changes after this)
- Stable runtime
- Core stdlib complete
- Module system stable
- Garbage collector stable
- Ready for real programs

---

## Summary

| Version | Focus |
|---------|-------|
| v0.1 | Foundation ✅ |
| v0.2 | User input & builtins ✅ |
| v0.3 | Arrays, stdlib, modules ✅ |
| v0.4 | Scope improvements ✅ |
| v0.5 | Extended stdlib |
| v0.6 | Error handling |
| v0.7 | File I/O |
| v0.8 | Garbage collector |
| v0.9 | Stabilization |
| v1.0.0-rc1 | Release candidate |
| v1.0.0 | Stable release 🎉 |
