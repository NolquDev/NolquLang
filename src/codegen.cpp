/*
 * codegen.c — Nolqu → C transpiler
 *
 * Emits a self-contained C11 file from a Nolqu AST.
 * A minimal NqStr runtime is embedded at the top so the
 * output compiles with:  gcc -O2 out.c -o prog -lm
 *
 * Type system:
 *   - Numbers, booleans   → double / bool
 *   - Strings             → NqStr  (struct { char* s; int len; })
 *   - Variables           → tracked in CodeGen.syms[]
 *   - Concat (..)         → auto-coerce numbers via nqs_num()
 */
#include "codegen.h"
#include "ast.h"
#include "lexer.h"
#include "common.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/* ─────────────────────────────────────────────
 *  Symbol table helpers
 * ───────────────────────────────────────────── */
static void sym_set(CodeGen* cg, const char* name, bool is_str) {
    for (int i = 0; i < cg->nsyms; i++) {
        if (strcmp(cg->syms[i].name, name) == 0) {
            cg->syms[i].is_str = is_str; return;
        }
    }
    if (cg->nsyms < NQ_SYM_MAX) {
        int n = cg->nsyms++;
        snprintf(cg->syms[n].name, 64, "%s", name);
        cg->syms[n].is_str = is_str;
    }
}

static bool sym_is_str(CodeGen* cg, const char* name) {
    for (int i = 0; i < cg->nsyms; i++)
        if (strcmp(cg->syms[i].name, name) == 0)
            return cg->syms[i].is_str;
    return false;  /* default: number */
}

/* ─────────────────────────────────────────────
 *  Expression type predicates (static analysis)
 * ───────────────────────────────────────────── */
static bool expr_is_str(CodeGen* cg, ASTNode* node) {
    if (!node) return false;
    switch (node->type) {
        case NODE_STRING: return true;
        case NODE_IDENT:  return sym_is_str(cg, node->data.ident.name);
        case NODE_BINARY: return node->data.binary.op == TK_DOTDOT;
        case NODE_CALL:
            if (node->data.call.callee->type == NODE_IDENT) {
                const char* fn = node->data.call.callee->data.ident.name;
                return (!strcmp(fn,"str")     || !strcmp(fn,"upper")  ||
                        !strcmp(fn,"lower")   || !strcmp(fn,"trim")   ||
                        !strcmp(fn,"replace") || !strcmp(fn,"slice")  ||
                        !strcmp(fn,"repeat")  || !strcmp(fn,"join")   ||
                        !strcmp(fn,"input"));
            }
            return false;
        default: return false;
    }
}

/* ─────────────────────────────────────────────
 *  Recursively find all identifiers that appear
 *  as direct operands of any TK_DOTDOT node.
 *  Marks those params as NqStr in the symbol table.
 * ───────────────────────────────────────────── */
static void scan_concat_idents(CodeGen* cg, ASTNode* node,
                                char** params, int np) {
    if (!node) return;
    if (node->type == NODE_BINARY && node->data.binary.op == TK_DOTDOT) {
        /* Both direct children of a concat must be NqStr */
        ASTNode* sides[2] = { node->data.binary.left, node->data.binary.right };
        for (int s = 0; s < 2; s++) {
            if (!sides[s]) continue;
            if (sides[s]->type == NODE_IDENT) {
                /* Mark this param as string */
                for (int i = 0; i < np; i++)
                    if (!strcmp(sides[s]->data.ident.name, params[i]))
                        sym_set(cg, params[i], true);
            }
            /* Recurse into nested concat */
            scan_concat_idents(cg, sides[s], params, np);
        }
    } else {
        /* Recurse into all other node types */
        switch (node->type) {
            case NODE_BINARY:
                scan_concat_idents(cg, node->data.binary.left, params, np);
                scan_concat_idents(cg, node->data.binary.right, params, np);
                break;
            case NODE_UNARY:
                scan_concat_idents(cg, node->data.unary.operand, params, np);
                break;
            case NODE_CALL: {
                NodeList* al = &node->data.call.args;
                for (int i = 0; i < al->count; i++)
                    scan_concat_idents(cg, al->items[i], params, np);
                break;
            }
            case NODE_RETURN:
                scan_concat_idents(cg, node->data.ret.expr, params, np); break;
            case NODE_LET:
                scan_concat_idents(cg, node->data.let.value, params, np); break;
            case NODE_ASSIGN:
                scan_concat_idents(cg, node->data.assign.value, params, np); break;
            case NODE_PRINT:
                scan_concat_idents(cg, node->data.print.expr, params, np); break;
            case NODE_IF:
                scan_concat_idents(cg, node->data.if_stmt.cond, params, np);
                scan_concat_idents(cg, node->data.if_stmt.then_block, params, np);
                scan_concat_idents(cg, node->data.if_stmt.else_block, params, np);
                break;
            case NODE_LOOP:
                scan_concat_idents(cg, node->data.loop.cond, params, np);
                scan_concat_idents(cg, node->data.loop.body, params, np);
                break;
            case NODE_BLOCK: case NODE_PROGRAM: {
                NodeList* sl = &node->data.block.stmts;
                for (int i = 0; i < sl->count; i++)
                    scan_concat_idents(cg, sl->items[i], params, np);
                break;
            }
            default: break;
        }
    }
}
static void do_indent(CodeGen* cg) {
    for (int i = 0; i < cg->indent; i++) fprintf(cg->out, "    ");
}
static void emit_line(CodeGen* cg, const char* fmt, ...) {
    do_indent(cg);
    va_list ap; va_start(ap, fmt); vfprintf(cg->out, fmt, ap); va_end(ap);
    fputc('\n', cg->out);
}

static void emit_stmt(CodeGen* cg, ASTNode* node);
static void emit_expr(CodeGen* cg, ASTNode* node);

/* Emit an expression that must be NqStr — wrap numbers with nqs_num() */
static void emit_as_str(CodeGen* cg, ASTNode* node) {
    if (expr_is_str(cg, node)) {
        emit_expr(cg, node);
    } else {
        fprintf(cg->out, "nqs_num(");
        emit_expr(cg, node);
        fputc(')', cg->out);
    }
}

static void emit_block_stmts(CodeGen* cg, ASTNode* block) {
    if (!block) return;
    if (block->type == NODE_BLOCK || block->type == NODE_PROGRAM) {
        for (int i = 0; i < block->data.block.stmts.count; i++)
            emit_stmt(cg, block->data.block.stmts.items[i]);
    } else {
        emit_stmt(cg, block);
    }
}

/* ─────────────────────────────────────────────
 *  Embedded C runtime (printed once per file)
 * ───────────────────────────────────────────── */
static void emit_preamble(CodeGen* cg) {
    fprintf(cg->out, "/* Generated by Nolqu %s */\n", NQ_VERSION);
    fprintf(cg->out, "/* Compile:  gcc -O2 this_file.c -o program -lm */\n\n");
    fprintf(cg->out,
        "#include <stdio.h>\n#include <stdlib.h>\n"
        "#include <string.h>\n#include <math.h>\n"
        "#include <stdbool.h>\n\n");

    fprintf(cg->out, "/* ── Nolqu inline runtime ── */\n");
    fprintf(cg->out, "typedef struct { char* s; int len; } NqStr;\n\n");

    fprintf(cg->out,
        "static NqStr nqs(const char* cs) {\n"
        "    NqStr r; r.len=(int)strlen(cs);\n"
        "    r.s=(char*)malloc((size_t)(r.len+1));\n"
        "    memcpy(r.s,cs,(size_t)(r.len+1)); return r; }\n\n");

    fprintf(cg->out,
        "static NqStr nqs_num(double n) {\n"
        "    char buf[64];\n"
        "    if (n==(long long)n) snprintf(buf,sizeof(buf),\"%%.0f\",n);\n"
        "    else snprintf(buf,sizeof(buf),\"%%g\",n);\n"
        "    return nqs(buf); }\n\n");

    fprintf(cg->out,
        "static NqStr nqs_cat(NqStr a, NqStr b) {\n"
        "    NqStr r; r.len=a.len+b.len;\n"
        "    r.s=(char*)malloc((size_t)(r.len+1));\n"
        "    memcpy(r.s,a.s,(size_t)a.len);\n"
        "    memcpy(r.s+a.len,b.s,(size_t)b.len);\n"
        "    r.s[r.len]='\\0'; return r; }\n\n");

    fprintf(cg->out,
        "static void nq_print_num(double n) {\n"
        "    if (n==(long long)n) printf(\"%%.0f\\n\",n);\n"
        "    else printf(\"%%g\\n\",n); }\n\n");

    fprintf(cg->out,
        "static void nq_print_str(NqStr s) { printf(\"%%s\\n\",s.s); }\n"
        "static void nq_print_bool(bool b)  { puts(b ? \"true\" : \"false\"); }\n");

    fprintf(cg->out, "/* ── End runtime ── */\n\n");
}

/* ─────────────────────────────────────────────
 *  Binary operator → C string
 * ───────────────────────────────────────────── */
static const char* binop_c(TokenType op) {
    switch (op) {
        case TK_PLUS:   return "+";
        case TK_MINUS:  return "-";
        case TK_STAR:   return "*";
        case TK_SLASH:  return "/";
        case TK_EQEQ:  return "==";
        case TK_BANGEQ: return "!=";
        case TK_LT:     return "<";
        case TK_GT:     return ">";
        case TK_LTEQ:   return "<=";
        case TK_GTEQ:   return ">=";
        case TK_AND:    return "&&";
        case TK_OR:     return "||";
        default:        return "?";
    }
}

/* ─────────────────────────────────────────────
 *  Expression emitter
 * ───────────────────────────────────────────── */
static void emit_expr(CodeGen* cg, ASTNode* node) {
    if (!node) { fprintf(cg->out, "0"); return; }
    switch (node->type) {
        case NODE_NUMBER:
            fprintf(cg->out, "%g", node->data.number.value); break;

        case NODE_BOOL:
            fprintf(cg->out, "%s", node->data.boolean.value ? "true" : "false"); break;

        case NODE_NIL:
            fprintf(cg->out, "0 /* nil */"); break;

        case NODE_STRING:
            fprintf(cg->out, "nqs(\"");
            for (int i = 0; i < node->data.string.length; i++) {
                char c = node->data.string.value[i];
                if      (c=='"')  fputs("\\\"", cg->out);
                else if (c=='\\') fputs("\\\\", cg->out);
                else if (c=='\n') fputs("\\n",  cg->out);
                else if (c=='\t') fputs("\\t",  cg->out);
                else fputc(c, cg->out);
            }
            fprintf(cg->out, "\")");
            break;

        case NODE_IDENT:
            fprintf(cg->out, "%s", node->data.ident.name); break;

        case NODE_UNARY:
            if (node->data.unary.op == TK_MINUS) {
                fprintf(cg->out, "(-");
            } else {
                fprintf(cg->out, "(!");
            }
            emit_expr(cg, node->data.unary.operand);
            fputc(')', cg->out);
            break;

        case NODE_BINARY: {
            TokenType op = node->data.binary.op;
            if (op == TK_DOTDOT) {
                /* String concat — both sides must be NqStr */
                fprintf(cg->out, "nqs_cat(");
                emit_as_str(cg, node->data.binary.left);
                fprintf(cg->out, ", ");
                emit_as_str(cg, node->data.binary.right);
                fputc(')', cg->out);
            } else if (op == TK_PERCENT) {
                fprintf(cg->out, "fmod(");
                emit_expr(cg, node->data.binary.left);
                fprintf(cg->out, ", ");
                emit_expr(cg, node->data.binary.right);
                fputc(')', cg->out);
            } else {
                fputc('(', cg->out);
                emit_expr(cg, node->data.binary.left);
                fprintf(cg->out, " %s ", binop_c(op));
                emit_expr(cg, node->data.binary.right);
                fputc(')', cg->out);
            }
            break;
        }

        case NODE_CALL: {
            const char* fn = (node->data.call.callee->type == NODE_IDENT)
                ? node->data.call.callee->data.ident.name : NULL;
            int ac = node->data.call.args.count;
            NodeList* al = &node->data.call.args;

            /* Map common Nolqu builtins → C */
            if (fn) {
                if (!strcmp(fn,"sqrt")  && ac==1){ fprintf(cg->out,"sqrt(");  emit_expr(cg,al->items[0]); fputc(')',cg->out); break; }
                if (!strcmp(fn,"floor") && ac==1){ fprintf(cg->out,"floor("); emit_expr(cg,al->items[0]); fputc(')',cg->out); break; }
                if (!strcmp(fn,"ceil")  && ac==1){ fprintf(cg->out,"ceil(");  emit_expr(cg,al->items[0]); fputc(')',cg->out); break; }
                if (!strcmp(fn,"round") && ac==1){ fprintf(cg->out,"round("); emit_expr(cg,al->items[0]); fputc(')',cg->out); break; }
                if (!strcmp(fn,"abs")   && ac==1){ fprintf(cg->out,"fabs(");  emit_expr(cg,al->items[0]); fputc(')',cg->out); break; }
                if (!strcmp(fn,"pow")   && ac==2){
                    fprintf(cg->out,"pow("); emit_expr(cg,al->items[0]);
                    fputc(',',cg->out); emit_expr(cg,al->items[1]); fputc(')',cg->out); break;
                }
                if (!strcmp(fn,"str")   && ac==1){ fprintf(cg->out,"nqs_num("); emit_expr(cg,al->items[0]); fputc(')',cg->out); break; }
                if (!strcmp(fn,"len")   && ac==1){ fputc('(',cg->out); emit_expr(cg,al->items[0]); fprintf(cg->out,").len"); break; }
            }

            /* Generic function call — pass args with correct types */
            emit_expr(cg, node->data.call.callee);
            fputc('(', cg->out);
            for (int i = 0; i < ac; i++) {
                if (i) fprintf(cg->out, ", ");
                emit_expr(cg, al->items[i]);
            }
            fputc(')', cg->out);
            break;
        }

        case NODE_GET_INDEX:
            emit_expr(cg, node->data.get_index.obj);
            fprintf(cg->out, "[(int)(");
            emit_expr(cg, node->data.get_index.index);
            fprintf(cg->out, ")]");
            break;

        case NODE_ARRAY:
            fprintf(cg->out, "NULL /* array literal needs NQ runtime */");
            break;

        default:
            fprintf(cg->out, "0 /* unsupported expr */");
            break;
    }
}

/* ─────────────────────────────────────────────
 *  Statement emitter
 * ───────────────────────────────────────────── */
static void emit_stmt(CodeGen* cg, ASTNode* node) {
    if (!node) return;
    switch (node->type) {

        case NODE_LET: {
            const char* name = node->data.let.name;
            ASTNode*    val  = node->data.let.value;
            bool vstr = val ? expr_is_str(cg, val) : false;
            sym_set(cg, name, vstr);

            do_indent(cg);
            if (!val) {
                fprintf(cg->out, "double %s = 0;\n", name);
            } else if (val->type == NODE_NUMBER) {
                fprintf(cg->out, "double %s = %g;\n", name, val->data.number.value);
            } else if (val->type == NODE_BOOL) {
                fprintf(cg->out, "bool %s = %s;\n", name,
                    val->data.boolean.value ? "true" : "false");
            } else if (vstr) {
                fprintf(cg->out, "NqStr %s = ", name);
                emit_expr(cg, val);
                fprintf(cg->out, ";\n");
            } else {
                fprintf(cg->out, "double %s = ", name);
                emit_expr(cg, val);
                fprintf(cg->out, ";\n");
            }
            break;
        }

        case NODE_ASSIGN: {
            do_indent(cg);
            fprintf(cg->out, "%s = ", node->data.assign.name);
            /* Update symbol type if reassigning with different type */
            bool vstr = node->data.assign.value ? expr_is_str(cg, node->data.assign.value) : false;
            sym_set(cg, node->data.assign.name, vstr);
            emit_expr(cg, node->data.assign.value);
            fprintf(cg->out, ";\n");
            break;
        }

        case NODE_PRINT: {
            ASTNode* val = node->data.print.expr;
            if (!val) { emit_line(cg, "puts(\"\");"); break; }

            do_indent(cg);
            if (val->type == NODE_STRING) {
                fprintf(cg->out, "printf(\"");
                for (int i = 0; i < val->data.string.length; i++) {
                    char c = val->data.string.value[i];
                    if      (c=='"')  fputs("\\\"", cg->out);
                    else if (c=='%')  fputs("%%",   cg->out);
                    else if (c=='\\') fputs("\\\\", cg->out);
                    else fputc(c, cg->out);
                }
                fprintf(cg->out, "\\n\");\n");
            } else if (val->type == NODE_NUMBER) {
                fprintf(cg->out, "nq_print_num(%g);\n", val->data.number.value);
            } else if (val->type == NODE_BOOL) {
                fprintf(cg->out, "nq_print_bool(%s);\n",
                    val->data.boolean.value ? "true" : "false");
            } else if (expr_is_str(cg, val)) {
                fprintf(cg->out, "nq_print_str(");
                emit_expr(cg, val);
                fprintf(cg->out, ");\n");
            } else {
                fprintf(cg->out, "nq_print_num((double)(");
                emit_expr(cg, val);
                fprintf(cg->out, "));\n");
            }
            break;
        }

        case NODE_IF: {
            do_indent(cg); fprintf(cg->out, "if (");
            emit_expr(cg, node->data.if_stmt.cond);
            fprintf(cg->out, ") {\n");
            cg->indent++;
            emit_block_stmts(cg, node->data.if_stmt.then_block);
            cg->indent--;
            if (node->data.if_stmt.else_block) {
                emit_line(cg, "} else {");
                cg->indent++;
                emit_block_stmts(cg, node->data.if_stmt.else_block);
                cg->indent--;
            }
            emit_line(cg, "}");
            break;
        }

        case NODE_LOOP: {
            do_indent(cg); fprintf(cg->out, "while (");
            emit_expr(cg, node->data.loop.cond);
            fprintf(cg->out, ") {\n");
            cg->indent++;
            emit_block_stmts(cg, node->data.loop.body);
            cg->indent--;
            emit_line(cg, "}");
            break;
        }

        case NODE_FUNCTION: {
            int np  = node->data.function.param_count;
            ASTNode* body = node->data.function.body;
            int saved_nsyms = cg->nsyms;

            /* Param type inference — mark all as double first, then
               recursively scan body for concat usage */
            for (int i = 0; i < np; i++)
                sym_set(cg, node->data.function.params[i], false);
            if (body)
                scan_concat_idents(cg, body, node->data.function.params, np);

            /* Determine return type */
            bool fn_ret_str = false;
            if (body) {
                NodeList* sl = (body->type==NODE_BLOCK||body->type==NODE_PROGRAM)
                    ? &body->data.block.stmts : NULL;
                if (sl) for (int i=0;i<sl->count;i++) {
                    ASTNode* s=sl->items[i];
                    if (s && s->type==NODE_RETURN && s->data.ret.expr &&
                        expr_is_str(cg, s->data.ret.expr)) { fn_ret_str=true; break; }
                }
            }

            /* Register the function itself in the symbol table */
            sym_set(cg, node->data.function.name, fn_ret_str);

            /* Emit signature */
            fprintf(cg->out, "%s %s(", fn_ret_str ? "NqStr" : "double",
                    node->data.function.name);
            for (int i = 0; i < np; i++) {
                if (i) fprintf(cg->out, ", ");
                bool ps = sym_is_str(cg, node->data.function.params[i]);
                fprintf(cg->out, "%s %s", ps ? "NqStr" : "double",
                        node->data.function.params[i]);
            }
            if (np == 0) fprintf(cg->out, "void");
            fprintf(cg->out, ") {\n");
            cg->indent++;
            emit_block_stmts(cg, body);
            emit_line(cg, fn_ret_str ? "return nqs(\"\");" : "return 0;");
            cg->indent--;
            fprintf(cg->out, "}\n\n");

            /* Restore symbol table scope */
            cg->nsyms = saved_nsyms;
            break;
        }

        case NODE_RETURN: {
            do_indent(cg); fprintf(cg->out, "return ");
            if (node->data.ret.expr) emit_expr(cg, node->data.ret.expr);
            else fprintf(cg->out, "0");
            fprintf(cg->out, ";\n");
            break;
        }

        case NODE_EXPR_STMT: {
            do_indent(cg);
            emit_expr(cg, node->data.expr_stmt.expr);
            fprintf(cg->out, ";\n");
            break;
        }

        case NODE_SET_INDEX: {
            do_indent(cg);
            emit_expr(cg, node->data.set_index.obj);
            fprintf(cg->out, "[(int)(");
            emit_expr(cg, node->data.set_index.index);
            fprintf(cg->out, ")] = ");
            emit_expr(cg, node->data.set_index.value);
            fprintf(cg->out, ";\n");
            break;
        }

        case NODE_IMPORT:
            do_indent(cg);
            fprintf(cg->out, "/* import \"%s\" — not supported in transpiled output */\n",
                node->data.import.path);
            break;

        case NODE_TRY:
            do_indent(cg);
            fprintf(cg->out, "/* try/catch — not supported in transpiled output */\n");
            do_indent(cg); fprintf(cg->out, "{\n");
            cg->indent++;
            emit_block_stmts(cg, node->data.try_stmt.body);
            cg->indent--;
            emit_line(cg, "}");
            break;

        case NODE_BLOCK:
        case NODE_PROGRAM:
            emit_block_stmts(cg, node);
            break;

        default:
            do_indent(cg);
            fprintf(cg->out, "/* unsupported stmt %d */\n", (int)node->type);
            break;
    }
}

/* ─────────────────────────────────────────────
 *  Public API
 * ───────────────────────────────────────────── */
void codegen_init(CodeGen* cg, FILE* out) {
    cg->out = out; cg->indent = 0; cg->tmp_count = 0;
    cg->had_error = false; cg->error_msg[0] = '\0';
    cg->nsyms = 0;
}

void codegen_emit(CodeGen* cg, ASTNode* root) {
    emit_preamble(cg);

    if (!root || (root->type != NODE_PROGRAM && root->type != NODE_BLOCK)) {
        emit_stmt(cg, root);
        return;
    }

    NodeList* stmts = &root->data.block.stmts;

    /* Forward declarations — do a pre-pass to infer types */
    bool any_fwd = false;
    for (int i = 0; i < stmts->count; i++) {
        ASTNode* s = stmts->items[i];
        if (!s || s->type != NODE_FUNCTION) continue;
        if (!any_fwd) { fprintf(cg->out, "/* Forward declarations */\n"); any_fwd = true; }

        int np = s->data.function.param_count;
        ASTNode* body = s->data.function.body;

        /* Param type inference using recursive concat scan */
        for (int j = 0; j < np; j++)
            sym_set(cg, s->data.function.params[j], false);
        if (body)
            scan_concat_idents(cg, body, s->data.function.params, np);

        /* Infer return type */
        bool fn_ret_str = false;
        if (body) {
            NodeList* sl=(body->type==NODE_BLOCK||body->type==NODE_PROGRAM)?&body->data.block.stmts:NULL;
            if (sl) for (int j=0;j<sl->count;j++) {
                ASTNode* rs=sl->items[j];
                if (rs&&rs->type==NODE_RETURN&&rs->data.ret.expr&&
                    expr_is_str(cg,rs->data.ret.expr)){fn_ret_str=true;break;}
            }
        }

        /* Register function in symbol table with return type */
        sym_set(cg, s->data.function.name, fn_ret_str);

        fprintf(cg->out, "%s %s(", fn_ret_str ? "NqStr" : "double", s->data.function.name);
        for (int j = 0; j < np; j++) {
            if (j) fprintf(cg->out, ", ");
            fprintf(cg->out, "%s", sym_is_str(cg, s->data.function.params[j]) ? "NqStr" : "double");
        }
        if (np == 0) fprintf(cg->out, "void");
        fprintf(cg->out, ");\n");

        /* Clear param symbols after fwd decl */
        for (int j = 0; j < np; j++) {
            for (int k = 0; k < cg->nsyms; k++) {
                if (!strcmp(cg->syms[k].name, s->data.function.params[j])) {
                    /* Remove by shifting */
                    for (int m=k; m<cg->nsyms-1; m++) cg->syms[m]=cg->syms[m+1];
                    cg->nsyms--; break;
                }
            }
        }
    }
    if (any_fwd) fprintf(cg->out, "\n");

    /* Emit function bodies */
    for (int i = 0; i < stmts->count; i++) {
        ASTNode* s = stmts->items[i];
        if (s && s->type == NODE_FUNCTION) emit_stmt(cg, s);
    }

    /* Emit main() */
    fprintf(cg->out, "int main(void) {\n");
    cg->indent = 1;
    for (int i = 0; i < stmts->count; i++) {
        ASTNode* s = stmts->items[i];
        if (s && s->type != NODE_FUNCTION) emit_stmt(cg, s);
    }
    fprintf(cg->out, "    return 0;\n}\n");
}
