# Nolqu Language Reference — v1.2.0-rc1

> [!NOTE]
> **Development documentation — Nolqu v1.1.x (alpha)**
> This document describes the current alpha series.
> For the v1.0.0 stable reference see [`docs/stable/language.md`](../stable/language.md).

Complete reference for Nolqu v1.2.0-rc1.
For the formal grammar see [grammar.md](grammar.md).
For VM internals see [vm_design.md](vm_design.md).

---

## Table of Contents

1. [Basic Concepts](#1-basic-concepts)
2. [Variables and Assignment](#2-variables-and-assignment)
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

Nolqu programs are plain text files with the `.nq` extension.

```bash
nq program.nq          # run
nq check program.nq    # parse + compile only
nq repl                # interactive REPL
```

### Comments

```nolqu
# This is a comment
let x = 10   # inline comment
```

### Statements

One statement per line. Newlines are the terminator. Blank lines are ignored.

### Print

`print` is a keyword, not a function. It prints any value followed by a newline.

```nolqu
print "Hello"       # Hello
print 42            # 42
print [1, 2, 3]     # [1, 2, 3]
```

---

## 2. Variables and Assignment

### Declaration

```nolqu
let name  = "Alice"
let count = 0
let flag  = true
let empty = nil    # or: null (same value)
```

### Reassignment

```nolqu
count = count + 1
name  = "Bob"
```

### Compound Assignment *(v1.1.1a2+)*

Modify a variable in place without repeating the name.

```nolqu
let score = 0
score += 10     # score = score + 10  → 10
score -= 2      # score = score - 2   → 8
score *= 3      # score = score * 3   → 24
score /= 4      # score = score / 4   → 6

let msg = "hello"
msg ..= " world"   # msg = msg .. " world"
print msg           # hello world
```

Works on local and global variables. Throws a catchable error if the variable
is not yet declared.

### Unused variable warning

The compiler warns when a local variable is declared but never read.
Prefix with `_` to suppress:

```nolqu
let _ignored = expensive_call()   # no warning
```

---

## 3. Data Types

Nolqu is **dynamically typed**. Every value has a runtime type.

| Type | Literals | Notes |
|---|---|---|
| `nil` | `nil` | Absence of value |
| `bool` | `true` `false` | |
| `number` | `0` `3.14` `-5` | Always 64-bit float |
| `string` | `"hello"` `"line\n"` | Immutable, interned |
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

`nil`/`null`, `false`, `0`, and `""` (empty string) are **falsy**.
Everything else is truthy — including `[]` (empty array).

> See [semantics.md](semantics.md) for the full truthiness rules.

### Type checking

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
print ord("A")          # 65
print chr(65)           # A
```

---

## 4. Operators

### Arithmetic

```nolqu
print 10 + 3     # 13
print 10 - 3     # 7
print 10 * 3     # 30
print 10 / 3     # 3.33333...
print 10 % 3     # 1
print -5         # unary negation
```

Division and modulo by zero throw a **catchable** error.

### String concatenation

Use `..` to join strings. Numbers and booleans are auto-converted.

```nolqu
print "Hello, " .. "World!"   # Hello, World!
print "Pi ≈ " .. 3.14          # Pi ≈ 3.14
```

### Comparison

```nolqu
print 1 == 1    # true
print 1 != 2    # true
print 3 < 5     # true
print 3 <= 3    # true
```

`<` `>` `<=` `>=` only work on numbers and throw a catchable error on other types.

### Logical

```nolqu
print true and false    # false
print true or false     # true
print not true          # false
```

`!` is an alias for `not`.

### Operator precedence (high → low)

| Level | Operators |
|---|---|
| Unary | `-` `not` `!` |
| Multiply | `*` `/` `%` |
| Add | `+` `-` |
| Concat | `..` |
| Compare | `<` `>` `<=` `>=` |
| Equality | `==` `!=` |
| And | `and` |
| Or | `or` |

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

### loop / end

While-style loop. Runs as long as the condition is truthy.

```nolqu
let i = 1
loop i <= 10
  print i
  i += 1
end
```

### for / in / end *(v1.1.1a2+)*

Range-based loop — iterate over every element of an array.

```nolqu
let fruits = ["apple", "banana", "cherry"]
for fruit in fruits
  print fruit
end
```

The loop variable is local to the `for` block.

```nolqu
# with continue — skip evens
let odds = []
for n in [1, 2, 3, 4, 5, 6]
  if n % 2 == 0
    continue
  end
  push(odds, n)
end
print join(odds, ", ")   # 1, 3, 5

# with break — stop at first negative
for n in [3, 7, -1, 4]
  if n < 0
    break
  end
  print n
end

# nested
let matrix = [[1, 2], [3, 4]]
for row in matrix
  for val in row
    print val
  end
end
```

### break / continue *(v1.1.0+)*

`break` exits the nearest enclosing `loop` or `for` immediately.
`continue` skips to the next iteration (re-evaluates condition for `loop`,
increments index for `for`).

Using either outside a loop is a **compile-time error**.

---

## 6. Functions

```nolqu
function greet(name)
  return "Hello, " .. name .. "!"
end

print greet("World")     # Hello, World!
```

A function with no `return` returns `nil`. Functions are first-class values.

```nolqu
function factorial(n)
  if n <= 1
    return 1
  end
  return n * factorial(n - 1)
end

print factorial(10)      # 3628800
```

### First-class functions

```nolqu
import "stdlib/array"

function double(x)
  return x * 2
end

let nums    = [1, 2, 3, 4, 5]
let doubled = map(nums, double)
print join(doubled, ", ")        # 2, 4, 6, 8, 10
```

> **Note:** Closures (capturing outer variables) are not supported in v1.1.x.

See [semantics.md](semantics.md) for precise semantics of `null`, truthiness, `and`/`or`, `const`, and slices.

---

## 7. Arrays

```nolqu
let a = [3, 1, 4, 1, 5, 9]

print a[0]       # 3
print a[-1]      # 9   (negative indexing)
print len(a)     # 6

push(a, 2)       # append
let v = pop(a)   # remove last → v = 2
a[0] = 99        # index assignment
remove(a, 1)     # remove at index 1

sort(a)
print join(a, ", ")
```

### Iterating

```nolqu
# preferred in v1.1.x
for item in a
  print item
end

# classic style still works
let i = 0
loop i < len(a)
  print a[i]
  i += 1
end
```

### Nested arrays

```nolqu
let matrix = [[1, 2], [3, 4], [5, 6]]
print matrix[1][0]    # 3
```

---

## 8. Error Handling

```nolqu
try
  let result = 10 / 0
catch err
  print "Caught: " .. err     # Caught: Division by zero.
end
```

### Throwing errors

```nolqu
function divide(a, b)
  if b == 0
    error("divide: second argument must not be zero")
  end
  return a / b
end
```

### assert

```nolqu
assert(1 + 1 == 2)                  # passes silently
assert(len("hi") == 2, "len bug")   # passes

try
  assert(false, "something is wrong")
catch msg
  print msg      # something is wrong
end
```

### Nesting

```nolqu
try
  try
    error("inner")
  catch e
    print "inner: " .. e
    error("re-thrown")
  end
catch e
  print "outer: " .. e
end
```

### What throws (all catchable)

| Operation | Error |
|---|---|
| `error(msg)` | `msg` |
| `assert(false, msg)` | `msg` (or `"Assertion failed."`) |
| Division / modulo by zero | `"Division by zero."` |
| Arithmetic on non-numbers | `"This operation only works on numbers."` |
| `<` `>` `<=` `>=` on non-numbers | `"Operator '<' only works on numbers."` |
| Array index out of bounds | `"IndexError: array index N out of bounds (length M)."` |
| String index out of bounds | `"String index N out of bounds (length M)."` |
| Non-number index | `"TypeError: array index must be a number."` |
| File read / write failure | `"file_read: cannot open 'path'"` etc. |
| Calling a non-function | `"TypeError: only functions can be callable, not <type>."` |
| Builtin wrong argument count | `"Built-in function 'x' expects N argument(s)."` |
| `pop` on empty array | `"pop: cannot pop from an empty array"` |
| `len` on wrong type | `"len: expected string or array, got <type>"` |
| Accessing undefined variable | `"NameError: undefined variable 'name'."`  |
| Assigning to undeclared variable | `"NameError: variable 'name' is not declared."` |

---

## 9. Modules

```nolqu
import "stdlib/math"            # original quoted form
import stdlib/math             # unquoted — same effect
import stdlib/math as math     # alias (parsed, no namespace effect)
from stdlib/math import PI     # selective (module runs, names are global)
import "my_module"             # custom module from CWD     # loads my_module.nq from CWD
```

- Path is relative to the **working directory** where `nq` is run.
- `.nq` extension is added automatically.
- Each module is executed **at most once** per program — re-importing is a no-op. *(fixed in v1.1.1a2)*
- No cycle detection — circular imports will loop forever.

**All built-in modules:**

```nolqu
import "stdlib/math"     # clamp lerp sign  +  PI TAU E sin cos tan log gcd lcm ...
import "stdlib/array"    # map filter reduce reverse  +  sum flatten unique zip ...
import "stdlib/file"     # read_or_default write_lines count_lines
import "stdlib/string"   # is_empty lines words replace_all pad_left title_case ...
import "stdlib/path"     # path_join basename dirname ext normalize ...
import "stdlib/time"     # now millis elapsed sleep format_duration benchmark
import "stdlib/json"     # json_object json_set json_get json_stringify json_parse ...
import "stdlib/test"     # suite expect expect_eq expect_err done
import "stdlib/os"       # read_lines write_lines touch path_exists file_size ...
import "stdlib/fmt"      # fmt printf fmt_num fmt_pad fmt_table
```

See [stdlib.md](stdlib.md) for full reference.

---

## 10. File I/O

```nolqu
file_write("notes.txt", "First line\nSecond line\n")
file_append("notes.txt", "Third line\n")

if file_exists("notes.txt")
  print "exists"
end

let content = file_read("notes.txt")
let lines   = file_lines("notes.txt")

let i = 0
loop i < len(lines)
  print str(i + 1) .. ": " .. lines[i]
  i += 1
end
```

---

## 11. Built-in Functions

### I/O

| Function | Description |
|---|---|
| `input()` | Read a line from stdin |
| `input(prompt)` | Print prompt, then read a line |

### Type conversion

| Function | Description |
|---|---|
| `str(v)` | Convert any value to string (14 significant digits for floats) |
| `num(v)` | Parse string → number; `nil` if invalid |
| `type(v)` | Return `"nil"` `"bool"` `"number"` `"string"` `"array"` `"function"` |

### Type predicates

| Function | Returns |
|---|---|
| `is_nil(v)` | `true` if `v` is `nil` / `null` |
| `is_num(v)` | `true` if `v` is a number |
| `is_str(v)` | `true` if `v` is a string |
| `is_bool(v)` | `true` if `v` is a bool |
| `is_array(v)` | `true` if `v` is an array |
| `bool(v)` | Coerce any value to `true`/`false` using truthiness rules |
| `error_type(e)` | Extract type prefix from a typed error: `"TypeError"`, etc. |
| `ord(ch)` | Code point of first character (0–127) |
| `chr(n)` | Single-character string for code point `n` (0–127) |

### Math

| Function | Description |
|---|---|
| `sqrt(n)` `floor(n)` `ceil(n)` `round(n)` `abs(n)` | Standard math |
| `pow(base, exp)` | Exponentiation |
| `min(a, b)` `max(a, b)` | Smaller / larger of two numbers |
| `random()` | Float in `[0.0, 1.0)` |
| `rand_int(lo, hi)` | Integer in `[lo, hi]` |

### String (built-in)

| Function | Description |
|---|---|
| `len(s)` | Length in bytes |
| `upper(s)` `lower(s)` | Case conversion |
| `trim(s)` | Remove leading/trailing whitespace |
| `slice(s, start, end?)` | Substring — supports negative indices |
| `replace(s, old, new)` | Replace **first** occurrence |
| `split(s, sep)` | Split → array |
| `join(arr, sep)` | Array → string |
| `index(s, sub)` | First position of `sub`, or `-1` |
| `repeat(s, n)` | Repeat `n` times |
| `startswith(s, prefix)` `endswith(s, suffix)` | Prefix / suffix check |

### Array (built-in)

| Function | Description |
|---|---|
| `len(arr)` | Number of elements |
| `push(arr, val)` | Append, return array |
| `pop(arr)` | Remove and return last element |
| `remove(arr, idx)` | Remove element at index |
| `contains(arr, val)` | `true` if `val` is in `arr` |
| `sort(arr)` | Sort in-place, return array |

### File I/O (built-in)

| Function | Description |
|---|---|
| `file_read(path)` | Read entire file → string |
| `file_write(path, content)` | Overwrite file |
| `file_append(path, content)` | Append to file |
| `file_exists(path)` | `true` if readable |
| `file_lines(path)` | Read file → array of lines |

### Error and assertion

| Function | Description |
|---|---|
| `error(msg)` | Throw a runtime error |
| `assert(cond, msg?)` | Throw if `cond` is falsy |

### Memory and time

| Function | Description |
|---|---|
| `clock()` | Seconds since program start |
| `mem_usage()` | Bytes currently on the heap |
| `gc_collect()` | Run GC manually, return bytes freed |

---

## 12. Scoping Rules

```nolqu
let x = "global"

function test()
  let x = "local"    # shadows global x
  print x            # local
end

test()
print x              # global
```

- `let` at top level → **global**
- `let` inside function → **local** to that function
- `let` inside `if` / `loop` / `for` / `try` / `catch` → **local** to that block
- Locals shadow outer names within their scope

---

## 13. Known Limitations (v1.2.0-rc1)

| Limitation | Notes |
|---|---|
| No closures | Functions cannot capture variables from enclosing scopes |
| No hash maps | Only arrays and strings are built-in collections |
| No integer type | All numbers are 64-bit floats |
| No cycle detection | Circular imports loop forever |
| No string mutation | All string operations return new strings |
| Single-pass compiler | No forward references to functions in the same file |
| `replace()` first only | Built-in replaces first occurrence; use `replace_all()` from `stdlib/string` |
| Transpiler experimental | `codegen` does not support arrays, try/catch, or import |
| `chr()` ASCII only | Range 0–127; no full Unicode support |
