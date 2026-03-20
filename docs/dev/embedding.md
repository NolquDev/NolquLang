# Embedding Nolqu — v1.2.2a3

> [!NOTE]
> **Nolqu v1.2.0 — Stable**
> The embed API (`nolqu.h`) is **identical** in all 1.x releases.
> The embed API is unchanged since v1.0.0.
> For the stable version see [`docs/stable/embedding.md`](../stable/embedding.md).

Embed the Nolqu interpreter in any C or C++ application using `include/nolqu.h`.

---

## Quick Start

```c
#include "nolqu.h"

int main(void) {
    NqState* nq = nq_open();

    nq_setglobal_string(nq, "player", "Alice");
    nq_setglobal_number(nq, "score",  9001);

    nq_dostring(nq, "print player .. \" scored \" .. score");
    nq_dofile(nq, "game_logic.nq");

    NqValue result = nq_getglobal(nq, "final_score");
    if (result.type == NQ_TYPE_NUMBER)
        printf("Final: %g\n", result.as.number);

    nq_close(nq);
    return 0;
}
```

Compile:
```bash
gcc -Iinclude myapp.c src/memory.c src/value.c src/chunk.c \
    src/object.c src/table.c src/gc.c src/vm.c \
    src/lexer.cpp src/ast.cpp src/parser.cpp src/compiler.cpp \
    src/nq_embed.cpp -lm -lstdc++ -o myapp
```

---

## Lifecycle

```c
NqState* nq_open(void);    /* create VM */
void     nq_close(NqState* nq);  /* destroy VM, free all resources */
```

Each `NqState` is independent. Multiple states can coexist in one process.
States are **not thread-safe** — do not share across threads.

---

## Running Code

```c
int nq_dostring(NqState* nq, const char* source);
int nq_dostring_named(NqState* nq, const char* source, const char* name);
int nq_dofile(NqState* nq, const char* path);
```

Returns `0` on success, non-zero on error.
Use `nq_lasterror(nq)` to retrieve the error message.

---

## Exchanging Values

```c
typedef enum {
    NQ_TYPE_NIL, NQ_TYPE_BOOL, NQ_TYPE_NUMBER,
    NQ_TYPE_STRING, NQ_TYPE_ARRAY, NQ_TYPE_FUNCTION
} NqType;

typedef struct {
    NqType type;
    union { bool boolean; double number; const char* string; } as;
} NqValue;

/* Constructors */
NqValue nq_nil(void);
NqValue nq_bool(bool b);
NqValue nq_number(double n);
NqValue nq_string(const char* s);

/* Set globals */
void nq_setglobal_number(NqState* nq, const char* name, double value);
void nq_setglobal_string(NqState* nq, const char* name, const char* value);
void nq_setglobal_bool  (NqState* nq, const char* name, bool value);
void nq_setglobal_nil   (NqState* nq, const char* name);

/* Get globals */
NqValue nq_getglobal(NqState* nq, const char* name);
```

> **String lifetime:** `const char*` inside `NqValue.as.string` points into
> the Nolqu heap. Copy with `strdup()` if you need it beyond the next GC cycle.

---

## Registering Host Functions

```c
typedef NqValue (*NqNativeFn)(NqState* nq, int argc, NqValue* argv);
void nq_register(NqState* nq, const char* name, NqNativeFn fn, int arity);
```

`arity = -1` for variadic. Up to **64** host functions per `NqState` (raised from 16 in v1.0.0).

```c
static NqValue host_log(NqState* nq, int argc, NqValue* argv) {
    (void)nq;
    if (argc >= 1 && argv[0].type == NQ_TYPE_STRING)
        printf("[LOG] %s\n", argv[0].as.string);
    return nq_nil();
}

nq_register(nq, "log", host_log, 1);
```

---

## GC and Memory

```c
size_t nq_gc(NqState* nq);                        /* force GC, return bytes freed */
void   nq_set_gc_threshold(NqState* nq, size_t);  /* tune trigger threshold       */
```

---

## Error Handling

```c
const char* nq_lasterror(NqState* nq);
```

```c
if (nq_dofile(nq, "script.nq") != 0)
    fprintf(stderr, "Error: %s\n", nq_lasterror(nq));
```

---

## Notes

- `nq_open()` / `nq_close()` can be called multiple times (e.g. one state per plugin).
- Each state's GC is independent — `nq_gc_threshold` is global, but `nq_gc()` only affects the provided state's objects.
- Nolqu imports (`import "stdlib/math"`) executed via `nq_dostring`/`nq_dofile` resolve paths relative to the **process working directory**, not any source file path.
