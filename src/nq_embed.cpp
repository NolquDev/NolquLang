/*
 * nq_embed.c — Nolqu C/C++ Embedding API Implementation
 */
#include "../include/nolqu.h"
#include "vm.h"
#include "parser.h"
#include "compiler.h"
#include "object.h"
#include "table.h"
#include "gc.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─────────────────────────────────────────────
 *  NqState — public opaque wrapper around VM
 * ───────────────────────────────────────────── */
struct NqState {
    VM   vm;
    char last_error[1024];
};

/* ─────────────────────────────────────────────
 *  Internal helpers
 * ───────────────────────────────────────────── */

static NqValue internal_to_nqvalue(Value v) {
    NqValue out;
    if (IS_NIL(v))     { out.type = NQ_TYPE_NIL;      out.as.number = 0;             return out; }
    if (IS_BOOL(v))    { out.type = NQ_TYPE_BOOL;     out.as.boolean = AS_BOOL(v);   return out; }
    if (IS_NUMBER(v))  { out.type = NQ_TYPE_NUMBER;   out.as.number = AS_NUMBER(v);  return out; }
    if (IS_STRING(v))  { out.type = NQ_TYPE_STRING;   out.as.string = AS_CSTRING(v); return out; }
    if (IS_ARRAY(v))   { out.type = NQ_TYPE_ARRAY;    out.as.number = 0;             return out; }
    if (IS_FUNCTION(v) || IS_NATIVE(v)) { out.type = NQ_TYPE_FUNCTION; out.as.number = 0; return out; }
    out.type = NQ_TYPE_NIL; out.as.number = 0;
    return out;
}

static Value nqvalue_to_internal(NqState* nq, NqValue v) {
    switch (v.type) {
        case NQ_TYPE_NIL:     return NIL_VAL;
        case NQ_TYPE_BOOL:    return BOOL_VAL(v.as.boolean);
        case NQ_TYPE_NUMBER:  return NUMBER_VAL(v.as.number);
        case NQ_TYPE_STRING:  return OBJ_VAL(copyString(v.as.string, (int)strlen(v.as.string)));
        default:              (void)nq; return NIL_VAL;
    }
}

/* ─────────────────────────────────────────────
 *  Lifecycle
 * ───────────────────────────────────────────── */

NqState* nq_open(void) {
    NqState* nq = (NqState*)calloc(1, sizeof(NqState));
    if (!nq) return NULL;
    initVM(&nq->vm);
    nq->last_error[0] = '\0';
    return nq;
}

void nq_close(NqState* nq) {
    if (!nq) return;
    freeVM(&nq->vm);
    free(nq);
}

/* ─────────────────────────────────────────────
 *  Execution
 * ───────────────────────────────────────────── */

int nq_dostring_named(NqState* nq, const char* source, const char* name) {
    Parser parser;
    initParser(&parser, source, name);
    ASTNode* ast = parse(&parser);

    if (parser.had_error) {
        snprintf(nq->last_error, sizeof(nq->last_error),
            "Parse error in %s", name);
        freeNode(ast);
        return 1;
    }

    CompileResult result = compile(ast, name);
    freeNode(ast);

    if (result.had_error || !result.function) {
        snprintf(nq->last_error, sizeof(nq->last_error),
            "Compile error in %s", name);
        return 1;
    }

    InterpretResult ir = runVM(&nq->vm, result.function, name);
    if (ir != INTERPRET_OK) {
        snprintf(nq->last_error, sizeof(nq->last_error),
            "Runtime error in %s", name);
        return 1;
    }

    nq->last_error[0] = '\0';
    return 0;
}

int nq_dostring(NqState* nq, const char* source) {
    return nq_dostring_named(nq, source, "<string>");
}

int nq_dofile(NqState* nq, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        snprintf(nq->last_error, sizeof(nq->last_error),
            "Cannot open file: %s", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f); rewind(f);
    char* buf = (char*)malloc((size_t)(size + 1));
    if (!buf) { fclose(f); return 1; }
    size_t n = fread(buf, 1, (size_t)size, f);
    buf[n] = '\0'; fclose(f);
    int r = nq_dostring_named(nq, buf, path);
    free(buf);
    return r;
}

/* ─────────────────────────────────────────────
 *  Global variable access
 * ───────────────────────────────────────────── */

NqValue nq_getglobal(NqState* nq, const char* name) {
    ObjString* key = copyString(name, (int)strlen(name));
    Value val;
    if (!tableGet(&nq->vm.globals, key, &val)) return nq_nil();
    return internal_to_nqvalue(val);
}

static void set_global_raw(NqState* nq, const char* name, Value val) {
    ObjString* key = copyString(name, (int)strlen(name));
    tableSet(&nq->vm.globals, key, val);
}

void nq_setglobal_number(NqState* nq, const char* name, double value) {
    set_global_raw(nq, name, NUMBER_VAL(value));
}
void nq_setglobal_string(NqState* nq, const char* name, const char* value) {
    set_global_raw(nq, name, OBJ_VAL(copyString(value, (int)strlen(value))));
}
void nq_setglobal_bool(NqState* nq, const char* name, bool value) {
    set_global_raw(nq, name, BOOL_VAL(value));
}
void nq_setglobal_nil(NqState* nq, const char* name) {
    set_global_raw(nq, name, NIL_VAL);
}

/* ─────────────────────────────────────────────
 *  Calling Nolqu functions
 * ───────────────────────────────────────────── */

int nq_call(NqState* nq, const char* fn_name,
            int argc, NqValue* argv, NqValue* result) {
    /* Build a call expression as a mini-script:
     * fn_name(arg0, arg1, ...)
     * For numeric args this works cleanly; string args need quoting. */
    char script[4096];
    int  pos = snprintf(script, sizeof(script), "print %s(", fn_name);

    for (int i = 0; i < argc && pos < (int)sizeof(script) - 64; i++) {
        if (i) pos += snprintf(script+pos, sizeof(script)-pos, ",");
        NqValue* a = &argv[i];
        switch (a->type) {
            case NQ_TYPE_NUMBER:
                pos += snprintf(script+pos, sizeof(script)-pos, "%g", a->as.number); break;
            case NQ_TYPE_BOOL:
                pos += snprintf(script+pos, sizeof(script)-pos, "%s",
                    a->as.boolean ? "true" : "false"); break;
            case NQ_TYPE_NIL:
                pos += snprintf(script+pos, sizeof(script)-pos, "nil"); break;
            case NQ_TYPE_STRING:
                pos += snprintf(script+pos, sizeof(script)-pos, "\"%s\"",
                    a->as.string ? a->as.string : ""); break;
            default:
                pos += snprintf(script+pos, sizeof(script)-pos, "nil"); break;
        }
    }
    snprintf(script+pos, sizeof(script)-pos, ")");

    if (result) *result = nq_nil();
    return nq_dostring_named(nq, script, "<nq_call>");
}

/* ─────────────────────────────────────────────
 *  Native function registration
 * ───────────────────────────────────────────── */

/*
 * Host-native registration — C++ solution.
 *
 * Each registered function gets its own heap-allocated Context that
 * stores the NqState* and the host callback.  A templated trampoline
 * reads the context from a thread-local pointer set just before the
 * call; this avoids the original 16-slot macro table.
 *
 * Max registered natives: MAX_HOST_NATIVES (64).
 */
#define MAX_HOST_NATIVES 64

struct NqNativeCtx {
    NqState*   nq;
    NqNativeFn host_fn;
};

static NqNativeCtx  g_ctxs[MAX_HOST_NATIVES];
static int          g_ctx_count = 0;

/*
 * Each slot index N gets a unique trampoline function via a template.
 * The template is instantiated once per slot at compile time — no
 * macros, no dispatch table, no dead generic wrapper.
 */
template<int N>
static Value nq_trampoline(int argc, Value* args) {
    NqNativeCtx* ctx = &g_ctxs[N];
    NqValue nqargs[32];
    int n = (argc < 32) ? argc : 32;
    for (int i = 0; i < n; i++)
        nqargs[i] = internal_to_nqvalue(args[i]);
    NqValue r = ctx->host_fn(ctx->nq, n, nqargs);
    return nqvalue_to_internal(ctx->nq, r);
}

/* Build a compile-time array of trampoline function pointers. */
template<int... Is>
struct TrampolineTable {
    static constexpr NativeFn fns[] = { nq_trampoline<Is>... };
};

/* Generate indices 0..N-1 via index_sequence (C++14). */
template<int N, int... Is>
struct MakeSeq : MakeSeq<N-1, N-1, Is...> {};
template<int... Is>
struct MakeSeq<0, Is...> : TrampolineTable<Is...> {};

/* Instantiate the full table. */
using Trampolines = MakeSeq<MAX_HOST_NATIVES>;

void nq_register(NqState* nq, const char* name, NqNativeFn fn, int arity) {
    if (g_ctx_count >= MAX_HOST_NATIVES) {
        fprintf(stderr, "[nolqu] nq_register: too many host natives (max %d)\n",
                MAX_HOST_NATIVES);
        return;
    }
    int idx = g_ctx_count++;
    g_ctxs[idx].nq      = nq;
    g_ctxs[idx].host_fn = fn;

    ObjNative* native = newNative(Trampolines::fns[idx], name, arity);
    ObjString* key    = copyString(name, (int)strlen(name));
    tableSet(&nq->vm.globals, key, OBJ_VAL(native));
}

/* ─────────────────────────────────────────────
 *  Error / GC
 * ───────────────────────────────────────────── */

const char* nq_lasterror(NqState* nq) {
    return nq->last_error;
}

size_t nq_gc(NqState* nq) {
    size_t before = nq_bytes_allocated;
    nq_collect(&nq->vm);
    size_t after  = nq_bytes_allocated;
    return before > after ? before - after : 0;
}

void nq_set_gc_threshold(NqState* nq, size_t bytes) {
    (void)nq;
    nq_gc_threshold = bytes;
}
