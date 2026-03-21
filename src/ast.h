/*
 * ast.h — Nolqu Abstract Syntax Tree
 *
 * The parser produces a tree of ASTNode values.  Each node carries a
 * NodeType tag and a union of per-type data.  Nodes are heap-allocated
 * and ownership is passed to the compiler; freeNode() walks the tree
 * and releases everything.
 *
 * NodeList — a growable array of ASTNode* used for statement lists
 *            and argument lists.
 */

#ifndef NQ_AST_H
#define NQ_AST_H

#include "common.h"
#include "lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Node types ──────────────────────────────────────────────────── */
typedef enum {
    /* Statements */
    NODE_PROGRAM,       /* root: flat list of statements              */
    NODE_LET,           /* let name = expr                            */
    NODE_ASSIGN,        /* name = expr  (reassignment)                */
    NODE_PRINT,         /* print expr                                 */
    NODE_IF,
    NODE_WHEN,         /* when x | val1: body | val2: body | else: body end */
    NODE_LOOP,          /* loop cond body end                         */
    NODE_FUNCTION,      /* function name(params) body end             */
    NODE_RETURN,        /* return [expr]                              */
    NODE_IMPORT,        /* import "path"                              */
    NODE_EXPR_STMT,     /* expression used as a statement             */

    /* Expressions */
    NODE_BINARY,        /* left op right                              */
    NODE_UNARY,         /* op operand                                 */
    NODE_CALL,          /* callee(args...)                            */
    NODE_IDENT,         /* variable reference                         */
    NODE_NUMBER,        /* numeric literal                            */
    NODE_STRING,        /* string literal (escapes already resolved)  */
    NODE_BOOL,          /* true / false                               */
    NODE_NIL,           /* nil                                        */

    NODE_BLOCK,         /* sequence of statements (function body etc) */

    /* Arrays */
    NODE_ARRAY,         /* [expr, expr, ...]                          */
    NODE_GET_INDEX,     /* arr[index]                                 */
    NODE_SET_INDEX,     /* arr[index] = value  (as statement)         */

    /* Error handling */
    NODE_TRY,           /* try body catch(err) handler end            */

    /* Loop control */
    NODE_BREAK,         /* break    — exit the nearest loop           */
    NODE_CONTINUE,      /* continue — jump to next loop iteration     */

    /* Compound assignment */
    NODE_COMPOUND_ASSIGN,

    /* const declaration (immutable let) */
    NODE_CONST,

    /* arr[start:end] slice expression */
    NODE_SLICE,

    /* for item in array — range-based loop */
    NODE_FOR,
    NODE_FOR_RANGE,     /* for i = start to stop [step s] body end    */

    NODE_COUNT          /* sentinel                                   */
} NodeType;

/* ── Forward declaration ─────────────────────────────────────────── */
typedef struct ASTNode ASTNode;

/* ── NodeList — growable array of ASTNode* ───────────────────────── */
typedef struct {
    ASTNode** items;
    int       count;
    int       capacity;
} NodeList;

void initNodeList  (NodeList* list);
void appendNode    (NodeList* list, ASTNode* node);
void freeNodeList  (NodeList* list);

/* ── ASTNode ─────────────────────────────────────────────────────── */
struct ASTNode {
    NodeType type;
    int      line;      /* source line — used in error reporting      */

    union {

        /* NODE_PROGRAM / NODE_BLOCK */
        struct { NodeList stmts; } block;

        /* NODE_LET: let name = value */
        struct { char* name; ASTNode* value; } let;

        /* NODE_ASSIGN: name = value */
        struct { char* name; ASTNode* value; } assign;

        /* NODE_COMPOUND_ASSIGN */
        struct { char* name; TokenType op; ASTNode* value; } compound_assign;

        /* NODE_CONST: const name = value */
        struct { char* name; ASTNode* value; } const_decl;

        /* NODE_SLICE: obj[start:end]  (start or end may be NULL = omitted) */
        struct { ASTNode* obj; ASTNode* start; ASTNode* end; } slice;

        /* NODE_FOR: for item in iterable body end */
        struct { char* item; ASTNode* iterable; ASTNode* body; } for_loop;

        /* NODE_FOR_RANGE: for i = start to stop [step s] body end */
        struct {
            char*    var;    /* loop variable name */
            ASTNode* start;
            ASTNode* stop;
            ASTNode* step;   /* NULL → default +1 */
            ASTNode* body;
        } for_range;

        /* NODE_PRINT: print expr */
        struct { ASTNode* expr; } print;

        /* NODE_IF */
        struct {
            ASTNode* cond;
            ASTNode* then_block;
            ASTNode* else_block;    /* NULL if no else branch         */
        } if_stmt;

        /* NODE_WHEN */
        struct {
            ASTNode*  subject;
            ASTNode** values;   /* NULL entry = else case */
            ASTNode** bodies;
            int       count;
        } when_stmt;

        /* NODE_LOOP */
        struct { ASTNode* cond; ASTNode* body; } loop;

        /* NODE_FUNCTION */
        struct {
            char*     name;
            char**    params;
            ASTNode** defaults;   /* NULL or default expr per param */
            int       param_count;
            ASTNode*  body;
        } function;

        /* NODE_RETURN */
        struct { ASTNode* expr; } ret;    /* expr may be NULL         */

        /* NODE_IMPORT */
        struct {
            char*  path;
            char** names;      /* NULL = import all; non-NULL = from-import name list */
            int    name_count;
        } import;

        /* NODE_EXPR_STMT */
        struct { ASTNode* expr; } expr_stmt;

        /* NODE_BINARY: left op right */
        struct {
            TokenType op;
            ASTNode*  left;
            ASTNode*  right;
        } binary;

        /* NODE_UNARY: op operand */
        struct { TokenType op; ASTNode* operand; } unary;

        /* NODE_CALL: callee(args) */
        struct { ASTNode* callee; NodeList args; } call;

        /* NODE_IDENT */
        struct { char* name; } ident;

        /* NODE_NUMBER */
        struct { double value; } number;

        /* NODE_STRING — escape sequences resolved at parse time */
        struct { char* value; int length; } string;

        /* NODE_BOOL */
        struct { bool value; } boolean;

        /* NODE_NIL — no extra data */

        /* NODE_ARRAY */
        struct { NodeList items; } array;

        /* NODE_GET_INDEX: obj[index] */
        struct { ASTNode* obj; ASTNode* index; } get_index;

        /* NODE_SET_INDEX: obj[index] = value */
        struct { ASTNode* obj; ASTNode* index; ASTNode* value; } set_index;

        /* NODE_TRY */
        struct {
            ASTNode* body;          /* try block                      */
            char*    err_name;      /* catch variable name            */
            ASTNode* handler;       /* catch block                    */
        } try_stmt;

    } data;
};

/* ── Node constructors / destructor ──────────────────────────────── */
ASTNode* makeNode(NodeType type, int line);
void     freeNode(ASTNode* node);   /* deep-frees the whole subtree   */

/* ── Debug printer ───────────────────────────────────────────────── */
void printAST(ASTNode* node, int indent);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_AST_H */
