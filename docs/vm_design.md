# Nolqu VM Design — v1.0.0

Internal architecture reference for contributors and embedders.

For the language reference see [language.md](language.md).
For the formal grammar see [grammar.md](grammar.md).
For the C/C++ embedding API see [embedding.md](embedding.md).

---

## Pipeline Overview

```
source.nq
    │
    ▼ Lexer (lexer.cpp)
    │   Scans characters → flat Token stream
    │   Tokens point directly into the source buffer (no copies)
    │
    ▼ Parser (parser.cpp)
    │   Recursive-descent, LL(1), Pratt-style expression precedence
    │   Produces heap-allocated ASTNode tree
    │
    ▼ Compiler (compiler.cpp)
    │   Tree-walk, single-pass
    │   Emits bytecode into an ObjFunction (top-level "script" function)
    │   Resolves locals at compile time (stack slots)
    │   Emits unused-variable warnings
    │
    ▼ VM (vm.c)
        Stack-based bytecode interpreter
        Dispatches opcodes in a switch/goto loop
        Runs natively compiled C code for built-ins
```

---

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
    ValueType type;     /* VAL_NIL | VAL_BOOL | VAL_NUMBER | VAL_OBJ */
    union {
        bool   boolean;
        double number;
        Obj*   obj;
    } as;
} Value;
```

All Nolqu numbers are 64-bit IEEE 754 doubles.  
Heap-allocated types (strings, functions, arrays) are represented as `VAL_OBJ`
pointing to a tagged `Obj` base header.

---

## Heap Object Types

```
Obj (base header)
  ├── ObjString   — interned, immutable, FNV-1a hash
  ├── ObjFunction — owns a Chunk (bytecode + constants + line table)
  ├── ObjNative   — function pointer to a C built-in
  └── ObjArray    — dynamic Value array (GROW_ARRAY growth policy)
```

**String interning:** every string passes through `tableFindString` on creation.
Identical byte sequences share one heap object → equality is O(1) pointer compare.

---

## Memory Management

All allocations go through one function:

```c
void* nq_realloc(void* ptr, size_t old_size, size_t new_size);
```

- Allocate: `ptr=NULL, old_size=0, new_size>0`
- Resize: `ptr!=NULL, old_size>0, new_size>0`
- Free: `ptr!=NULL, old_size>0, new_size=0`

`nq_bytes_allocated` tracks every byte. The GC fires when it exceeds
`nq_gc_threshold` (starts at 1 MB; doubles after each collection).

**Rule:** Never call `malloc`/`realloc`/`free` directly in the runtime.
Use `nq_realloc`, `NQ_ALLOC`, `NQ_FREE`, `GROW_ARRAY`, or `FREE_ARRAY`.

---

## Garbage Collector

Mark-and-sweep, triggered before each object allocation via `nq_gc_maybe()`.

**Phases:**

1. **Mark** — `markRoots(vm)` walks:
   - All values on the operand stack (`vm->stack`)
   - All `ObjFunction*` in active call frames
   - All entries in `vm->globals` (hash table)
   - `vm->thrown` (pending error value)

2. **Prune strings** — `removeWhiteStrings()` rebuilds the intern table,
   keeping only marked strings. Must run before sweep so sweep can safely free them.

3. **Sweep** — walk `nq_all_objects` linked list; free unmarked objects.

4. **Adjust threshold** — `nq_gc_threshold = max(nq_bytes_allocated * 2, 1 MB)`

---

## VM State

```c
struct VM {
    CallFrame  frames[NQ_FRAMES_MAX];  /* call stack, max 64 frames    */
    int        frame_count;

    Value      stack[NQ_STACK_MAX];    /* operand stack, max 512 slots */
    Value*     stack_top;              /* one past top element         */

    TryHandler try_stack[NQ_TRY_MAX];  /* try/catch state, max 64      */
    int        try_depth;

    Value      thrown;                 /* pending error value          */
    Table      globals;                /* global variable hash table   */
    const char* source_path;           /* for error messages           */
};
```

### Call Frame

```c
typedef struct {
    ObjFunction* function;  /* function being executed      */
    uint8_t*     ip;        /* instruction pointer          */
    Value*       slots;     /* base of frame on value stack */
} CallFrame;
```

Local variable at index `N` is `frame->slots[N]` — directly on the value stack.
No separate local-variable array; locals and temporaries share the same stack.

### Try Handler

```c
typedef struct {
    uint8_t* catch_ip;      /* jump here on error           */
    Value*   stack_top;     /* stack level to restore       */
    int      frame_count;   /* frame depth to restore       */
} TryHandler;
```

`OP_TRY` pushes a handler; `OP_TRY_END` pops it (normal exit); any thrown error
pops the handler and jumps to `catch_ip` with the error value on the stack.

---

## Instruction Set (37 opcodes)

All opcodes are 1 byte. Operands follow immediately.

### Literals
| Opcode | Operands | Stack effect |
|---|---|---|
| `OP_CONST` | `[hi, lo]` | `→ constants[idx]` — 16-bit index (max 65535) |
| `OP_NIL` | — | `→ nil` |
| `OP_TRUE` | — | `→ true` |
| `OP_FALSE` | — | `→ false` |

### Stack
| Opcode | Operands | Stack effect |
|---|---|---|
| `OP_POP` | — | `a →` |
| `OP_DUP` | — | `a → a a` |

### Variables
| Opcode | Operands | Stack effect |
|---|---|---|
| `OP_DEFINE_GLOBAL` | `[name_idx]` | `a → (globals[name]=a)` |
| `OP_GET_GLOBAL` | `[name_idx]` | `→ globals[name]` |
| `OP_SET_GLOBAL` | `[name_idx]` | `a → a (globals[name]=a)` |
| `OP_GET_LOCAL` | `[slot]` | `→ stack[base+slot]` |
| `OP_SET_LOCAL` | `[slot]` | `a → a (stack[base+slot]=a)` |

### Arithmetic
| Opcode | Stack effect | Notes |
|---|---|---|
| `OP_ADD` | `b a → a+b` | numbers only |
| `OP_SUB` | `b a → a-b` | |
| `OP_MUL` | `b a → a*b` | |
| `OP_DIV` | `b a → a/b` | throws on b==0 |
| `OP_MOD` | `b a → a%b` | throws on b==0 |
| `OP_NEGATE` | `a → -a` | |
| `OP_CONCAT` | `b a → a..b` | auto-converts to string |

### Comparison
| Opcode | Stack effect |
|---|---|
| `OP_EQ` `OP_NEQ` | `b a → bool` |
| `OP_LT` `OP_GT` `OP_LTE` `OP_GTE` | `b a → bool` (numbers only) |
| `OP_NOT` | `a → !a` |

### Control Flow (16-bit offset operands)
| Opcode | Operands | Behaviour |
|---|---|---|
| `OP_JUMP` | `[hi, lo]` | `ip += offset` |
| `OP_JUMP_IF_FALSE` | `[hi, lo]` | if `!peek`: `ip += offset`; pops condition |
| `OP_LOOP` | `[hi, lo]` | `ip -= offset` |

### Arrays
| Opcode | Operands | Stack effect |
|---|---|---|
| `OP_BUILD_ARRAY` | `[count]` | pop `count` values → push array |
| `OP_GET_INDEX` | — | `idx arr → arr[idx]` |
| `OP_SET_INDEX` | — | `val idx arr →` (mutates `arr`) |

### Error Handling (16-bit offset for `OP_TRY`)
| Opcode | Operands | Behaviour |
|---|---|---|
| `OP_TRY` | `[hi, lo]` | push TryHandler; `catch_ip = ip + offset` |
| `OP_TRY_END` | — | pop TryHandler (normal exit) |
| `OP_THROW` | — | pop error; seek nearest handler or abort |

### Functions
| Opcode | Operands | Behaviour |
|---|---|---|
| `OP_CALL` | `[argc]` | call `stack[-argc-1]` with `argc` args |
| `OP_RETURN` | — | pop result; restore frame |
| `OP_HALT` | — | stop execution |

---

## Compiler: Locals vs Globals

```
scope_depth == 0    → variable is global → OP_DEFINE_GLOBAL / OP_GET_GLOBAL
scope_depth >  0    → variable is local  → OP_GET_LOCAL / OP_SET_LOCAL (slot index)
```

Local slots are allocated sequentially in `CompilerCtx.locals[]`.
At `endScope()` the compiler emits `OP_POP` for each local that falls out of scope
and emits a warning for any that were never used (unless prefixed with `_`).

---

## Error Propagation

```
Native function calls error() or assert() fails
    │
    ├── sets g_vm_for_error->thrown
    │
VM dispatch checks vm->thrown after every native call
    │
    ├── try_depth > 0 → throwError(): restore stack/frames, jump to catch_ip
    │
    └── try_depth == 0 → vmRuntimeError(): print trace, return INTERPRET_RUNTIME_ERROR
```

The `THROW_ERROR(fmt, ...)` macro in `runVM` handles VM-internal errors
(division by zero, type mismatch, etc.) the same way.

---

## String Interning Table

Open-addressing hash set of `ObjString*`, load factor ≤ 75%.

- `copyString(chars, len)` — always interns; returns existing if found
- `takeString(chars, len)` — takes ownership of a `nq_realloc`'d buffer; interns it
- GC hook: `removeWhiteStrings()` rebuilds the table keeping only marked strings

**Important:** buffers passed to `takeString` must be allocated via `nq_realloc`,
not `malloc`, so the memory tracker stays accurate.

---


## Compiler: `for-in` Desugaring

```nolqu
for item in iterable
  body
end
```

Is compiled as if written:

```
[outer scope]
  let __for_arr__ = <iterable>    ← evaluated once
  let __for_idx__ = 0             ← hidden index
  loop __for_idx__ < len(__for_arr__)
    [inner scope]
    let <item> = __for_arr__[__for_idx__]
    <body>
    __for_idx__ = __for_idx__ + 1
  end
[end outer scope]
```

Key invariants:
- `__for_arr__` and `__for_idx__` are hidden locals (compiler-generated names).
- `break` pops all locals back to before `__for_arr__` (`outer_local_count`).
- `continue` jumps to the increment step (forward-patched at emit time).
- After a `break`, the outer scope's `endScope` pops `__for_arr__` and `__for_idx__` normally.

## "Did You Mean?" Suggestions

When `OP_GET_GLOBAL` fails (undefined variable), the VM runs a linear scan of
`vm->globals`, computing Levenshtein distance (two-row DP, max length 32, max
edit distance 3) against every key. The closest match is shown in the error:

```
Undefined variable 'lenght'. Did you mean 'length'?
```

---

## Limits Summary

| Resource | Limit |
|---|---|
| Operand stack | 512 values |
| Call frames | 64 |
| Try/catch nesting | 64 |
| Locals per function | 256 |
| Constants per chunk | 65535 (16-bit index) |
| Jump offset | 65535 bytes |
| Host-registered natives (`nq_register`) | 64 |
