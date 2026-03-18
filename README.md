# Nolqu

```
  _   _       _
 | \ | | ___ | | __ _ _   _
 |  \| |/ _ \| |/ _` | | | |
 | |\  | (_) | | (_| | |_| |
 |_| \_|\___/|_|\__, |\__,_|
                |___/
```

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
- Module system — `import "stdlib/math"` or your own `.nq` files
- File I/O — read, write, append, split by lines
- 40+ built-in functions — math, string, array, random, time, memory
- **C/C++ embed API** — drop Nolqu into any C or C++ project via `nolqu.h`
- Runtime core in **C11**, tooling in **C++17**

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

let i = 1
loop i <= 3
  print greet("World " .. i)
  i = i + 1
end
```

```
Hello, World 1!
Hello, World 2!
Hello, World 3!
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
| [docs/stdlib.md](docs/stdlib.md) | Standard library modules reference |
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
