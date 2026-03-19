#include "ast.h"
#include "memory.h"

// ─────────────────────────────────────────────
//  NodeList
// ─────────────────────────────────────────────
void initNodeList(NodeList* list) {
    list->items    = NULL;
    list->count    = 0;
    list->capacity = 0;
}

void appendNode(NodeList* list, ASTNode* node) {
    if (list->count >= list->capacity) {
        int old = list->capacity;
        list->capacity = GROW_CAPACITY(old);
        list->items = GROW_ARRAY(ASTNode*, list->items, old, list->capacity);
    }
    list->items[list->count++] = node;
}

void freeNodeList(NodeList* list) {
    for (int i = 0; i < list->count; i++) {
        freeNode(list->items[i]);
    }
    FREE_ARRAY(ASTNode*, list->items, list->capacity);
    initNodeList(list);
}

// ─────────────────────────────────────────────
//  Node allocation
// ─────────────────────────────────────────────
ASTNode* makeNode(NodeType type, int line) {
    ASTNode* n = NQ_ALLOC(ASTNode, 1);
    memset(n, 0, sizeof(ASTNode));
    n->type = type;
    n->line = line;
    return n;
}

// ─────────────────────────────────────────────
//  Node deallocation (recursive)
// ─────────────────────────────────────────────
void freeNode(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_PROGRAM:
        case NODE_BLOCK:
            freeNodeList(&node->data.block.stmts);
            break;

        case NODE_LET:
            free(node->data.let.name);
            freeNode(node->data.let.value);
            break;

        case NODE_ASSIGN:
            free(node->data.assign.name);
            freeNode(node->data.assign.value);
            break;
        case NODE_COMPOUND_ASSIGN:
            free(node->data.compound_assign.name);
            freeNode(node->data.compound_assign.value);
            break;
        case NODE_CONST:
            free(node->data.const_decl.name);
            freeNode(node->data.const_decl.value);
            break;
        case NODE_SLICE:
            freeNode(node->data.slice.obj);
            freeNode(node->data.slice.start);
            freeNode(node->data.slice.end);
            break;
        case NODE_FOR:
            free(node->data.for_loop.item);
            freeNode(node->data.for_loop.iterable);
            freeNode(node->data.for_loop.body);
            break;

        case NODE_PRINT:
            freeNode(node->data.print.expr);
            break;

        case NODE_IF:
            freeNode(node->data.if_stmt.cond);
            freeNode(node->data.if_stmt.then_block);
            freeNode(node->data.if_stmt.else_block);
            break;

        case NODE_LOOP:
            freeNode(node->data.loop.cond);
            freeNode(node->data.loop.body);
            break;

        case NODE_FUNCTION:
            free(node->data.function.name);
            for (int i = 0; i < node->data.function.param_count; i++) {
                free(node->data.function.params[i]);
                if (node->data.function.defaults)
                    freeNode(node->data.function.defaults[i]);
            }
            free(node->data.function.params);
            if (node->data.function.defaults)
                free(node->data.function.defaults);
            freeNode(node->data.function.body);
            break;

        case NODE_RETURN:
            freeNode(node->data.ret.expr);
            break;

        case NODE_IMPORT:
            free(node->data.import.path);
            break;

        case NODE_EXPR_STMT:
            freeNode(node->data.expr_stmt.expr);
            break;

        case NODE_BINARY:
            freeNode(node->data.binary.left);
            freeNode(node->data.binary.right);
            break;

        case NODE_UNARY:
            freeNode(node->data.unary.operand);
            break;

        case NODE_CALL:
            freeNode(node->data.call.callee);
            freeNodeList(&node->data.call.args);
            break;

        case NODE_IDENT:
            free(node->data.ident.name);
            break;

        case NODE_STRING:
            free(node->data.string.value);
            break;

        case NODE_NUMBER:
        case NODE_BOOL:
        case NODE_NIL:
            break; // no heap data

        case NODE_ARRAY:
            freeNodeList(&node->data.array.items);
            break;

        case NODE_GET_INDEX:
            freeNode(node->data.get_index.obj);
            freeNode(node->data.get_index.index);
            break;

        case NODE_SET_INDEX:
            freeNode(node->data.set_index.obj);
            freeNode(node->data.set_index.index);
            freeNode(node->data.set_index.value);
            break;

        case NODE_TRY:
            freeNode(node->data.try_stmt.body);
            free(node->data.try_stmt.err_name);
            freeNode(node->data.try_stmt.handler);
            break;

        default:
            break;
    }

    free(node);
}

// ─────────────────────────────────────────────
//  AST pretty-printer (debug)
// ─────────────────────────────────────────────
static void indent_print(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}

void printAST(ASTNode* node, int indent) {
    if (!node) { indent_print(indent); printf("(null)\n"); return; }
    indent_print(indent);

    switch (node->type) {
        case NODE_PROGRAM:
            printf("[PROGRAM] %d stmt(s)\n", node->data.block.stmts.count);
            for (int i = 0; i < node->data.block.stmts.count; i++)
                printAST(node->data.block.stmts.items[i], indent + 1);
            break;
        case NODE_LET:
            printf("[LET] %s\n", node->data.let.name);
            printAST(node->data.let.value, indent + 1);
            break;
        case NODE_ASSIGN:
            printf("[ASSIGN] %s\n", node->data.assign.name);
            printAST(node->data.assign.value, indent + 1);
            break;
        case NODE_PRINT:
            printf("[PRINT]\n");
            printAST(node->data.print.expr, indent + 1);
            break;
        case NODE_IF:
            printf("[IF]\n");
            indent_print(indent + 1); printf("cond:\n");
            printAST(node->data.if_stmt.cond, indent + 2);
            indent_print(indent + 1); printf("then:\n");
            printAST(node->data.if_stmt.then_block, indent + 2);
            if (node->data.if_stmt.else_block) {
                indent_print(indent + 1); printf("else:\n");
                printAST(node->data.if_stmt.else_block, indent + 2);
            }
            break;
        case NODE_LOOP:
            printf("[LOOP]\n");
            printAST(node->data.loop.cond, indent + 1);
            printAST(node->data.loop.body, indent + 1);
            break;
        case NODE_FUNCTION:
            printf("[FUNCTION] %s(%d params)\n",
                node->data.function.name, node->data.function.param_count);
            printAST(node->data.function.body, indent + 1);
            break;
        case NODE_RETURN:
            printf("[RETURN]\n");
            if (node->data.ret.expr) printAST(node->data.ret.expr, indent + 1);
            break;
        case NODE_BINARY:
            printf("[BINARY] %s\n", tokenTypeName(node->data.binary.op));
            printAST(node->data.binary.left,  indent + 1);
            printAST(node->data.binary.right, indent + 1);
            break;
        case NODE_UNARY:
            printf("[UNARY] %s\n", tokenTypeName(node->data.unary.op));
            printAST(node->data.unary.operand, indent + 1);
            break;
        case NODE_CALL:
            printf("[CALL]\n");
            printAST(node->data.call.callee, indent + 1);
            for (int i = 0; i < node->data.call.args.count; i++)
                printAST(node->data.call.args.items[i], indent + 1);
            break;
        case NODE_IDENT:   printf("[IDENT] %s\n",  node->data.ident.name); break;
        case NODE_NUMBER:  printf("[NUM]   %g\n",   node->data.number.value); break;
        case NODE_STRING:  printf("[STR]   \"%s\"\n", node->data.string.value); break;
        case NODE_BOOL:    printf("[BOOL]  %s\n",   node->data.boolean.value ? "true" : "false"); break;
        case NODE_NIL:     printf("[NIL]\n"); break;
        default:           printf("[???]\n"); break;
    }
}
