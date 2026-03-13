/*
 * lexer.h — Nolqu tokeniser
 *
 * Converts raw source text into a flat stream of Tokens.
 * The lexer is a simple hand-written scanner; it holds a pointer into
 * the source string and does not copy any text (all Token.start
 * pointers point directly into the source buffer).
 *
 * The caller must keep the source buffer alive for the lifetime of
 * any Tokens derived from it.
 */

#ifndef NQ_LEXER_H
#define NQ_LEXER_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Token types ─────────────────────────────────────────────────── */
typedef enum {
    /* Literals */
    TK_NUMBER,      /* 42 / 3.14                                     */
    TK_STRING,      /* "hello"  (escape sequences resolved at parse)  */
    TK_IDENT,       /* variable / function name                       */

    /* Keywords */
    TK_LET,         /* let      */
    TK_PRINT,       /* print    */
    TK_IF,          /* if       */
    TK_ELSE,        /* else     */
    TK_LOOP,        /* loop     */
    TK_FUNCTION,    /* function */
    TK_RETURN,      /* return   */
    TK_IMPORT,      /* import   */
    TK_TRY,         /* try      */
    TK_CATCH,       /* catch    */
    TK_END,         /* end      */
    TK_TRUE,        /* true     */
    TK_FALSE,       /* false    */
    TK_NIL,         /* nil      */
    TK_AND,         /* and      */
    TK_OR,          /* or       */
    TK_NOT,         /* not      */

    /* Arithmetic operators */
    TK_PLUS,        /* +  */
    TK_MINUS,       /* -  */
    TK_STAR,        /* *  */
    TK_SLASH,       /* /  */
    TK_PERCENT,     /* %  */

    /* Relational / equality */
    TK_EQ,          /* =   (assignment)       */
    TK_EQEQ,        /* ==  (equality test)    */
    TK_BANGEQ,      /* !=                     */
    TK_BANG,        /* !   (prefix not)       */
    TK_LT,          /* <   */
    TK_GT,          /* >   */
    TK_LTEQ,        /* <=  */
    TK_GTEQ,        /* >=  */

    TK_DOTDOT,      /* ..  (string concat)    */

    /* Punctuation */
    TK_LPAREN,      /* (  */
    TK_RPAREN,      /* )  */
    TK_COMMA,       /* ,  */
    TK_LBRACKET,    /* [  */
    TK_RBRACKET,    /* ]  */

    /* Structural */
    TK_NEWLINE,     /* \n  statement terminator                      */
    TK_EOF,         /* end of source                                 */
    TK_ERROR,       /* lexer error (Token.error describes it)        */
} TokenType;

/* ── Token ───────────────────────────────────────────────────────── */
typedef struct {
    TokenType   type;
    const char* start;      /* pointer into source buffer             */
    int         length;
    int         line;
    const char* error;      /* non-NULL only when type == TK_ERROR    */
} Token;

/* ── Lexer state ─────────────────────────────────────────────────── */
typedef struct {
    const char* source;
    const char* start;      /* start of the token being scanned       */
    const char* current;    /* current scan position                  */
    int         line;
} Lexer;

/* ── Lexer API ───────────────────────────────────────────────────── */
void  initLexer(Lexer* lex, const char* source);
Token nextToken(Lexer* lex);

/* Debug utility — returns a printable name for any TokenType value. */
const char* tokenTypeName(TokenType t);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_LEXER_H */
