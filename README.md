# Nolqu Programming Language

```
 _   _       _
| \ | | ___ | | __ _ _   _
|  \| |/ _ \| |/ _` | | | |
| |\  | (_) | | (_| | |_| |
|_| \_|\___/|_|\__, |\__,_|
               |___/
v0.9.0-beta
```

Nolqu is a programming language designed so that code reads like plain sentences.
One goal: write programs without fighting the syntax. Great for learning programming
from scratch, or for developers who want a language that stays out of the way.

---

## Installation

### Linux / macOS

```bash
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu
make
sudo make install   # install to /usr/local/bin/nq
```

### Termux (Android)

```bash
pkg update && pkg install git clang make
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu
make
cp nq $PREFIX/bin/
```

---

## Usage

```bash
nq program.nq          # Run a program
nq run program.nq      # Run a program (alternative)
nq repl                # Interactive REPL mode
nq version             # Show version
nq help                # Show help
```

---

## Basic Syntax

### Variables

```nolqu
let name = "Alice"
let age  = 25
let active = true
let empty  = nil

print name
print age
```

### Arithmetic

```nolqu
let x = 10
let y = 3

print x + y    # 13
print x - y    # 7
print x * y    # 30
print x / y    # 3.33333...
print x % y    # 1
```

### String Concatenation

```nolqu
let greeting = "Hello, " .. "World" .. "!"
print greeting    # Hello, World!

let n = 42
print "The answer is: " .. n
```

### User Input (new in v0.2.0)

```nolqu
let name = input("What is your name? ")
print "Hello, " .. name .. "!"

let age_str = input("How old are you? ")
let age = num(age_str)
print "Next year you will be " .. age + 1
```

### Conditionals

```nolqu
let score = 75

if score >= 90
  print "Grade A"
else
  if score >= 75
    print "Grade B"
  else
    print "Grade C"
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
```

### Functions

```nolqu
function greet(name)
  return "Hello, " .. name .. "!"
end

print greet("World")

function factorial(n)
  if n <= 1
    return 1
  end
  return n * factorial(n - 1)
end

print factorial(5)    # 120
```

### Boolean Logic

```nolqu
let a = 10
let b = 20

if a > 0 and b > 0
  print "Both positive"
end

if a > 100 or b > 10
  print "At least one is true"
end

if not false
  print "This always prints"
end
```

---

## Built-in Functions

| Function         | Description                                          |
|------------------|------------------------------------------------------|
| `input(prompt?)` | Read a line from stdin. Optional prompt string.      |
| `str(value)`     | Convert any value to a string.                       |
| `num(value)`     | Convert a string to a number. Returns nil if invalid.|
| `type(value)`    | Return the type name of a value as a string.         |

```nolqu
print type(42)        # number
print type("hello")   # string
print type(true)      # bool
print type(nil)       # nil

print str(99)         # "99"
let n = num("3.14")
print n + 1           # 4.14
```

---

## Data Types

| Type       | Example               | Description                  |
|------------|-----------------------|------------------------------|
| `number`   | `42`, `3.14`, `-5`    | 64-bit floating point        |
| `string`   | `"Hello World"`       | Immutable Unicode string     |
| `bool`     | `true`, `false`       | Boolean value                |
| `nil`      | `nil`                 | No value                     |
| `function` | `function f() ...`    | Function                     |

---

## Keywords

```
let       — variable declaration
print     — print to output
if        — conditional
else      — alternative branch
loop      — condition-based loop (while-style)
function  — function declaration
return    — return a value from a function
import    — import a file (not yet active)
end       — close a block (if / loop / function)
true      — boolean true
false     — boolean false
nil       — empty value
and       — logical AND
or        — logical OR
not       — logical NOT
```

---

## Full Example

### Fibonacci

```nolqu
function fibonacci(n)
  if n <= 1
    return n
  end
  return fibonacci(n - 1) + fibonacci(n - 2)
end

let i = 0
loop i < 10
  print fibonacci(i)
  i = i + 1
end
```

### FizzBuzz

```nolqu
let n = 1
loop n <= 20
  if n % 15 == 0
    print "FizzBuzz"
  else
    if n % 3 == 0
      print "Fizz"
    else
      if n % 5 == 0
        print "Buzz"
      else
        print n
      end
    end
  end
  n = n + 1
end
```

### Interactive Greeting

```nolqu
let name = input("Your name: ")
let age  = num(input("Your age: "))

print "Hello, " .. name .. "!"

if age >= 18
  print "You are an adult."
else
  print "You are a minor."
end
```

---

## Architecture

```
source.nq
    │
    ▼ Lexer         → Token stream
    ▼ Parser        → AST (Abstract Syntax Tree)
    ▼ Compiler      → Bytecode Chunk
    ▼ Nolqu VM      → Execution (stack-based)
```

### Project Structure

```
nolqu/
├── src/
│   ├── common.h      — Shared headers & macros
│   ├── memory.h/.c   — Memory management
│   ├── value.h/.c    — Value types (nil, bool, number, obj)
│   ├── chunk.h/.c    — Bytecode chunk + disassembler
│   ├── object.h/.c   — Heap objects (string, function, native)
│   ├── table.h/.c    — Hash table (globals, string interning)
│   ├── lexer.h/.c    — Lexer / tokenizer
│   ├── ast.h/.c      — AST node types
│   ├── parser.h/.c   — Recursive descent parser
│   ├── compiler.h/.c — AST → Bytecode compiler
│   ├── vm.h/.c       — Nolqu Virtual Machine
│   ├── repl.h/.c     — Interactive REPL
│   └── main.c        — CLI entry point (nq)
├── examples/
│   ├── hello.nq
│   ├── fibonacci.nq
│   ├── functions.nq
│   ├── counter.nq
│   └── input.nq
├── docs/
│   ├── grammar.md
│   └── vm_design.md
├── Makefile
├── README.md
├── CHANGELOG.md
└── LICENSE
```

---

## Build

```bash
make          # Release build → ./nq
make debug    # Debug build   → ./nq-debug
make test     # Run example programs
make clean    # Remove build artifacts
```

---

## Error Messages

Nolqu provides clear, actionable error messages:

```
[ Runtime Error ] program.nq:5
  Undefined variable 'x'.
  Hint: Did you declare it with 'let x = ...'?
```

```
[ Runtime Error ] program.nq:8
  Division by zero is not allowed.
  Hint: Check the divisor value before dividing.
```

```
[ Runtime Error ] program.nq:12
  Function 'add' expects 2 argument(s), but got 1.
```

---

## License

MIT License — free to use and modify.
