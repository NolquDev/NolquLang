# Nolqu VM Design — v1.2.0

> [!NOTE]
> **Nolqu v1.2.0 — Stable**
> For the v1.0.0 internals see [`docs/stable/vm_design.md`](../stable/vm_design.md).

Internal architecture reference for contributors and embedders.
For the language reference see [language.md](language.md).
For the formal grammar see [grammar.md](grammar.md).
For the C/C++ embedding API see [embedding.md](embedding.md).

---

## Pipeline

```
source.nq
    │
    ▼ Lexer (lexer.cpp)         — source text → Token stream
    ▼ Parser (parser.cpp)       — Token stream → AST
    ▼ Compiler (compiler.cpp)   — AST → ObjFunction (bytecode)
    ▼ VM (vm.c)                 — executes bytecode
```

## Source Layout: C vs C++

| Layer | Files | Compiled as |
|---|---|---|
| Runtime core | `memory.c` `value.c` `chunk.c` `object.c` `table.c` `gc.c` `vm.c` | C11 |
| Tooling | `lexer.cpp` `ast.cpp` `parser.cpp` `compiler.cpp` `codegen.cpp` `repl.cpp` `main.cpp` `nq_embed.cpp` | C++17 |

All public headers carry `extern "C"` guards for correct interoperability.

---

## Value Representation

```c
typedef struct {
    ValueType type;   /* VAL_NIL | VAL_BOOL | VAL_NUMBER | VAL_OBJ */
    union {
        bool   boolean;
        double number;
        Obj*   obj;
    } as;
} Value;
```

All Nolqu numbers are 64-bit IEEE 754 doubles.
Heap-allocated types point to a tagged `Obj` base header.

---

## Heap Object Types

```
Obj (base header: type, marked, next)
  ├── ObjString   — immutable, interned, FNV-1a hash cached
  ├── ObjFunction — owns a Chunk (bytecode + constants + line table)
  ├── ObjNative   — C function pointer + name + arity
  └── ObjArray    — dynamic Value[] with GROW_ARRAY growth
```

---

## Memory Management

All allocations route through:

```c
void* nq_realloc(void* ptr, size_t old_size, size_t new_size);
```

`nq_bytes_allocated` tracks every byte. GC fires when it exceeds
`nq_gc_threshold` (starts at 1 MB; doubles after each collection).

**Rule:** Never call `malloc`/`realloc`/`free` directly. Always use
`nq_realloc`, `NQ_ALLOC`, `NQ_FREE`, `GROW_ARRAY`, or `FREE_ARRAY`.

---

## Garbage Collector

Mark-and-sweep, triggered before each allocation via `nq_gc_maybe()`.

1. **Mark** — walk VM stack, call frames, globals, `vm->thrown`
2. **Prune strings** — rebuild intern table keeping only marked strings
3. **Sweep** — free all unmarked objects; reset mark bits
4. **Threshold** — `nq_gc_threshold = max(2 × surviving, 1 MB)`

---

## VM State

```c
struct VM {
    CallFrame  frames[NQ_FRAMES_MAX];  /* call stack, max 64        */
    int        frame_count;

    Value      stack[NQ_STACK_MAX];   /* operand stack, max 512     */
    Value*     stack_top;

    TryHandler try_stack[NQ_TRY_MAX]; /* error handlers, max 64     */
    int        try_depth;

    Value      thrown;                /* pending error value         */
    Table      globals;               /* global variable hash table  */
    const char* source_path;
};
```

### Call Frame

```c
typedef struct {
    ObjFunction* function;
    uint8_t*     ip;      /* instruction pointer       */
    Value*       slots;   /* base of frame on stack    */
} CallFrame;
```

### Try Handler

```c
typedef struct {
    uint8_t* catch_ip;    /* jump here on error        */
    Value*   stack_top;   /* stack level to restore    */
    int      frame_count; /* frame depth to restore    */
} TryHandler;
```

---

## Instruction Set (37 opcodes)

### Constants and Stack

| Opcode | Operands | Effect |
|---|---|---|
| `OP_CONST` | `[hi, lo]` | push `constants[idx]` — **16-bit index** |
| `OP_NIL` | — | push nil |
| `OP_TRUE` / `OP_FALSE` | — | push bool |
| `OP_POP` | — | discard top |
| `OP_DUP` | — | duplicate top |

### Variables

| Opcode | Operands | Effect |
|---|---|---|
| `OP_DEFINE_GLOBAL` | `[hi, lo]` | pop → globals[name] |
| `OP_GET_GLOBAL` | `[hi, lo]` | push globals[name]; throws if missing |
| `OP_SET_GLOBAL` | `[hi, lo]` | peek → globals[name]; throws if undeclared |
| `OP_GET_LOCAL` | `[slot]` | push stack[base+slot] |
| `OP_SET_LOCAL` | `[slot]` | peek → stack[base+slot] |

### Arithmetic & Logic

| Opcode | Effect |
|---|---|
| `OP_ADD` `OP_SUB` `OP_MUL` | numbers only; `ADD` also used in `for-in` index increment |
| `OP_DIV` `OP_MOD` | throws catchable error on division by zero |
| `OP_NEGATE` | unary minus; throws on non-number |
| `OP_CONCAT` | auto-converts both sides to string |
| `OP_EQ` `OP_NEQ` | any types |
| `OP_LT` `OP_GT` `OP_LTE` `OP_GTE` | numbers only; throws on non-number |
| `OP_NOT` | logical not |

### Control Flow (16-bit offset operands)

| Opcode | Effect |
|---|---|
| `OP_JUMP` | `ip += offset` |
| `OP_JUMP_IF_FALSE` | if `!peek`: `ip += offset`; pops condition |
| `OP_LOOP` | `ip -= offset` |

### Arrays

| Opcode | Effect |
|---|---|
| `OP_BUILD_ARRAY [count]` | pop `count` values → push array |
| `OP_GET_INDEX` | `idx arr → arr[idx]` |
| `OP_SET_INDEX` | `val idx arr →` mutates arr |

### Error Handling

| Opcode | Effect |
|---|---|
| `OP_TRY [hi, lo]` | push TryHandler; catch_ip = ip + offset |
| `OP_TRY_END` | pop TryHandler (normal exit) |
| `OP_THROW` | pop error; seek handler or abort |

### Functions

| Opcode | Effect |
|---|---|
| `OP_CALL [argc]` | call stack[-argc-1] with argc args |
| `OP_RETURN` | return top of stack |
| `OP_HALT` | stop execution |

---

## `for-in` Desugaring

```nolqu
for item in iterable
  body
end
```

The compiler emits this as if it were written:

```
[outer scope]
  let __for_arr__ = <iterable>    ← evaluated once
  let __for_idx__ = 0
  loop __for_idx__ < len(__for_arr__)
    [inner scope]
    let <item> = __for_arr__[__for_idx__]
    <body>
    <continue patches land here>
    __for_idx__ += 1
  end
  <break patches land here>
[end outer scope]
```

**Invariants:**

- `break` pops all locals back to before `__for_arr__` (`outer_local_count`) — including the hidden locals.
- `continue` forward-jumps to the increment step via patches set during body compilation.
- After a `break`, the outer `endScope` still emits its POPs — but they run only on the normal exit path.
- `break` and `continue` patches for the `for` loop land in the correct positions because `LoopCtx` tracks both `outer_local_count` (for break) and `continue_patches` (forward jumps for continue).

---

## Compiler: Locals vs Globals

```
scope_depth == 0 → global  → OP_DEFINE_GLOBAL / OP_GET_GLOBAL / OP_SET_GLOBAL
scope_depth >  0 → local   → OP_GET_LOCAL / OP_SET_LOCAL (slot index)
```

At `endScope()`, the compiler emits `OP_POP` for each local that falls out of
scope and warns about unused locals (unless prefixed with `_`).

## LoopCtx in the Compiler

```cpp
struct LoopCtx {
    int               loop_start;       /* bytecode offset of condition */
    int               continue_target;  /* -1 = use patches (for-in)    */
    int               scope_depth;      /* scope at loop entry          */
    int               local_count;      /* locals at body entry         */
    int               outer_local_count;/* locals before loop scope     */
    std::vector<int>  break_patches;    /* forward jumps for break      */
    std::vector<int>  continue_patches; /* forward jumps for continue   */
};
```

---

## Error Propagation

```
THROW_ERROR(fmt, ...) macro
    │
    ├─ try_depth > 0 → throwError(): restore stack/frames, jump to catch_ip
    │
    └─ try_depth == 0 → vmRuntimeError(): print trace, return INTERPRET_RUNTIME_ERROR
```

Native functions set `g_vm_for_error->thrown`; the dispatch loop checks this
after every native call.

---

## "Did You Mean?" Suggestions

On `OP_GET_GLOBAL` failure, the VM scans `vm->globals` computing Levenshtein
distance (two-row DP, max length 32, max distance 3) against every key.

---

## String Interning

All strings pass through `copyString()` → `tableFindString()`.
Identical byte sequences share one heap object — equality is O(1) pointer compare.

`removeWhiteStrings()` in the GC rebuilds the intern table before sweep,
keeping only marked strings. All allocations use `NQ_ALLOC`/`FREE_ARRAY`
(not `calloc`/`free`) so the byte counter stays accurate.

---

## Limits

| Resource | v1.0.0 | v1.2.0 |
|---|---|---|
| Operand stack | 512 | 512 |
| Call frames | 64 | 64 |
| Try/catch nesting | 64 | 64 |
| Locals per function | 256 | 256 |
| **Constants per chunk** | **256 (8-bit)** | **65535 (16-bit)** |
| Host-registered natives | 16 | 64 |
| Jump offset | 65535 bytes | 65535 bytes |
