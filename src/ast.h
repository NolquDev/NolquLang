#ifndef NQ_AST_H
#define NQ_AST_H

#include "common.h"
#include "lexer.h"

// ─────────────────────────────────────────────
//  AST Node types
// ─────────────────────────────────────────────
typedef enum {
    // Statements
    NODE_PROGRAM,       // root: list of statements
    NODE_LET,           // let name = expr
    NODE_ASSIGN,        // name = expr  (reassign)
    NODE_PRINT,         // print expr
    NODE_IF,            // if cond { then } [else { else }]
    NODE_LOOP,          // loop cond { body }
    NODE_FUNCTION,      // function name(params) { body }
    NODE_RETURN,        // return [expr]
    NODE_IMPORT,        // import "file"
    NODE_EXPR_STMT,     // bare expression as statement

    // Expressions
    NODE_BINARY,        // left op right
    NODE_UNARY,         // op operand
    NODE_CALL,          // callee(args...)
    NODE_IDENT,         // variable reference
    NODE_NUMBER,        // numeric literal
    NODE_STRING,        // string literal
    NODE_BOOL,          // true / false
    NODE_NIL,           // nil

    NODE_BLOCK,         // sequence of statements

    // Array nodes (new in 0.3.0)
    NODE_ARRAY,         // [expr, expr, ...]  array literal
    NODE_GET_INDEX,     // arr[index]
    NODE_SET_INDEX,     // arr[index] = value  (as statement)

    NODE_COUNT
} NodeType;

// ─────────────────────────────────────────────
//  Forward declaration
// ─────────────────────────────────────────────
typedef struct ASTNode ASTNode;

// ─────────────────────────────────────────────
//  Dynamic array of ASTNode pointers
// ─────────────────────────────────────────────
typedef struct {
    ASTNode** items;
    int       count;
    int       capacity;
} NodeList;

void initNodeList(NodeList* list);
void appendNode(NodeList* list, ASTNode* node);
void freeNodeList(NodeList* list);

// ─────────────────────────────────────────────
//  The AST Node union
// ─────────────────────────────────────────────
struct ASTNode {
    NodeType type;
    int      line;  // source line (for error messages)

    union {

        // NODE_PROGRAM / NODE_BLOCK
        struct {
            NodeList stmts;
        } block;

        // NODE_LET: let name = value
        struct {
            char* name;
            ASTNode* value;
        } let;

        // NODE_ASSIGN: name = value
        struct {
            char* name;
            ASTNode* value;
        } assign;

        // NODE_PRINT: print expr
        struct {
            ASTNode* expr;
        } print;

        // NODE_IF: if cond then [else alt]
        struct {
            ASTNode* cond;
            ASTNode* then_block;
            ASTNode* else_block; // may be NULL
        } if_stmt;

        // NODE_LOOP: loop cond body
        struct {
            ASTNode* cond;
            ASTNode* body;
        } loop;

        // NODE_FUNCTION: function name(params) body
        struct {
            char*    name;
            char**   params;
            int      param_count;
            ASTNode* body;
        } function;

        // NODE_RETURN: return [expr]
        struct {
            ASTNode* expr; // may be NULL
        } ret;

        // NODE_IMPORT: import "path"
        struct {
            char* path;
        } import;

        // NODE_EXPR_STMT
        struct {
            ASTNode* expr;
        } expr_stmt;

        // NODE_BINARY: left op right
        struct {
            TokenType op;
            ASTNode*  left;
            ASTNode*  right;
        } binary;

        // NODE_UNARY: op operand
        struct {
            TokenType op;
            ASTNode*  operand;
        } unary;

        // NODE_CALL: callee(args)
        struct {
            ASTNode* callee;
            NodeList args;
        } call;

        // NODE_IDENT
        struct {
            char* name;
        } ident;

        // NODE_NUMBER
        struct {
            double value;
        } number;

        // NODE_STRING
        struct {
            char* value;
            int   length;
        } string;

        // NODE_BOOL
        struct {
            bool value;
        } boolean;

        // NODE_NIL — no data

        // NODE_ARRAY: [items...]
        struct {
            NodeList items;
        } array;

        // NODE_GET_INDEX: obj[index]
        struct {
            ASTNode* obj;
            ASTNode* index;
        } get_index;

        // NODE_SET_INDEX: obj[index] = value
        struct {
            ASTNode* obj;
            ASTNode* index;
            ASTNode* value;
        } set_index;

    } data;
};

// ─────────────────────────────────────────────
//  Node constructors
// ─────────────────────────────────────────────
ASTNode* makeNode(NodeType type, int line);
void     freeNode(ASTNode* node);

// ─────────────────────────────────────────────
//  Debug printer
// ─────────────────────────────────────────────
void printAST(ASTNode* node, int indent);

#endif // NQ_AST_H
