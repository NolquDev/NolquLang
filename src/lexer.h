#ifndef NQ_LEXER_H
#define NQ_LEXER_H

#include "common.h"

// ─────────────────────────────────────────────
//  Token types
// ─────────────────────────────────────────────
typedef enum {
    // ── Literals ──────────────────────────────
    TK_NUMBER,      // 42 / 3.14
    TK_STRING,      // "hello"
    TK_IDENT,       // variable / function name

    // ── Keywords ──────────────────────────────
    TK_LET,         // let
    TK_PRINT,       // print
    TK_IF,          // if
    TK_ELSE,        // else
    TK_LOOP,        // loop
    TK_FUNCTION,    // function
    TK_RETURN,      // return
    TK_IMPORT,      // import
    TK_TRY,         // try
    TK_CATCH,       // catch
    TK_END,         // end
    TK_TRUE,        // true
    TK_FALSE,       // false
    TK_NIL,         // nil
    TK_AND,         // and
    TK_OR,          // or
    TK_NOT,         // not

    // ── Operators ─────────────────────────────
    TK_PLUS,        // +
    TK_MINUS,       // -
    TK_STAR,        // *
    TK_SLASH,       // /
    TK_PERCENT,     // %

    TK_EQ,          // =
    TK_EQEQ,        // ==
    TK_BANGEQ,      // !=
    TK_LT,          // <
    TK_GT,          // >
    TK_LTEQ,        // <=
    TK_GTEQ,        // >=

    TK_DOTDOT,      // .. (string concat)

    // ── Punctuation ───────────────────────────
    TK_LPAREN,      // (
    TK_RPAREN,      // )
    TK_COMMA,       // ,
    TK_LBRACKET,    // [
    TK_RBRACKET,    // ]

    // ── Structural ────────────────────────────
    TK_NEWLINE,     // \n (statement terminator)
    TK_EOF,         // end of file
    TK_ERROR,       // lexer error
} TokenType;

// ─────────────────────────────────────────────
//  Token
// ─────────────────────────────────────────────
typedef struct {
    TokenType   type;
    const char* start;  // pointer into source
    int         length;
    int         line;
    const char* error;  // set when type == TK_ERROR
} Token;

// ─────────────────────────────────────────────
//  Lexer state
// ─────────────────────────────────────────────
typedef struct {
    const char* source;
    const char* start;
    const char* current;
    int         line;
} Lexer;

// ─────────────────────────────────────────────
//  Lexer API
// ─────────────────────────────────────────────
void  initLexer(Lexer* lex, const char* source);
Token nextToken(Lexer* lex);

// Utility
const char* tokenTypeName(TokenType t);

#endif // NQ_LEXER_H
