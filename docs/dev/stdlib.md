# Nolqu Standard Library — v1.1.1a4 (Alpha)

> [!NOTE]
> **Development documentation — Nolqu v1.1.x (alpha)**
> Describes all 10 modules available in the current alpha.
> v1.0.0 stable only includes `stdlib/math`, `stdlib/array`, `stdlib/file`.
> See [`docs/stable/stdlib.md`](../stable/stdlib.md) for the stable subset.

---

## Quick Reference

| Module | Import | Key functions |
|---|---|---|
| [math](#stdlibmath) | `import "stdlib/math"` | `clamp` `lerp` `sin` `cos` `log` `gcd` `lcm` + constants |
| [array](#stdlibarray) | `import "stdlib/array"` | `map` `filter` `reduce` `sum` `flatten` `zip` `chunk` |
| [file](#stdlibfile) | `import "stdlib/file"` | `read_or_default` `write_lines` `count_lines` |
| [string](#stdlibstring) | `import "stdlib/string"` | `replace_all` `title_case` `pad_left` `center` `words` |
| [path](#stdlibpath) | `import "stdlib/path"` | `path_join` `basename` `dirname` `ext` `normalize` |
| [time](#stdlibtime) | `import "stdlib/time"` | `now` `elapsed` `sleep` `format_duration` `benchmark` |
| [json](#stdlibjson) | `import "stdlib/json"` | `json_object` `json_set` `json_get` `json_stringify` `json_parse` |
| [test](#stdlibtest) | `import "stdlib/test"` | `suite` `expect` `expect_eq` `expect_err` `done` |
| [os](#stdlibos) | `import "stdlib/os"` | `read_lines` `write_lines` `touch` `path_exists` `file_size` |
| [fmt](#stdlibfmt) | `import "stdlib/fmt"` | `fmt` `printf` `fmt_num` `fmt_pad` `fmt_table` |

---

## stdlib/math

Mathematical constants, trigonometry, logarithms, and number utilities.

### Constants

```nolqu
import "stdlib/math"

print PI    # 3.14159265358979...
print TAU   # 6.28318530717958...  (2 × PI)
print E     # 2.71828182845904...
```

### Helpers from v1.0.0

```nolqu
print clamp(15, 0, 10)     # 10
print clamp(-3, 0, 10)     # 0
print lerp(0, 100, 0.25)   # 25
print sign(-7)             # -1
print sign(0)              # 0
```

### Trigonometry (radians)

```nolqu
print sin(PI / 2)    # ≈ 1.0
print cos(0)         # ≈ 1.0
print tan(PI / 4)    # ≈ 1.0
print degrees(PI)    # ≈ 180
print radians(90)    # ≈ PI/2
```

### Logarithm

```nolqu
print log(E)         # ≈ 1.0   (natural log)
print log2(8)        # ≈ 3.0
print log10(1000)    # ≈ 3.0
```

Throws a catchable error for `log(0)` or `log(-1)`.

### Number utilities *(v1.1.1a4)*

```nolqu
print is_nan(PI)         # false
print is_nan(0)          # false
print is_inf(PI)         # false

print hypot(3, 4)        # 5.0   (Euclidean distance)
print hypot(5, 12)       # 13.0

print gcd(12, 8)         # 4
print gcd(100, 75)       # 25
print gcd(7, 13)         # 1    (coprime)

print lcm(4, 6)          # 12
print lcm(12, 18)        # 36
```

**All functions:** `clamp` · `lerp` · `sign` · `PI` · `TAU` · `E` · `sin` · `cos` · `tan` · `degrees` · `radians` · `log` · `log2` · `log10` · `is_nan` · `is_inf` · `hypot` · `gcd` · `lcm`

---

## stdlib/array

Higher-order array operations. Callbacks must be named functions.

### Classic (v1.0.0)

```nolqu
import "stdlib/array"

function double(x)
  return x * 2
end
function is_pos(x)
  return x > 0
end
function add(acc, x)
  return acc + x
end

let nums = [1, 2, 3, 4, 5]

print join(map(nums, double), ", ")      # 2, 4, 6, 8, 10
print join(filter(nums, is_pos), ", ")   # 1, 2, 3, 4, 5
print reduce(nums, add, 0)               # 15
print join(reverse(nums), ", ")          # 5, 4, 3, 2, 1
```

### Aggregation *(v1.1.1a4)*

```nolqu
print sum([1, 2, 3, 4, 5])    # 15
print sum([])                  # 0

print min_arr([3, 1, 4, 1, 5]) # 1
print max_arr([3, 1, 4, 1, 5]) # 5
```

### Predicates *(v1.1.1a4)*

```nolqu
function is_neg(x)
  return x < 0
end

print any([1, -2, 3], is_neg)     # true
print any([1,  2, 3], is_neg)     # false

print all([1, 2, 3], is_pos)      # true
print all([1,-2, 3], is_pos)      # false
print all([], is_pos)             # true  (vacuous)
```

### Structure *(v1.1.1a4)*

```nolqu
# flatten — one level deep
let f = flatten([[1, 2], [3, 4], [5]])
print join(f, ", ")     # 1, 2, 3, 4, 5
print flatten([])       # []

# unique — remove duplicates, preserve order
print join(unique([1, 2, 1, 3, 2, 4]), ", ")   # 1, 2, 3, 4

# zip — pair two arrays element-wise
let z = zip([1, 2, 3], ["a", "b", "c"])
# z = [[1,"a"], [2,"b"], [3,"c"]]
print z[0][0]   # 1
print z[0][1]   # a

# chunk — split into fixed-size groups
let c = chunk([1, 2, 3, 4, 5], 2)
# c = [[1,2], [3,4], [5]]
print len(c)       # 3
print len(c[2])    # 1  (last chunk may be smaller)
```

**All functions:** `map` · `filter` · `reduce` · `reverse` · `sum` · `min_arr` · `max_arr` · `any` · `all` · `flatten` · `unique` · `zip` · `chunk`

---

## stdlib/file

Higher-level file helpers built on the built-in file functions.

```nolqu
import "stdlib/file"

# Read file or return a default if missing
let config = read_or_default("config.txt", "")

# Write an array of strings as lines (joined with \n)
write_lines("output.txt", ["Alice", "Bob", "Carol"])

# Count lines in a file (0 if not found)
print count_lines("output.txt")    # 3
```

**Functions:** `read_or_default(path, default)` · `write_lines(path, arr)` · `count_lines(path)`

---

## stdlib/string

Extended string utilities — complements the built-in string functions.

```nolqu
import "stdlib/string"
```

### Whitespace

```nolqu
print is_empty("")          # true
print is_empty("   ")       # true
print is_empty(nil)         # true
print is_empty("hi")        # false

print lstrip("  hello  ")   # "hello  "
print rstrip("  hello  ")   # "  hello"
# trim() is a built-in (strips both sides)
```

### Splitting

```nolqu
print lines("one\ntwo\nthree")   # ["one", "two", "three"]
print words("  hello   world ")  # ["hello", "world"]
```

### Replace all

```nolqu
print replace_all("a.b.c.d", ".", "-")   # a-b-c-d
print replace_all("aabbcc", "b", "X")    # aaXXcc
# The built-in replace() only changes the first match.
```

### Count

```nolqu
print count("banana", "an")    # 2
print count("aaaa", "aa")      # 2  (non-overlapping)
print count("hello", "xyz")    # 0
```

### Padding and alignment

```nolqu
print pad_left("42", 6)        # "    42"
print pad_right("name", 10)    # "name      "
print center("hi", 8)          # "   hi   "
print truncate("Hello, World!", 8)  # "Hello..."
```

### Character utilities *(v1.1.1a4)*

```nolqu
print char_at("hello", 0)    # h
print char_at("hello", -1)   # o   (negative indexing)
# throws catchable error if out of bounds

print contains_str("hello world", "world")   # true
print contains_str("hello", "xyz")           # false

print title_case("hello world")        # Hello World
print title_case("the quick brown fox") # The Quick Brown Fox
```

**All functions:** `is_empty` · `lines` · `words` · `count` · `replace_all` · `lstrip` · `rstrip` · `pad_left` · `pad_right` · `center` · `truncate` · `char_at` · `contains_str` · `title_case`

---

## stdlib/path

String-only path manipulation — does not touch the filesystem.

```nolqu
import "stdlib/path"

print path_join("usr/local", "bin")    # usr/local/bin
print path_join("usr/local/", "bin")   # usr/local/bin  (trailing / handled)
print path_join("", "bin")             # bin

print basename("docs/readme.md")       # readme.md
print dirname("docs/readme.md")        # docs
print dirname("file.txt")              # .
print dirname("/file.txt")             # /

print ext("readme.md")                 # .md
print ext("archive.tar.gz")            # .gz
print ext("makefile")                  # ""
print ext(".bashrc")                   # ""   (hidden file, not an extension)

print strip_ext("readme.md")           # readme
print strip_ext("archive.tar.gz")      # archive.tar

print is_absolute("/usr/local")        # true
print is_absolute("./docs")            # false

print normalize("usr//local//bin")     # usr/local/bin
print normalize("docs/")              # docs
```

**Functions:** `path_join` · `basename` · `dirname` · `ext` · `strip_ext` · `is_absolute` · `normalize`

---

## stdlib/time

Time measurement and formatting.

```nolqu
import "stdlib/time"

let t0 = now()           # seconds since program start (float)
let ms = millis()        # milliseconds since program start (int)

# ... do work ...

print elapsed(t0)        # seconds since t0
print format_duration(3725)    # 1h 2m 5s
print format_duration(90)      # 1m 30s
print format_duration(45)      # 45s

# Pause (busy-wait — use short durations only)
sleep(0.1)    # ~100ms

# Time a function call
function work()
  let i = 0
  loop i < 100000
    i += 1
  end
end

benchmark("loop 100k", work)   # prints: loop 100k: 0.003s
```

**Functions:** `now()` · `millis()` · `elapsed(start)` · `sleep(seconds)` · `format_duration(secs)` · `benchmark(label, fn)`

---

## stdlib/json

JSON encode and decode. JSON objects are represented as flat `["key", val, ...]` arrays.

### Object API

```nolqu
import "stdlib/json"

let person = json_object()
json_set(person, "name", "Alice")
json_set(person, "age", 30)
json_set(person, "active", true)
json_set(person, "scores", [10, 20, 30])

print json_has(person, "name")      # true
print json_get(person, "name")      # Alice
print json_get(person, "missing")   # nil

print json_keys(person)             # ["name", "age", "active", "scores"]
print json_is_object(person)        # true
```

### Encode

```nolqu
print json_stringify(nil)           # null
print json_stringify(true)          # true
print json_stringify(42)            # 42
print json_stringify("hello")       # "hello"
print json_stringify([1, 2, 3])     # [1,2,3]
print json_stringify(person)        # {"name":"Alice","age":30,...}
```

### Decode

```nolqu
let data = json_parse("{\"name\":\"Alice\",\"age\":30}")
print json_get(data, "name")    # Alice
print json_get(data, "age")     # 30

let arr = json_parse("[1, 2, 3]")
print arr[0]    # 1

# Roundtrip
let encoded = json_stringify(person)
let decoded  = json_parse(encoded)
print json_get(decoded, "name")   # Alice
```

**Object API:** `json_object` · `json_set` · `json_get` · `json_has` · `json_keys` · `json_is_object`
**Encode:** `json_stringify`
**Decode:** `json_parse` · `json_parse_error`

---

## stdlib/test

Lightweight test framework.

```nolqu
import "stdlib/test"

suite("arithmetic")
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
  ── arithmetic ──
    [PASS] addition
    [PASS] str(42)

  ── errors ──
    [PASS] division by zero throws

  All 3 test(s) passed.
```

**Functions:** `suite(name)` · `expect(cond, label)` · `expect_eq(actual, expected, label)` · `expect_err(fn, label)` · `done()`

---

## stdlib/os

File-system utilities and environment access.

```nolqu
import "stdlib/os"

# File presence and metadata
print path_exists("config.txt")     # true / false
print file_size("data.csv")         # bytes, or -1 if missing
print is_empty_file("log.txt")      # true if exists and is 0 bytes

# Create an empty file if not present
touch("app.log")

# Line-oriented I/O (handles trailing blank line automatically)
write_lines("names.txt", ["Alice", "Bob", "Carol"])
let names = read_lines("names.txt")     # ["Alice", "Bob", "Carol"]
print len(names)                        # 3

append_line("names.txt", "Dave")
let names2 = read_lines("names.txt")   # 4 names

# Environment variables via .env/ convention
# (Write shell vars to .env/<NAME> before running nq)
let home = env_or("HOME", "/home/user")
```

**Functions:** `env_or` · `home_dir` · `path_exists` · `read_lines` · `write_lines` · `append_line` · `file_size` · `touch` · `is_empty_file`

---

## stdlib/fmt

Printf-style string formatting.

```nolqu
import "stdlib/fmt"
```

### `fmt(template, args)` — format a string

```nolqu
print fmt("Hello, {}!", ["Alice"])           # Hello, Alice!
print fmt("{} + {} = {}", [1, 2, 3])         # 1 + 2 = 3
print fmt("{0} and {0} again", ["echo"])     # echo and echo again
print fmt("{{not a placeholder}}", [])       # {not a placeholder}
```

Type specifiers:

```nolqu
print fmt("n={:d}", [42.9])        # n=42    (integer)
print fmt("v={:f}", [3.14159])     # v=3.14  (2 decimal places)
print fmt("{:s}", ["hello"])       # hello   (explicit string)
```

### `printf(template, args)` — format and print

```nolqu
printf("{} items at ${:f} each", [3, 9.99])
# 3 items at $9.99 each
```

### `fmt_num(n, decimals)` — fixed decimal places

```nolqu
print fmt_num(3.14159, 2)    # 3.14
print fmt_num(42, 3)         # 42.000
print fmt_num(0.5, 0)        # 1      (rounded)
```

### `fmt_pad(s, width, char)` — left-pad to width

```nolqu
print fmt_pad("42", 6, " ")    #     42
print fmt_pad("42", 6, "0")    # 000042
```

### `fmt_table(rows, col_widths)` — plain-text table

```nolqu
let rows = [
  ["Name",  "Score", "Grade"],
  ["-----", "-----", "-----"],
  ["Alice", "98",    "A"],
  ["Bob",   "74",    "B"]
]
print fmt_table(rows, [10, 7, 6])
```

```
Name      Score  Grade
-----     -----  -----
Alice     98     A
Bob       74     B
```

**Functions:** `fmt` · `printf` · `fmt_num` · `fmt_pad` · `fmt_table`

---

## Writing Your Own Modules

Any `.nq` file can be used as a module. Everything declared at the top level
becomes globally available after import.

**utils.nq:**
```nolqu
function clamp_str(s, max_len)
  if len(s) <= max_len
    return s
  end
  return slice(s, 0, max_len - 3) .. "..."
end
```

**main.nq:**
```nolqu
import "utils"

print clamp_str("Hello, World!", 8)    # Hello...
```

> **Note:** Import paths are relative to the working directory where `nq` is
> invoked, not to the script file itself.
