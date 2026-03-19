# Nolqu™

<img src="assets/Banner.png" width="100%">
</p>

![version](https://img.shields.io/github/v/tag/Nadzil123/Nolqu?include_prereleases&label=version)
![status](https://img.shields.io/badge/status-active-success)
![license](https://img.shields.io/github/license/Nadzil123/Nolqu)

> [!IMPORTANT]
> **Upgrading from v1.0.0?** v1.2.0 has one breaking change:
> `0` and `""` (empty string) are now **falsy**. Review any conditions that test a number or string directly.
> See [RELEASE_NOTES.md](RELEASE_NOTES.md) for the full upgrade guide.

**A small, fast scripting language with its own stack-based bytecode VM.**

Nolqu is designed so code reads like plain sentences — no braces, no semicolons.
Great for learning, scripting, and embedding into C or C++ projects.

---

## Features

- Clean, readable syntax — `if`, `loop`, `for`, `function`, `end`
- Stack-based bytecode VM — fast and portable
- Mark-and-sweep garbage collector — automatic, tunable
- Structured error handling — `try` / `catch` / `end`
- Dynamic arrays with negative indexing
- `for item in array` — range-based loop
- `break` and `continue` in loops
- Compound assignment: `+=` `-=` `*=` `/=` `..=`
- `const` declarations — compile-time immutable bindings
- Default function parameters — `function greet(name = "world")`
- Slice expressions — `arr[1:3]` and `s[1:3]` for arrays and strings
- `null` keyword — alias for `nil`, with extended falsy (`0`, `""`)
- Module system — `import "stdlib/math"` or your own `.nq` files
- File I/O — read, write, append, split by lines
- 50+ built-in functions — math, string, array, random, time, memory
- **C/C++ embed API** — drop Nolqu into any C or C++ project via `nolqu.h`
- Runtime core in **C11**, tooling in **C++17**
- Current version: **v1.2.0** (stable)

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

A fuller taste of the language:

```nolqu
# Functions with default parameters
function greet(name = "World")
  return "Hello, " .. name .. "!"
end

# Range-based loop
let names = ["Alice", "Bob", "Carol"]
for name in names
  print greet(name)
end

# Slices
let nums = [1, 2, 3, 4, 5]
print nums[1:4]    # [2, 3, 4]
print nums[:2]     # [1, 2]
print nums[-2:]    # [4, 5]

# Constants and compound assignment
const MAX = 100
let score = 0
score += 10
score *= 2
print score        # 20

# Error handling
try
  error("something went wrong")
catch e
  print "Caught: " .. e
end

# Modules
import "stdlib/json"
let obj = json_object()
json_set(obj, "lang", "nolqu")
json_set(obj, "version", "1.2.0")
print json_stringify(obj)
```

---

## CLI

```bash
nq program.nq          # run a file
nq check program.nq    # parse + compile, no run
nq repl                # interactive REPL
nq version             # show version
nq help                # show built-in reference
```

---

## REPL

```
$ nq repl

  +--------------------------------------+
  |  Nolqu 1.2.0    Interactive REPL    |
  |  'help' for help  'exit' to quit    |
  +--------------------------------------+

nq > let x = 10
nq > print x * x
100
nq > exit
```

REPL commands: `help` · `clear` · `exit` · `quit`

---

## Standard Library

10 modules included:

| Module | Key functions |
|---|---|
| `stdlib/math` | `clamp` `lerp` `sin` `cos` `log` `gcd` `lcm` · `PI` `TAU` `E` |
| `stdlib/array` | `map` `filter` `reduce` `sum` `flatten` `zip` `chunk` `unique` |
| `stdlib/string` | `replace_all` `title_case` `pad_left` `center` `words` `contains_str` |
| `stdlib/path` | `path_join` `basename` `dirname` `ext` `normalize` |
| `stdlib/json` | `json_object` `json_set` `json_get` `json_stringify` `json_parse` |
| `stdlib/fmt` | `fmt` `printf` `fmt_num` `fmt_pad` `fmt_table` |
| `stdlib/time` | `now` `elapsed` `sleep` `format_duration` `benchmark` |
| `stdlib/test` | `suite` `expect` `expect_eq` `expect_err` `done` |
| `stdlib/os` | `read_lines` `write_lines` `touch` `path_exists` `file_size` |
| `stdlib/io` | `read_file` `write_file` `append_file` `read_lines` `write_lines` `copy_file` |
| `stdlib/file` | `read_or_default` `write_lines` `count_lines` |

---

## Documentation

| | |
|---|---|
| [docs/README.md](docs/README.md) | Documentation index — stable vs dev, version comparison |
| [docs/stable/](docs/stable/) | Docs for **v1.2.0 stable** |
| [docs/dev/](docs/dev/) | Docs for **v1.2.0** (current) |
| [docs/dev/semantics.md](docs/dev/semantics.md) | Truthiness, null, const, slice, logical operators |
| [CHANGELOG.md](CHANGELOG.md) | Full version history |
| [ROADMAP.md](ROADMAP.md) | What's coming |

---

## Build

```bash
make            # release build  → ./nq
make debug      # debug + ASan   → ./nq-debug
make test       # run test suite
make clean      # remove artefacts
```

Runtime core (`memory`, `value`, `chunk`, `object`, `table`, `gc`, `vm`) compiled
as **C11**. Tooling layer (`lexer`, `parser`, `compiler`, `repl`, `main`, `nq_embed`)
compiled as **C++17**.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for code style, commit format, and the PR process.

To report a security vulnerability, see [SECURITY.md](SECURITY.md).

---

## License

MIT — free to use, modify, and distribute. See [LICENSE](LICENSE).

---

## Trademark

Nolqu™ is a trademark of Nadzil. See [TRADEMARK.md](TRADEMARK.md) for usage guidelines.

---

*Nolqu™ · nadzil123 · nolquteam@gmail.com*
