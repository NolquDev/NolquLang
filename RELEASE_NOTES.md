# Nolqu v1.0.0 — Release Notes

> **First stable release.**
> Language syntax and bytecode format are now frozen.
> Programs written for v1.0.0 will continue to run on all future 1.x versions.

---

## What is Nolqu?

Nolqu is a small, fast scripting language with its own stack-based bytecode VM.
It is designed to read like plain sentences, with minimal punctuation and no braces.

```nolqu
function greet(name)
  return "Hello, " .. name .. "!"
end

print greet("World")
```

---

## Highlights of This Release

### Runtime Bug Fixes

Seven native string functions were using `malloc()` for buffers passed to
`takeString()`, while the GC freed them via `nq_realloc()` — causing
`nq_bytes_allocated` to silently underflow. The GC would never trigger
correctly on programs that created many strings. **All native buffers now
go through `nq_realloc`.** This is the most impactful fix in v1.0.0.

### C / C++ Hybrid Architecture (from rc1)

The runtime core (VM, GC, memory, value system) stays as **C11** for
performance and binary size. The tooling layer (lexer, parser, compiler,
REPL, CLI) is now **C++17**, enabling cleaner abstractions without
touching the hot path.

The `nq_register()` embed API was refactored from a 16-slot macro dispatch
table to a zero-overhead C++ template trampoline system — limit raised to 64.

### Completed Documentation

- `README.md` — full language tour, all 40+ builtins, embed API, project structure
- `docs/grammar.md` — complete EBNF, type system, scoping rules, limitations
- `docs/vm_design.md` — full instruction set, GC algorithm, error propagation

---

## Full Changelog

See [CHANGELOG.md](CHANGELOG.md) for detailed version history.

---

## Installing

```bash
# Linux / macOS
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu && make && sudo make install

# Termux (Android)
pkg install git clang make
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu && make CC=clang CXX=clang++
cp nq $PREFIX/bin/

# Windows (MinGW-w64 / MSYS2)
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu && mingw32-make
```

---

## Quick Start

```bash
echo 'print "Hello from Nolqu!"' > hello.nq
nq hello.nq
```

```bash
nq repl         # interactive mode
nq check file.nq  # syntax check without running
nq help         # full built-in reference
```

---

## Test Results

```
── Core examples ──────────────────────
  ✓ hello        ✓ fibonacci     ✓ functions
  ✓ counter      ✓ input         ✓ arrays
  ✓ stdlib       ✓ import        ✓ file I/O

── Unit tests ─────────────────────────
  ✓ math_test    ✓ string_test
  ✓ array_test   ✓ error_test

── CLI ────────────────────────────────
  ✓ nq check     ✓ nq version    ✓ nq help

── Memory ─────────────────────────────
  ✓ gc_collect

  Results: 17/17 passed
  ASan + UBSan: clean
```

---

## Git Tag

```bash
git tag -a v1.0.0 \
  -m "Nolqu v1.0.0 — First stable release" \
  -m "" \
  -m "Bug fixes:" \
  -m "  - vm.c: 7 native fns used malloc for takeString buffers (GC underflow)" \
  -m "  - vm.c: nativeFileLines free(buf) → nq_realloc" \
  -m "" \
  -m "Improvements:" \
  -m "  - nq_embed: macro dispatch table → C++ template trampolines (limit 16→64)" \
  -m "  - docs: README, grammar.md, vm_design.md completed" \
  -m "  - tests: all 4 test files rewritten with full coverage" \
  -m "" \
  -m "No syntax changes. No bytecode changes. No API changes."
```

---

## License

MIT — free to use, modify, and distribute.
