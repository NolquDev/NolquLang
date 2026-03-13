# Nolqu

```
  _   _       _
 | \ | | ___ | | __ _ _   _
 |  \| |/ _ \| |/ _` | | | |
 | |\  | (_) | | (_| | |_| |
 |_| \_|\___/|_|\__, |\__,_|
                |___/
 v1.0.0  —  Stable Release
```

A small, fast programming language with a clean syntax and its own bytecode VM.
Nolqu is designed so code reads like plain sentences — great for learning, scripting,
and embedding into C or C++ applications.

---

## Features

- **Clean syntax** — `if`, `loop`, `function`, `end` — no braces, no semicolons
- **Stack-based bytecode VM** — fast and portable
- **Garbage collected** — mark-and-sweep GC, automatic
- **Try / catch** — structured error handling
- **Arrays** — dynamic, heterogeneous, with negative indexing
- **Modules** — `import "stdlib/math"` and your own `.nq` files
- **File I/O** — read, write, append, split by lines
- **Rich standard library** — math, string, array, random, file helpers
- **Embed API** — use Nolqu inside any C or C++ project via `nolqu.h`
- **C/C++ runtime** — core VM in C11, tooling in C++17

---

## Quick Install

### Linux / macOS

```bash
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu
make
sudo make install        # installs to /usr/local/bin/nq
```

### Termux (Android)

```bash
pkg update && pkg install git clang make
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu
make CC=clang CXX=clang++
cp nq $PREFIX/bin/
```

### Windows (MinGW-w64 / MSYS2)

```bash
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu
mingw32-make
```

---

## Running Programs

```bash
nq program.nq            # run a file
nq run program.nq        # same, explicit command
nq check program.nq      # parse + compile only (no run)
nq repl                  # interactive REPL
nq version               # show version
nq help                  # show help and built-in list
```

---

## Language Tour

### Variables

```nolqu
let name   = "Alice"
let age    = 25
let active = true
let empty  = nil

print name              # Alice
print "Age: " .. age    # Age: 25
```

### Arithmetic & Strings

```nolqu
let x = 10
let y = 3

print x + y    # 13
print x - y    # 7
print x * y    # 30
print x / y    # 3.33333...
print x % y    # 1

# String concatenation with ..
let greeting = "Hello, " .. name .. "!"
print greeting          # Hello, Alice!
```

### Conditionals

```nolqu
let score = 82

if score >= 90
  print "A"
else
  if score >= 75
    print "B"
  else
    print "C"
  end
end
```

### Loops

```nolqu
let i = 1
loop i <= 5
  print i
  i = i + 1
end
# prints 1 2 3 4 5
```

### Functions

```nolqu
function greet(name)
  return "Hello, " .. name .. "!"
end

print greet("World")    # Hello, World!

function factorial(n)
  if n <= 1
    return 1
  end
  return n * factorial(n - 1)
end

print factorial(10)     # 3628800
```

### Boolean Logic

```nolqu
let a = 10
let b = 20

if a > 0 and b > 0
  print "both positive"
end

if a > 100 or b > 10
  print "at least one"
end

if not false
  print "always"
end
```

### Arrays

```nolqu
let nums = [3, 1, 4, 1, 5, 9]

print len(nums)         # 6
print nums[0]           # 3
print nums[-1]          # 9  (negative indexing)

push(nums, 2)           # append
let last = pop(nums)    # remove and return last

sort(nums)              # sort in-place
print join(nums, ", ")  # 1, 1, 3, 4, 5

# Array slice (strings too)
let words = ["one", "two", "three"]
print words[1]          # two
```

### Error Handling

```nolqu
try
  let result = 10 / 0
catch err
  print "Caught: " .. err    # Caught: Division by zero.
end

# User errors
try
  error("something went wrong")
catch msg
  print msg
end

# Assert
assert(1 + 1 == 2, "math is broken")

# is_* type predicates
print is_num(42)        # true
print is_str("hi")      # true
print is_array([])      # true
print type(3.14)        # number
```

### File I/O

```nolqu
# Write
file_write("hello.txt", "Hello from Nolqu!\n")

# Read
let content = file_read("hello.txt")
print content

# Check existence
if file_exists("hello.txt")
  print "file found"
end

# Read as lines
let lines = file_lines("hello.txt")
print len(lines) .. " lines"

# Append
file_append("log.txt", "new entry\n")
```

### Modules

```nolqu
import "stdlib/math"
import "stdlib/array"
import "stdlib/file"

# stdlib/math provides: clamp, lerp, sign
print clamp(15, 0, 10)     # 10
print lerp(0, 100, 0.5)    # 50

# stdlib/array provides: map, filter, reduce, reverse
import "stdlib/array"

function double(x)
  return x * 2
end

let nums = [1, 2, 3, 4, 5]
let doubled = map(nums, double)
print join(doubled, " ")    # 2 4 6 8 10
```

### User Input

```nolqu
let name = input("Your name: ")
let age  = num(input("Your age: "))

print "Hello, " .. name .. "!"

if age >= 18
  print "You are an adult."
else
  print "You are " .. (18 - age) .. " years from adulthood."
end
```

---

## Built-in Functions

### I/O
| Function | Description |
|---|---|
| `input(prompt?)` | Read a line from stdin |
| `print expr` | Print value with newline (keyword, not function) |

### Type
| Function | Description |
|---|---|
| `str(v)` | Convert to string |
| `num(v)` | String → number, nil if invalid |
| `type(v)` | Return type name: `"nil"` `"bool"` `"number"` `"string"` `"array"` `"function"` |
| `is_nil(v)` `is_num(v)` `is_str(v)` `is_bool(v)` `is_array(v)` | Type predicates |

### Math
| Function | Description |
|---|---|
| `sqrt(n)` `floor(n)` `ceil(n)` `round(n)` `abs(n)` | Standard math |
| `pow(base, exp)` | Exponentiation |
| `min(a, b)` `max(a, b)` | Comparisons |
| `random()` | Float in `[0, 1)` |
| `rand_int(lo, hi)` | Integer in `[lo, hi]` |

### String
| Function | Description |
|---|---|
| `len(s)` | String length |
| `upper(s)` `lower(s)` | Case conversion |
| `trim(s)` | Strip leading/trailing whitespace |
| `slice(s, start, end?)` | Substring — supports negative indices |
| `replace(s, old, new)` | Replace first occurrence |
| `split(s, sep)` | Split → array |
| `join(arr, sep)` | Array → string |
| `index(s, sub)` | First position of `sub`, or `-1` |
| `repeat(s, n)` | Repeat string `n` times |
| `startswith(s, prefix)` `endswith(s, suffix)` | Prefix / suffix check |

### Array
| Function | Description |
|---|---|
| `len(arr)` | Array length |
| `push(arr, val)` | Append, return array |
| `pop(arr)` | Remove and return last element |
| `remove(arr, idx)` | Remove at index, return removed value |
| `contains(arr, val)` | True if value is in array |
| `sort(arr)` | Sort in-place, return array |

### File I/O
| Function | Description |
|---|---|
| `file_read(path)` | Read entire file as string |
| `file_write(path, content)` | Overwrite file, return bool |
| `file_append(path, content)` | Append to file, return bool |
| `file_exists(path)` | True if file is readable |
| `file_lines(path)` | Read file → array of lines |

### Error & Assert
| Function | Description |
|---|---|
| `error(msg)` | Throw a runtime error |
| `assert(cond, msg?)` | Throw if condition is false |

### Memory & Time
| Function | Description |
|---|---|
| `clock()` | Seconds since program start |
| `mem_usage()` | Bytes currently allocated |
| `gc_collect()` | Trigger GC, return bytes freed |

---

## Standard Library Modules

```nolqu
import "stdlib/math"    # clamp(v,lo,hi)  lerp(a,b,t)  sign(n)
import "stdlib/array"   # map(arr,fn)  filter(arr,fn)  reduce(arr,fn,init)  reverse(arr)
import "stdlib/file"    # read_or_default(path,def)  write_lines(path,arr)  count_lines(path)
```

---

## REPL

```bash
$ nq repl

  +--------------------------------------+
  |  Nolqu 1.0.0    Interactive REPL    |
  |  'help' for help  'exit' to quit    |
  +--------------------------------------+

nq > let x = 10
nq > print x * x
100
nq > function add(a, b)
  ...   return a + b
  ... end
nq > print add(3, 4)
7
nq > exit
```

**REPL commands:** `help` · `clear` · `exit` · `quit`

---

## Embed in C / C++

```c
#include "nolqu.h"

int main(void) {
    NqState* nq = nq_open();

    nq_setglobal_string(nq, "player", "Alice");
    nq_setglobal_number(nq, "score", 9001);

    nq_dostring(nq, "print player .. \" scored \" .. score");
    nq_dofile(nq, "game_logic.nq");

    NqValue result = nq_getglobal(nq, "final_score");
    if (result.type == NQ_TYPE_NUMBER)
        printf("Final: %g\n", result.as.number);

    nq_close(nq);
    return 0;
}
```

See `include/nolqu.h` for the full embedding API.

---

## Project Structure

```
nolqu/
├── src/
│   ├── common.h          Shared macros, limits, ANSI colours
│   ├── memory.c/h        Tracked allocator — all heap through nq_realloc
│   ├── value.c/h         Value tagged union (nil/bool/number/obj)
│   ├── chunk.c/h         Bytecode chunk + disassembler
│   ├── object.c/h        Heap objects: ObjString, ObjFunction, ObjNative, ObjArray
│   ├── table.c/h         Open-addressing hash table (globals, string intern)
│   ├── gc.c/h            Mark-and-sweep garbage collector
│   ├── vm.c/h            Stack-based bytecode VM + all built-in functions
│   ├── lexer.cpp/h       Tokeniser
│   ├── ast.cpp/h         AST node types + NodeList
│   ├── parser.cpp/h      Recursive-descent parser
│   ├── compiler.cpp/h    AST → bytecode compiler
│   ├── codegen.cpp/h     Nolqu → C transpiler (experimental)
│   ├── repl.cpp/h        Interactive REPL
│   ├── nq_embed.cpp      C/C++ embedding API implementation
│   └── main.cpp          CLI entry point
├── include/
│   └── nolqu.h           Public embedding API header
├── stdlib/
│   ├── math.nq           clamp, lerp, sign
│   ├── array.nq          map, filter, reduce, reverse
│   └── file.nq           read_or_default, write_lines, count_lines
├── examples/
│   ├── hello.nq
│   ├── fibonacci.nq
│   ├── functions.nq
│   ├── counter.nq
│   ├── input.nq
│   ├── arrays.nq
│   ├── stdlib.nq
│   ├── import_demo.nq
│   ├── files.nq
│   └── tests/
│       ├── math_test.nq
│       ├── string_test.nq
│       ├── array_test.nq
│       └── error_test.nq
├── docs/
│   ├── grammar.md        Formal EBNF grammar
│   └── vm_design.md      VM architecture and bytecode reference
├── Makefile
├── README.md
├── CHANGELOG.md
├── ROADMAP.md
└── LICENSE               MIT
```

---

## Build System

```bash
make              # Release build  → ./nq
make debug        # Debug + ASan   → ./nq-debug
make test         # Run test suite
make install      # Copy to /usr/local/bin/nq
make clean        # Remove build artefacts
```

The runtime core (`memory`, `value`, `chunk`, `object`, `table`, `gc`, `vm`) is
compiled as **C11**. The tooling layer (`lexer`, `parser`, `ast`, `compiler`,
`repl`, `main`) is compiled as **C++17**. All public headers carry `extern "C"`
guards for correct interoperability.

---

## Error Messages

Nolqu gives clear, actionable messages:

```
[ Runtime Error ] program.nq:7
  Undefined variable 'scroe'. Did you mean 'score'?
  Hint: Use 'let scroe = ...' to declare a variable.

[ Runtime Error ] program.nq:12
  Division by zero.

[ Compile Error ] program.nq:4
  Function 'greet' expects 2 argument(s), but got 1.

[ Warning ] line 9: Local variable 'tmp' is declared but never used.
  Hint: Prefix with _ to suppress: _tmp
```

---

## License

MIT License — free to use, modify, and distribute.  
See `LICENSE` for the full text.
