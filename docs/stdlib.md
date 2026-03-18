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

---

## stdlib/time

Time measurement utilities.

```nolqu
import "stdlib/time"

let t0 = now()
# ... do work ...
print "elapsed: " .. elapsed(t0) .. "s"
print "millis:  " .. millis()

sleep(0.1)   # pause ~100ms

print format_duration(3725)   # 1h 2m 5s

function work()
  let i = 0
  loop i < 100000
    i = i + 1
  end
end
benchmark("loop 100k", work)   # loop 100k: 0.003s
```

**Functions:** `now()` · `millis()` · `elapsed(start)` · `sleep(seconds)` · `format_duration(secs)` · `benchmark(label, fn)`

---

## stdlib/string

Extended string utilities (complements the built-ins).

```nolqu
import "stdlib/string"

print is_empty("")           # true
print is_empty("  ")         # true

print lines("a\nb\nc")       # ["a", "b", "c"]
print words("  hello world ") # ["hello", "world"]

print count("banana", "an")  # 2
print replace_all("a.b.c", ".", "-")  # a-b-c

print pad_left("42", 6)      # "    42"
print pad_right("name", 10)  # "name      "
print center("hi", 8)        # "   hi   "
print truncate("Hello, World!", 8)   # "Hello..."
```

**Functions:** `is_empty` · `lines` · `words` · `count` · `replace_all` · `lstrip` · `rstrip` · `pad_left` · `pad_right` · `center` · `truncate`  
**New in v1.1.1a4:** `char_at(s,i)` · `contains_str(s,sub)` · `title_case(s)`

---

## stdlib/path

File path manipulation (string-only, does not touch the filesystem).

```nolqu
import "stdlib/path"

print path_join("usr/local", "bin")    # usr/local/bin
print basename("docs/readme.md")       # readme.md
print dirname("docs/readme.md")        # docs
print ext("main.nq")                   # .nq
print strip_ext("main.nq")             # main
print is_absolute("/usr/bin")          # true
print normalize("a//b///c/")           # a/b/c
```

**Functions:** `path_join` · `basename` · `dirname` · `ext` · `strip_ext` · `is_absolute` · `normalize`

---

## stdlib/json

JSON encode and decode using a flat array object representation.

```nolqu
import "stdlib/json"

let person = json_object()
json_set(person, "name", "Alice")
json_set(person, "age", 30)
json_set(person, "scores", [10, 20, 30])

print json_stringify(person)
# {"name":"Alice","age":30,"scores":[10,20,30]}

let data = json_parse("{\"city\":\"Jakarta\",\"pop\":10000000}")
print json_get(data, "city")    # Jakarta
print json_get(data, "pop")     # 10000000
```

**Object API:** `json_object` · `json_set` · `json_get` · `json_has` · `json_keys` · `json_is_object`  
**Encoding:** `json_stringify`  
**Decoding:** `json_parse` · `json_parse_error`

---

## stdlib/math (extended)

In addition to `clamp`, `lerp`, `sign` — the math module now includes:

```nolqu
import "stdlib/math"

# Constants
print PI      # 3.14159...
print E       # 2.71828...
print TAU     # 6.28318...

# Trigonometry (radians)
print sin(PI / 2)    # ~1.0
print cos(0)         # ~1.0
print tan(PI / 4)    # ~1.0
print degrees(PI)    # ~180
print radians(90)    # ~PI/2

# Logarithm
print log(E)         # ~1.0
print log2(8)        # ~3.0
print log10(1000)    # ~3.0
```

**New in v1.1.0:** `PI` · `TAU` · `E` · `sin` · `cos` · `tan` · `degrees` · `radians` · `log` · `log2` · `log10`  
**New in v1.1.1a4:** `is_nan(n)` · `is_inf(n)` · `hypot(a,b)` · `gcd(a,b)` · `lcm(a,b)`

---

## stdlib/test

Lightweight test framework.

```nolqu
import "stdlib/test"

suite("math")
expect(1 + 1 == 2,     "addition")
expect_eq(str(42), "42", "str(42)")

suite("errors")
function div_zero()
  let _x = 1 / 0
end
expect_err(div_zero, "division by zero throws")

done()
```

Output:
```
  ── math ──
    [PASS] addition
    [PASS] str(42)

  ── errors ──
    [PASS] division by zero throws

  All 3 test(s) passed.
```

**Functions:** `suite(name)` · `expect(cond, label)` · `expect_eq(actual, expected, label)` · `expect_err(fn, label)` · `done()`

---

## stdlib/os

OS-level utilities: file existence, line-oriented I/O, environment variables via `.env/`.

```nolqu
import "stdlib/os"

# File operations
touch("app.log")
assert(path_exists("app.log"), "exists")
assert(is_empty_file("app.log"), "empty")

write_lines("names.txt", ["Alice", "Bob", "Carol"])
let names = read_lines("names.txt")   # strips trailing blank line
print len(names)   # 3

append_line("names.txt", "Dave")
print file_size("names.txt")   # bytes

# Environment (via .env/ directory convention)
let home = env_or("HOME", "/home/user")
```

**Functions:** `env_or` · `home_dir` · `path_exists` · `read_lines` · `write_lines` · `append_line` · `file_size` · `touch` · `is_empty_file`

---

## stdlib/fmt

Printf-style string formatting.

```nolqu
import "stdlib/fmt"

# {} positional placeholders
print fmt("Hello, {}!", ["Alice"])          # Hello, Alice!
print fmt("{} + {} = {}", [1, 2, 3])        # 1 + 2 = 3

# Indexed placeholders
print fmt("{0} and {0}", ["echo"])           # echo and echo

# Type specifiers
print fmt("Score: {:d}", [99.9])            # Score: 99
print fmt("Pi: {:f}", [3.14159])            # Pi: 3.14

# Escape braces
print fmt("{{not a placeholder}}", [])      # {not a placeholder}

# fmt_num — fixed decimal places
print fmt_num(3.14159, 2)    # 3.14
print fmt_num(42, 3)         # 42.000

# fmt_pad — left-pad to width
print fmt_pad("42", 6, " ")  #     42
print fmt_pad("42", 6, "0")  # 000042

# printf — fmt + print
printf("{} items at ${:f} each", [3, 9.99])
```

**Functions:** `fmt(template, args)` · `printf(template, args)` · `fmt_num(n, decimals)` · `fmt_pad(s, width, char)` · `fmt_table(rows, col_widths)`
