# Embedding Nolqu — v1.0.0

Nolqu can be embedded into any C or C++ application using the public API
defined in `include/nolqu.h`.

---

## Quick Start

```c
#include "nolqu.h"
#include <stdio.h>

int main(void) {
    NqState* nq = nq_open();

    nq_dostring(nq, "print \"Hello from embedded Nolqu!\"");

    nq_close(nq);
    return 0;
}
```

**Compile:**
```bash
gcc -Iinclude myapp.c \
    src/memory.c src/value.c src/chunk.c src/object.c \
    src/table.c src/gc.c src/vm.c \
    src/lexer.cpp src/ast.cpp src/parser.cpp src/compiler.cpp \
    src/nq_embed.cpp \
    -lm -lstdc++ -o myapp
```

Or simpler with the Makefile as a reference — just link all `.c` and `.cpp`
files in `src/` together with your application.

---

## API Reference

### Lifecycle

```c
/* Create a new Nolqu state (independent VM + globals). */
NqState* nq_open(void);

/* Destroy the state and free all resources. */
void nq_close(NqState* nq);
```

---

### Running Code

```c
/* Execute a string of Nolqu source code. */
int nq_dostring(NqState* nq, const char* source);

/* Execute source with a custom name (used in error messages). */
int nq_dostring_named(NqState* nq, const char* source, const char* name);

/* Load and execute a .nq file. */
int nq_dofile(NqState* nq, const char* path);
```

All three return `0` on success, non-zero on error.
Use `nq_lasterror()` to retrieve the error message.

```c
NqState* nq = nq_open();

if (nq_dofile(nq, "game_logic.nq") != 0) {
    fprintf(stderr, "Error: %s\n", nq_lasterror(nq));
}

nq_close(nq);
```

---

### Exchanging Values

#### `NqValue` — the host-visible value type

```c
typedef enum {
    NQ_TYPE_NIL     = 0,
    NQ_TYPE_BOOL    = 1,
    NQ_TYPE_NUMBER  = 2,
    NQ_TYPE_STRING  = 3,
    NQ_TYPE_ARRAY   = 4,
    NQ_TYPE_FUNCTION= 5,
} NqType;

typedef struct {
    NqType type;
    union {
        bool        boolean;
        double      number;
        const char* string;   /* valid until next GC cycle */
    } as;
} NqValue;
```

#### Helper constructors (defined in `nolqu.h`)

```c
NqValue nq_nil(void);
NqValue nq_bool(bool b);
NqValue nq_number(double n);
NqValue nq_string(const char* s);
```

#### Setting globals

```c
void nq_setglobal_number(NqState* nq, const char* name, double value);
void nq_setglobal_string(NqState* nq, const char* name, const char* value);
void nq_setglobal_bool  (NqState* nq, const char* name, bool value);
void nq_setglobal_nil   (NqState* nq, const char* name);
```

#### Getting globals

```c
NqValue nq_getglobal(NqState* nq, const char* name);
```

Returns `nq_nil()` if the variable does not exist.

**Example — pass data in, read results out:**

```c
nq_setglobal_string(nq, "player_name", "Alice");
nq_setglobal_number(nq, "level", 5);

nq_dofile(nq, "game.nq");

NqValue score = nq_getglobal(nq, "final_score");
if (score.type == NQ_TYPE_NUMBER) {
    printf("Score: %g\n", score.as.number);
}
```

---

### Registering Host Functions

Expose a C/C++ function to Nolqu code:

```c
typedef NqValue (*NqNativeFn)(NqState* nq, int argc, NqValue* argv);

void nq_register(NqState* nq, const char* name, NqNativeFn fn, int arity);
```

`arity` is the expected argument count. Use `-1` for variadic.

**Example — expose `get_time()` to Nolqu:**

```c
#include <time.h>

NqValue host_get_time(NqState* nq, int argc, NqValue* argv) {
    (void)nq; (void)argc; (void)argv;
    return nq_number((double)time(NULL));
}

// register before running any scripts
nq_register(nq, "get_time", host_get_time, 0);
```

Then in Nolqu:
```nolqu
let t = get_time()
print "Unix time: " .. t
```

Up to **64** host-native functions can be registered per `NqState`.

**Example — expose a logging function:**

```c
NqValue host_log(NqState* nq, int argc, NqValue* argv) {
    (void)nq;
    if (argc >= 1 && argv[0].type == NQ_TYPE_STRING)
        printf("[LOG] %s\n", argv[0].as.string);
    return nq_nil();
}

nq_register(nq, "log", host_log, 1);
```

---

### GC and Memory

```c
/* Trigger a full GC cycle. Returns bytes freed. */
size_t nq_gc(NqState* nq);

/* Set the GC threshold (bytes). GC fires when allocation exceeds this. */
void nq_set_gc_threshold(NqState* nq, size_t bytes);
```

> **String lifetime:** `const char*` pointers inside `NqValue.as.string` point
> into the Nolqu heap. They remain valid until the next GC cycle that collects
> that string. If you need to hold a string across GC cycles, copy it with
> `strdup()` or store it in a Nolqu global.

---

### Error Handling

```c
/* Return the last error message, or "" if none. */
const char* nq_lasterror(NqState* nq);
```

```c
if (nq_dostring(nq, code) != 0) {
    fprintf(stderr, "Nolqu error: %s\n", nq_lasterror(nq));
}
```

---

## Full Example — Script Host

```c
#include "nolqu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Host function: platform_name() → string */
static NqValue host_platform(NqState* nq, int argc, NqValue* argv) {
    (void)nq; (void)argc; (void)argv;
#if defined(_WIN32)
    return nq_string("windows");
#elif defined(__ANDROID__)
    return nq_string("android");
#else
    return nq_string("unix");
#endif
}

/* Host function: exit_code(n) */
static NqValue host_exit(NqState* nq, int argc, NqValue* argv) {
    (void)nq;
    int code = (argc >= 1 && argv[0].type == NQ_TYPE_NUMBER)
        ? (int)argv[0].as.number : 0;
    exit(code);
    return nq_nil();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: myapp script.nq\n");
        return 1;
    }

    NqState* nq = nq_open();

    /* Expose host functions */
    nq_register(nq, "platform_name", host_platform, 0);
    nq_register(nq, "exit_code",     host_exit,     1);

    /* Pass command-line argument count */
    nq_setglobal_number(nq, "ARGC", (double)(argc - 2));

    /* Run the script */
    int result = nq_dofile(nq, argv[1]);
    if (result != 0)
        fprintf(stderr, "%s\n", nq_lasterror(nq));

    nq_close(nq);
    return result;
}
```

**script.nq:**
```nolqu
print "Running on: " .. platform_name()
print "Arguments:  " .. ARGC
```

---

## Notes

- Each `NqState` is completely independent — multiple states can coexist in
  the same process.
- States are **not thread-safe** — do not share one `NqState` across threads.
- The GC runs automatically. You can tune it with `nq_set_gc_threshold`.
- `nq_open()` / `nq_close()` can be called multiple times (e.g. one state
  per game level, one state per plugin).
