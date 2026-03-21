#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "parser.h"
#include <string.h>
#include <stdio.h>
#include <vector>
#include <unordered_set>
#include <string>

static CompilerCtx* current           = NULL;
static bool         had_compile_error = false;
static const char*  src_path          = NULL;
static bool         repl_mode         = false;

/*
 * Loop context — tracks information needed to compile break and continue.
 */
struct LoopCtx {
    int               loop_start;
    int               continue_target; /* -1 = use forward patches (for-in) */
    int               scope_depth;     /* scope depth at loop entry */
    int               local_count;     /* locals at body entry (continue cleanup) */
    int               outer_local_count; /* locals before loop scope (break cleanup) */
    std::vector<int>  break_patches;
    std::vector<int>  continue_patches;
};

static std::vector<LoopCtx>        loop_stack;

/*
 * Import deduplication — tracks resolved module paths that have already
 * been compiled in this compilation session.  Prevents re-executing
 * modules loaded more than once (e.g. two stdlib users both importing
 * "stdlib/math").
 */
static std::unordered_set<std::string> imported_modules;

/*
 * Import stack — tracks the chain of modules currently being compiled.
 * Used to detect circular imports: if a module appears in this stack
 * when it is about to be imported, we have a cycle.
 * e.g. A imports B, B imports A → stack = ["A", "B"] when B tries to import A.
 */
static std::vector<std::string> import_stack;

/*
 * Const globals — global variable names declared with 'const'.
 * Any assignment to these after declaration is a compile-time error.
 */
static std::unordered_set<std::string> const_globals;

static void compileError(int line, const char* fmt, ...) {
    had_compile_error = true;
    va_list args;
    va_start(args, fmt);

    /* Format first so we can extract the error type prefix */
    char msg[1024];
    va_list args2;
    va_copy(args2, args);
    vsnprintf(msg, sizeof(msg), fmt, args2);
    va_end(args2);

    /* Extract type prefix (e.g. "ImportError: ..." → "ImportError") */
    const char* colon = strstr(msg, ": ");
    char header[64] = "SyntaxError";
    if (colon && colon > msg) {
        int prefix_len = (int)(colon - msg);
        if (prefix_len <= 20 && prefix_len > 0) {
            int valid = 1;
            for (int i = 0; i < prefix_len; i++) {
                char c = msg[i];
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
                    valid = 0; break;
                }
            }
            if (valid) snprintf(header, sizeof(header), "%.*s", prefix_len, msg);
        }
    }

    fprintf(stderr, NQ_COLOR_RED "[%s]" NQ_COLOR_RESET, header);
    if (src_path) fprintf(stderr, " %s", src_path);
    fprintf(stderr, ":%d\n  ", line);
    fprintf(stderr, "%s", msg);
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

/* emit a 16-bit constant index (hi byte, lo byte) */
static void emitUint16(uint16_t val, int line) {
    emit((uint8_t)((val >> 8) & 0xFF), line);
    emit((uint8_t)(val & 0xFF),         line);
}

static int emitConst(Value val, int line) {
    int idx = addConstant(currentChunk(), val);
    if (idx > 65535) { compileError(line, "Too many constants in one function (max 65535)."); return 0; }
    emit(OP_CONST, line);
    emitUint16((uint16_t)idx, line);
    return idx;
}

static uint16_t identConst(const char* name, int line) {
    ObjString* s = copyString(name, (int)strlen(name));
    int idx = addConstant(currentChunk(), OBJ_VAL(s));
    if (idx > 65535) { compileError(line, "Too many global variable names (max 65535)."); return 0; }
    return (uint16_t)idx;
}

static int resolveLocal(CompilerCtx* ctx, const char* name) {
    for (int i = ctx->local_count - 1; i >= 0; i--)
        if (strcmp(ctx->locals[i].name, name) == 0) {
            ctx->locals[i].used = true;
            return i;
        }
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
    local->used        = false;
    local->decl_line   = line;
}

static void beginScope(void) {
    current->scope_depth++;
}

static void endScope(int line) {
    current->scope_depth--;
    // Warn about unused locals (skip internal slots: empty name, underscore prefix)
    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth > current->scope_depth) {
        Local* loc = &current->locals[current->local_count - 1];
        if (!loc->used && loc->name[0] != '\0' && loc->name[0] != '_') {
            fprintf(stderr,
                NQ_COLOR_YELLOW "[Warning]" NQ_COLOR_RESET
                " line %d: Local variable '%s' is declared but never used.\n",
                loc->decl_line, loc->name);
        }
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
            else           { emit(OP_GET_GLOBAL, line); emitUint16(identConst(name, line), line); }
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
        case NODE_SLICE: {
            /*
             * arr[start:end]
             * Push: obj, start (or NIL if omitted), end (or NIL if omitted)
             * Then OP_SLICE — VM resolves NIL → 0 for start, len for end.
             */
            compileExpr(node->data.slice.obj);
            if (node->data.slice.start) compileExpr(node->data.slice.start);
            else                        emit(OP_NIL, line);
            if (node->data.slice.end)   compileExpr(node->data.slice.end);
            else                        emit(OP_NIL, line);
            emit(OP_SLICE, line);
            break;
        }
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
                { emit(OP_DEFINE_GLOBAL, line); emitUint16(identConst(name, line), line); }
            }
            break;
        }
        case NODE_CONST: {
            /* Compile exactly like NODE_LET, but mark the name as const. */
            const char* name = node->data.const_decl.name;
            compileExpr(node->data.const_decl.value);
            if (current->scope_depth > 0) {
                addLocal(name, line);
                int idx = current->local_count - 1;
                current->locals[idx].initialized = true;
                current->locals[idx].is_const    = true;
            } else {
                { emit(OP_DEFINE_GLOBAL, line); emitUint16(identConst(name, line), line); }
                const_globals.insert(std::string(name));
            }
            break;
        }
        case NODE_ASSIGN: {
            const char* name = node->data.assign.name;
            /* Check const for locals */
            int slot = resolveLocal(current, name);
            if (slot >= 0 && current->locals[slot].is_const) {
                compileError(line, "Cannot reassign const variable '%s'.", name);
                break;
            }
            /* Check const for globals */
            if (slot < 0 && const_globals.count(std::string(name))) {
                compileError(line, "Cannot reassign const variable '%s'.", name);
                break;
            }
            compileExpr(node->data.assign.value);
            if (slot >= 0) emit2(OP_SET_LOCAL,  (uint8_t)slot,          line);
            else           { emit(OP_SET_GLOBAL, line); emitUint16(identConst(name, line), line); }
            emit(OP_POP, line);
            break;
        }
        case NODE_COMPOUND_ASSIGN: {
            const char* name = node->data.compound_assign.name;
            TokenType   op   = node->data.compound_assign.op;
            int slot = resolveLocal(current, name);
            /* Const check */
            if (slot >= 0 && current->locals[slot].is_const) {
                compileError(line, "Cannot reassign const variable '%s'.", name);
                break;
            }
            if (slot < 0 && const_globals.count(std::string(name))) {
                compileError(line, "Cannot reassign const variable '%s'.", name);
                break;
            }

            /* Superinstruction: local += number_literal → OP_ADD_LOCAL_CONST */
            if (slot >= 0 && op == TK_PLUS &&
                node->data.compound_assign.value->type == NODE_NUMBER) {
                double cv = node->data.compound_assign.value->data.number.value;
                int cidx  = addConstant(currentChunk(), NUMBER_VAL(cv));
                emit(OP_ADD_LOCAL_CONST, line);
                emit((uint8_t)slot, line);
                emitUint16((uint16_t)cidx, line);
                break;
            }
            /* Superinstruction: local -= number_literal → OP_ADD_LOCAL_CONST with -val */
            if (slot >= 0 && op == TK_MINUS &&
                node->data.compound_assign.value->type == NODE_NUMBER) {
                double cv = -node->data.compound_assign.value->data.number.value;
                int cidx  = addConstant(currentChunk(), NUMBER_VAL(cv));
                emit(OP_ADD_LOCAL_CONST, line);
                emit((uint8_t)slot, line);
                emitUint16((uint16_t)cidx, line);
                break;
            }

            /* General case */
            if (slot >= 0) emit2(OP_GET_LOCAL, (uint8_t)slot, line);
            else { emit(OP_GET_GLOBAL, line); emitUint16(identConst(name, line), line); }
            compileExpr(node->data.compound_assign.value);
            switch (op) {
                case TK_PLUS:   emit(OP_ADD,    line); break;
                case TK_MINUS:  emit(OP_SUB,    line); break;
                case TK_STAR:   emit(OP_MUL,    line); break;
                case TK_SLASH:  emit(OP_DIV,    line); break;
                case TK_DOTDOT: emit(OP_CONCAT, line); break;
                default: compileError(line, "Unsupported compound assignment operator."); break;
            }
            if (slot >= 0) emit2(OP_SET_LOCAL, (uint8_t)slot, line);
            else { emit(OP_SET_GLOBAL, line); emitUint16(identConst(name, line), line); }
            emit(OP_POP, line);
            break;
        }
        case NODE_PRINT:
            compileExpr(node->data.print.expr);
            emit(OP_PRINT, line);
            break;
        case NODE_WHEN: {
            /*
             * Compile:  when subject | v1: b1 | v2: b2 | else: bN | end
             *
             * The subject is stored in a hidden local so it's evaluated once.
             * Each case: GET_LOCAL subject, CONST value, EQ, JUMP_IF_FALSE next
             * The else branch (NULL value) always matches.
             *
             * Emits:
             *   LET __when = subject
             *   GET __when; CONST v1; EQ; JUMP_IF_FALSE → case2
             *   POP; [body1]; JUMP → end
             *   case2: POP; GET __when; CONST v2; EQ; JUMP_IF_FALSE → case3
             *   POP; [body2]; JUMP → end
             *   ...
             *   else: POP; [bodyN]
             *   end:
             */
            beginScope();
            compileExpr(node->data.when_stmt.subject);
            int subj_slot = current->local_count;
            addLocal("__when__", line);
            current->locals[subj_slot].initialized = true;
            current->locals[subj_slot].used        = true;

            std::vector<int> end_jumps;
            int next_jump = -1;

            for (int ci = 0; ci < node->data.when_stmt.count; ci++) {
                if (next_jump >= 0) {
                    patchJumpAt(next_jump);
                    emit(OP_POP, line);  /* pop condition=false */
                }
                ASTNode* val = node->data.when_stmt.values[ci];
                if (val) {
                    /* case: GET __when; CONST val; EQ; JUMP_IF_FALSE */
                    emit2(OP_GET_LOCAL, (uint8_t)subj_slot, line);
                    compileExpr(val);
                    emit(OP_EQ, line);
                    next_jump = emitJump(OP_JUMP_IF_FALSE, line);
                    emit(OP_POP, line);  /* pop condition=true */
                } else {
                    /* else case — always runs */
                    next_jump = -1;
                }
                beginScope();
                compileBlock(node->data.when_stmt.bodies[ci]);
                endScope(line);
                if (ci < node->data.when_stmt.count - 1)
                    end_jumps.push_back(emitJump(OP_JUMP, line));
            }

            /* Patch final condition fail (if no else) */
            if (next_jump >= 0) {
                patchJumpAt(next_jump);
                emit(OP_POP, line);
            }

            /* All end jumps land here */
            for (int ej : end_jumps) patchJumpAt(ej);

            endScope(line);  /* pop __when__ */
            break;
        }
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
            LoopCtx lctx;
            lctx.loop_start        = currentChunk()->count;
            lctx.continue_target   = lctx.loop_start;
            lctx.scope_depth       = current->scope_depth;
            lctx.local_count       = current->local_count;
            lctx.outer_local_count = current->local_count;
            loop_stack.push_back(lctx);

            compileExpr(node->data.loop.cond);
            int exit_jump = emitJump(OP_JUMP_IF_FALSE, line);
            emit(OP_POP, line);
            beginScope();
            compileBlock(node->data.loop.body);
            endScope(line);
            emitLoop(loop_stack.back().loop_start, line);
            patchJumpAt(exit_jump);
            emit(OP_POP, line);
            for (int patch : loop_stack.back().break_patches)
                patchJumpAt(patch);
            loop_stack.pop_back();
            break;
        }
        case NODE_BREAK: {
            if (loop_stack.empty()) {
                compileError(line, "'break' used outside of a loop.");
                break;
            }
            /* Pop ALL locals back to before the loop scope (including hidden
             * __for_arr__ / __for_idx__ locals in for-in loops). */
            int target = loop_stack.back().outer_local_count;
            for (int i = current->local_count - 1; i >= target; i--)
                emit(OP_POP, line);
            int patch = emitJump(OP_JUMP, line);
            loop_stack.back().break_patches.push_back(patch);
            break;
        }
        case NODE_CONTINUE: {
            if (loop_stack.empty()) {
                compileError(line, "'continue' used outside of a loop.");
                break;
            }
            /* Pop all locals down to the loop body entry level so the
             * stack is clean before jumping to the increment / condition. */
            int target_locals = loop_stack.back().local_count;
            for (int i = current->local_count - 1; i >= target_locals; i--)
                emit(OP_POP, line);
            if (loop_stack.back().continue_target == -1) {
                /* for-in: forward jump to increment (patched after body) */
                int patch = emitJump(OP_JUMP, line);
                loop_stack.back().continue_patches.push_back(patch);
            } else {
                /* regular loop: backward jump to condition */
                emitLoop(loop_stack.back().continue_target, line);
            }
            break;
        }
        case NODE_FOR: {
            /*
             * Desugars to:
             *   let __i = 0
             *   let __arr = <iterable>   ← evaluated once
             *   loop __i < len(__arr)
             *     let <item> = __arr[__i]
             *     <body>
             *     __i = __i + 1
             *   end
             *
             * All variables are locals — no globals polluted.
             * break/continue work naturally via loop_stack.
             */
            int outer_locals = current->local_count;  /* before ANY for scope */
            beginScope();

            /* Evaluate the iterable once and store in a hidden local. */
            compileExpr(node->data.for_loop.iterable);
            /* Hidden local __arr (name "" so no warnings) */
            int arr_slot = current->local_count;
            addLocal("__for_arr__", line);
            current->locals[current->local_count - 1].initialized = true;
            current->locals[current->local_count - 1].used = true;

            /* Hidden index local __i = 0 */
            emitConst(NUMBER_VAL(0), line);
            int idx_slot = current->local_count;
            addLocal("__for_idx__", line);
            current->locals[current->local_count - 1].initialized = true;
            current->locals[current->local_count - 1].used = true;

            /* Loop start: condition  __i < len(__arr)
             *
             * Stack for OP_CALL: [callee, arg1, ...]
             * We need: __i on stack, then len(__arr) result.
             * Emit: GET_LOCAL idx, GET_GLOBAL "len", GET_LOCAL arr, CALL 1, LT
             */
            LoopCtx lctx;
            lctx.loop_start        = currentChunk()->count;
            lctx.continue_target   = -1;
            lctx.scope_depth       = 0;
            lctx.local_count       = 0;
            lctx.outer_local_count = outer_locals;  /* break pops all the way here */
            loop_stack.push_back(lctx);

            emit2(OP_GET_LOCAL, (uint8_t)idx_slot, line);
            { emit(OP_GET_GLOBAL, line); emitUint16(identConst("len", line), line); }
            emit2(OP_GET_LOCAL, (uint8_t)arr_slot, line);
            emit2(OP_CALL, 1, line);
            emit(OP_LT, line);

            int exit_jump = emitJump(OP_JUMP_IF_FALSE, line);
            emit(OP_POP, line);

            beginScope();
            emit2(OP_GET_LOCAL, (uint8_t)arr_slot, line);
            emit2(OP_GET_LOCAL, (uint8_t)idx_slot, line);
            emit(OP_GET_INDEX, line);
            addLocal(node->data.for_loop.item, line);
            current->locals[current->local_count - 1].initialized = true;
            current->locals[current->local_count - 1].used = true;

            /* Record scope/locals AFTER item is declared.
             * continue must pop back to here so item is cleaned up. */
            loop_stack.back().scope_depth = current->scope_depth;
            loop_stack.back().local_count = current->local_count;

            compileBlock(node->data.for_loop.body);

            /* Patch all continue jumps to land here (before increment). */
            for (int patch : loop_stack.back().continue_patches)
                patchJumpAt(patch);

            /* Increment __i */
            emit2(OP_GET_LOCAL, (uint8_t)idx_slot, line);
            emitConst(NUMBER_VAL(1), line);
            emit(OP_ADD, line);
            emit2(OP_SET_LOCAL, (uint8_t)idx_slot, line);
            emit(OP_POP, line);

            endScope(line);   /* pops item local */
            emitLoop(loop_stack.back().loop_start, line);

            patchJumpAt(exit_jump);
            emit(OP_POP, line);   /* pop condition false (normal exit) */

            endScope(line);   /* pops __arr and __idx (normal exit) */

            /* Break patches land HERE — after endScope has emitted its POPs in
             * the normal path. The break handler already popped everything
             * manually via outer_local_count, so it jumps directly to here. */
            for (int patch : loop_stack.back().break_patches)
                patchJumpAt(patch);
            loop_stack.pop_back();
            break;
        }
        case NODE_FOR_RANGE: {
            /*
             * Numeric range loop:  for i = start to stop [step s]
             *
             * Desugars to:
             *   let __i   = start      (hidden local, slot 0)
             *   let __stop = stop      (hidden local, slot 1, evaluated once)
             *   let __step = step|1    (hidden local, slot 2, evaluated once)
             *   loop (__step > 0) ? __i < __stop : __i > __stop
             *     let i = __i          (visible loop var)
             *     <body>
             *     __i = __i + __step
             *   end
             *
             * break/continue work via loop_stack, same as for-in.
             * The loop variable 'i' is read-only in spirit but writable
             * (modifying it just affects the visible name, not __i).
             */
            int outer_locals = current->local_count;
            beginScope();

            /* __i = start */
            compileExpr(node->data.for_range.start);
            int i_slot = current->local_count;
            addLocal("__fr_i__", line);
            current->locals[i_slot].initialized = true;
            current->locals[i_slot].used        = true;

            /* __stop = stop (evaluated once) */
            compileExpr(node->data.for_range.stop);
            int stop_slot = current->local_count;
            addLocal("__fr_stop__", line);
            current->locals[stop_slot].initialized = true;
            current->locals[stop_slot].used        = true;

            /* __step = step (or literal 1 if not provided) */
            if (node->data.for_range.step)
                compileExpr(node->data.for_range.step);
            else
                emitConst(NUMBER_VAL(1), line);
            int step_slot = current->local_count;
            addLocal("__fr_step__", line);
            current->locals[step_slot].initialized = true;
            current->locals[step_slot].used        = true;

            /*
             * Hoist step-direction check out of the loop.
             *
             * If step is a compile-time constant (the common case):
             *   Emit a specialized loop with one comparison per iteration.
             *   No per-iteration branch.
             *
             * If step is a runtime expression:
             *   Check direction once before the loop, then jump to
             *   the appropriate loop. Two loops, one direction check.
             */
            bool step_is_const = false;
            double step_val    = 1.0;
            if (node->data.for_range.step == NULL) {
                step_is_const = true; step_val = 1.0;
            } else if (node->data.for_range.step->type == NODE_NUMBER) {
                step_is_const = true;
                step_val = node->data.for_range.step->data.number.value;
            } else if (node->data.for_range.step->type == NODE_UNARY &&
                       node->data.for_range.step->data.unary.op == TK_MINUS &&
                       node->data.for_range.step->data.unary.operand->type == NODE_NUMBER) {
                step_is_const = true;
                step_val = -node->data.for_range.step->data.unary.operand->data.number.value;
            }
            if (step_is_const && step_val == 0.0) {
                compileError(line, "ValueError: for-range step cannot be zero.");
                endScope(line); break;
            }

            /*
             * ── JIT fast path ─────────────────────────────────────────
             *
             * If the body consists ONLY of "local += literal_number"
             * compound assignments AND step is a positive constant,
             * emit OP_JIT_FOR_RANGE — a single opcode that the VM
             * executes via native x86-64 machine code.
             *
             * JIT-able body:  every statement is NODE_COMPOUND_ASSIGN
             *   with TK_PLUS op, local target (not i or stop), and
             *   a NUMBER literal RHS.
             *
             * Non-JIT-able:  any call, conditional, string op, etc.
             *   → falls through to the normal bytecode fast path.
             */
            bool body_jit_ok = step_is_const && step_val > 0.0;
            /* Body vars: (slot, delta) pairs — up to NQ_JIT_MAX_BODY_VARS */
            struct BodyVar { int slot; double delta; int op; /* 0=add, 1=mul */ };
            BodyVar body_vars[8];
            int     n_body_vars = 0;

            if (body_jit_ok && node->data.for_range.body) {
                ASTNode* body = node->data.for_range.body;
                NodeList& stmts = body->data.block.stmts;
                for (int bi = 0; bi < stmts.count && body_jit_ok; bi++) {
                    ASTNode* s = stmts.items[bi];
                    if (!s) continue;
                    if (s->type != NODE_COMPOUND_ASSIGN) { body_jit_ok = false; break; }
                    TokenType bop = s->data.compound_assign.op;
                    if (bop != TK_PLUS && bop != TK_MINUS && bop != TK_STAR) { body_jit_ok = false; break; }
                    if (!s->data.compound_assign.value ||
                        s->data.compound_assign.value->type != NODE_NUMBER) {
                        body_jit_ok = false; break;
                    }
                    int bslot = resolveLocal(current, s->data.compound_assign.name);
                    if (bslot < 0 || bslot == i_slot || bslot == stop_slot) {
                        body_jit_ok = false; break;
                    }
                    if (n_body_vars >= 8) { body_jit_ok = false; break; }
                    body_vars[n_body_vars].slot  = bslot;
                    body_vars[n_body_vars].delta = s->data.compound_assign.value->data.number.value;
                    /* -= is add with negative delta */
                    if (bop == TK_MINUS)
                        body_vars[n_body_vars].delta = -body_vars[n_body_vars].delta;
                    body_vars[n_body_vars].op = (bop == TK_STAR) ? 1 : 0;
                    n_body_vars++;
                }
            }

            if (body_jit_ok) {
                /*
                 * Emit OP_JIT_FOR_RANGE:
                 *
                 *   opcode  (1 byte)
                 *   i_slot  (1 byte)
                 *   stop_slot (1 byte)
                 *   step_const_idx (2 bytes)
                 *   n_body_vars (1 byte)
                 *   for each body var:
                 *     slot (1 byte)
                 *     delta_const_idx (2 bytes)
                 *
                 * The VM reads these and builds a JITLoopSpec.
                 * After execution, __i, __stop, __step are still on the
                 * stack; we just need to clean them up.
                 */
                int step_cidx = addConstant(currentChunk(), NUMBER_VAL(step_val));
                emit(OP_JIT_FOR_RANGE, line);
                emit((uint8_t)i_slot,   line);
                emit((uint8_t)stop_slot, line);
                emitUint16((uint16_t)step_cidx, line);
                emit((uint8_t)n_body_vars, line);
                for (int k = 0; k < n_body_vars; k++) {
                    int dcidx = addConstant(currentChunk(), NUMBER_VAL(body_vars[k].delta));
                    emit((uint8_t)body_vars[k].slot, line);
                    emitUint16((uint16_t)dcidx, line);
                    emit((uint8_t)body_vars[k].op, line);  /* JIT op: 0=add, 1=mul */
                }

                /* Mark all involved locals as used so no spurious warnings */
                current->locals[i_slot].used    = true;
                current->locals[stop_slot].used = true;
                current->locals[step_slot].used = true;
                for (int k = 0; k < n_body_vars; k++)
                    current->locals[body_vars[k].slot].used = true;

                /* JIT handles everything including __i sync;
                 * just clean up the scope's hidden locals from the stack. */
                endScope(line);
                break;
            }

            if (step_is_const) {
                /* ── Fast path: direction known at compile time ── */
                LoopCtx lctx;
                lctx.loop_start      = currentChunk()->count;
                lctx.continue_target = -1;
                lctx.scope_depth     = 0;
                lctx.local_count     = current->local_count;
                lctx.outer_local_count = outer_locals;
                loop_stack.push_back(lctx);

                emit2(OP_GET_LOCAL, (uint8_t)i_slot, line);
                emit2(OP_GET_LOCAL, (uint8_t)stop_slot, line);
                emit(step_val > 0 ? OP_LT : OP_GT, line);
                int exit_j = emitJump(OP_JUMP_IF_FALSE, line);
                emit(OP_POP, line);

                /*
                 * Visible loop variable — zero-copy alias.
                 *
                 * Instead of pushing __i onto a new stack slot every
                 * iteration (GET __i → new slot, POP new slot on endScope),
                 * we rename __i's slot to the user's variable name.
                 * The body reads/writes the counter directly.
                 *
                 * Saves 2 opcodes per iteration: OP_GET_LOCAL + OP_POP.
                 */
                char saved_name[64];
                strncpy(saved_name, current->locals[i_slot].name, 63);
                saved_name[63] = '\0';
                strncpy(current->locals[i_slot].name,
                        node->data.for_range.var, 63);
                current->locals[i_slot].used = true;

                compileBlock(node->data.for_range.body);

                /* Restore hidden name so endScope doesn't warn about it */
                strncpy(current->locals[i_slot].name, saved_name, 63);

                for (int p : loop_stack.back().continue_patches) patchJumpAt(p);

                /* OP_ADD_LOCAL_CONST: fuses GET_LOCAL + CONST + ADD + SET_LOCAL + POP
                 * into a single opcode that modifies the slot in-place.
                 * Saves 4 opcodes per iteration. */
                int step_cidx = addConstant(currentChunk(), NUMBER_VAL(step_val));
                emit(OP_ADD_LOCAL_CONST, line);
                emit((uint8_t)i_slot, line);
                emitUint16((uint16_t)step_cidx, line);

                emitLoop(loop_stack.back().loop_start, line);

                patchJumpAt(exit_j);
                emit(OP_POP, line);
                endScope(line);
                for (int p : loop_stack.back().break_patches) patchJumpAt(p);
                loop_stack.pop_back();

            } else {
                /* ── Slow path: direction check once before loop ── */
                auto emit_one_loop = [&](uint8_t cmp_op) {
                    LoopCtx lx;
                    lx.loop_start      = currentChunk()->count;
                    lx.continue_target = -1;
                    lx.scope_depth     = 0;
                    lx.local_count     = current->local_count;
                    lx.outer_local_count = outer_locals;
                    loop_stack.push_back(lx);

                    emit2(OP_GET_LOCAL, (uint8_t)i_slot, line);
                    emit2(OP_GET_LOCAL, (uint8_t)stop_slot, line);
                    emit(cmp_op, line);
                    int ej = emitJump(OP_JUMP_IF_FALSE, line);
                    emit(OP_POP, line);

                    /* Zero-copy alias: rename i_slot to the user's var name */
                    char sn[64];
                    strncpy(sn, current->locals[i_slot].name, 63); sn[63]='\0';
                    strncpy(current->locals[i_slot].name,
                            node->data.for_range.var, 63);
                    current->locals[i_slot].used = true;
                    compileBlock(node->data.for_range.body);
                    strncpy(current->locals[i_slot].name, sn, 63);

                    for (int p : loop_stack.back().continue_patches) patchJumpAt(p);

                    emit2(OP_GET_LOCAL, (uint8_t)i_slot, line);
                    emit2(OP_GET_LOCAL, (uint8_t)step_slot, line);
                    emit(OP_ADD, line);
                    emit2(OP_SET_LOCAL, (uint8_t)i_slot, line);
                    emit(OP_POP, line);
                    emitLoop(loop_stack.back().loop_start, line);

                    patchJumpAt(ej);
                    emit(OP_POP, line);

                    for (int p : loop_stack.back().break_patches) patchJumpAt(p);
                    loop_stack.pop_back();
                };

                emit2(OP_GET_LOCAL, (uint8_t)step_slot, line);
                emitConst(NUMBER_VAL(0), line);
                emit(OP_GTE, line);
                int to_neg = emitJump(OP_JUMP_IF_FALSE, line);
                emit(OP_POP, line);
                emit_one_loop(OP_LT);
                int to_end = emitJump(OP_JUMP, line);

                patchJumpAt(to_neg);
                emit(OP_POP, line);
                emit_one_loop(OP_GT);
                endScope(line);

                patchJumpAt(to_end);
            }
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
            /*
             * Default parameter injection.
             * For each parameter that has a default expression, emit:
             *   if param == nil then param = <default>
             * This runs at the top of the function body before user code.
             *
             * Pattern:
             *   GET_LOCAL slot
             *   OP_NIL
             *   OP_EQ
             *   JUMP_IF_FALSE skip       ← param was provided, skip default
             *   OP_POP                   ← pop eq result (true)
             *   <default_expr>
             *   SET_LOCAL slot
             *   OP_POP
             * skip:
             *   OP_POP                   ← pop eq result (false)
             */
            if (node->data.function.defaults) {
                for (int i = 0; i < node->data.function.param_count; i++) {
                    if (!node->data.function.defaults[i]) continue;
                    int slot = i + 1; /* slot 0 = fn obj, params start at 1 */
                    emit2(OP_GET_LOCAL, (uint8_t)slot, line);
                    emit(OP_NIL, line);
                    emit(OP_EQ, line);
                    int skip = emitJump(OP_JUMP_IF_FALSE, line);
                    emit(OP_POP, line);                    /* pop eq=true */
                    compileExpr(node->data.function.defaults[i]);
                    emit2(OP_SET_LOCAL, (uint8_t)slot, line);
                    emit(OP_POP, line);                    /* pop SET_LOCAL copy */
                    int end_jump = emitJump(OP_JUMP, line);/* skip false-branch pop */
                    patchJumpAt(skip);
                    emit(OP_POP, line);                    /* pop eq=false */
                    patchJumpAt(end_jump);
                }
            }
            compileBlock(node->data.function.body);
            ObjFunction* compiled_fn = endCompilerCtx();
            int fn_idx = addConstant(currentChunk(), OBJ_VAL(compiled_fn));
            emit(OP_CONST, line); emitUint16((uint16_t)fn_idx, line);
            { emit(OP_DEFINE_GLOBAL, line); emitUint16(identConst(node->data.function.name, line), line); }
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
            if (repl_mode) {
                /*
                 * REPL auto-print: print the expression result if it is not null.
                 *   OP_DUP        — duplicate top (keep for potential print)
                 *   OP_NIL        — push nil for comparison
                 *   OP_EQ         — is it nil/null?
                 *   JUMP_IF_FALSE → skip_print
                 *   OP_POP        — discard eq=true (it IS nil, skip)
                 *   OP_POP        — discard value
                 *   JUMP → end
                 * skip_print:
                 *   OP_POP        — discard eq=false
                 *   OP_PRINT      — print the duplicated value
                 * end:
                 */
                emit(OP_DUP, line);
                emit(OP_NIL, line);
                emit(OP_EQ, line);
                int skip = emitJump(OP_JUMP_IF_FALSE, line);
                /* true branch — value is nil, don't print */
                emit(OP_POP, line);  /* pop eq=true */
                emit(OP_POP, line);  /* pop duplicated nil */
                int end = emitJump(OP_JUMP, line);
                patchJumpAt(skip);
                /* false branch — value is non-nil, print it */
                emit(OP_POP, line);  /* pop eq=false */
                emit(OP_PRINT, line);
                patchJumpAt(end);
            } else {
                emit(OP_POP, line);
            }
            break;
        case NODE_SET_INDEX:
            compileExpr(node->data.set_index.obj);
            compileExpr(node->data.set_index.index);
            compileExpr(node->data.set_index.value);
            emit(OP_SET_INDEX, line);
            break;
        case NODE_TRY: {
            // Emit OP_TRY with a forward jump to the catch block
            int try_jump = emitJump(OP_TRY, line);

            // Compile try body in its own scope
            beginScope();
            compileBlock(node->data.try_stmt.body);
            endScope(line);

            // No error: pop try handler, jump over catch block
            emit(OP_TRY_END, line);
            int over_catch = emitJump(OP_JUMP, line);

            // Patch OP_TRY to jump here on error
            patchJumpAt(try_jump);

            // Catch block: error value is on the stack, bind to err_name
            beginScope();
            addLocal(node->data.try_stmt.err_name, line);
            current->locals[current->local_count - 1].initialized = true;
            compileBlock(node->data.try_stmt.handler);
            endScope(line);

            patchJumpAt(over_catch);
            break;
        }
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
                /* Show which paths were actually searched */
                char tried[768] = {0};
                if (src_path) {
                    const char* sl = strrchr(src_path, '/');
                    if (!sl) sl = strrchr(src_path, '\\');
                    if (sl) {
                        int dl = (int)(sl - src_path + 1);
                        char rel[512];
                        snprintf(rel, sizeof(rel), "%.*s%s.nq", dl, src_path, mod_path);
                        strncat(tried, "\n      ", sizeof(tried) - strlen(tried) - 1);
                        strncat(tried, rel, sizeof(tried) - strlen(tried) - 1);
                    }
                }
                char cwd_p[512];
                snprintf(cwd_p, sizeof(cwd_p), "%s.nq", mod_path);
                strncat(tried, "\n      ", sizeof(tried) - strlen(tried) - 1);
                strncat(tried, cwd_p, sizeof(tried) - strlen(tried) - 1);
                compileError(line,
                    "ImportError: module '%s' not found.\n"
                    "  Searched:%s",
                    mod_path, tried);
                break;
            }

            /* Deduplication: skip if this exact resolved path was already imported. */
            bool already_imported = imported_modules.count(std::string(resolved)) > 0;

            /*
             * from-import name verification (scan only, before execution check).
             */
            if (node->data.import.name_count > 0) {
                /* Read and parse the module to collect its top-level declarations */
                FILE* f_scan = fopen(resolved, "rb");
                if (f_scan) {
                    fseek(f_scan, 0, SEEK_END);
                    long sz = ftell(f_scan); rewind(f_scan);
                    char* src_buf = (char*)malloc((size_t)(sz + 1));
                    size_t nr = fread(src_buf, 1, (size_t)sz, f_scan);
                    src_buf[nr] = '\0';
                    fclose(f_scan);

                    Parser scan_p;
                    initParser(&scan_p, src_buf, resolved);
                    ASTNode* scan_ast = parse(&scan_p);
                    free(src_buf);

                    /* Collect top-level declared names from the module */
                    std::unordered_set<std::string> mod_exports;
                    if (!scan_p.had_error && scan_ast &&
                        (scan_ast->type == NODE_BLOCK || scan_ast->type == NODE_PROGRAM)) {
                        NodeList& stmts = scan_ast->data.block.stmts;
                        for (int i = 0; i < stmts.count; i++) {
                            ASTNode* s = stmts.items[i];
                            if (!s) continue;
                            if (s->type == NODE_LET)
                                mod_exports.insert(s->data.let.name);
                            else if (s->type == NODE_CONST)
                                mod_exports.insert(s->data.const_decl.name);
                            else if (s->type == NODE_FUNCTION)
                                mod_exports.insert(s->data.function.name);
                        }
                    }
                    freeNode(scan_ast);

                    /* Verify each requested name */
                    for (int i = 0; i < node->data.import.name_count; i++) {
                        const char* req = node->data.import.names[i];
                        if (mod_exports.find(std::string(req)) == mod_exports.end()) {
                            compileError(line,
                                "ImportError: '%s' not found in module '%s'.\n"
                                "  Check the name is defined at the top level of the module.\n"
                                "  Use  import \"%s\"  to see all available names.",
                                req, mod_path, mod_path);
                        }
                    }
                }

                if (had_compile_error) break;
            }

            if (had_compile_error) break;

            /* Circular import check: is this module already on the compile stack? */
            std::string resolved_str(resolved);
            for (size_t si = 0; si < import_stack.size(); si++) {
                if (import_stack[si] == resolved_str) {
                    /* Build the cycle chain string: A -> B -> A */
                    char chain[1024] = {0};
                    int chain_pos = 0;
                    for (size_t ci = si; ci < import_stack.size(); ci++) {
                        chain_pos += snprintf(chain + chain_pos,
                            sizeof(chain) - (size_t)chain_pos, "%s -> ",
                            import_stack[ci].c_str());
                        if (chain_pos >= (int)sizeof(chain) - 1) break;
                    }
                    snprintf(chain + chain_pos, sizeof(chain) - (size_t)chain_pos,
                        "%s", resolved);
                    compileError(line,
                        "ImportError: circular import detected.\n"
                        "  Cycle: %s", chain);
                    break;
                }
            }
            if (had_compile_error) break;

            /* Deduplication: skip if already compiled (but NOT if it's in the stack — that's circular) */
            if (already_imported) break;

            imported_modules.insert(resolved_str);

            // Read file
            FILE* f = fopen(resolved, "rb");
            if (!f) { compileError(line, "ImportError: cannot open module '%s'.", resolved); break; }
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
                compileError(line, "ImportError: errors in module '%s'.", resolved);
                break;
            }

            // Compile module as a self-calling function
            ObjFunction* mod_fn = newFunction();
            mod_fn->name = copyString(resolved, (int)strlen(resolved));
            CompilerCtx mod_ctx;
            initCompilerCtx(&mod_ctx, mod_fn);
            mod_ctx.scope_depth = 0;
            import_stack.push_back(std::string(resolved));
            compileBlock(mod_ast);
            import_stack.pop_back();
            freeNode(mod_ast);
            emit(OP_NIL, line);
            emit(OP_RETURN, line);
            ObjFunction* compiled_mod = endCompilerCtx();

            int fn_idx = addConstant(currentChunk(), OBJ_VAL(compiled_mod));
            emit(OP_CONST, line); emitUint16((uint16_t)fn_idx, line);
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

CompileResult compile(ASTNode* ast, const char* source_path, bool is_repl) {
    had_compile_error = false;
    src_path          = source_path;
    repl_mode         = is_repl;
    /* Reset per-compilation state so REPL calls start clean. */
    loop_stack.clear();
    imported_modules.clear();
    import_stack.clear();
    const_globals.clear();
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
