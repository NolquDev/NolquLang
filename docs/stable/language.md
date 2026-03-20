# Nolqu Language Reference — v1.2.1 (Stable)

> [!NOTE]
> **Stable documentation — Nolqu v1.2.0**
> This document describes the v1.2.1 stable release.
> For the latest features see [`docs/dev/`](../dev/).

Complete reference for Nolqu v1.0.0.
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

```bash
nq program.nq
```

### Comments

```nolqu
# This is a comment
let x = 10   # inline comment
```

### Statements and newlines

Each statement occupies one line. Blank lines are ignored.

### Print

`print` is a keyword (not a function). Prints any value followed by a newline.

```nolqu
print "Hello"
print 42
print true
print nil
```

---

## 2. Variables

```nolqu
let name = "Alice"
let count = 0

count = count + 1   # reassign (no let)
```

Prefix with `_` to suppress unused-variable warnings:

```nolqu
let _ignored = some_call()
```

---

## 3. Data Types

| Type | Literals | Notes |
|---|---|---|
| `nil` | `nil` | Absence of value |
| `bool` | `true` `false` | |
| `number` | `0` `3.14` `-5` | Always 64-bit float |
| `string` | `"hello"` `"line\n"` | Immutable, interned |
| `array` | `[1, "two", true]` | Dynamic, heterogeneous |
| `function` | `function f(x) ... end` | First-class value |

### String escape sequences

`\n` `\t` `\r` `\\` `\"`

### Truthiness

`nil` and `false` are falsy. Everything else is truthy (including `0` and `""`).

---

## 4. Operators

```nolqu
# Arithmetic
print 10 + 3    # 13
print 10 - 3    # 7
print 10 * 3    # 30
print 10 / 3    # 3.33333...
print 10 % 3    # 1
print -5        # unary negation

# String concatenation
print "Hello" .. " " .. "World"

# Comparison
print 1 == 1    # true
print 1 != 2    # true
print 3 < 5     # true
print 5 > 3     # true
print 3 <= 3    # true
print 4 >= 4    # true

# Logical
print true and false    # false
print true or false     # true
print not true          # false
```

**Operator precedence (high → low):** unary · `*` `/` `%` · `+` `-` · `..` · `<` `>` `<=` `>=` · `==` `!=` · `and` · `or`

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

While-style loop.

```nolqu
let i = 1
loop i <= 10
  print i
  i = i + 1
end
```

> **Note:** `break` and `continue` are not available in v1.0.0.
> Use a condition variable to exit early.

---

## 6. Functions

```nolqu
function add(a, b)
  return a + b
end

print add(3, 4)   # 7
```

Functions with no `return` return `nil`. Functions are first-class values.

---

## 7. Arrays

```nolqu
let fruits = ["apple", "banana", "cherry"]

print fruits[0]    # apple
print fruits[-1]   # cherry  (negative indexing)
print len(fruits)  # 3

push(fruits, "date")
let last = pop(fruits)
sort(fruits)
print join(fruits, ", ")
```

---

## 8. Error Handling

```nolqu
try
  let result = 10 / 0
catch err
  print "Caught: " .. err
end
```

Catchable errors: `error(msg)`, `assert(cond, msg)`, division/modulo by zero,
type mismatch, index out of bounds, file I/O failures, `pop` on empty array.

---

## 9. Modules

```nolqu
import "stdlib/math"
import "stdlib/array"
import "stdlib/file"
import "my_module"      # loads my_module.nq from CWD
```

> **Note:** In v1.0.0, importing a module twice re-executes it.
> This is fixed in v1.2.0.

---

## 10. File I/O

```nolqu
file_write("notes.txt", "First line\n")
file_append("notes.txt", "Second line\n")
if file_exists("notes.txt")
  let content = file_read("notes.txt")
  let lines = file_lines("notes.txt")
end
```

---

## 11. Built-in Functions

### I/O
`input(prompt?)` · `print expr` (keyword)

### Type
`str(v)` · `num(v)` · `type(v)` · `is_nil(v)` · `is_num(v)` · `is_str(v)` · `is_bool(v)` · `is_array(v)`

### Math
`sqrt` · `floor` · `ceil` · `round` · `abs` · `pow` · `min` · `max` · `random()` · `rand_int(lo,hi)`

### String
`len(s)` · `upper` · `lower` · `trim` · `slice` · `replace` · `split` · `join` · `index` · `repeat` · `startswith` · `endswith`

### Array
`len(arr)` · `push` · `pop` · `remove` · `contains` · `sort`

### File I/O
`file_read` · `file_write` · `file_append` · `file_exists` · `file_lines`

### Error
`error(msg)` · `assert(cond, msg?)`

### Memory & Time
`clock()` · `mem_usage()` · `gc_collect()`

---

## 12. Scoping Rules

- `let` at top level → global
- `let` inside function → local to function
- `let` inside `if`/`loop`/`try` → local to that block
- Locals shadow outer variables with the same name

---

## 13. Known Limitations (v1.0.0)

| Limitation | Status |
|---|---|
| No `break` / `continue` | Fixed in v1.1.0 |
| No `for item in array` | Added in v1.2.0 |
| No `+=` `-=` `*=` `/=` | Added in v1.2.0 |
| No `ord()` / `chr()` | Added in v1.2.0 |
| `import` re-executes on double import | Fixed in v1.2.0 |
| Constant pool limit 256 | Raised to 65535 in v1.2.0 |
| Accessing undefined variable not catchable | Fixed in v1.2.0 |
| No closures | Planned for v1.2 |
| No hash maps | Planned for v2.0 |
| No integer type | All numbers are 64-bit floats |
| Circular imports loop forever | No fix planned yet |
| `replace()` first occurrence only | `replace_all()` in `stdlib/string` |
| Transpiler experimental | `codegen` does not support arrays/try/catch |
