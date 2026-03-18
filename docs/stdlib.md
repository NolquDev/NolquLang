# Nolqu Standard Library — v1.0.0

The standard library consists of three `.nq` modules in the `stdlib/` directory.
Import them with:

```nolqu
import "stdlib/math"
import "stdlib/array"
import "stdlib/file"
```

---

## stdlib/math

Mathematical helper functions.

### `clamp(val, lo, hi)`

Constrain `val` to the range `[lo, hi]`.

```nolqu
import "stdlib/math"

print clamp(15, 0, 10)      # 10  (clamped to hi)
print clamp(-3, 0, 10)      # 0   (clamped to lo)
print clamp(5,  0, 10)      # 5   (unchanged)
```

### `lerp(a, b, t)`

Linear interpolation between `a` and `b` by factor `t`.
`t = 0` returns `a`, `t = 1` returns `b`.

```nolqu
import "stdlib/math"

print lerp(0, 100, 0.0)     # 0
print lerp(0, 100, 0.5)     # 50
print lerp(0, 100, 1.0)     # 100
print lerp(10, 20, 0.25)    # 12.5
```

### `sign(n)`

Return `1` if `n > 0`, `-1` if `n < 0`, `0` if `n == 0`.

```nolqu
import "stdlib/math"

print sign(42)      # 1
print sign(-7)      # -1
print sign(0)       # 0
```

---

## stdlib/array

Higher-order array operations. Callbacks must be named functions (no anonymous functions in v1.0.0).

### `map(arr, fn)`

Apply `fn` to each element, return a new array with the results.

```nolqu
import "stdlib/array"

function double(x)
  return x * 2
end

let nums    = [1, 2, 3, 4, 5]
let doubled = map(nums, double)
print join(doubled, ", ")       # 2, 4, 6, 8, 10
```

### `filter(arr, fn)`

Return a new array containing only elements for which `fn` returns truthy.

```nolqu
import "stdlib/array"

function is_even(x)
  return x % 2 == 0
end

let nums  = [1, 2, 3, 4, 5, 6]
let evens = filter(nums, is_even)
print join(evens, ", ")          # 2, 4, 6
```

### `reduce(arr, fn, initial)`

Fold the array left-to-right. `fn(accumulator, element)` is called for each item
starting from `initial`.

```nolqu
import "stdlib/array"

function add(acc, x)
  return acc + x
end

let nums = [1, 2, 3, 4, 5]
let sum  = reduce(nums, add, 0)
print sum                         # 15

function longest(acc, s)
  if len(s) > len(acc)
    return s
  end
  return acc
end

let words  = ["cat", "elephant", "dog", "rhinoceros"]
let result = reduce(words, longest, "")
print result                      # rhinoceros
```

### `reverse(arr)`

Return a new array with elements in reversed order. Does not mutate the original.

```nolqu
import "stdlib/array"

let a = [1, 2, 3, 4, 5]
let r = reverse(a)
print join(r, ", ")     # 5, 4, 3, 2, 1
print join(a, ", ")     # 1, 2, 3, 4, 5  (original unchanged)
```

---

## stdlib/file

Higher-level file helpers built on top of the built-in file functions.

### `read_or_default(path, default_val)`

Return the contents of `path` as a string. If the file does not exist or
cannot be read, return `default_val` instead.

```nolqu
import "stdlib/file"

let config = read_or_default("config.txt", "")
if config == ""
  print "No config found, using defaults"
else
  print "Config loaded: " .. config
end
```

### `write_lines(path, arr)`

Write an array of strings to `path`, one per line (joined with `\n`).
Returns `true` on success, `false` on failure.

```nolqu
import "stdlib/file"

let lines = ["Alice", "Bob", "Charlie"]
write_lines("names.txt", lines)

# Equivalent to:
# file_write("names.txt", join(lines, "\n"))
```

### `count_lines(path)`

Return the number of lines in `path`. Returns `0` if the file does not exist.

```nolqu
import "stdlib/file"

let n = count_lines("names.txt")
print "File has " .. n .. " lines"
```

---

## Writing Your Own Modules

Any `.nq` file can be used as a module. Declare top-level functions and variables
in the module file; they become available globally after import.

**utils.nq:**
```nolqu
# Utility functions

function clamp_str(s, max_len)
  if len(s) <= max_len
    return s
  end
  return slice(s, 0, max_len) .. "..."
end

function pad_left(s, width)
  loop len(s) < width
    s = " " .. s
  end
  return s
end
```

**main.nq:**
```nolqu
import "utils"

print clamp_str("Hello, World!", 5)     # Hello...
print pad_left("42", 6)                 # "    42"
```

> **Note:** Import paths are relative to the working directory where `nq` is
> invoked, not to the script file itself.
