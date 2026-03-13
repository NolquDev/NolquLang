/*
 * codegen.h — Nolqu → C transpiler
 *
 * Translates a Nolqu AST into a self-contained C11 source file.
 * The emitted file embeds a minimal runtime (NqStr type + helpers)
 * and compiles with:
 *   gcc -O2 output.c -o program -lm
 *
 * Limitations (v1.0.0):
 *   - Arrays map to plain C arrays (fixed-size limitation)
 *   - import / try-catch are not supported in transpiled output
 *   - Type inference is heuristic (string vs double)
 */

#ifndef NQ_CODEGEN_H
#define NQ_CODEGEN_H

#include "ast.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NQ_SYM_MAX 256

typedef struct {
    char name[64];
    bool is_str;    /* true → NqStr, false → double */
} NqSym;

typedef struct {
    FILE*  out;
    int    indent;
    int    tmp_count;
    bool   had_error;
    char   error_msg[256];
    NqSym  syms[NQ_SYM_MAX];
    int    nsyms;
} CodeGen;

void codegen_init(CodeGen* cg, FILE* out);
void codegen_emit(CodeGen* cg, ASTNode* ast);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_CODEGEN_H */
