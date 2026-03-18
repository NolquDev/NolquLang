# Nolqu Language Reference — v1.0.0

Complete reference for the Nolqu programming language.
For the formal grammar see [grammar.md](grammar.md).
For VM internals see [vm_design.md](vm_design.md).

---

## Table of Contents

1. [Basic Concepts](#1-basic-concepts)
2. [Variables](#2-variables)
3. [Data Types](#3-data-types)
4. [Operators](#4-operators)
5. [Control Flow](#5-control-flow)
6. [Functions](#6-functions)
7. [Arrays](#7-arrays)
8. [Error Handling](#8-error-handling)
9. [Modules](#9-modules)
10. [File I/O](#10-file-io)
11. [Built-in Functions](#11-built-in-functions)
12. [Scoping Rules](#12-scoping-rules)
13. [Known Limitations](#13-known-limitations)

---

## 1. Basic Concepts

### Source files

Nolqu programs are plain text files with the `.nq` extension.
Run them with:

```bash
nq program.nq
```

### Comments

```nolqu
# This is a comment — from # to end of line
let x = 10   # inline comment
```

Block comments are not supported.

### Statements and newlines

Each statement occupies one line. Newlines are the statement terminator.
Blank lines are ignored.

```nolqu
let a = 1
let b = 2
print a + b
```

### Print

`print` is a keyword (not a function). It prints any value followed by a newline.

```nolqu
print "Hello"       # Hello
print 42            # 42
print true          # true
print nil           # nil
print [1, 2, 3]     # [1, 2, 3]
```

---

## 2. Variables

Variables are declared with `let`. Assignment without `let` reassigns an
existing variable.

```nolqu
let name = "Alice"     # declare and assign
let count = 0

count = count + 1      # reassign (no let)
```

Variables declared at the top level are **global**.
Variables declared inside a block (function, if, loop) are **local** to that block.

### Unused variable warnings

The compiler warns if a local variable is declared but never read:

```
[ Warning ] line 3: Local variable 'tmp' is declared but never used.
  Hint: Prefix with _ to suppress: _tmp
```

Prefix the name with `_` to explicitly mark it as intentionally unused:

```nolqu
let _ignored = some_call()   # no warning
```

### Compound assignment

`+=` `-=` `*=` `/=` `..=` modify a variable in place.

```nolqu
let score = 0
score += 10     # score = score + 10
score -= 2      # score = score - 2
score *= 3      # score = score * 3
score /= 6      # score = score / 6
print score     # 4

let msg = "hello"
msg ..= " world"   # msg = msg .. " world"
print msg           # hello world
```

Works on both local and global variables.

---

---

## 3. Data Types

Nolqu is **dynamically typed**. Every value has a runtime type.

### Primitives

| Type | Literals | Notes |
|---|---|---|
| `nil` | `nil` | Absence of value |
| `bool` | `true` `false` | |
| `number` | `0` `3.14` `-5` `1e6` | Always 64-bit float (no integer type) |
| `string` | `"hello"` `"line\n"` | Immutable, interned |

### Compound types

| Type | Example | Notes |
|---|---|---|
| `array` | `[1, "two", true]` | Dynamic, heterogeneous |
| `function` | `function f(x) ... end` | First-class value |

### String escape sequences

| Sequence | Meaning |
|---|---|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\\` | Backslash |
| `\"` | Double quote |

### Truthiness

- `nil` and `false` are **falsy**.
- Everything else is **truthy** — including `0` and `""`.

### Type checking at runtime

```nolqu
print type(42)          # number
print type("hi")        # string
print type(true)        # bool
print type(nil)         # nil
print type([1,2])       # array

print is_num(42)        # true
print is_str("hi")      # true
print is_bool(false)    # true
print is_nil(nil)       # true
print is_array([])      # true
```

---

## 4. Operators

### Arithmetic

```nolqu
print 10 + 3     # 13
print 10 - 3     # 7
print 10 * 3     # 30
print 10 / 3     # 3.33333...
print 10 % 3     # 1   (modulo)
print -5         # -5  (unary negation)
```

Division and modulo by zero throw a catchable error.

### String concatenation

Use `..` to concatenate. Numbers and booleans are auto-converted.

```nolqu
print "Hello, " .. "World!"    # Hello, World!
print "Value: " .. 42          # Value: 42
print "Pi ≈ " .. 3.14          # Pi ≈ 3.14
```

### Comparison

```nolqu
print 1 == 1      # true
print 1 != 2      # true
print 3 < 5       # true
print 5 > 3       # true
print 3 <= 3      # true
print 4 >= 4      # true
```

Comparison operators (`<`, `>`, `<=`, `>=`) only work on numbers and throw a
catchable error on other types.

### Logical

```nolqu
print true and false    # false
print true or false     # true
print not true          # false
print not nil           # true  (nil is falsy)
```

`!` is an alias for `not` when used as a prefix:

```nolqu
if not contains(arr, x)
  # ...
end
```

### Operator precedence (high to low)

| Level | Operators |
|---|---|
| 7 — unary | `-` `not` `!` |
| 6 — multiply | `*` `/` `%` |
| 5 — add | `+` `-` |
| 4 — concat | `..` |
| 3 — compare | `<` `>` `<=` `>=` |
| 2 — equality | `==` `!=` |
| 1 — and | `and` |
| 0 — or | `or` |

Use parentheses to override.

---

## 5. Control Flow

### if / else / end

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

Conditions can be any expression. `nil` and `false` are falsy; everything else is truthy.

### loop / end

`loop` is a while-style loop. It runs as long as the condition is truthy.

```nolqu
let i = 1
loop i <= 10
  print i
  i = i + 1
end
```

Infinite loop pattern:

```nolqu
loop true
  let line = input()
  if line == "quit"
    break
  end
end
```

---

## 6. Functions

### Declaration

```nolqu
function add(a, b)
  return a + b
end

print add(3, 4)     # 7
```

### Return value

Functions return the value of the `return` statement.
A function with no `return` returns `nil`.

```nolqu
function greet(name)
  return "Hello, " .. name .. "!"
end

let msg = greet("Alice")
print msg           # Hello, Alice!
```

### Recursion

```nolqu
function factorial(n)
  if n <= 1
    return 1
  end
  return n * factorial(n - 1)
end

print factorial(10)     # 3628800
```

### First-class functions

Functions are values. They can be stored in variables and passed as arguments.

```nolqu
function double(x)
  return x * 2
end

import "stdlib/array"

let nums    = [1, 2, 3, 4, 5]
let doubled = map(nums, double)
print join(doubled, ", ")       # 2, 4, 6, 8, 10
```

> **Note:** Closures (capturing variables from outer scopes) are not supported in v1.0.0.
> Functions can only access global variables and their own parameters/locals.

---

## 7. Arrays

### Literals and access

```nolqu
let fruits = ["apple", "banana", "cherry"]

print fruits[0]      # apple
print fruits[1]      # banana
print fruits[-1]     # cherry   (negative indexing)
print fruits[-2]     # banana
```

### Mutation

```nolqu
let a = [1, 2, 3]

push(a, 4)           # append → [1, 2, 3, 4]
let v = pop(a)       # remove last → v=4, a=[1, 2, 3]

a[0] = 99            # index assignment
print a              # [99, 2, 3]

remove(a, 1)         # remove index 1 → [99, 3]
```

### Useful array operations

```nolqu
let nums = [5, 3, 1, 4, 2]

sort(nums)
print join(nums, ", ")       # 1, 2, 3, 4, 5

print len(nums)              # 5
print contains(nums, 3)      # true
print contains(nums, 99)     # false
```

### Iterating

```nolqu
let items = ["a", "b", "c"]
let i = 0
loop i < len(items)
  print items[i]
  i = i + 1
end
```

### Nested arrays

```nolqu
let matrix = [[1, 2], [3, 4], [5, 6]]
print matrix[1][0]    # 3
```

---

## 8. Error Handling

### try / catch / end

```nolqu
try
  let result = 10 / 0
catch err
  print "Caught: " .. err     # Caught: Division by zero.
end
```

The catch variable receives the error message as a string.

### Throwing errors

```nolqu
function divide(a, b)
  if b == 0
    error("divide: second argument must not be zero")
  end
  return a / b
end

try
  print divide(10, 0)
catch msg
  print msg
end
```

### assert

```nolqu
assert(1 + 1 == 2)                  # passes — no effect
assert(len("hi") == 2, "len bug")   # passes

try
  assert(false, "something is wrong")
catch msg
  print msg       # something is wrong
end
```

### Nesting

```nolqu
try
  try
    error("inner")
  catch e
    print "inner caught: " .. e
    error("re-thrown")
  end
catch e
  print "outer caught: " .. e
end
```

### What throws

All of these throw catchable errors:

| Operation | Error message |
|---|---|
| `error(msg)` | `msg` |
| `assert(false, msg)` | `msg` (or `"Assertion failed."`) |
| Division / modulo by zero | `"Division by zero."` |
| Arithmetic on non-numbers | `"This operation only works on numbers."` |
| `<` `>` `<=` `>=` on non-numbers | `"Operator '<' only works on numbers."` |
| Array index out of bounds | `"Array index N out of bounds (length M)."` |
| String index out of bounds | `"String index N out of bounds (length M)."` |
| Non-number index | `"Array index must be a number."` |
| File read / write failure | `"file_read: cannot open 'path'"` etc. |
| Calling a non-function | `"Only functions can be called, not <type>."` |
| Accessing undefined variable | `"Undefined variable 'name'."` |
| Assigning to undeclared variable | `"Variable 'name' is not declared."` |
| `pop` on empty array | `"pop: cannot pop from an empty array"` |
| `len` on wrong type | `"len: expected string or array, got <type>"` |

---

## 9. Modules

```nolqu
import "stdlib/math"
import "stdlib/array"
import "my_module"        # loads my_module.nq from the current directory
```

- The path is relative to the **working directory** when `nq` is invoked.
- `.nq` extension is appended automatically.
- Importing executes the file in the **global scope** — all top-level declarations become globals.
- No cycle detection in v1.0.0 — circular imports will loop forever.

**Built-in modules:**

```nolqu
import "stdlib/math"    # clamp  lerp  sign
import "stdlib/array"   # map  filter  reduce  reverse
import "stdlib/file"    # read_or_default  write_lines  count_lines
```

See [stdlib.md](stdlib.md) for full module reference.

---

## 10. File I/O

```nolqu
# Write (creates or overwrites)
file_write("notes.txt", "First line\nSecond line\n")

# Append
file_append("notes.txt", "Third line\n")

# Check existence
if file_exists("notes.txt")
  print "exists"
end

# Read entire file
let content = file_read("notes.txt")
print content

# Read as array of lines
let lines = file_lines("notes.txt")
let i = 0
loop i < len(lines)
  print str(i + 1) .. ": " .. lines[i]
  i = i + 1
end
```

All file operations throw catchable errors on failure.

---

## 11. Built-in Functions

### I/O

| Function | Description |
|---|---|
| `input()` | Read a line from stdin, return as string |
| `input(prompt)` | Print prompt, then read a line |

### Type conversion

| Function | Description |
|---|---|
| `str(v)` | Convert any value to string |
| `num(v)` | Parse string to number; returns `nil` if invalid |
| `type(v)` | Return type name: `"nil"` `"bool"` `"number"` `"string"` `"array"` `"function"` |

### Type predicates

| Function | Returns |
|---|---|
| `ord(ch)` | Code point of first character (0–127) |
| `chr(n)` | Single-character string for code point `n` (0–127) |
| `is_nil(v)` | `true` if `v` is `nil` |
| `is_num(v)` | `true` if `v` is a number |
| `is_str(v)` | `true` if `v` is a string |
| `is_bool(v)` | `true` if `v` is a bool |
| `is_array(v)` | `true` if `v` is an array |

### Math

| Function | Description |
|---|---|
| `sqrt(n)` | Square root |
| `floor(n)` | Round down |
| `ceil(n)` | Round up |
| `round(n)` | Round to nearest integer |
| `abs(n)` | Absolute value |
| `pow(base, exp)` | Exponentiation |
| `min(a, b)` | Smaller of two numbers |
| `max(a, b)` | Larger of two numbers |
| `random()` | Float in `[0.0, 1.0)` |
| `rand_int(lo, hi)` | Integer in `[lo, hi]` inclusive |

### String

| Function | Description |
|---|---|
| `len(s)` | String length in bytes |
| `upper(s)` | Uppercase |
| `lower(s)` | Lowercase |
| `trim(s)` | Remove leading and trailing whitespace |
| `slice(s, start)` | Substring from `start` to end |
| `slice(s, start, end)` | Substring `[start, end)` — supports negative indices |
| `replace(s, old, new)` | Replace **first** occurrence of `old` with `new` |
| `split(s, sep)` | Split `s` on `sep`, return array of strings |
| `join(arr, sep)` | Join array elements into a string, separated by `sep` |
| `index(s, sub)` | First position of `sub` in `s`, or `-1` |
| `repeat(s, n)` | Repeat string `n` times |
| `startswith(s, prefix)` | `true` if `s` starts with `prefix` |
| `endswith(s, suffix)` | `true` if `s` ends with `suffix` |

### Array

| Function | Description |
|---|---|
| `len(arr)` | Number of elements |
| `push(arr, val)` | Append `val`, return the array |
| `pop(arr)` | Remove and return last element (throws on empty) |
| `remove(arr, idx)` | Remove element at `idx`, return removed value |
| `contains(arr, val)` | `true` if `val` is in `arr` |
| `sort(arr)` | Sort in-place (numbers then strings), return `arr` |

### File I/O

| Function | Description |
|---|---|
| `file_read(path)` | Read entire file, return string (throws on error) |
| `file_write(path, content)` | Overwrite file, return `true`/`false` |
| `file_append(path, content)` | Append to file, return `true`/`false` |
| `file_exists(path)` | `true` if the file can be opened for reading |
| `file_lines(path)` | Read file, return array of lines (newlines stripped) |

### Error and assertion

| Function | Description |
|---|---|
| `error(msg)` | Throw `msg` as a runtime error |
| `assert(cond)` | Throw `"Assertion failed."` if `cond` is falsy |
| `assert(cond, msg)` | Throw `msg` if `cond` is falsy |

### Memory and time

| Function | Description |
|---|---|
| `clock()` | Seconds elapsed since program start (float) |
| `mem_usage()` | Bytes currently allocated on the heap |
| `gc_collect()` | Manually trigger GC, return bytes freed |

---

## 12. Scoping Rules

```nolqu
let x = "global"

function test()
  let x = "local"      # shadows global x inside the function
  print x              # local
end

test()
print x                # global
```

```nolqu
let total = 0

let i = 0
loop i < 5
  let item = i * 2     # local to loop block
  total = total + item
  i = i + 1
end

# item is not accessible here
print total             # 20
```

Rules summary:
- `let` at the top level → global variable
- `let` inside a function → local to that function
- `let` inside `if`, `loop`, `try`, or `catch` → local to that block
- Local variables **shadow** outer ones with the same name
- `catch` variable is local to the catch block

---

## 13. Known Limitations (v1.0.0)

| Limitation | Notes |
|---|---|
| No closures | Functions cannot capture variables from enclosing scopes |
| No hash maps | Only arrays and strings are built-in collections |
| No integer type | All numbers are 64-bit floats |
| No cycle detection | Circular imports loop forever |
| No string mutation | All string operations return new strings |
| Single-pass compiler | No forward references to functions within a file |
| `replace()` first only | The built-in replaces only the first occurrence; use `replace_all()` from `stdlib/string` for all occurrences |
| Transpiler experimental | `codegen` does not support arrays, try/catch, or import |
