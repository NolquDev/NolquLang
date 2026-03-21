# Nolqu Semantics Reference — v1.2.2a6

> [!NOTE]
> **Nolqu v1.2.0 — Stable**
> This document describes the precise runtime semantics of the language.
> For syntax see [grammar.md](grammar.md). For a feature overview see [language.md](language.md).

---

## Table of Contents

1. [Truthy and Falsy Values](#1-truthy-and-falsy-values)
2. [null and nil](#2-null-and-nil)
3. [Function Return Behavior](#3-function-return-behavior)
4. [Logical Operators](#4-logical-operators)
5. [const Variables](#5-const-variables)
6. [Slice Expressions](#6-slice-expressions)

---

## 1. Truthy and Falsy Values

Nolqu evaluates conditions in `if`, `loop`, `for`, `and`, `or`, and `not`
expressions by testing truthiness. This is distinct from equality — truthiness
is a property of a value, not a comparison.

### Falsy values

The following values are **falsy** (treated as false in conditional contexts):

| Value | Type | Notes |
|---|---|---|
| `false` | bool | |
| `null` / `nil` | null | Both keywords are identical |
| `0` | number | Exactly zero (0.0 in IEEE 754) |
| `""` | string | Empty string — zero length |

### Truthy values

Everything else is **truthy**, including:

| Value | Notes |
|---|---|
| `true` | |
| Any non-zero number | `1`, `-1`, `0.001`, `3.14`, `-0.001` |
| Any non-empty string | `"hello"`, `"false"`, `"0"` |
| Any array | `[]`, `[1, 2, 3]` — even empty arrays are truthy |
| Any function | |

> [!IMPORTANT]
> **`0` and `""` are falsy in Nolqu.** This is different from many languages.
> In v1.0.0, only `nil` and `false` were falsy.
> Always use explicit comparisons (`x != 0`, `len(s) > 0`) when you want to
> check a value rather than just its truthiness.

### Examples

```nolqu
# These conditions do NOT execute the body
if false   end
if null    end
if nil     end
if 0       end
if ""      end

# These DO execute the body
if true    end
if 1       end
if -1      end
if "hi"    end
if "false" end   # string "false" is truthy
if "0"     end   # string "0" is truthy
if []      end   # empty array is truthy
```

### Truthiness in not

```nolqu
print not false   # true
print not null    # true
print not 0       # true
print not ""      # true
print not 1       # false
print not "hi"    # false
```

---

## 2. null and nil

`null` and `nil` are two keywords for the same value — the absence of a value.

```nolqu
print null == nil     # true
print type(null)      # null
print type(nil)       # null
print str(null)       # null
print null            # null
```

### When null appears

- **Explicit literal:** `null` or `nil` in source code.
- **Function with no return:** a function that exits without `return` returns `null`.
- **Missing default arguments:** parameters not provided at a call site receive `null` (which is then replaced by the default expression if one exists).
- **Accessing undefined:** accessing an undeclared variable throws an error — it does **not** return `null`.

### null is falsy

```nolqu
let x = null
if not x
  print "x is null"    # this runs
end

# or pattern — null falls through to right side
let name = null
let display = name or "anonymous"
print display    # anonymous
```

---

## 3. Function Return Behavior

Every function call produces a value.

### Explicit return

```nolqu
function add(a, b)
  return a + b
end
print add(3, 4)    # 7
```

### Implicit return (no return statement)

A function that reaches the end of its body without a `return` statement
returns `null`.

```nolqu
function greet(name)
  print "Hello, " .. name
  # no return
end

let result = greet("Alice")   # prints: Hello, Alice
print result                  # null
print type(result)            # null
```

### Early return

`return` with no expression returns `null`.

```nolqu
function validate(x)
  if x < 0
    return    # returns null
  end
  return x * 2
end

print validate(-1)   # null
print validate(5)    # 10
```

### Return in nested structures

`return` always exits the **innermost function**, not just the current block.

```nolqu
function find_first(arr, target)
  for item in arr
    if item == target
      return item   # exits the function
    end
  end
  return null       # not found
end

print find_first([3, 1, 4, 1, 5], 4)   # 4
print find_first([3, 1, 4, 1, 5], 9)   # null
```

---

## 4. Logical Operators

`and` and `or` use **short-circuit evaluation** and return the **actual value**
that determined the result, not a boolean.

### and

Returns the **first falsy operand**, or the **last operand** if all are truthy.

```
left and right
```

- If `left` is falsy → return `left` (right is not evaluated)
- If `left` is truthy → return `right`

```nolqu
print (1 and 2)         # 2      (left truthy → return right)
print (0 and 2)         # 0      (left falsy → return left)
print (false and 2)     # false
print (null and 2)      # null
print ("" and "hi")     # ""     (empty string is falsy)
print ("a" and "b")     # b
```

### or

Returns the **first truthy operand**, or the **last operand** if all are falsy.

```
left or right
```

- If `left` is truthy → return `left` (right is not evaluated)
- If `left` is falsy → return `right`

```nolqu
print (1 or 2)          # 1      (left truthy → return left)
print (0 or 2)          # 2      (left falsy → return right)
print (false or null)   # null   (both falsy → return last)
print (null or "hi")    # hi
print ("" or "default") # default
```

### Short-circuit: right side not evaluated

```nolqu
function side_effect()
  print "evaluated!"
  return 1
end

# short-circuit: side_effect() is never called
let x = false and side_effect()   # nothing printed
let y = true  or  side_effect()   # nothing printed
```

### not

`not` always returns a boolean.

```nolqu
print not false    # true
print not 0        # true
print not ""       # true
print not null     # true
print not 1        # false
print not "hi"     # false
```

### Common patterns

```nolqu
# Default value with or
let config = user_config or {}
let name   = input_name or "anonymous"

# Guard with and
let result = arr and arr[0]    # null if arr is null, else first element

# Chained and
let valid = age > 0 and age < 150 and is_str(name)
```

### Type-coercing comparison: use == not truthiness

```nolqu
# These are NOT the same:
if x             end   # truthy check (x=0 or x="" → false)
if x != null     end   # explicit null check
if x != null and x != false  end  # explicit non-null/false check

# Be explicit when the distinction matters
```

---

## 5. const Variables

`const` declares an immutable binding. The value cannot be changed after
declaration.

### Syntax

```nolqu
const PI  = 3.14159265358979
const MAX = 100
const TAG = "nolqu"
```

### Behavior

- `const` works at both global and local scope.
- Attempting to reassign a `const` is a **compile-time error** — the program
  will not run at all.
- Compound assignment (`+=`, `-=`, etc.) on a `const` is also a compile-time error.

```nolqu
const SIZE = 10
SIZE = 20     # Compile Error: Cannot reassign const variable 'SIZE'.
SIZE += 1     # Compile Error: Cannot reassign const variable 'SIZE'.
```

### const vs let

| | `let` | `const` |
|---|---|---|
| Mutable | ✓ | ✗ |
| Reassignable | ✓ | ✗ |
| Compound assign | ✓ | ✗ |
| Scope | global / local | global / local |

### const with mutable values

`const` makes the **binding** immutable, not the value.
An array declared with `const` can still have its elements modified.

```nolqu
const NUMS = [1, 2, 3]
NUMS[0] = 99          # OK — modifying element
push(NUMS, 4)         # OK — adding element
NUMS = [4, 5, 6]      # Compile Error — rebinding
```

> This means `const` on an array prevents *rebinding* the name, but does not
> prevent *mutation* of the array's contents. There is no freeze/immutable-array
> mechanism in v1.2.0. Use `const` for primitive values (`const PI = 3.14`) and
> by-convention immutability for arrays.

### Local const

```nolqu
function compute(n)
  const threshold = 100
  if n > threshold
    return n * 2
  end
  return n
end
```

---

## 6. Slice Expressions

Slicing extracts a sub-array or substring using `[start:end]` syntax.

### Syntax

```
expr[start:end]   -- start inclusive, end exclusive
expr[:end]        -- from 0 to end
expr[start:]      -- from start to end of sequence
expr[:]           -- full copy
```

### Array slices

```nolqu
let arr = [1, 2, 3, 4, 5]

print arr[1:3]    # [2, 3]       (index 1 and 2)
print arr[:2]     # [1, 2]       (first 2 elements)
print arr[3:]     # [4, 5]       (from index 3 onward)
print arr[:]      # [1, 2, 3, 4, 5]  (full copy)
```

### String slices

```nolqu
let s = "hello"

print s[1:3]      # "el"
print s[:3]       # "hel"
print s[3:]       # "lo"
print s[:]        # "hello"   (copy)
```

### Negative indices

Negative indices count from the end of the sequence: `-1` is the last element,
`-2` is second-to-last, and so on.

```nolqu
let arr = [1, 2, 3, 4, 5]
print arr[-2:]    # [4, 5]      (last 2 elements)
print arr[:-1]    # [1, 2, 3, 4]  (all but last)
print arr[-3:-1]  # [3, 4]

let s = "hello"
print s[-3:]      # "llo"
print s[:-2]      # "hel"
```

### Edge cases

```nolqu
let arr = [1, 2, 3]

print arr[2:2]    # []     (empty — start == end)
print arr[5:10]   # []     (out of range → clamped to len)
print arr[-10:]   # [1, 2, 3]  (negative beyond start → clamps to 0)
```

### Slices create new values

A slice always returns a **new** array or string — modifying the result does
not affect the original.

```nolqu
let arr = [1, 2, 3, 4]
let sub = arr[1:3]
sub[0] = 99
print arr    # [1, 2, 3, 4]   (unchanged)
print sub    # [99, 3]
```

### Slice vs index

```nolqu
let arr = [10, 20, 30]
print arr[1]      # 20        (single value — number)
print arr[1:2]    # [20]      (slice — array with one element)
```


---

## 7. Typed Errors

All runtime errors carry a **type prefix** that identifies the category of error.
Use `error_type(e)` to extract the prefix, or `startswith(e, "TypeError:")` to dispatch.

### Error types

| Type | Cause |
|---|---|
| `TypeError` | Wrong type for an operation (arithmetic on string, call a non-function, wrong arg count) |
| `NameError` | Accessing or assigning an undefined/undeclared variable |
| `IndexError` | Array or string index out of bounds |
| `ValueError` | Value is invalid for the operation (division by zero, log of negative) |
| `RuntimeError` | Catch-all for errors thrown with `error("plain message")` |

### Reading the error type

```nolqu
try
  let _x = undefined_var
catch e
  print e               # NameError: undefined variable 'undefined_var'.
  print error_type(e)   # NameError
end

try
  let arr = [1, 2, 3]
  print arr[99]
catch e
  print error_type(e)   # IndexError
end
```

### Dispatching on error type

```nolqu
try
  some_operation()
catch e
  let kind = error_type(e)
  if kind == "TypeError"
    print "Wrong type: " .. e
  else
    if kind == "IndexError"
      print "Out of bounds: " .. e
    else
      print "Error: " .. e
    end
  end
end
```

### Throwing typed errors

Use the type prefix in your own `error()` calls for consistency:

```nolqu
function divide(a, b)
  if not is_num(a) or not is_num(b)
    error("TypeError: divide() requires two numbers")
  end
  if b == 0
    error("ValueError: divide() cannot divide by zero")
  end
  return a / b
end
```

---

## Summary Table

| Concept | Rule |
|---|---|
| Falsy values | `false`, `null`/`nil`, `0`, `""` |
| Truthy values | Everything else, including `[]` and `"0"` |
| `null` vs `nil` | Identical — same value, two keywords |
| `type(null)` | `"null"` |
| No-return function | Returns `null` |
| `return` (no expr) | Returns `null` |
| `and` result | First falsy, or last (no bool coercion) |
| `or` result | First truthy, or last (no bool coercion) |
| `not` result | Always a bool |
| `bool(v)` | Coerces any value to bool using truthiness |
| `const` | Compile-time immutable binding |
| `const` array contents | Mutable (binding is const, not value) |
| `arr[a:b]` | New array/string, `a` inclusive, `b` exclusive |
| Negative slice index | Counts from end: `-1` = last |
| Out-of-range slice | Clamped to `[0, len]` — no error |
| Comparison chaining | Supported for simple middle operands; complex → compile error |
