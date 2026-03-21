/*
 * parser.h — Nolqu recursive-descent parser
 *
 * Converts a flat Token stream (produced by the Lexer) into an AST.
 * The parser uses one token of lookahead (LL(1)) and implements
 * Pratt-style precedence climbing for expressions.
 *
 * Errors are reported in-place (to stderr); the parser continues in
 * "panic mode" to find more errors before stopping.
 */

#ifndef NQ_PARSER_H
#define NQ_PARSER_H

#include "common.h"
#include "lexer.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Parser state ────────────────────────────────────────────────── */
typedef struct {
    Lexer       lexer;
    Token       current;
    Token       previous;
    bool        had_error;
    bool        panic_mode;     /* suppress cascading errors          */
    const char* source_path;
    bool        repl_mode;      /* true when parsing REPL input; allows bare expressions */
    bool        silent_errors;  /* suppress error output (used by interpolation inner parser) */
} Parser;

/* ── Parser API ──────────────────────────────────────────────────── */
void     initParser(Parser* p, const char* source, const char* path);

/*
 * parse() — consume all tokens and return a NODE_PROGRAM root.
 * Returns a valid (possibly empty) AST even on error;
 * check Parser.had_error afterwards.
 */
ASTNode* parse(Parser* p);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_PARSER_H */
