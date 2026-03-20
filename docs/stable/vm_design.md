# Nolqu VM Design — v1.2.1 (Stable)

> [!NOTE]
> **Stable documentation — Nolqu v1.0.0**
> For the latest architecture including for-in desugaring and 16-bit constant
> pool see [`docs/dev/vm_design.md`](../dev/vm_design.md).

---

## Pipeline

```
source.nq → Lexer → Parser → AST → Compiler → Bytecode → VM → output
```

## Architecture: C vs C++

| Layer | Files | Standard |
|---|---|---|
| Runtime core | `memory.c` `value.c` `chunk.c` `object.c` `table.c` `gc.c` `vm.c` | C11 |
| Tooling | `lexer.cpp` `ast.cpp` `parser.cpp` `compiler.cpp` `repl.cpp` `main.cpp` `nq_embed.cpp` | C++17 |

## Value Representation

```c
typedef struct {
    ValueType type;   /* VAL_NIL | VAL_BOOL | VAL_NUMBER | VAL_OBJ */
    union { bool boolean; double number; Obj* obj; } as;
} Value;
```

## VM State

```c
struct VM {
    CallFrame  frames[64];   /* call stack         */
    Value      stack[512];   /* operand stack      */
    TryHandler try_stack[64];/* error handlers     */
    Table      globals;      /* global variables   */
    Value      thrown;       /* pending error      */
};
```

## Garbage Collector

Mark-and-sweep, triggered when `nq_bytes_allocated > nq_gc_threshold` (1 MB initial).

1. **Mark** — stack, frames, globals, thrown value
2. **Prune** — remove dead strings from intern table
3. **Sweep** — free all unmarked objects
4. **Threshold** — `max(2 × surviving bytes, 1 MB)`

## Instruction Set (37 opcodes, v1.0.0)

Constants, stack ops, globals, locals, arithmetic, comparison, concat,
control flow (jump/loop), arrays, error handling (try/throw), functions, halt.

**v1.0.0 constant pool:** 8-bit index, max **256** constants per function.
(Raised to 16-bit / max 65535 in v1.2.0.)

## Limits

| Resource | v1.0.0 | v1.2.0 |
|---|---|---|
| Operand stack | 512 | 512 |
| Call frames | 64 | 64 |
| Try/catch nesting | 64 | 64 |
| Locals per function | 256 | 256 |
| **Constants per chunk** | **256** | **65535** |
| Host-registered natives | 16 | 64 |

For full details see [`docs/dev/vm_design.md`](../dev/vm_design.md).
