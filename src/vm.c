#define _POSIX_C_SOURCE 200809L
#include "vm.h"
#include "memory.h"
#include "object.h"
#include "gc.h"
#include "jit.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

// Forward declaration — set in initVM, used by native error helpers
static VM* g_vm_for_error = NULL;
static bool throwError(VM* vm, Value error_val);  /* defined later */

static void runJitFallbackLoop(const JITLoopSpec* spec) {
    if (spec->step > 0) {
        while (*spec->i_ptr < spec->stop) {
            *spec->i_ptr += spec->step;
            for (int k = 0; k < spec->n_vars; k++) {
                if (spec->ops[k] == JIT_OP_MUL) *spec->var_ptrs[k] *= spec->deltas[k];
                else                            *spec->var_ptrs[k] += spec->deltas[k];
            }
        }
    } else if (spec->step < 0) {
        while (*spec->i_ptr > spec->stop) {
            *spec->i_ptr += spec->step;
            for (int k = 0; k < spec->n_vars; k++) {
                if (spec->ops[k] == JIT_OP_MUL) *spec->var_ptrs[k] *= spec->deltas[k];
                else                            *spec->var_ptrs[k] += spec->deltas[k];
            }
        }
    }
}

// ─────────────────────────────────────────────
//  Native function implementations
// ─────────────────────────────────────────────


// str(value) — convert any value to string
static Value nativeStr(int argc, Value* args) {
    if (argc != 1) return OBJ_VAL(copyString("", 0));
    Value v = args[0];
    char buf[64];
    if (IS_STRING(v))  return v;
    if (IS_NIL(v))     return OBJ_VAL(copyString("null", 4));
    if (IS_BOOL(v))    return OBJ_VAL(copyString(AS_BOOL(v) ? "true" : "false",
                                                  AS_BOOL(v) ? 4 : 5));
    if (IS_NUMBER(v)) {
        double n = AS_NUMBER(v);
        if (n == (long long)n && n >= -9007199254740992.0 && n <= 9007199254740992.0)
            snprintf(buf, sizeof(buf), "%lld", (long long)n);
        else snprintf(buf, sizeof(buf), "%.14g", n);
        return OBJ_VAL(copyString(buf, (int)strlen(buf)));
    }
    return OBJ_VAL(copyString("<object>", 8));
}

// num(value) — convert string to number, returns nil if invalid
static Value nativeNum(int argc, Value* args) {
    if (argc != 1) return NIL_VAL;
    if (IS_NUMBER(args[0])) return args[0];
    if (IS_STRING(args[0])) {
        char* end;
        double n = strtod(AS_CSTRING(args[0]), &end);
        if (end != AS_CSTRING(args[0]) && *end == '\0') return NUMBER_VAL(n);
        return NIL_VAL;
    }
    return NIL_VAL;
}

// type(value) — return the type name as a string
static Value nativeType(int argc, Value* args) {
    if (argc != 1) return NIL_VAL;
    Value v = args[0];
    const char* name;
    if (IS_NIL(v))      name = "null";
    else if (IS_BOOL(v))   name = "bool";
    else if (IS_NUMBER(v)) name = "number";
    else if (IS_STRING(v)) name = "string";
    else if (IS_ARRAY(v))  name = "array";
    else if (IS_FUNCTION(v) || IS_NATIVE(v)) name = "function";
    else name = "object";
    return OBJ_VAL(copyString(name, (int)strlen(name)));
}

// len(arr_or_string) — return length
static Value nativeLen(int argc, Value* args) {
    if (argc != 1) {
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString("len: expected 1 argument", 24));
        return NIL_VAL;
    }
    if (IS_ARRAY(args[0]))  return NUMBER_VAL(AS_ARRAY(args[0])->count);
    if (IS_STRING(args[0])) return NUMBER_VAL(AS_STRING(args[0])->length);
    char msg[128];
    snprintf(msg, sizeof(msg), "len: expected string or array, got %s",
        IS_NUMBER(args[0]) ? "number" : IS_BOOL(args[0]) ? "bool" : "null");
    if (g_vm_for_error) g_vm_for_error->thrown = OBJ_VAL(copyString(msg, (int)strlen(msg)));
    return NIL_VAL;
}

// push(arr, value) — append to array, return array
static Value nativePush(int argc, Value* args) {
    if (argc != 2 || !IS_ARRAY(args[0])) return NIL_VAL;
    arrayPush(AS_ARRAY(args[0]), args[1]);
    return args[0];
}

// pop(arr) — remove and return last element
static Value nativePop(int argc, Value* args) {
    if (argc != 1 || !IS_ARRAY(args[0])) {
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString("pop: expected an array", 21));
        return NIL_VAL;
    }
    ObjArray* arr = AS_ARRAY(args[0]);
    if (arr->count == 0) {
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString("pop: cannot pop from an empty array", 35));
        return NIL_VAL;
    }
    return arrayPop(arr);
}

// remove(arr, index) — remove element at index, return removed value
static Value nativeRemove(int argc, Value* args) {
    if (argc != 2 || !IS_ARRAY(args[0]) || !IS_NUMBER(args[1])) {
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString("remove: expected array and index", 31));
        return NIL_VAL;
    }
    ObjArray* arr = AS_ARRAY(args[0]);
    int idx = (int)AS_NUMBER(args[1]);
    if (idx < 0) idx = arr->count + idx;
    if (idx < 0 || idx >= arr->count) {
        char msg[128];
        snprintf(msg, sizeof(msg),
            "remove: index %d out of bounds (array length %d)",
            (int)AS_NUMBER(args[1]), arr->count);
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString(msg, (int)strlen(msg)));
        return NIL_VAL;
    }
    Value removed = arr->items[idx];
    for (int i = idx; i < arr->count - 1; i++)
        arr->items[i] = arr->items[i + 1];
    arr->count--;
    return removed;
}

// contains(arr, value) — return true if value is in array
static Value nativeContains(int argc, Value* args) {
    if (argc != 2 || !IS_ARRAY(args[0])) return BOOL_VAL(false);
    ObjArray* arr = AS_ARRAY(args[0]);
    for (int i = 0; i < arr->count; i++)
        if (valuesEqual(arr->items[i], args[1])) return BOOL_VAL(true);
    return BOOL_VAL(false);
}

// ─────────────────────────────────────────────
//  stdlib: math
// ─────────────────────────────────────────────
static Value nativeSqrt(int argc, Value* args)  { if (argc!=1||!IS_NUMBER(args[0])) return NIL_VAL; return NUMBER_VAL(sqrt(AS_NUMBER(args[0]))); }
static Value nativeFloor(int argc, Value* args) { if (argc!=1||!IS_NUMBER(args[0])) return NIL_VAL; return NUMBER_VAL(floor(AS_NUMBER(args[0]))); }
static Value nativeCeil(int argc, Value* args)  { if (argc!=1||!IS_NUMBER(args[0])) return NIL_VAL; return NUMBER_VAL(ceil(AS_NUMBER(args[0]))); }
static Value nativeRound(int argc, Value* args) { if (argc!=1||!IS_NUMBER(args[0])) return NIL_VAL; return NUMBER_VAL(round(AS_NUMBER(args[0]))); }
static Value nativeAbs(int argc, Value* args)   { if (argc!=1||!IS_NUMBER(args[0])) return NIL_VAL; return NUMBER_VAL(fabs(AS_NUMBER(args[0]))); }
static Value nativePow(int argc, Value* args)   { if (argc!=2||!IS_NUMBER(args[0])||!IS_NUMBER(args[1])) return NIL_VAL; return NUMBER_VAL(pow(AS_NUMBER(args[0]),AS_NUMBER(args[1]))); }
static Value nativeMax(int argc, Value* args)   { if (argc!=2||!IS_NUMBER(args[0])||!IS_NUMBER(args[1])) return NIL_VAL; return NUMBER_VAL(AS_NUMBER(args[0])>AS_NUMBER(args[1])?AS_NUMBER(args[0]):AS_NUMBER(args[1])); }
static Value nativeMin(int argc, Value* args)   { if (argc!=2||!IS_NUMBER(args[0])||!IS_NUMBER(args[1])) return NIL_VAL; return NUMBER_VAL(AS_NUMBER(args[0])<AS_NUMBER(args[1])?AS_NUMBER(args[0]):AS_NUMBER(args[1])); }

// ─────────────────────────────────────────────
//  stdlib: string
// ─────────────────────────────────────────────

// upper(s) — convert to uppercase
static Value nativeUpper(int argc, Value* args) {
    if (argc!=1||!IS_STRING(args[0])) return NIL_VAL;
    ObjString* s = AS_STRING(args[0]);
    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(s->length + 1));
    for (int i=0;i<s->length;i++) buf[i]=(char)toupper((unsigned char)s->chars[i]);
    buf[s->length]='\0';
    return OBJ_VAL(takeString(buf, s->length));
}

// lower(s) — convert to lowercase
static Value nativeLower(int argc, Value* args) {
    if (argc!=1||!IS_STRING(args[0])) return NIL_VAL;
    ObjString* s = AS_STRING(args[0]);
    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(s->length + 1));
    for (int i=0;i<s->length;i++) buf[i]=(char)tolower((unsigned char)s->chars[i]);
    buf[s->length]='\0';
    return OBJ_VAL(takeString(buf, s->length));
}

// slice(s, start, end) — substring [start, end)
static Value nativeSlice(int argc, Value* args) {
    if (argc<2||!IS_STRING(args[0])||!IS_NUMBER(args[1])) return NIL_VAL;
    ObjString* s = AS_STRING(args[0]);
    int start = (int)AS_NUMBER(args[1]);
    int end   = (argc==3&&IS_NUMBER(args[2])) ? (int)AS_NUMBER(args[2]) : s->length;
    if (start<0) start=s->length+start;
    if (end<0)   end=s->length+end;
    if (start<0) start=0;
    if (end>s->length) end=s->length;
    if (start>=end) return OBJ_VAL(copyString("",0));
    return OBJ_VAL(copyString(s->chars+start, end-start));
}

// trim(s) — remove leading/trailing whitespace
static Value nativeTrim(int argc, Value* args) {
    if (argc!=1||!IS_STRING(args[0])) return NIL_VAL;
    const char* c = AS_CSTRING(args[0]);
    int len = (int)strlen(c);
    int start=0, end=len-1;
    while (start<=end && isspace((unsigned char)c[start])) start++;
    while (end>=start && isspace((unsigned char)c[end]))   end--;
    return OBJ_VAL(copyString(c+start, end-start+1));
}

// replace(s, old, new) — replace first occurrence
static Value nativeReplace(int argc, Value* args) {
    if (argc!=3||!IS_STRING(args[0])||!IS_STRING(args[1])||!IS_STRING(args[2])) return NIL_VAL;
    const char* src  = AS_CSTRING(args[0]);
    const char* old  = AS_CSTRING(args[1]);
    const char* repl = AS_CSTRING(args[2]);
    int src_len  = AS_STRING(args[0])->length;
    int old_len  = AS_STRING(args[1])->length;
    int repl_len = AS_STRING(args[2])->length;
    if (old_len==0) return args[0];
    char* found = strstr(src, old);
    if (!found) return args[0];
    int prefix = (int)(found - src);
    int new_len = prefix + repl_len + (src_len - prefix - old_len);
    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(new_len + 1));
    memcpy(buf,           src,    (size_t)prefix);
    memcpy(buf+prefix,    repl,   (size_t)repl_len);
    memcpy(buf+prefix+repl_len, found+old_len, (size_t)(src_len-prefix-old_len));
    buf[new_len]='\0';
    return OBJ_VAL(takeString(buf, new_len));
}

// split(s, sep) — split string into array
static Value nativeSplit(int argc, Value* args) {
    if (argc!=2||!IS_STRING(args[0])||!IS_STRING(args[1])) return NIL_VAL;
    const char* src = AS_CSTRING(args[0]);
    const char* sep = AS_CSTRING(args[1]);
    int sep_len = AS_STRING(args[1])->length;
    ObjArray* arr = newArray();
    if (sep_len==0) {
        // split into individual characters
        for (int i=0; src[i]; i++)
            arrayPush(arr, OBJ_VAL(copyString(src+i, 1)));
        return OBJ_VAL(arr);
    }
    const char* cur = src;
    const char* found;
    while ((found = strstr(cur, sep)) != NULL) {
        arrayPush(arr, OBJ_VAL(copyString(cur, (int)(found-cur))));
        cur = found + sep_len;
    }
    arrayPush(arr, OBJ_VAL(copyString(cur, (int)strlen(cur))));
    return OBJ_VAL(arr);
}

// startswith(s, prefix) / endswith(s, suffix)
static Value nativeStartsWith(int argc, Value* args) {
    if (argc!=2||!IS_STRING(args[0])||!IS_STRING(args[1])) return BOOL_VAL(false);
    ObjString *s=AS_STRING(args[0]), *p=AS_STRING(args[1]);
    if (p->length>s->length) return BOOL_VAL(false);
    return BOOL_VAL(memcmp(s->chars, p->chars, (size_t)p->length)==0);
}
static Value nativeEndsWith(int argc, Value* args) {
    if (argc!=2||!IS_STRING(args[0])||!IS_STRING(args[1])) return BOOL_VAL(false);
    ObjString *s=AS_STRING(args[0]), *sf=AS_STRING(args[1]);
    if (sf->length>s->length) return BOOL_VAL(false);
    return BOOL_VAL(memcmp(s->chars+s->length-sf->length, sf->chars, (size_t)sf->length)==0);
}

// ─────────────────────────────────────────────
//  stdlib v0.5.0 — extended utilities
// ─────────────────────────────────────────────

// random() — float in [0, 1)
static Value nativeRandom(int argc, Value* args) {
    (void)argc; (void)args;
    return NUMBER_VAL((double)rand() / ((double)RAND_MAX + 1.0));
}

// rand_int(lo, hi) — integer in [lo, hi]
static Value nativeRandInt(int argc, Value* args) {
    if (argc!=2||!IS_NUMBER(args[0])||!IS_NUMBER(args[1])) return NIL_VAL;
    int lo = (int)AS_NUMBER(args[0]);
    int hi = (int)AS_NUMBER(args[1]);
    if (lo > hi) return NIL_VAL;
    return NUMBER_VAL(lo + rand() % (hi - lo + 1));
}

// index(s, sub) — first position of sub in s, or -1
static Value nativeIndex(int argc, Value* args) {
    if (argc!=2||!IS_STRING(args[0])||!IS_STRING(args[1])) return NUMBER_VAL(-1);
    const char* src = AS_CSTRING(args[0]);
    const char* sub = AS_CSTRING(args[1]);
    char* found = strstr(src, sub);
    if (!found) return NUMBER_VAL(-1);
    return NUMBER_VAL((double)(found - src));
}

// repeat(s, n) — repeat string n times
static Value nativeRepeat(int argc, Value* args) {
    if (argc!=2||!IS_STRING(args[0])||!IS_NUMBER(args[1])) return NIL_VAL;
    ObjString* s = AS_STRING(args[0]);
    int n = (int)AS_NUMBER(args[1]);
    if (n <= 0) return OBJ_VAL(copyString("", 0));
    int new_len = s->length * n;
    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(new_len + 1));
    for (int i = 0; i < n; i++)
        memcpy(buf + i * s->length, s->chars, (size_t)s->length);
    buf[new_len] = '\0';
    return OBJ_VAL(takeString(buf, new_len));
}

// join(arr, sep) — join array elements into a string
static Value nativeJoin(int argc, Value* args) {
    if (argc!=2||!IS_ARRAY(args[0])||!IS_STRING(args[1])) return NIL_VAL;
    ObjArray*   arr = AS_ARRAY(args[0]);
    ObjString*  sep = AS_STRING(args[1]);
    if (arr->count == 0) return OBJ_VAL(copyString("", 0));

    // First pass: measure total length
    int total = sep->length * (arr->count - 1);
    for (int i = 0; i < arr->count; i++) {
        if (IS_STRING(arr->items[i]))      total += AS_STRING(arr->items[i])->length;
        else if (IS_NUMBER(arr->items[i])) total += 32; // max number string
        else if (IS_BOOL(arr->items[i]))   total += 5;
        else                               total += 3;  // nil
    }

    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(total + 1));
    int pos = 0;
    for (int i = 0; i < arr->count; i++) {
        if (i > 0) { memcpy(buf + pos, sep->chars, (size_t)sep->length); pos += sep->length; }
        if (IS_STRING(arr->items[i])) {
            ObjString* item = AS_STRING(arr->items[i]);
            memcpy(buf + pos, item->chars, (size_t)item->length);
            pos += item->length;
        } else if (IS_NUMBER(arr->items[i])) {
            double v = AS_NUMBER(arr->items[i]);
            int written = (v == (long long)v)
                ? snprintf(buf + pos, 32, "%lld", (long long)v)
                : snprintf(buf + pos, 32, "%g",   v);
            pos += written;
        } else if (IS_BOOL(arr->items[i])) {
            const char* b = AS_BOOL(arr->items[i]) ? "true" : "false";
            int len = (int)strlen(b);
            memcpy(buf + pos, b, (size_t)len); pos += len;
        } else {
            memcpy(buf + pos, "null", 4); pos += 4;
        }
    }
    buf[pos] = '\0';
    return OBJ_VAL(takeString(buf, pos));
}

// sort(arr) — sort array in-place (numbers and strings), return arr
static int cmp_values(const void* a, const void* b) {
    Value va = *(const Value*)a;
    Value vb = *(const Value*)b;
    if (IS_NUMBER(va) && IS_NUMBER(vb)) {
        double d = AS_NUMBER(va) - AS_NUMBER(vb);
        return (d > 0) - (d < 0);
    }
    if (IS_STRING(va) && IS_STRING(vb))
        return strcmp(AS_CSTRING(va), AS_CSTRING(vb));
    // numbers before strings
    if (IS_NUMBER(va)) return -1;
    return 1;
}
static Value nativeSort(int argc, Value* args) {
    if (argc!=1||!IS_ARRAY(args[0])) return NIL_VAL;
    ObjArray* arr = AS_ARRAY(args[0]);
    if (arr->count > 1)
        qsort(arr->items, (size_t)arr->count, sizeof(Value), cmp_values);
    return args[0];
}

// error(msg) — throw a user error (uses OP_THROW via special sentinel)
// We can't call OP_THROW from C directly, so we store the message and
// return a special "error sentinel" — the VM checks for it after native calls.
// Simpler approach: we mark vm->thrown and the VM dispatch will re-raise it.
// Actually, for native functions we return NIL_VAL and set a flag.
// Cleanest: return a special tagged value. We use a dedicated approach:
// nativeError sets vm->thrown and returns NIL_VAL; after native dispatch
// we check vm->thrown.

static Value nativeError(int argc, Value* args) {
    if (argc < 1) return NIL_VAL;
    Value msg = args[0];
    if (!IS_STRING(msg)) {
        char buf[64];
        if (IS_NUMBER(msg)) snprintf(buf, sizeof(buf), "%g", AS_NUMBER(msg));
        else if (IS_BOOL(msg)) snprintf(buf, sizeof(buf), "%s", AS_BOOL(msg) ? "true" : "false");
        else snprintf(buf, sizeof(buf), "null");
        msg = OBJ_VAL(copyString(buf, (int)strlen(buf)));
    }
    if (g_vm_for_error) g_vm_for_error->thrown = msg;
    return NIL_VAL;
}

// ─────────────────────────────────────────────
//  stdlib v0.7.0 — File I/O
// ─────────────────────────────────────────────

// file_read(path) — read entire file as string, throws on error
static Value nativeFileRead(int argc, Value* args) {
    if (argc != 1 || !IS_STRING(args[0])) {
        if (g_vm_for_error) g_vm_for_error->thrown = OBJ_VAL(copyString("file_read: expected a path string", 32));
        return NIL_VAL;
    }
    const char* path = AS_CSTRING(args[0]);
    FILE* f = fopen(path, "rb");
    if (!f) {
        char msg[512]; snprintf(msg, sizeof(msg), "file_read: cannot open '%s'", path);
        if (g_vm_for_error) g_vm_for_error->thrown = OBJ_VAL(copyString(msg, (int)strlen(msg)));
        return NIL_VAL;
    }
    fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(size + 1));
    size_t n = fread(buf, 1, (size_t)size, f); buf[n] = '\0'; fclose(f);
    return OBJ_VAL(takeString(buf, (int)n));
}

// file_write(path, content) — overwrite file, return true/false
static Value nativeFileWrite(int argc, Value* args) {
    if (argc != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        if (g_vm_for_error) g_vm_for_error->thrown = OBJ_VAL(copyString("file_write: expected path and content", 36));
        return BOOL_VAL(false);
    }
    FILE* f = fopen(AS_CSTRING(args[0]), "wb");
    if (!f) {
        char msg[512]; snprintf(msg, sizeof(msg), "file_write: cannot write to '%s'", AS_CSTRING(args[0]));
        if (g_vm_for_error) g_vm_for_error->thrown = OBJ_VAL(copyString(msg, (int)strlen(msg)));
        return BOOL_VAL(false);
    }
    ObjString* content = AS_STRING(args[1]);
    fwrite(content->chars, 1, (size_t)content->length, f); fclose(f);
    return BOOL_VAL(true);
}

// file_append(path, content) — append to file, return true/false
static Value nativeFileAppend(int argc, Value* args) {
    if (argc != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        if (g_vm_for_error) g_vm_for_error->thrown = OBJ_VAL(copyString("file_append: expected path and content", 37));
        return BOOL_VAL(false);
    }
    FILE* f = fopen(AS_CSTRING(args[0]), "ab");
    if (!f) {
        char msg[512]; snprintf(msg, sizeof(msg), "file_append: cannot open '%s'", AS_CSTRING(args[0]));
        if (g_vm_for_error) g_vm_for_error->thrown = OBJ_VAL(copyString(msg, (int)strlen(msg)));
        return BOOL_VAL(false);
    }
    ObjString* content = AS_STRING(args[1]);
    fwrite(content->chars, 1, (size_t)content->length, f); fclose(f);
    return BOOL_VAL(true);
}

// file_exists(path) — return true if file is readable
static Value nativeFileExists(int argc, Value* args) {
    if (argc != 1 || !IS_STRING(args[0])) return BOOL_VAL(false);
    FILE* f = fopen(AS_CSTRING(args[0]), "r");
    if (!f) return BOOL_VAL(false);
    fclose(f); return BOOL_VAL(true);
}

// file_lines(path) — return array of lines (newline stripped)
static Value nativeFileLines(int argc, Value* args) {
    if (argc != 1 || !IS_STRING(args[0])) {
        if (g_vm_for_error) g_vm_for_error->thrown = OBJ_VAL(copyString("file_lines: expected a path string", 33));
        return NIL_VAL;
    }
    const char* path = AS_CSTRING(args[0]);
    FILE* f = fopen(path, "rb");
    if (!f) {
        char msg[512]; snprintf(msg, sizeof(msg), "file_lines: cannot open '%s'", path);
        if (g_vm_for_error) g_vm_for_error->thrown = OBJ_VAL(copyString(msg, (int)strlen(msg)));
        return NIL_VAL;
    }
    fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(size + 1));
    size_t n = fread(buf, 1, (size_t)size, f); buf[n] = '\0'; fclose(f);

    ObjArray* arr = newArray();
    char* cur = buf;
    char* end = buf + n;
    while (cur <= end) {
        char* nl = (char*)memchr(cur, '\n', (size_t)(end - cur));
        int len  = nl ? (int)(nl - cur) : (int)(end - cur);
        if (len > 0 && cur[len - 1] == '\r') len--; // strip \r
        arrayPush(arr, OBJ_VAL(copyString(cur, len)));
        if (!nl) break;
        cur = nl + 1;
    }
    nq_realloc(buf, (size_t)(size + 1), 0);   /* paired with nq_realloc alloc above */
    return OBJ_VAL(arr);
}

// gc_collect() — manually trigger a GC cycle, returns bytes freed
static Value nativeGcCollect(int argc, Value* args) {
    (void)argc; (void)args;
    if (!nq_gc_vm) return NIL_VAL;
    size_t before = nq_bytes_allocated;
    nq_collect(nq_gc_vm);
    size_t after  = nq_bytes_allocated;
    return NUMBER_VAL((double)(before > after ? before - after : 0));
}

// ─────────────────────────────────────────────
//  stdlib v0.9.0 — Stabilization builtins
// ─────────────────────────────────────────────

// assert(condition, message) — throw if condition is false
static Value nativeAssert(int argc, Value* args) {
    if (argc < 1) return NIL_VAL;
    if (isTruthy(args[0])) return BOOL_VAL(true);
    // Condition is false — throw
    const char* msg = (argc >= 2 && IS_STRING(args[1]))
        ? AS_CSTRING(args[1]) : "Assertion failed.";
    if (g_vm_for_error)
        g_vm_for_error->thrown = OBJ_VAL(copyString(msg, (int)strlen(msg)));
    return NIL_VAL;
}

// clock() — seconds since program start (float)
static Value nativeClock(int argc, Value* args) {
    (void)argc; (void)args;
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// mem_usage() — bytes currently allocated on the heap
static Value nativeMemUsage(int argc, Value* args) {
    (void)argc; (void)args;
    return NUMBER_VAL((double)nq_bytes_allocated);
}

/* jit_enabled() -> bool, jit_enable(bool) -> bool */
static Value nativeJitEnabled(int argc, Value* args) {
    (void)argc; (void)args;
    return BOOL_VAL(nq_jit_is_enabled());
}

static Value nativeJitEnable(int argc, Value* args) {
    if (argc != 1 || !IS_BOOL(args[0])) {
        const char* msg = "TypeError: jit_enable(bool) expects 1 boolean argument";
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString(msg, (int)strlen(msg)));
        return NIL_VAL;
    }
    return BOOL_VAL(nq_jit_set_enabled(AS_BOOL(args[0])));
}

/* jit_stats() -> "attempts=..., hits=..., misses=..., compiled=..., fallbacks=..." */
static Value nativeJitStats(int argc, Value* args) {
    (void)argc; (void)args;
    NQJitStats stats;
    nq_jit_get_stats(&stats);
    char buf[192];
    snprintf(buf, sizeof(buf),
             "attempts=%llu hits=%llu misses=%llu compiled=%llu fallbacks=%llu",
             (unsigned long long)stats.attempts,
             (unsigned long long)stats.cache_hits,
             (unsigned long long)stats.cache_misses,
             (unsigned long long)stats.compiled,
             (unsigned long long)stats.fallbacks);
    return OBJ_VAL(copyString(buf, (int)strlen(buf)));
}

/* jit_reset_stats() -> nil */
static Value nativeJitResetStats(int argc, Value* args) {
    (void)argc; (void)args;
    nq_jit_reset_stats();
    return NIL_VAL;
}

// is_nil(v) / is_num(v) / is_str(v) / is_bool(v) / is_array(v) — type predicates
static Value nativeIsNil(int argc, Value* args)   { return BOOL_VAL(argc==1 && IS_NIL(args[0]));    }
static Value nativeIsNum(int argc, Value* args)   { return BOOL_VAL(argc==1 && IS_NUMBER(args[0])); }

/* bool(v) — coerce any value to boolean using truthiness rules */
static Value nativeBool(int argc, Value* args) {
    if (argc != 1) {
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString("TypeError: bool() expects 1 argument", 36));
        return NIL_VAL;
    }
    return BOOL_VAL(isTruthy(args[0]));
}

/*
 * error_type(e) — extract the type prefix from a typed error string.
 * "TypeError: foo" → "TypeError"
 * "hello" (no prefix) → "RuntimeError"
 */
static Value nativeErrorType(int argc, Value* args) {
    if (argc != 1 || !IS_STRING(args[0])) return OBJ_VAL(copyString("RuntimeError", 12));
    const char* s = AS_CSTRING(args[0]);
    const char* colon = strstr(s, ": ");
    if (!colon) return OBJ_VAL(copyString("RuntimeError", 12));
    /* Check it looks like a known type name (no spaces before the colon) */
    int prefix_len = (int)(colon - s);
    if (prefix_len <= 0 || prefix_len > 20) return OBJ_VAL(copyString("RuntimeError", 12));
    for (int i = 0; i < prefix_len; i++) {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
            return OBJ_VAL(copyString("RuntimeError", 12));
    }
    return OBJ_VAL(copyString(s, prefix_len));
}
static Value nativeIsStr(int argc, Value* args)   { return BOOL_VAL(argc==1 && IS_STRING(args[0])); }
static Value nativeIsBool(int argc, Value* args)  { return BOOL_VAL(argc==1 && IS_BOOL(args[0]));   }
static Value nativeIsArray(int argc, Value* args) { return BOOL_VAL(argc==1 && IS_ARRAY(args[0]));  }

/* ord(ch) — return the ASCII/Unicode code point of the first char of a string */
static Value nativeOrd(int argc, Value* args) {
    if (argc != 1 || !IS_STRING(args[0])) {
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString("ord: expected a string argument", 31));
        return NIL_VAL;
    }
    ObjString* s = AS_STRING(args[0]);
    if (s->length == 0) {
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString("ord: string must not be empty", 29));
        return NIL_VAL;
    }
    return NUMBER_VAL((double)(unsigned char)s->chars[0]);
}

/* chr(n) — return a one-character string for the given code point */
static Value nativeChr(int argc, Value* args) {
    if (argc != 1 || !IS_NUMBER(args[0])) {
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString("chr: expected a number argument", 31));
        return NIL_VAL;
    }
    int code = (int)AS_NUMBER(args[0]);
    if (code < 0 || code > 127) {
        char msg[64];
        snprintf(msg, sizeof(msg), "chr: code point %d out of range (0-127)", code);
        if (g_vm_for_error) g_vm_for_error->thrown =
            OBJ_VAL(copyString(msg, (int)strlen(msg)));
        return NIL_VAL;
    }
    char buf[2] = { (char)code, '\0' };
    return OBJ_VAL(copyString(buf, 1));
}

static void registerNative(VM* vm, const char* name, NativeFn fn, int arity) {
    ObjNative*  native   = newNative(fn, name, arity);
    ObjString*  key      = copyString(name, (int)strlen(name));
    tableSet(&vm->globals, key, OBJ_VAL(native));
}


/* ── exit / env / sleep / input natives ────────────────────────────── */
#include <stdlib.h>
#include <time.h>

static Value nativeExit(int argc, Value* args) {
    int code = (argc >= 1 && IS_NUMBER(args[0])) ? (int)AS_NUMBER(args[0]) : 0;
    exit(code);
    return NIL_VAL; /* unreachable */
}

static Value nativeEnv(int argc, Value* args) {
    if (argc < 1 || !IS_STRING(args[0])) {
        { if (g_vm_for_error) {
            ObjString* _e = copyString("ValueError: env() requires a string argument.", 45);
            throwError(g_vm_for_error, OBJ_VAL(_e));
          } return NIL_VAL; }
    }
    const char* val = getenv(AS_STRING(args[0])->chars);
    if (!val) return NIL_VAL;
    return OBJ_VAL(copyString(val, (int)strlen(val)));
}

static Value nativeSleep(int argc, Value* args) {
    if (argc < 1 || !IS_NUMBER(args[0])) {
        { if (g_vm_for_error) {
            ObjString* _e = copyString("ValueError: sleep() requires a number (milliseconds).", 52);
            throwError(g_vm_for_error, OBJ_VAL(_e));
          } return NIL_VAL; }
    }
    long ms = (long)AS_NUMBER(args[0]);
    if (ms > 0) {
        /* portable ms sleep: loop sleep(1) for large values, busy-wait otherwise */
        if (ms >= 1000) { sleep((unsigned int)(ms / 1000)); ms %= 1000; }
        if (ms > 0) {
            volatile int dummy = 0;
            for (long _i = 0; _i < ms * 10000L; _i++) dummy++;
        }
    }
    return NIL_VAL;
}

static Value nativeInput(int argc, Value* args) {
    /* input(prompt) — print prompt (no newline), read one line from stdin */
    if (argc >= 1 && IS_STRING(args[0])) {
        printf("%s", AS_STRING(args[0])->chars);
        fflush(stdout);
    }
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return NIL_VAL;
    int n = (int)strlen(buf);
    if (n > 0 && buf[n-1] == '\n') buf[--n] = '\0';
    return OBJ_VAL(copyString(buf, n));
}

void initVM(VM* vm) {
    vm->stack_top   = vm->stack;
    vm->frame_count = 0;
    vm->source_path = NULL;
    vm->try_depth   = 0;
    vm->thrown      = NIL_VAL;
    initTable(&vm->globals);
    initStringTable(&nq_string_table);
    nq_set_args(vm, 0, NULL);  // ensure ARGS is always available

    // Register built-in functions
    registerNative(vm, "input",      nativeInput,      -1);
    registerNative(vm, "str",        nativeStr,         1);
    registerNative(vm, "num",        nativeNum,         1);
    registerNative(vm, "type",       nativeType,        1);
    // array
    registerNative(vm, "len",        nativeLen,         1);
    registerNative(vm, "push",       nativePush,        2);
    registerNative(vm, "pop",        nativePop,         1);
    registerNative(vm, "remove",     nativeRemove,      2);
    registerNative(vm, "contains",   nativeContains,    2);
    // math
    registerNative(vm, "sqrt",       nativeSqrt,        1);
    registerNative(vm, "floor",      nativeFloor,       1);
    registerNative(vm, "ceil",       nativeCeil,        1);
    registerNative(vm, "round",      nativeRound,       1);
    registerNative(vm, "abs",        nativeAbs,         1);
    registerNative(vm, "pow",        nativePow,         2);
    registerNative(vm, "max",        nativeMax,         2);
    registerNative(vm, "min",        nativeMin,         2);
    // string
    registerNative(vm, "upper",      nativeUpper,       1);
    registerNative(vm, "lower",      nativeLower,       1);
    registerNative(vm, "slice",      nativeSlice,      -1); // 2 or 3 args
    registerNative(vm, "trim",       nativeTrim,        1);
    registerNative(vm, "replace",    nativeReplace,     3);
    registerNative(vm, "split",      nativeSplit,       2);
    registerNative(vm, "startswith", nativeStartsWith,  2);
    registerNative(vm, "endswith",   nativeEndsWith,    2);
    // extended stdlib (v0.5.0)
    srand((unsigned int)time(NULL));
    registerNative(vm, "random",     nativeRandom,      0);
    registerNative(vm, "rand_int",   nativeRandInt,     2);
    registerNative(vm, "index",      nativeIndex,       2);
    registerNative(vm, "repeat",     nativeRepeat,      2);
    registerNative(vm, "join",       nativeJoin,        2);
    registerNative(vm, "sort",       nativeSort,        1);
    // error handling (v0.6.0)
    g_vm_for_error = vm;
    registerNative(vm, "error",       nativeError,       1);
    // file I/O (v0.7.0)
    registerNative(vm, "file_read",   nativeFileRead,    1);
    registerNative(vm, "file_write",  nativeFileWrite,   2);
    registerNative(vm, "file_append", nativeFileAppend,  2);
    registerNative(vm, "file_exists", nativeFileExists,  1);
    registerNative(vm, "file_lines",  nativeFileLines,   1);
    // GC (v0.8.0)
    registerNative(vm, "gc_collect",  nativeGcCollect,   0);
    // stabilization builtins (v0.9.0)
    registerNative(vm, "assert",      nativeAssert,     -1); // 1 or 2 args
    registerNative(vm, "clock",       nativeClock,       0);
    registerNative(vm, "mem_usage",   nativeMemUsage,    0);
    registerNative(vm, "jit_enabled", nativeJitEnabled,  0);
    registerNative(vm, "jit_enable",  nativeJitEnable,   1);
    registerNative(vm, "jit_stats",   nativeJitStats,    0);
    registerNative(vm, "jit_reset_stats", nativeJitResetStats, 0);
    registerNative(vm, "is_nil",      nativeIsNil,       1);
    registerNative(vm, "is_num",      nativeIsNum,       1);
    registerNative(vm, "bool",        nativeBool,        1);
    registerNative(vm, "error_type",  nativeErrorType,   1);
    registerNative(vm, "is_str",      nativeIsStr,       1);
    registerNative(vm, "is_bool",     nativeIsBool,      1);
    registerNative(vm, "is_array",    nativeIsArray,     1);
    registerNative(vm, "ord",         nativeOrd,         1);
    registerNative(vm, "chr",         nativeChr,         1);
    /* System natives */
    registerNative(vm, "exit",        nativeExit,       -1);
    registerNative(vm, "env",         nativeEnv,         1);
    registerNative(vm, "sleep",       nativeSleep,       1);
    registerNative(vm, "input",       nativeInput,      -1);
}


void freeVM(VM* vm) {
    nq_jit_flush_cache(); // release executable pages from JIT cache
    freeTable(&vm->globals);
    freeStringTable(&nq_string_table);
    freeAllObjects();
}

// ─────────────────────────────────────────────
//  Stack
// ─────────────────────────────────────────────
static void push(VM* vm, Value v) {
    if (vm->stack_top >= vm->stack + NQ_STACK_MAX) {
        fprintf(stderr,
            NQ_COLOR_RED "[RuntimeError]" NQ_COLOR_RESET
            " Stack overflow! Possible infinite recursion.\n");
        exit(1);
    }
    *vm->stack_top++ = v;
}

static Value pop(VM* vm)              { return *--vm->stack_top; }
static Value peek(VM* vm, int dist)   { return vm->stack_top[-1 - dist]; }

// ─────────────────────────────────────────────
//  Runtime error
// ─────────────────────────────────────────────
void vmRuntimeError(VM* vm, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    /*
     * Format the message first so we can extract the error type prefix.
     * e.g. "TypeError: ..."  →  header = "[TypeError]"
     *      "NameError: ..."  →  header = "[NameError]"
     *      "plain message"   →  header = "[RuntimeError]"
     */
    char msg[1024];
    va_list args2;
    va_copy(args2, args);
    vsnprintf(msg, sizeof(msg), fmt, args2);
    va_end(args2);

    /* Extract type prefix: word before the first ": " */
    const char* colon = strstr(msg, ": ");
    char header[64] = "RuntimeError";
    if (colon && colon > msg) {
        int prefix_len = (int)(colon - msg);
        /* Validate: prefix must be all letters, max 20 chars */
        if (prefix_len <= 20 && prefix_len > 0) {
            int valid = 1;
            for (int i = 0; i < prefix_len; i++) {
                char c = msg[i];
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
                    valid = 0; break;
                }
            }
            if (valid) {
                snprintf(header, sizeof(header), "%.*s", prefix_len, msg);
            }
        }
    }

    fprintf(stderr, NQ_COLOR_RED "[%s]" NQ_COLOR_RESET, header);

    // Print stack trace (innermost first)
    for (int i = vm->frame_count - 1; i >= 0; i--) {
        CallFrame*   frame = &vm->frames[i];
        ObjFunction* fn    = frame->function;
        int offset = (int)(frame->ip - fn->chunk->code - 1);
        if (offset < 0) offset = 0;
        int line = fn->chunk->lines[offset];
        if (i == vm->frame_count - 1) {
            const char* src = vm->source_path ? vm->source_path : "<script>";
            fprintf(stderr, " %s:%d\n", src, line);
        } else {
            if (fn->name)
                fprintf(stderr, "  called from '%s' line %d\n", fn->name->chars, line);
        }
        if (fn->name)
            fprintf(stderr, NQ_COLOR_YELLOW "  in function '%s'" NQ_COLOR_RESET "\n",
                    fn->name->chars);
    }

    fprintf(stderr, NQ_COLOR_RED "  " NQ_COLOR_RESET);
    fprintf(stderr, "%s", msg);
    fprintf(stderr, "\n\n");
    va_end(args);
}

// throwError — set vm->thrown and return true if a try handler caught it.
// The caller should unwind the stack and jump to the catch block.
static bool throwError(VM* vm, Value error_val) {
    vm->thrown = error_val;
    if (vm->try_depth > 0) {
        TryHandler* h = &vm->try_stack[--vm->try_depth];
        // Restore stack and frame count
        vm->stack_top   = h->stack_top;
        vm->frame_count = h->frame_count;
        // Push error value for the catch variable
        vm->stack_top[0] = error_val;
        vm->stack_top++;
        // Redirect the active frame's ip to the catch block
        vm->frames[vm->frame_count - 1].ip = h->catch_ip;
        return true;  // caught
    }
    return false;  // uncaught
}

// ─────────────────────────────────────────────
//  Type helpers
// ─────────────────────────────────────────────

static ObjString* valueToString(Value v) {
    if (IS_STRING(v)) return AS_STRING(v);
    char buf[64];
    if (IS_NIL(v))       snprintf(buf, sizeof(buf), "null");
    else if (IS_BOOL(v)) snprintf(buf, sizeof(buf), "%s", AS_BOOL(v) ? "true" : "false");
    else if (IS_NUMBER(v)) {
        double n = AS_NUMBER(v);
        if (n == (long long)n && n >= -9007199254740992.0 && n <= 9007199254740992.0)
            snprintf(buf, sizeof(buf), "%lld", (long long)n);
        else snprintf(buf, sizeof(buf), "%.14g", n);
    } else snprintf(buf, sizeof(buf), "<object>");
    return copyString(buf, (int)strlen(buf));
}

static Value concatStrings(ObjString* a, ObjString* b) {
    int len  = a->length + b->length;
    char* buf = (char*)nq_realloc(NULL, 0, (size_t)(len + 1));
    memcpy(buf,             a->chars, (size_t)a->length);
    memcpy(buf + a->length, b->chars, (size_t)b->length);
    buf[len] = '\0';
    return OBJ_VAL(takeString(buf, len));
}

// ─────────────────────────────────────────────
//  Function call
// ─────────────────────────────────────────────
static bool callFunction(VM* vm, ObjFunction* fn, int argc) {
    if (argc > fn->arity) {
        vmRuntimeError(vm,
            "TypeError: function '%s' expects %d argument(s), but got %d.",
            fn->name ? fn->name->chars : "<script>",
            fn->arity, argc);
        return false;
    }
    if (vm->frame_count >= NQ_FRAMES_MAX) {
        vmRuntimeError(vm, "Stack overflow — too many nested function calls.");
        return false;
    }
    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->function  = fn;
    frame->ip        = fn->chunk->code;
    frame->slots     = vm->stack_top - argc - 1;
    /* Push nil for any missing arguments (filled in by default param code) */
    while (argc < fn->arity) {
        push(vm, NIL_VAL);
        argc++;
    }
    return true;
}

// ─────────────────────────────────────────────
//  Main execution loop
// ─────────────────────────────────────────────
// ─────────────────────────────────────────────
//  "Did you mean?" — Levenshtein distance suggestion
// ─────────────────────────────────────────────
static int levenshtein(const char* a, int la, const char* b, int lb) {
    if (la > 32 || lb > 32) return 99;
    int prev[33], curr[33];
    for (int j = 0; j <= lb; j++) prev[j] = j;
    for (int i = 1; i <= la; i++) {
        curr[0] = i;
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int del  = prev[j]   + 1;
            int ins  = curr[j-1] + 1;
            int sub  = prev[j-1] + cost;
            curr[j]  = del < ins ? (del < sub ? del : sub) : (ins < sub ? ins : sub);
        }
        for (int j = 0; j <= lb; j++) prev[j] = curr[j];
    }
    return prev[lb];
}

static const char* didYouMean(VM* vm, const char* name) {
    int best_dist   = 3;
    const char* best = NULL;
    int nlen = (int)strlen(name);
    for (int i = 0; i < vm->globals.capacity; i++) {
        Entry* e = &vm->globals.entries[i];
        if (!e->key) continue;
        int d = levenshtein(name, nlen, e->key->chars, e->key->length);
        if (d < best_dist) { best_dist = d; best = e->key->chars; }
    }
    return best;
}

InterpretResult runVM(VM* vm, ObjFunction* script, const char* source_path) {
    nq_gc_vm        = vm;  // register VM for GC trigger
    g_vm_for_error  = vm;
    vm->source_path = source_path;
    push(vm, OBJ_VAL(script));
    callFunction(vm, script, 0);
    CallFrame* frame = &vm->frames[vm->frame_count - 1];

/* Register-cached frame fields — updated when frame changes (CALL/RETURN/THROW) */
#define LOAD_FRAME() \
    frame = &vm->frames[vm->frame_count - 1]

#define READ_BYTE()    (*frame->ip++)
#define READ_UINT16()  (frame->ip += 2, \
                        (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONST()   (frame->function->chunk->constants.values[READ_UINT16()])

// THROW_ERROR: format, make ObjString error, try to catch or abort
#define THROW_ERROR(...) do { \
    char _ebuf[512]; \
    snprintf(_ebuf, sizeof(_ebuf), __VA_ARGS__); \
    ObjString* _estr = copyString(_ebuf, (int)strlen(_ebuf)); \
    if (throwError(vm, OBJ_VAL(_estr))) { \
        frame = &vm->frames[vm->frame_count - 1]; \
        goto dispatch_loop_top; \
    } \
    vmRuntimeError(vm, "%s", _ebuf); \
    return INTERPRET_RUNTIME_ERROR; \
} while (0)

#define BINARY_NUM(op) do { \
    if (NQ_UNLIKELY(!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1)))) { \
        THROW_ERROR("TypeError: arithmetic requires numbers, got %s and %s.", \
            IS_NIL(peek(vm,1)) ? "null" : IS_BOOL(peek(vm,1)) ? "bool" : \
            IS_STRING(peek(vm,1)) ? "string" : IS_ARRAY(peek(vm,1)) ? "array" : "number", \
            IS_NIL(peek(vm,0)) ? "null" : IS_BOOL(peek(vm,0)) ? "bool" : \
            IS_STRING(peek(vm,0)) ? "string" : IS_ARRAY(peek(vm,0)) ? "array" : "number"); \
    } \
    double b = AS_NUMBER(pop(vm)); \
    double a = AS_NUMBER(pop(vm)); \
    push(vm, NUMBER_VAL(a op b)); \
} while (0)

    for (;;) {
        dispatch_loop_top: ;
        uint8_t instruction = READ_BYTE();
        switch (instruction) {

            case OP_CONST:  push(vm, READ_CONST()); break;
            case OP_NIL:    push(vm, NIL_VAL);       break;
            case OP_TRUE:   push(vm, BOOL_VAL(true));  break;
            case OP_FALSE:  push(vm, BOOL_VAL(false)); break;
            case OP_POP:    pop(vm); break;
            case OP_DUP:    push(vm, peek(vm, 0)); break;

            case OP_DEFINE_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONST());
                tableSet(&vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONST());
                Value val;
                if (!tableGet(&vm->globals, name, &val)) {
                    const char* suggestion = didYouMean(vm, name->chars);
                    if (suggestion)
                        THROW_ERROR("NameError: '%s' is not defined. Did you mean '%s'?",
                            name->chars, suggestion);
                    else
                        THROW_ERROR("NameError: '%s' is not defined.", name->chars);
                }
                push(vm, val);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = AS_STRING(READ_CONST());
                if (tableSet(&vm->globals, name, peek(vm, 0))) {
                    tableDelete(&vm->globals, name);
                    THROW_ERROR("NameError: cannot assign to '%s' — it has not been declared. Use 'let %s = value' first.",
                        name->chars, name->chars);
                }
                break;
            }
            case OP_GET_LOCAL: { uint8_t slot = READ_BYTE(); push(vm, frame->slots[slot]); break; }
            case OP_SET_LOCAL: { uint8_t slot = READ_BYTE(); frame->slots[slot] = peek(vm, 0); break; }

            case OP_ADD: BINARY_NUM(+); break;
            case OP_SUB: BINARY_NUM(-); break;
            case OP_MUL: {
                /* number * number  OR  string * number (repeat) */
                if (IS_STRING(peek(vm,1)) && IS_NUMBER(peek(vm,0))) {
                    int n = (int)AS_NUMBER(pop(vm));
                    ObjString* s = AS_STRING(pop(vm));
                    if (n <= 0) {
                        push(vm, OBJ_VAL(copyString("", 0)));
                    } else {
                        int slen = s->length;
                        int total = slen * n;
                        char* buf = (char*)malloc((size_t)(total + 1));
                        for (int ri = 0; ri < n; ri++)
                            memcpy(buf + ri * slen, s->chars, (size_t)slen);
                        buf[total] = '\0';
                        ObjString* r = takeString(buf, total);
                        push(vm, OBJ_VAL(r));
                    }
                } else {
                    BINARY_NUM(*);
                }
                break;
            }
            case OP_DIV: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) {
                    THROW_ERROR("TypeError: division requires numbers.");
                }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm));
                if (b == 0.0) { THROW_ERROR("ValueError: division by zero. Check the divisor before dividing."); }
                push(vm, NUMBER_VAL(a / b));
                break;
            }
            case OP_MOD: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1))) {
                    THROW_ERROR("TypeError: modulo requires numbers.");
                }
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm));
                if (b == 0.0) { THROW_ERROR("ValueError: modulo by zero."); }
                push(vm, NUMBER_VAL(fmod(a, b)));
                break;
            }
            case OP_NEGATE: {
                Value v = peek(vm, 0);
                if (!IS_NUMBER(v)) {
                    THROW_ERROR("TypeError: cannot apply '-' to a %s value. Only numbers can be negated (e.g. let x = -5)",
                        IS_STRING(v) ? "string" : IS_BOOL(v) ? "bool" : IS_NIL(v) ? "nil" : "object");
                }
                pop(vm); push(vm, NUMBER_VAL(-AS_NUMBER(v)));
                break;
            }
            case OP_CONCAT: {
                Value b_val = pop(vm), a_val = pop(vm);
                push(vm, concatStrings(valueToString(a_val), valueToString(b_val)));
                break;
            }
            case OP_EQ:  { Value b = pop(vm), a = pop(vm); push(vm, BOOL_VAL( valuesEqual(a, b))); break; }
            case OP_NEQ: { Value b = pop(vm), a = pop(vm); push(vm, BOOL_VAL(!valuesEqual(a, b))); break; }
            case OP_LT: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1)))
                    THROW_ERROR("TypeError: '<' requires numbers.");
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm)); push(vm, BOOL_VAL(a < b)); break;
            }
            case OP_GT: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1)))
                    THROW_ERROR("TypeError: '>' requires numbers.");
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm)); push(vm, BOOL_VAL(a > b)); break;
            }
            case OP_LTE: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1)))
                    THROW_ERROR("TypeError: '<=' requires numbers.");
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm)); push(vm, BOOL_VAL(a <= b)); break;
            }
            case OP_GTE: {
                if (!IS_NUMBER(peek(vm,0)) || !IS_NUMBER(peek(vm,1)))
                    THROW_ERROR("TypeError: '>=' requires numbers.");
                double b = AS_NUMBER(pop(vm)), a = AS_NUMBER(pop(vm)); push(vm, BOOL_VAL(a >= b)); break;
            }
            case OP_NOT: { Value v = pop(vm); push(vm, BOOL_VAL(!isTruthy(v))); break; }

            case OP_PRINT: { printValue(pop(vm)); printf("\n"); break; }

            case OP_BUILD_ARRAY: {
                uint8_t count = READ_BYTE();
                ObjArray* arr = newArray();
                // items are on stack in order, bottom to top
                Value* base = vm->stack_top - count;
                for (int i = 0; i < count; i++) arrayPush(arr, base[i]);
                vm->stack_top -= count;
                push(vm, OBJ_VAL(arr));
                break;
            }
            case OP_GET_INDEX: {
                Value idx_val = pop(vm);
                Value obj_val = pop(vm);
                if (IS_ARRAY(obj_val)) {
                    if (!IS_NUMBER(idx_val))
                        THROW_ERROR("TypeError: array index must be a number, got %s.",
                            IS_STRING(idx_val) ? "string" : IS_BOOL(idx_val) ? "bool" : "null");
                    ObjArray* arr = AS_ARRAY(obj_val);
                    int idx = (int)AS_NUMBER(idx_val);
                    if (idx < 0) idx = arr->count + idx;
                    if (idx < 0 || idx >= arr->count)
                        THROW_ERROR("IndexError: array index %d is out of bounds (length %d).",
                            (int)AS_NUMBER(idx_val), arr->count);
                    push(vm, arr->items[idx]);
                } else if (IS_STRING(obj_val)) {
                    if (!IS_NUMBER(idx_val))
                        THROW_ERROR("TypeError: string index must be a number.");
                    ObjString* s = AS_STRING(obj_val);
                    int idx = (int)AS_NUMBER(idx_val);
                    if (idx < 0) idx = s->length + idx;
                    if (idx < 0 || idx >= s->length)
                        THROW_ERROR("IndexError: string index %d is out of bounds (length %d).",
                            (int)AS_NUMBER(idx_val), s->length);
                    push(vm, OBJ_VAL(copyString(&s->chars[idx], 1)));
                } else {
                    THROW_ERROR("TypeError: only arrays and strings support indexing, not %s.",
                        IS_NUMBER(obj_val) ? "number" : IS_BOOL(obj_val) ? "bool" : "null");
                }
                break;
            }
            case OP_SET_INDEX: {
                Value val     = pop(vm);
                Value idx_val = pop(vm);
                Value obj_val = pop(vm);
                if (!IS_ARRAY(obj_val)) {
                    THROW_ERROR("TypeError: only arrays support index assignment.");
                }
                if (!IS_NUMBER(idx_val)) {
                    THROW_ERROR("TypeError: array index must be a number.");
                }
                arraySet(AS_ARRAY(obj_val), (int)AS_NUMBER(idx_val), val);
                break;
            }

            case OP_SLICE: {
                /*
                 * Stack: obj, start, end  (end is on top)
                 * start/end may be NIL (omitted in source) → default to 0 / len
                 * Supports arrays and strings.
                 * Negative indices: -1 = last element, etc.
                 */
                Value end_val   = pop(vm);
                Value start_val = pop(vm);
                Value obj_val   = pop(vm);

                if (IS_STRING(obj_val)) {
                    ObjString* s = AS_STRING(obj_val);
                    int len = s->length;
                    int start = IS_NIL(start_val) ? 0 : (int)AS_NUMBER(start_val);
                    int end   = IS_NIL(end_val)   ? len : (int)AS_NUMBER(end_val);
                    if (start < 0) start = len + start;
                    if (end   < 0) end   = len + end;
                    if (start < 0)   start = 0;
                    if (end > len)   end   = len;
                    if (start > end) start = end;
                    push(vm, OBJ_VAL(copyString(s->chars + start, end - start)));
                } else if (IS_ARRAY(obj_val)) {
                    ObjArray* arr = AS_ARRAY(obj_val);
                    int len = arr->count;
                    int start = IS_NIL(start_val) ? 0 : (int)AS_NUMBER(start_val);
                    int end   = IS_NIL(end_val)   ? len : (int)AS_NUMBER(end_val);
                    if (start < 0) start = len + start;
                    if (end   < 0) end   = len + end;
                    if (start < 0)   start = 0;
                    if (end > len)   end   = len;
                    if (start > end) start = end;
                    /* Build a new array with the slice */
                    ObjArray* result = newArray();
                    push(vm, OBJ_VAL(result)); /* GC root */
                    for (int i = start; i < end; i++)
                        arrayPush(result, arr->items[i]);
                    pop(vm); /* un-root */
                    push(vm, OBJ_VAL(result));
                } else {
                    THROW_ERROR("TypeError: slice requires an array or string, not %s.",
                        IS_NIL(obj_val)    ? "nil"    :
                        IS_NUMBER(obj_val) ? "number" :
                        IS_BOOL(obj_val)   ? "bool"   : "value");
                }
                break;
            }

            case OP_TRY: {
                uint16_t offset = READ_UINT16();
                if (vm->try_depth >= NQ_TRY_MAX) {
                    vmRuntimeError(vm, "Too many nested try blocks (max %d).", NQ_TRY_MAX);
                    return INTERPRET_RUNTIME_ERROR;
                }
                TryHandler* h  = &vm->try_stack[vm->try_depth++];
                h->catch_ip    = frame->ip + offset;
                h->stack_top   = vm->stack_top;
                h->frame_count = vm->frame_count;
                break;
            }
            case OP_TRY_END: {
                if (vm->try_depth > 0) vm->try_depth--;
                break;
            }
            case OP_THROW: {
                Value err = pop(vm);
                // Ensure it's a string message
                if (!IS_STRING(err)) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), IS_NUMBER(err) ? "%g" :
                             IS_BOOL(err) ? (AS_BOOL(err) ? "true" : "false") : "null",
                             IS_NUMBER(err) ? AS_NUMBER(err) : 0.0);
                    err = OBJ_VAL(copyString(buf, (int)strlen(buf)));
                }
                if (throwError(vm, err)) {
                    frame = &vm->frames[vm->frame_count - 1];
                    break;
                }
                vmRuntimeError(vm, "%s", AS_CSTRING(err));
                return INTERPRET_RUNTIME_ERROR;
            }

            case OP_JUMP:          { uint16_t off = READ_UINT16(); frame->ip += off; break; }
            case OP_JUMP_IF_FALSE: { uint16_t off = READ_UINT16(); if (NQ_UNLIKELY(!isTruthy(peek(vm, 0)))) frame->ip += off; break; }
            case OP_LOOP:          { uint16_t off = READ_UINT16(); frame->ip -= off; break; }

            case OP_JIT_FOR_RANGE: {
                /*
                 * Template-JIT a numeric for loop.
                 *
                 * Read loop parameters from bytecode, build a JITLoopSpec,
                 * call the JIT (generates + executes native code on x86-64,
                 * or runs a C tight-loop on other architectures).
                 *
                 * After this opcode, the loop is fully complete.
                 * Hidden locals (__i, __stop, __step) were pushed onto the
                 * stack before this opcode; endScope already emitted their
                 * POPs so we have nothing to clean up here.
                 *
                 * The Value struct layout:
                 *   type  (4 bytes) | padding (4 bytes) | as.number (8 bytes)
                 * So the double is at byte offset 8 within each Value.
                 */
                uint8_t  i_slot    = READ_BYTE();
                uint8_t  stop_slot = READ_BYTE();
                uint16_t sc_idx    = READ_UINT16();
                uint8_t  n_vars    = READ_BYTE();

                /* Build spec */
                JITLoopSpec spec;
                /* .as.number is at offset 8 in Value (type=4, padding=4) */
                spec.i_ptr = &frame->slots[i_slot].as.number;
                spec.stop  = AS_NUMBER(frame->function->chunk->constants.values[sc_idx]);
                spec.step  = AS_NUMBER(frame->function->chunk->constants.values[sc_idx]);
                /* step is the same const for now — actually step = the value at sc_idx */
                /* (stop was evaluated at runtime and stored in the slot) */
                spec.stop  = frame->slots[stop_slot].as.number;
                spec.step  = AS_NUMBER(frame->function->chunk->constants.values[sc_idx]);
                spec.n_vars = (int)n_vars;

                for (int k = 0; k < (int)n_vars; k++) {
                    uint8_t  vslot  = READ_BYTE();
                    uint16_t dc_idx = READ_UINT16();
                    uint8_t  vop    = READ_BYTE();
                    spec.var_ptrs[k] = &frame->slots[vslot].as.number;
                    spec.deltas[k]   = AS_NUMBER(
                        frame->function->chunk->constants.values[dc_idx]);
                    spec.ops[k]      = (JITOp)vop;
                }

                if (!nq_jit_run_loop(&spec)) {
                    runJitFallbackLoop(&spec);
                }
                break;
            }

            case OP_STORE_LOCAL: {
                /* SET_LOCAL + POP fused — pops TOS into slot, no stack residue */
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = pop(vm);
                break;
            }
            case OP_ADD_LOCAL_CONST: {
                uint8_t slot = READ_BYTE();
                Value   cv   = READ_CONST();
                /* Only works for numbers — the compiler guarantees this */
                frame->slots[slot].as.number += AS_NUMBER(cv);
                break;
            }
            case OP_LOOP_IF_LT: {
                /* if locals[a] < locals[b]: ip -= offset  (loop back) */
                uint8_t a   = READ_BYTE();
                uint8_t b   = READ_BYTE();
                uint16_t off = READ_UINT16();
                if (NQ_LIKELY(AS_NUMBER(frame->slots[a]) <
                              AS_NUMBER(frame->slots[b])))
                    frame->ip -= off;
                break;
            }

            case OP_CALL: {
                int argc = READ_BYTE();
                Value callee = peek(vm, argc);

                // Handle native functions
                if (IS_NATIVE(callee)) {
                    ObjNative* native = AS_NATIVE(callee);
                    // Validate arity (-1 = variadic) — now catchable
                    if (native->arity != -1 && argc != native->arity) {
                        THROW_ERROR(
                            "TypeError: built-in '%s' expects %d argument(s), but got %d.",
                            native->name, native->arity, argc);
                    }
                    vm->thrown = NIL_VAL;  // clear before call
                    Value result = native->fn(argc, vm->stack_top - argc);
                    vm->stack_top -= argc + 1;
                    // Check if error() was called inside the native
                    if (!IS_NIL(vm->thrown)) {
                        if (throwError(vm, vm->thrown)) {
                            frame = &vm->frames[vm->frame_count - 1];
                            break;
                        }
                        vmRuntimeError(vm, "%s", IS_STRING(vm->thrown) ? AS_CSTRING(vm->thrown) : "error");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(vm, result);
                    break;
                }

                // Calling a non-function — now catchable
                if (!IS_FUNCTION(callee)) {
                    THROW_ERROR(
                        "TypeError: only functions can be called, not %s.",
                        IS_STRING(callee) ? "string" :
                        IS_NUMBER(callee) ? "number" :
                        IS_BOOL(callee)   ? "bool"   :
                        IS_NIL(callee)    ? "nil"    : "object");
                }
                if (!callFunction(vm, AS_FUNCTION(callee), argc))
                    return INTERPRET_RUNTIME_ERROR;
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_RETURN: {
                Value result = pop(vm);
                vm->frame_count--;
                if (vm->frame_count == 0) { pop(vm); return INTERPRET_OK; }
                vm->stack_top = frame->slots;
                push(vm, result);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_HALT:
                return INTERPRET_OK;

            default:
                vmRuntimeError(vm,
                    "Unknown bytecode instruction: %d. This is an internal bug.", instruction);
                return INTERPRET_RUNTIME_ERROR;
        }
    }

#undef READ_BYTE
#undef READ_UINT16
#undef READ_CONST
#undef BINARY_NUM
}

/* ── nq_set_args ─────────────────────────────────────────────────────
 * Inject script command-line arguments as a global ARGS array.
 * Call after initVM(), before runFile(). */
void nq_set_args(VM* vm, int argc, char** argv) {
    ObjArray* arr = newArray();
    /* Use vm->stack directly to root the array without needing push() */
    *vm->stack_top = OBJ_VAL(arr);
    vm->stack_top++;
    for (int i = 0; i < argc; i++) {
        ObjString* str = copyString(argv[i], (int)strlen(argv[i]));
        *vm->stack_top = OBJ_VAL(str);
        vm->stack_top++;
        arrayPush(arr, OBJ_VAL(str));
        vm->stack_top--;
    }
    ObjString* key = copyString("ARGS", 4);
    tableSet(&vm->globals, key, OBJ_VAL(arr));
    vm->stack_top--;  /* pop arr */
}
