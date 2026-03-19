#include "parser.h"
#include "memory.h"
#include <string.h>

static void errorAt(Parser* p, Token* tok, const char* msg) {
    if (p->panic_mode) return;
    p->panic_mode = true;
    p->had_error  = true;

    fprintf(stderr, NQ_COLOR_RED "[ Error ]" NQ_COLOR_RESET);
    if (p->source_path) fprintf(stderr, " %s", p->source_path);
    fprintf(stderr, ":%d\n", tok->line);
    fprintf(stderr, "  " NQ_COLOR_BOLD "%s" NQ_COLOR_RESET "\n", msg);

    if (tok->type == TK_ERROR && tok->error) {
        fprintf(stderr, "  Detail: %s\n", tok->error);
    } else if (tok->type != TK_EOF && tok->type != TK_NEWLINE) {
        fprintf(stderr, "  Found: '%.*s'\n", tok->length, tok->start);
    }
    fprintf(stderr, "\n");
}

static void errorAtCurrent(Parser* p, const char* msg) { errorAt(p, &p->current,  msg); }

static void advance(Parser* p) {
    p->previous = p->current;
    for (;;) {
        p->current = nextToken(&p->lexer);
        if (p->current.type != TK_ERROR) break;
        errorAtCurrent(p, "Unknown character.");
    }
}

static bool check(Parser* p, TokenType t) { return p->current.type == t; }

static bool match(Parser* p, TokenType t) {
    if (!check(p, t)) return false;
    advance(p);
    return true;
}

static void expect(Parser* p, TokenType t, const char* msg) {
    if (p->current.type == t) { advance(p); return; }
    errorAtCurrent(p, msg);
}

static void skipNewlines(Parser* p) {
    while (check(p, TK_NEWLINE)) advance(p);
}

static void expectNewline(Parser* p) {
    if (check(p, TK_EOF))     return;
    if (check(p, TK_NEWLINE)) { advance(p); return; }
    errorAtCurrent(p, "Expected a new line after statement.");
}

static char* dupStr(const char* src, int len) {
    char* s = (char*)malloc((size_t)(len + 1));
    memcpy(s, src, (size_t)len);
    s[len] = '\0';
    return s;
}

static char* extractString(const char* src, int total_len, int* out_len) {
    int content_len = total_len - 2;
    const char* content = src + 1;
    char* buf = (char*)malloc((size_t)(content_len + 1));
    int j = 0;
    for (int i = 0; i < content_len; i++) {
        if (content[i] == '\\' && i + 1 < content_len) {
            i++;
            switch (content[i]) {
                case 'n':  buf[j++] = '\n'; break;
                case 't':  buf[j++] = '\t'; break;
                case 'r':  buf[j++] = '\r'; break;
                case '"':  buf[j++] = '"';  break;
                case '\\': buf[j++] = '\\'; break;
                default:   buf[j++] = '\\'; buf[j++] = content[i]; break;
            }
        } else {
            buf[j++] = content[i];
        }
    }
    buf[j] = '\0';
    if (out_len) *out_len = j;
    return buf;
}

// Forward declarations
static ASTNode* parseStmt(Parser* p);
static ASTNode* parseExpr(Parser* p);
static ASTNode* parseOr(Parser* p);
static ASTNode* parseAnd(Parser* p);
static ASTNode* parseEquality(Parser* p);
static ASTNode* parseComparison(Parser* p);
static ASTNode* parseAddition(Parser* p);
static ASTNode* parseMultiply(Parser* p);
static ASTNode* parseUnary(Parser* p);
static ASTNode* parseCall(Parser* p);
static ASTNode* parsePrimary(Parser* p);
static ASTNode* parseBlock(Parser* p);

static ASTNode* parseLetStmt(Parser* p) {
    int line = p->previous.line;
    expect(p, TK_IDENT, "Expected a variable name after 'let'. Example: let name = \"Alice\"");
    char* name = dupStr(p->previous.start, p->previous.length);
    expect(p, TK_EQ, "Expected '=' after variable name. Example: let x = 10");
    ASTNode* val = parseExpr(p);
    expectNewline(p);
    ASTNode* n = makeNode(NODE_LET, line);
    n->data.let.name  = name;
    n->data.let.value = val;
    return n;
}

static ASTNode* parsePrintStmt(Parser* p) {
    int line = p->previous.line;
    ASTNode* expr = parseExpr(p);
    expectNewline(p);
    ASTNode* n = makeNode(NODE_PRINT, line);
    n->data.print.expr = expr;
    return n;
}

static ASTNode* parseIfStmt(Parser* p) {
    int line = p->previous.line;
    ASTNode* cond = parseExpr(p);
    expectNewline(p);
    ASTNode* then_block = parseBlock(p);
    ASTNode* else_block = NULL;
    if (check(p, TK_ELSE)) {
        advance(p);
        expectNewline(p);
        else_block = parseBlock(p);
    }
    expect(p, TK_END, "Expected 'end' to close 'if' block");
    expectNewline(p);
    ASTNode* n = makeNode(NODE_IF, line);
    n->data.if_stmt.cond       = cond;
    n->data.if_stmt.then_block = then_block;
    n->data.if_stmt.else_block = else_block;
    return n;
}

static ASTNode* parseLoopStmt(Parser* p) {
    int line = p->previous.line;
    ASTNode* cond = parseExpr(p);
    expectNewline(p);
    ASTNode* body = parseBlock(p);
    expect(p, TK_END, "Expected 'end' to close 'loop' block");
    expectNewline(p);
    ASTNode* n = makeNode(NODE_LOOP, line);
    n->data.loop.cond = cond;
    n->data.loop.body = body;
    return n;
}

/* for item in iterable
     body
   end  */
static ASTNode* parseForStmt(Parser* p) {
    int line = p->previous.line;
    expect(p, TK_IDENT, "Expected a variable name after 'for'. Example: for item in list");
    char* item = dupStr(p->previous.start, p->previous.length);
    expect(p, TK_IN, "Expected 'in' after loop variable. Example: for item in list");
    ASTNode* iterable = parseExpr(p);
    expectNewline(p);
    ASTNode* body = parseBlock(p);
    expect(p, TK_END, "Expected 'end' to close 'for' block");
    expectNewline(p);
    ASTNode* n = makeNode(NODE_FOR, line);
    n->data.for_loop.item     = item;
    n->data.for_loop.iterable = iterable;
    n->data.for_loop.body     = body;
    return n;
}

static ASTNode* parseFunctionDecl(Parser* p) {
    int line = p->previous.line;
    expect(p, TK_IDENT, "Expected a function name after 'function'");
    char* name = dupStr(p->previous.start, p->previous.length);
    expect(p, TK_LPAREN, "Expected '(' to open parameter list");

    char**    params   = NULL;
    ASTNode** defaults = NULL;
    int       n_params = 0;
    int       p_cap    = 0;
    bool      saw_default = false;

    while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
        expect(p, TK_IDENT, "Expected a parameter name");
        char* pname = dupStr(p->previous.start, p->previous.length);

        if (n_params >= p_cap) {
            int old = p_cap;
            p_cap    = GROW_CAPACITY(old);
            params   = GROW_ARRAY(char*,    params,   old, p_cap);
            defaults = GROW_ARRAY(ASTNode*, defaults, old, p_cap);
        }
        params[n_params]   = pname;
        defaults[n_params] = NULL;

        /* Optional default: param = expr */
        if (check(p, TK_EQ)) {
            advance(p);
            defaults[n_params] = parseExpr(p);
            saw_default = true;
        } else if (saw_default) {
            errorAt(p, &p->previous,
                "Non-default parameter after a default parameter.");
        }
        n_params++;
        if (!match(p, TK_COMMA)) break;
    }
    expect(p, TK_RPAREN, "Expected ')' to close parameter list");
    expectNewline(p);

    ASTNode* body = parseBlock(p);
    expect(p, TK_END, "Expected 'end' to close function body");
    expectNewline(p);

    /* Free defaults array if no defaults were used */
    if (!saw_default) {
        FREE_ARRAY(ASTNode*, defaults, p_cap);
        defaults = NULL;
    }

    ASTNode* n = makeNode(NODE_FUNCTION, line);
    n->data.function.name        = name;
    n->data.function.params      = params;
    n->data.function.defaults    = defaults;
    n->data.function.param_count = n_params;
    n->data.function.body        = body;
    return n;
}

static ASTNode* parseReturnStmt(Parser* p) {
    int line = p->previous.line;
    ASTNode* expr = NULL;
    if (!check(p, TK_NEWLINE) && !check(p, TK_EOF)) {
        expr = parseExpr(p);
    }
    expectNewline(p);
    ASTNode* n = makeNode(NODE_RETURN, line);
    n->data.ret.expr = expr;
    return n;
}

static ASTNode* parseImportStmt(Parser* p) {
    int line = p->previous.line;

    /*
     * Supported import forms (v1.2.0):
     *
     *   import "stdlib/math"     ← quoted string path (original form)
     *   import stdlib/math       ← unquoted slash-separated path
     *
     * Removed in v1.2.0 (were silent no-ops / misleading):
     *   import X as Y            → parse error: use 'import X' directly
     *   from X import a, b       → parse error: Nolqu has no selective import
     *
     * Module caching: each module is compiled and run at most once per
     * program execution regardless of how many times it is imported.
     */

    char* path = NULL;

    if (check(p, TK_STRING)) {
        advance(p);
        path = extractString(p->previous.start, p->previous.length, NULL);
    } else {
        /*
         * Unquoted path: read identifier segments separated by '/'.
         *   import stdlib/math   →  path = "stdlib/math"
         *   import mymod         →  path = "mymod"
         *
         * Stop as soon as the segment after an identifier is not '/'
         * (e.g. stop before 'as', newline, or anything else).
         */
        char buf[512] = {0};
        bool got_one = false;
        while (check(p, TK_IDENT)) {
            Token t = p->current; advance(p);
            strncat(buf, t.start, (size_t)t.length);
            got_one = true;
            if (check(p, TK_SLASH)) {
                advance(p);
                strncat(buf, "/", 1);
            } else {
                break;   /* no slash follows — end of path */
            }
        }
        if (!got_one) {
            errorAt(p, &p->current,
                "Expected a module path after 'import'.\n"
                "  Examples:  import \"stdlib/math\"\n"
                "             import stdlib/math");
            return NULL;
        }
        path = dupStr(buf, (int)strlen(buf));
    }

    /* Reject 'import X as Y' with a clear error */
    if (check(p, TK_AS)) {
        errorAt(p, &p->current,
            "'import X as Y' is not supported — Nolqu has no module namespaces.\n"
            "  Use:  import \"stdlib/math\"\n"
            "  Then access its functions directly: PI, sin, cos, ...");
        /* consume 'as <alias>' so parser can continue */
        advance(p);  /* skip 'as' */
        if (check(p, TK_IDENT)) advance(p);  /* skip alias */
        expectNewline(p);
        /* Return a valid import node so compilation can continue */
        ASTNode* n = makeNode(NODE_IMPORT, line);
        n->data.import.path = path;
        return n;
    }

    expectNewline(p);
    ASTNode* n = makeNode(NODE_IMPORT, line);
    n->data.import.path = path;
    return n;
}

static ASTNode* parseFromImportStmt(Parser* p) {
    /*
     * 'from X import Y' is not supported in v1.2.0.
     * Nolqu has no selective import — all module declarations become global.
     * Removed in v1.2.0 because the previous implementation silently imported
     * everything regardless of the names listed.
     *
     * Use:  import "stdlib/math"   (imports all of math's definitions)
     */
    errorAt(p, &p->current,
        "'from X import Y' is not supported.\n"
        "  Use:  import \"stdlib/math\"\n"
        "  All module definitions become available globally after import.");
    /* consume rest of line to allow parser to recover */
    while (!check(p, TK_NEWLINE) && !check(p, TK_EOF)) advance(p);
    return NULL;
}

static ASTNode* parseTryStmt(Parser* p) {
    int line = p->previous.line;
    expectNewline(p);

    // Parse try body
    ASTNode* body = makeNode(NODE_BLOCK, line);
    initNodeList(&body->data.block.stmts);
    while (!check(p, TK_CATCH) && !check(p, TK_EOF)) {
        ASTNode* stmt = parseStmt(p);
        if (stmt) appendNode(&body->data.block.stmts, stmt);
        skipNewlines(p);
    }
    expect(p, TK_CATCH, "Expected 'catch' after try block.");

    // catch err_name
    expect(p, TK_IDENT, "Expected a variable name after 'catch'. Example: catch err");
    char* err_name = dupStr(p->previous.start, p->previous.length);
    expectNewline(p);

    // Parse handler body
    ASTNode* handler = makeNode(NODE_BLOCK, p->previous.line);
    initNodeList(&handler->data.block.stmts);
    while (!check(p, TK_END) && !check(p, TK_EOF)) {
        ASTNode* stmt = parseStmt(p);
        if (stmt) appendNode(&handler->data.block.stmts, stmt);
        skipNewlines(p);
    }
    expect(p, TK_END, "Expected 'end' to close try/catch block.");
    expectNewline(p);

    ASTNode* n = makeNode(NODE_TRY, line);
    n->data.try_stmt.body     = body;
    n->data.try_stmt.err_name = err_name;
    n->data.try_stmt.handler  = handler;
    return n;
}

static ASTNode* parseStmt(Parser* p) {
    skipNewlines(p);
    if (check(p, TK_EOF)) return NULL;
    advance(p);
    Token tok = p->previous;

    switch (tok.type) {
        case TK_LET:      return parseLetStmt(p);
        case TK_CONST: {
            /* const name = expr  — immutable binding */
            int line = tok.line;
            expect(p, TK_IDENT, "Expected a name after 'const'. Example: const PI = 3.14");
            char* name = dupStr(p->previous.start, p->previous.length);
            expect(p, TK_EQ, "Expected '=' after const name.");
            ASTNode* val = parseExpr(p);
            expectNewline(p);
            ASTNode* n = makeNode(NODE_CONST, line);
            n->data.const_decl.name  = name;
            n->data.const_decl.value = val;
            return n;
        }
        case TK_PRINT:    return parsePrintStmt(p);
        case TK_IF:       return parseIfStmt(p);
        case TK_LOOP:     return parseLoopStmt(p);
        case TK_FOR:      return parseForStmt(p);
        case TK_FUNCTION: return parseFunctionDecl(p);
        case TK_RETURN:   return parseReturnStmt(p);
        case TK_IMPORT:   return parseImportStmt(p);
        case TK_FROM:     return parseFromImportStmt(p);
        case TK_TRY:      return parseTryStmt(p);
        case TK_BREAK: {
            expectNewline(p);
            return makeNode(NODE_BREAK, tok.line);
        }
        case TK_CONTINUE: {
            expectNewline(p);
            return makeNode(NODE_CONTINUE, tok.line);
        }

        case TK_IDENT: {
            if (check(p, TK_EQ)) {
                int line  = tok.line;
                char* name = dupStr(tok.start, tok.length);
                advance(p);
                ASTNode* val = parseExpr(p);
                expectNewline(p);
                ASTNode* n = makeNode(NODE_ASSIGN, line);
                n->data.assign.name  = name;
                n->data.assign.value = val;
                return n;
            }
            /* Compound assignment: name op= expr */
            if (check(p, TK_PLUS_EQ)  || check(p, TK_MINUS_EQ) ||
                check(p, TK_STAR_EQ)  || check(p, TK_SLASH_EQ) ||
                check(p, TK_DOTDOT_EQ)) {
                int line = tok.line;
                char* name = dupStr(tok.start, tok.length);
                /* Map op= token to its binary operator token */
                TokenType compound_tok = p->current.type;
                TokenType op = (compound_tok == TK_PLUS_EQ)   ? TK_PLUS  :
                               (compound_tok == TK_MINUS_EQ)  ? TK_MINUS :
                               (compound_tok == TK_STAR_EQ)   ? TK_STAR  :
                               (compound_tok == TK_SLASH_EQ)  ? TK_SLASH :
                                                                 TK_DOTDOT;
                advance(p); /* consume the op= token */
                ASTNode* rhs = parseExpr(p);
                expectNewline(p);
                ASTNode* n = makeNode(NODE_COMPOUND_ASSIGN, line);
                n->data.compound_assign.name  = name;
                n->data.compound_assign.op    = op;
                n->data.compound_assign.value = rhs;
                return n;
            }
            // arr[i] = val  (index assignment)
            if (check(p, TK_LBRACKET)) {
                int line = tok.line;
                ASTNode* obj = makeNode(NODE_IDENT, line);
                obj->data.ident.name = dupStr(tok.start, tok.length);
                advance(p); // consume '['
                ASTNode* idx = parseExpr(p);
                expect(p, TK_RBRACKET, "Expected ']' to close index");
                if (check(p, TK_EQ)) {
                    advance(p);
                    ASTNode* val = parseExpr(p);
                    expectNewline(p);
                    ASTNode* n = makeNode(NODE_SET_INDEX, line);
                    n->data.set_index.obj   = obj;
                    n->data.set_index.index = idx;
                    n->data.set_index.value = val;
                    return n;
                }
                // Otherwise it's a get-index used as expression statement
                ASTNode* get = makeNode(NODE_GET_INDEX, line);
                get->data.get_index.obj   = obj;
                get->data.get_index.index = idx;
                ASTNode* stmt = makeNode(NODE_EXPR_STMT, line);
                stmt->data.expr_stmt.expr = get;
                expectNewline(p);
                return stmt;
            }
            ASTNode* ident = makeNode(NODE_IDENT, tok.line);
            ident->data.ident.name = dupStr(tok.start, tok.length);

            ASTNode* expr = ident;
            while (check(p, TK_LPAREN)) {
                int line = p->current.line;
                advance(p);
                ASTNode* call_node = makeNode(NODE_CALL, line);
                call_node->data.call.callee = expr;
                initNodeList(&call_node->data.call.args);
                while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
                    appendNode(&call_node->data.call.args, parseExpr(p));
                    if (!match(p, TK_COMMA)) break;
                }
                expect(p, TK_RPAREN, "Expected ')' to close argument list");
                expr = call_node;
            }
            expectNewline(p);
            ASTNode* stmt = makeNode(NODE_EXPR_STMT, tok.line);
            stmt->data.expr_stmt.expr = expr;
            return stmt;
        }

        case TK_END:
        case TK_ELSE:
        case TK_EOF:
            return NULL;

        default: {
            char errbuf[128];
            snprintf(errbuf, sizeof(errbuf),
                "Invalid statement starting with '%s'. "
                "Did you forget a keyword like 'let', 'print', or 'if'?",
                tokenTypeName(tok.type));
            errorAt(p, &tok, errbuf);
            while (!check(p, TK_NEWLINE) && !check(p, TK_EOF)) advance(p);
            if (check(p, TK_NEWLINE)) advance(p);
            p->panic_mode = false;
            return NULL;
        }
    }
}

static ASTNode* parseBlock(Parser* p) {
    ASTNode* block = makeNode(NODE_BLOCK, p->current.line);
    initNodeList(&block->data.block.stmts);
    while (!check(p, TK_END) && !check(p, TK_ELSE) && !check(p, TK_EOF)) {
        skipNewlines(p);
        if (check(p, TK_END) || check(p, TK_ELSE) || check(p, TK_EOF)) break;
        ASTNode* stmt = parseStmt(p);
        if (stmt) appendNode(&block->data.block.stmts, stmt);
    }
    return block;
}

static ASTNode* parseExpr(Parser* p)  { return parseOr(p); }

static ASTNode* parseOr(Parser* p) {
    ASTNode* left = parseAnd(p);
    while (check(p, TK_OR)) {
        advance(p); int line = p->previous.line;
        ASTNode* right = parseAnd(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op = TK_OR; n->data.binary.left = left; n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseAnd(Parser* p) {
    ASTNode* left = parseEquality(p);
    while (check(p, TK_AND)) {
        advance(p); int line = p->previous.line;
        ASTNode* right = parseEquality(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op = TK_AND; n->data.binary.left = left; n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseEquality(Parser* p) {
    ASTNode* left = parseComparison(p);
    while (check(p, TK_EQEQ) || check(p, TK_BANGEQ)) {
        TokenType op = p->current.type; advance(p); int line = p->previous.line;
        ASTNode* right = parseComparison(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op = op; n->data.binary.left = left; n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseComparison(Parser* p) {
    /*
     * Simple left-to-right comparison — no chaining.
     *
     * Chained comparisons (1 < x < 10) were removed in v1.2.0 because
     * the implementation caused a heap double-free crash: the middle operand
     * AST node was shared between two binary nodes, and freeNode freed it
     * twice. The safe replacement is the explicit form:
     *
     *   1 < x and x < 10   ← use this instead
     *
     * A future version may reintroduce chaining with proper AST node cloning.
     */
    ASTNode* left = parseAddition(p);
    while (check(p, TK_LT) || check(p, TK_GT) || check(p, TK_LTEQ) || check(p, TK_GTEQ)) {
        TokenType op = p->current.type; advance(p);
        int line = p->previous.line;
        ASTNode* right = parseAddition(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op    = op;
        n->data.binary.left  = left;
        n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseAddition(Parser* p) {
    ASTNode* left = parseMultiply(p);
    while (check(p, TK_PLUS) || check(p, TK_MINUS) || check(p, TK_DOTDOT)) {
        TokenType op = p->current.type; advance(p); int line = p->previous.line;
        ASTNode* right = parseMultiply(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op = op; n->data.binary.left = left; n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseMultiply(Parser* p) {
    ASTNode* left = parseUnary(p);
    while (check(p, TK_STAR) || check(p, TK_SLASH) || check(p, TK_PERCENT)) {
        TokenType op = p->current.type; advance(p); int line = p->previous.line;
        ASTNode* right = parseUnary(p);
        ASTNode* n = makeNode(NODE_BINARY, line);
        n->data.binary.op = op; n->data.binary.left = left; n->data.binary.right = right;
        left = n;
    }
    return left;
}

static ASTNode* parseUnary(Parser* p) {
    if (check(p, TK_MINUS) || check(p, TK_NOT) || check(p, TK_BANG)) {
        TokenType op = p->current.type; advance(p); int line = p->previous.line;
        ASTNode* operand = parseUnary(p);
        ASTNode* n = makeNode(NODE_UNARY, line);
        // ! is syntactic sugar for not
        n->data.unary.op = (op == TK_BANG) ? TK_NOT : op;
        n->data.unary.operand = operand;
        return n;
    }
    return parseCall(p);
}

static ASTNode* parseCall(Parser* p) {
    ASTNode* expr = parsePrimary(p);
    // handle both calls and index access in sequence: f()[0], arr[i](args)
    for (;;) {
        if (check(p, TK_LPAREN)) {
            int line = p->current.line; advance(p);
            ASTNode* call = makeNode(NODE_CALL, line);
            call->data.call.callee = expr;
            initNodeList(&call->data.call.args);
            while (!check(p, TK_RPAREN) && !check(p, TK_EOF)) {
                appendNode(&call->data.call.args, parseExpr(p));
                if (!match(p, TK_COMMA)) break;
            }
            expect(p, TK_RPAREN, "Expected ')' to close argument list");
            expr = call;
        } else if (check(p, TK_LBRACKET)) {
            int line = p->current.line; advance(p);

            /* Detect slice: arr[start:end]  arr[:end]  arr[start:] */
            ASTNode* start_expr = NULL;
            if (!check(p, TK_COLON))
                start_expr = parseExpr(p);

            if (check(p, TK_COLON)) {
                /* It's a slice */
                advance(p); /* consume ':' */
                ASTNode* end_expr = NULL;
                if (!check(p, TK_RBRACKET))
                    end_expr = parseExpr(p);
                expect(p, TK_RBRACKET, "Expected ']' to close slice");
                ASTNode* sl = makeNode(NODE_SLICE, line);
                sl->data.slice.obj   = expr;
                sl->data.slice.start = start_expr;
                sl->data.slice.end   = end_expr;
                expr = sl;
            } else {
                /* Normal index access */
                expect(p, TK_RBRACKET, "Expected ']' to close index access");
                ASTNode* get = makeNode(NODE_GET_INDEX, line);
                get->data.get_index.obj   = expr;
                get->data.get_index.index = start_expr;
                expr = get;
            }
        } else {
            break;
        }
    }
    return expr;
}

static ASTNode* parsePrimary(Parser* p) {
    if (check(p, TK_NUMBER)) {
        advance(p);
        ASTNode* n = makeNode(NODE_NUMBER, p->previous.line);
        n->data.number.value = strtod(p->previous.start, NULL);
        return n;
    }
    if (check(p, TK_STRING)) {
        advance(p);
        int slen = 0;
        char* sval = extractString(p->previous.start, p->previous.length, &slen);
        ASTNode* n = makeNode(NODE_STRING, p->previous.line);
        n->data.string.value  = sval;
        n->data.string.length = slen;
        return n;
    }
    if (check(p, TK_TRUE))  { advance(p); ASTNode* n = makeNode(NODE_BOOL, p->previous.line); n->data.boolean.value = true;  return n; }
    if (check(p, TK_FALSE)) { advance(p); ASTNode* n = makeNode(NODE_BOOL, p->previous.line); n->data.boolean.value = false; return n; }
    if (check(p, TK_NIL))   { advance(p); return makeNode(NODE_NIL, p->previous.line); }
    if (check(p, TK_NULL))  { advance(p); return makeNode(NODE_NIL, p->previous.line); } /* null = alias for nil */
    // Array literal: [expr, expr, ...]
    if (check(p, TK_LBRACKET)) {
        int line = p->current.line; advance(p);
        ASTNode* arr = makeNode(NODE_ARRAY, line);
        initNodeList(&arr->data.array.items);
        while (!check(p, TK_RBRACKET) && !check(p, TK_EOF)) {
            appendNode(&arr->data.array.items, parseExpr(p));
            if (!match(p, TK_COMMA)) break;
        }
        expect(p, TK_RBRACKET, "Expected ']' to close array literal");
        return arr;
    }
    if (check(p, TK_IDENT)) {
        advance(p);
        ASTNode* n = makeNode(NODE_IDENT, p->previous.line);
        n->data.ident.name = dupStr(p->previous.start, p->previous.length);
        return n;
    }
    if (check(p, TK_LPAREN)) {
        advance(p);
        ASTNode* inner = parseExpr(p);
        expect(p, TK_RPAREN, "Expected ')' to close grouped expression");
        return inner;
    }

    errorAtCurrent(p, "Invalid expression. Expected a number, string, array, variable, or expression.");
    return makeNode(NODE_NIL, p->current.line);
}

void initParser(Parser* p, const char* source, const char* path) {
    initLexer(&p->lexer, source);
    p->had_error   = false;
    p->panic_mode  = false;
    p->source_path = path;
    advance(p);
}

ASTNode* parse(Parser* p) {
    ASTNode* root = makeNode(NODE_PROGRAM, 1);
    initNodeList(&root->data.block.stmts);
    skipNewlines(p);
    while (!check(p, TK_EOF)) {
        ASTNode* stmt = parseStmt(p);
        if (stmt) appendNode(&root->data.block.stmts, stmt);
        skipNewlines(p);
        p->panic_mode = false;
    }
    return root;
}

