#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "parser.h"
#include <string.h>
#include <stdio.h>

static CompilerCtx* current         = NULL;
static bool         had_compile_error = false;
static const char*  src_path        = NULL;

static void compileError(int line, const char* fmt, ...) {
    had_compile_error = true;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, NQ_COLOR_RED "[ Compile Error ]" NQ_COLOR_RESET);
    if (src_path) fprintf(stderr, " %s", src_path);
    fprintf(stderr, ":%d\n  ", line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n\n");
    va_end(args);
}

static Chunk* currentChunk(void) { return current->function->chunk; }

static void emit(uint8_t byte, int line) { writeChunk(currentChunk(), byte, line); }
static void emit2(uint8_t a, uint8_t b, int line) { emit(a, line); emit(b, line); }

static int emitJump(uint8_t jump_op, int line) {
    emit(jump_op, line); emit(0xFF, line); emit(0xFF, line);
    return currentChunk()->count - 2;
}

static void patchJumpAt(int offset) { patchJump(currentChunk(), offset); }

static void emitLoop(int loop_start, int line) {
    emit(OP_LOOP, line);
    int offset = currentChunk()->count - loop_start + 2;
    if (offset > 0xFFFF) compileError(line, "Loop body is too large.");
    emit((offset >> 8) & 0xFF, line);
    emit(offset & 0xFF,        line);
}

static int emitConst(Value val, int line) {
    int idx = addConstant(currentChunk(), val);
    if (idx > 255) { compileError(line, "Too many constants in one function."); return 0; }
    emit2(OP_CONST, (uint8_t)idx, line);
    return idx;
}

static uint8_t identConst(const char* name, int line) {
    ObjString* s = copyString(name, (int)strlen(name));
    int idx = addConstant(currentChunk(), OBJ_VAL(s));
    if (idx > 255) { compileError(line, "Too many global variable names."); return 0; }
    return (uint8_t)idx;
}

static int resolveLocal(CompilerCtx* ctx, const char* name) {
    for (int i = ctx->local_count - 1; i >= 0; i--)
        if (strcmp(ctx->locals[i].name, name) == 0) return i;
    return -1;
}

static void addLocal(const char* name, int line) {
    if (current->local_count >= NQ_LOCALS_MAX) {
        compileError(line, "Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->local_count++];
    strncpy(local->name, name, 63);
    local->name[63]    = '\0';
    local->depth       = current->scope_depth;
    local->initialized = false;
}

static void beginScope(void) {
    current->scope_depth++;
}

static void endScope(int line) {
    current->scope_depth--;
    // Pop all locals that belong to the scope we just left
    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth > current->scope_depth) {
        emit(OP_POP, line);
        current->local_count--;
    }
}

static void compileNode(ASTNode* node);
static void compileExpr(ASTNode* node);
static void compileBlock(ASTNode* block);

static void initCompilerCtx(CompilerCtx* ctx, ObjFunction* fn) {
    ctx->enclosing   = current;
    ctx->function    = fn;
    ctx->scope_depth = (current ? current->scope_depth : 0);
    ctx->local_count = 0;
    current          = ctx;
}

static ObjFunction* endCompilerCtx(void) {
    emit(OP_NIL, 0); emit(OP_RETURN, 0);
    ObjFunction* fn = current->function;
    current = current->enclosing;
    return fn;
}

static void compileExpr(ASTNode* node) {
    if (!node) return;
    int line = node->line;

    switch (node->type) {
        case NODE_NUMBER:
            emitConst(NUMBER_VAL(node->data.number.value), line);
            break;
        case NODE_STRING: {
            ObjString* s = copyString(node->data.string.value, node->data.string.length);
            emitConst(OBJ_VAL(s), line);
            break;
        }
        case NODE_BOOL:
            emit(node->data.boolean.value ? OP_TRUE : OP_FALSE, line);
            break;
        case NODE_NIL:
            emit(OP_NIL, line);
            break;
        case NODE_IDENT: {
            const char* name = node->data.ident.name;
            int slot = resolveLocal(current, name);
            if (slot >= 0) emit2(OP_GET_LOCAL,  (uint8_t)slot,          line);
            else           emit2(OP_GET_GLOBAL, identConst(name, line), line);
            break;
        }
        case NODE_UNARY:
            compileExpr(node->data.unary.operand);
            switch (node->data.unary.op) {
                case TK_MINUS: emit(OP_NEGATE, line); break;
                case TK_NOT:   emit(OP_NOT,    line); break;
                default: break;
            }
            break;
        case NODE_BINARY: {
            TokenType op = node->data.binary.op;
            if (op == TK_AND) {
                compileExpr(node->data.binary.left);
                int jump = emitJump(OP_JUMP_IF_FALSE, line);
                emit(OP_POP, line);
                compileExpr(node->data.binary.right);
                patchJumpAt(jump);
                break;
            }
            if (op == TK_OR) {
                compileExpr(node->data.binary.left);
                int else_jump = emitJump(OP_JUMP_IF_FALSE, line);
                int end_jump  = emitJump(OP_JUMP, line);
                patchJumpAt(else_jump);
                emit(OP_POP, line);
                compileExpr(node->data.binary.right);
                patchJumpAt(end_jump);
                break;
            }
            compileExpr(node->data.binary.left);
            compileExpr(node->data.binary.right);
            switch (op) {
                case TK_PLUS:    emit(OP_ADD,    line); break;
                case TK_MINUS:   emit(OP_SUB,    line); break;
                case TK_STAR:    emit(OP_MUL,    line); break;
                case TK_SLASH:   emit(OP_DIV,    line); break;
                case TK_PERCENT: emit(OP_MOD,    line); break;
                case TK_DOTDOT:  emit(OP_CONCAT, line); break;
                case TK_EQEQ:    emit(OP_EQ,     line); break;
                case TK_BANGEQ:  emit(OP_NEQ,    line); break;
                case TK_LT:      emit(OP_LT,     line); break;
                case TK_GT:      emit(OP_GT,     line); break;
                case TK_LTEQ:    emit(OP_LTE,    line); break;
                case TK_GTEQ:    emit(OP_GTE,    line); break;
                default:
                    compileError(line, "Unknown operator in binary expression.");
                    break;
            }
            break;
        }
        case NODE_CALL: {
            compileExpr(node->data.call.callee);
            int argc = node->data.call.args.count;
            for (int i = 0; i < argc; i++) compileExpr(node->data.call.args.items[i]);
            if (argc > 255) { compileError(line, "Too many arguments (max 255)."); argc = 255; }
            emit2(OP_CALL, (uint8_t)argc, line);
            break;
        }
        case NODE_ARRAY: {
            int count = node->data.array.items.count;
            for (int i = 0; i < count; i++) compileExpr(node->data.array.items.items[i]);
            if (count > 255) { compileError(line, "Too many items in array literal (max 255)."); count = 255; }
            emit2(OP_BUILD_ARRAY, (uint8_t)count, line);
            break;
        }
        case NODE_GET_INDEX:
            compileExpr(node->data.get_index.obj);
            compileExpr(node->data.get_index.index);
            emit(OP_GET_INDEX, line);
            break;
        default:
            compileError(line, "Cannot compile expression (type: %d).", node->type);
            break;
    }
}

static void compileNode(ASTNode* node) {
    if (!node || had_compile_error) return;
    int line = node->line;

    switch (node->type) {
        case NODE_LET: {
            const char* name = node->data.let.name;
            compileExpr(node->data.let.value);
            if (current->scope_depth > 0) {
                addLocal(name, line);
                current->locals[current->local_count - 1].initialized = true;
            } else {
                emit2(OP_DEFINE_GLOBAL, identConst(name, line), line);
            }
            break;
        }
        case NODE_ASSIGN: {
            const char* name = node->data.assign.name;
            compileExpr(node->data.assign.value);
            int slot = resolveLocal(current, name);
            if (slot >= 0) emit2(OP_SET_LOCAL,  (uint8_t)slot,          line);
            else           emit2(OP_SET_GLOBAL, identConst(name, line), line);
            emit(OP_POP, line);
            break;
        }
        case NODE_PRINT:
            compileExpr(node->data.print.expr);
            emit(OP_PRINT, line);
            break;
        case NODE_IF: {
            compileExpr(node->data.if_stmt.cond);
            int then_jump = emitJump(OP_JUMP_IF_FALSE, line);
            emit(OP_POP, line);                          // pop cond (true path)
            beginScope();
            compileBlock(node->data.if_stmt.then_block);
            endScope(line);
            int end_jump = emitJump(OP_JUMP, line);
            patchJumpAt(then_jump);
            emit(OP_POP, line);                          // pop cond (false path)
            if (node->data.if_stmt.else_block) {
                beginScope();
                compileBlock(node->data.if_stmt.else_block);
                endScope(line);
            }
            patchJumpAt(end_jump);
            break;
        }
        case NODE_LOOP: {
            int loop_start = currentChunk()->count;
            compileExpr(node->data.loop.cond);
            int exit_jump = emitJump(OP_JUMP_IF_FALSE, line);
            emit(OP_POP, line);
            beginScope();
            compileBlock(node->data.loop.body);
            endScope(line);
            emitLoop(loop_start, line);
            patchJumpAt(exit_jump);
            emit(OP_POP, line);
            break;
        }
        case NODE_FUNCTION: {
            ObjFunction* fn = newFunction();
            fn->arity = node->data.function.param_count;
            fn->name  = copyString(node->data.function.name,
                                   (int)strlen(node->data.function.name));
            CompilerCtx fn_ctx;
            initCompilerCtx(&fn_ctx, fn);
            fn_ctx.scope_depth = 1;
            // Slot 0 is reserved for the function object itself
            addLocal("", line);
            current->locals[current->local_count - 1].initialized = true;
            for (int i = 0; i < node->data.function.param_count; i++) {
                addLocal(node->data.function.params[i], line);
                current->locals[current->local_count - 1].initialized = true;
            }
            compileBlock(node->data.function.body);
            ObjFunction* compiled_fn = endCompilerCtx();
            int fn_idx = addConstant(currentChunk(), OBJ_VAL(compiled_fn));
            emit2(OP_CONST, (uint8_t)fn_idx, line);
            emit2(OP_DEFINE_GLOBAL, identConst(node->data.function.name, line), line);
            break;
        }
        case NODE_RETURN:
            if (current->enclosing == NULL) {
                compileError(line, "'return' cannot be used outside of a function.");
                break;
            }
            if (node->data.ret.expr) compileExpr(node->data.ret.expr);
            else                     emit(OP_NIL, line);
            emit(OP_RETURN, line);
            break;
        case NODE_EXPR_STMT:
            compileExpr(node->data.expr_stmt.expr);
            emit(OP_POP, line);
            break;
        case NODE_SET_INDEX:
            compileExpr(node->data.set_index.obj);
            compileExpr(node->data.set_index.index);
            compileExpr(node->data.set_index.value);
            emit(OP_SET_INDEX, line);
            break;
        case NODE_IMPORT: {
            const char* mod_path = node->data.import.path;

            // Try path relative to current source file first, then CWD
            char resolved[1024];
            bool found = false;

            // 1) relative to source file
            if (src_path) {
                const char* last_slash = strrchr(src_path, '/');
                if (!last_slash) last_slash = strrchr(src_path, '\\');
                if (last_slash) {
                    int dir_len = (int)(last_slash - src_path + 1);
                    snprintf(resolved, sizeof(resolved), "%.*s%s", dir_len, src_path, mod_path);
                    if (strstr(resolved, ".nq") == NULL)
                        strncat(resolved, ".nq", sizeof(resolved) - strlen(resolved) - 1);
                    FILE* test = fopen(resolved, "r");
                    if (test) { fclose(test); found = true; }
                }
            }
            // 2) relative to CWD
            if (!found) {
                snprintf(resolved, sizeof(resolved), "%s", mod_path);
                if (strstr(resolved, ".nq") == NULL)
                    strncat(resolved, ".nq", sizeof(resolved) - strlen(resolved) - 1);
                FILE* test = fopen(resolved, "r");
                if (test) { fclose(test); found = true; }
            }
            if (!found) {
                compileError(line, "Cannot find module '%s'.", mod_path);
                break;
            }

            // Read file
            FILE* f = fopen(resolved, "rb");
            if (!f) { compileError(line, "Cannot open module '%s'.", resolved); break; }
            fseek(f, 0, SEEK_END);
            long size = ftell(f); rewind(f);
            char* source = (char*)malloc((size_t)(size + 1));
            size_t nread = fread(source, 1, (size_t)size, f);
            source[nread] = '\0';
            fclose(f);

            // Parse module
            Parser mod_parser;
            initParser(&mod_parser, source, resolved);
            ASTNode* mod_ast = parse(&mod_parser);
            free(source);

            if (mod_parser.had_error) {
                freeNode(mod_ast);
                compileError(line, "Errors in module '%s'.", resolved);
                break;
            }

            // Compile module as a self-calling function
            ObjFunction* mod_fn = newFunction();
            mod_fn->name = copyString(resolved, (int)strlen(resolved));
            CompilerCtx mod_ctx;
            initCompilerCtx(&mod_ctx, mod_fn);
            mod_ctx.scope_depth = 0;
            compileBlock(mod_ast);
            freeNode(mod_ast);
            emit(OP_NIL, line);
            emit(OP_RETURN, line);
            ObjFunction* compiled_mod = endCompilerCtx();

            int fn_idx = addConstant(currentChunk(), OBJ_VAL(compiled_mod));
            emit2(OP_CONST, (uint8_t)fn_idx, line);
            emit2(OP_CALL, 0, line);
            emit(OP_POP, line);
            break;
        }
        case NODE_BLOCK:
        case NODE_PROGRAM:
            compileBlock(node);
            break;
        default:
            compileError(line, "Cannot compile node (type: %d).", node->type);
            break;
    }
}

static void compileBlock(ASTNode* block) {
    if (!block) return;
    NodeList* stmts = &block->data.block.stmts;
    for (int i = 0; i < stmts->count; i++) {
        compileNode(stmts->items[i]);
        if (had_compile_error) break;
    }
}

CompileResult compile(ASTNode* ast, const char* source_path) {
    had_compile_error = false;
    src_path          = source_path;
    ObjFunction* script = newFunction();
    script->name = NULL;
    CompilerCtx ctx;
    initCompilerCtx(&ctx, script);
    ctx.scope_depth = 0;
    // Reserve slot 0 for the script function object (same as user functions)
    addLocal("", 0);
    ctx.locals[ctx.local_count - 1].initialized = true;
    compileBlock(ast);
    emit(OP_HALT, 0);
    ObjFunction* fn = endCompilerCtx();
    CompileResult result;
    result.function  = had_compile_error ? NULL : fn;
    result.had_error = had_compile_error;
    return result;
}
