# Nolqu Standard Library — v1.0.0 (Stable)

> [!NOTE]
> **Stable documentation — Nolqu v1.0.0**
> For the latest alpha stdlib see [`docs/dev/stdlib.md`](../dev/stdlib.md).

Three modules are included in the stable release.

---

## stdlib/math

```nolqu
import "stdlib/math"

print clamp(15, 0, 10)     # 10
print lerp(0, 100, 0.5)    # 50
print sign(-7)             # -1
```

**Functions:** `clamp(val, lo, hi)` · `lerp(a, b, t)` · `sign(n)`

> **v1.1.x adds:** `PI` `TAU` `E` · trig (`sin` `cos` `tan` `degrees` `radians`) · log (`log` `log2` `log10`) · `is_nan` `is_inf` `hypot` `gcd` `lcm`

---

## stdlib/array

```nolqu
import "stdlib/array"

function double(x)
  return x * 2
end

let nums = [1, 2, 3, 4, 5]
print join(map(nums, double), ", ")   # 2, 4, 6, 8, 10
```

**Functions:** `map(arr, fn)` · `filter(arr, fn)` · `reduce(arr, fn, init)` · `reverse(arr)`

> **v1.1.x adds:** `sum` `min_arr` `max_arr` `any` `all` `flatten` `unique` `zip` `chunk`

---

## stdlib/file

```nolqu
import "stdlib/file"

let content = read_or_default("config.txt", "")
write_lines("output.txt", ["line1", "line2"])
print count_lines("output.txt")   # 2
```

**Functions:** `read_or_default(path, default)` · `write_lines(path, arr)` · `count_lines(path)`

---

## New in v1.1.x

The following modules are **not** part of v1.0.0 stable:

| Module | Added in |
|---|---|
| `stdlib/time` | v1.1.0 |
| `stdlib/string` | v1.1.0 |
| `stdlib/path` | v1.1.0 |
| `stdlib/json` | v1.1.0 |
| `stdlib/test` | v1.1.0 |
| `stdlib/os` | v1.1.1a2 |
| `stdlib/fmt` | v1.1.1a2 |
