/*
 * compiler.h — Nolqu AST-to-bytecode compiler
 *
 * The compiler walks the AST produced by the parser and emits bytecode
 * into a new ObjFunction (the "script" function that the VM executes).
 *
 * One CompilerCtx is allocated per lexical scope (script + each
 * function body).  Nested functions create a chain:
 *   script_ctx → outer_fn_ctx → inner_fn_ctx → …
 *
 * Local variables are stack-allocated (resolved to slot indices at
 * compile time).  Globals are looked up by name at runtime.
 */

#ifndef NQ_COMPILER_H
#define NQ_COMPILER_H

#include "common.h"
#include "ast.h"
#include "chunk.h"
#include "object.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Local variable descriptor ───────────────────────────────────── */
typedef struct {
    char  name[64];
    int   depth;            /* scope depth at declaration             */
    bool  initialized;      /* false until the initialiser is compiled*/
    bool  used;             /* set when OP_GET_LOCAL references this  */
    bool  is_const;         /* true if declared with 'const'          */
    int   decl_line;        /* source line — for "unused var" warning */
} Local;

/* ── Compiler context — one per function / script ────────────────── */
typedef struct CompilerCtx {
    struct CompilerCtx* enclosing;   /* parent scope                  */
    ObjFunction*        function;    /* function being compiled        */
    int                 scope_depth; /* 0 = global scope              */

    Local locals[NQ_LOCALS_MAX];
    int   local_count;
} CompilerCtx;

/* ── Compilation result ──────────────────────────────────────────── */
typedef struct {
    ObjFunction* function;  /* NULL on compile error                  */
    bool         had_error;
} CompileResult;

/* ── Compiler API ────────────────────────────────────────────────── */
/*
 * compile() — translate an AST into executable bytecode.
 *
 * Returns a CompileResult.  On success, result.function is a fully
 * initialised top-level ObjFunction ready to pass to runVM().
 * On failure, result.had_error is true and result.function is NULL.
 */
CompileResult compile(ASTNode* ast, const char* source_path);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_COMPILER_H */
