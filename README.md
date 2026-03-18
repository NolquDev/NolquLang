# Nolqu

<p align="center">
  <img src="assets/Banner.png" width="100%">
</p>

![version](https://img.shields.io/github/v/tag/Nadzil123/Nolqu?include_prereleases&label=version)
![status](https://img.shields.io/badge/status-active-success)
![license](https://img.shields.io/github/license/Nadzil123/Nolqu)

> [!WARNING]
> **This is an alpha release (v1.1.1a4).** The API and syntax are stable
> but this version has not been fully validated across all platforms.
> For production use, stick with the **[v1.0.0 stable release](https://github.com/Nadzil123/Nolqu/releases/tag/v1.0.0)**.
>
> ⚠️ This README describes the **1.1.x development series**.
> Development is ongoing — see [CHANGELOG.md](CHANGELOG.md) for what changed.

**A small, fast scripting language with its own bytecode VM.**

Nolqu is designed so code reads like plain sentences — no braces, no semicolons.
Great for learning, scripting, and embedding into C or C++ projects.

---

## Features

- Clean, readable syntax — `if`, `loop`, `function`, `end`
- Stack-based bytecode VM — fast and portable
- Mark-and-sweep garbage collector — automatic, tunable
- Structured error handling — `try` / `catch` / `end`
- Dynamic arrays with negative indexing
- `for item in array` — range-based loop
- `break` and `continue` in loops
- Compound assignment: `+=` `-=` `*=` `/=` `..=`
- Module system — `import "stdlib/math"` or your own `.nq` files
- File I/O — read, write, append, split by lines
- 40+ built-in functions — math, string, array, random, time, memory
- **C/C++ embed API** — drop Nolqu into any C or C++ project via `nolqu.h`
- Runtime core in **C11**, tooling in **C++17**
- Current version: **v1.1.1a4** (alpha) — stable: **v1.0.0**

---

## Install

**Linux / macOS**
```bash
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu
make
sudo make install       # → /usr/local/bin/nq
```

**Termux (Android)**
```bash
pkg install git clang make
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu && make CC=clang CXX=clang++
cp nq $PREFIX/bin/
```

**Windows (MinGW-w64 / MSYS2)**
```bash
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu && mingw32-make
```

---

## Quick Start

```bash
echo 'print "Hello, World!"' > hello.nq
nq hello.nq
```

```
Hello, World!
```

A slightly longer taste:

```nolqu
function greet(name)
  return "Hello, " .. name .. "!"
end

let names = ["Alice", "Bob", "Carol"]
for name in names
  print greet(name)
end
```

```
Hello, Alice!
Hello, Bob!
Hello, Carol!
```

---

## CLI

```bash
nq program.nq          # run a file
nq run program.nq      # same, explicit
nq check program.nq    # parse + compile only (no run)
nq repl                # interactive REPL
nq version             # show version
nq help                # show built-in list
```

---

## REPL

```
$ nq repl

  +--------------------------------------+
  |  Nolqu 1.0.0    Interactive REPL    |
  |  'help' for help  'exit' to quit    |
  +--------------------------------------+

nq > let x = 10
nq > print x * x
100
nq > exit
```

REPL commands: `help` · `clear` · `exit` · `quit`

---

## Documentation

| Document | Contents |
|---|---|
| [docs/language.md](docs/language.md) | Full language reference — syntax, types, all built-ins, modules |
| [docs/grammar.md](docs/grammar.md) | Formal EBNF grammar specification |
| [docs/stdlib.md](docs/stdlib.md) | Standard library modules reference (10 modules, 80+ functions) |
| [docs/embedding.md](docs/embedding.md) | Embed Nolqu in C / C++ via `nolqu.h` |
| [docs/vm_design.md](docs/vm_design.md) | VM internals, bytecode reference, GC algorithm |
| [CHANGELOG.md](CHANGELOG.md) | Version history |

---

## Build

```bash
make            # release build  → ./nq
make debug      # debug + ASan   → ./nq-debug
make test       # run test suite
make clean      # remove artefacts
```

Runtime core (`memory`, `value`, `chunk`, `object`, `table`, `gc`, `vm`) compiled
as C11. Tooling layer (`lexer`, `parser`, `compiler`, `repl`, `main`) compiled as
C++17.

---

## License

MIT — free to use, modify, and distribute. See [LICENSE](LICENSE).
