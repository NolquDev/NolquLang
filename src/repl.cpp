#include "repl.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define REPL_LINE_MAX  4096
#define REPL_BUF_MAX  (REPL_LINE_MAX * 16)

static bool opensBlock(const char* s) {
    return strncmp(s, "if ",       3) == 0 ||
           strncmp(s, "if\n",      3) == 0 ||
           strncmp(s, "loop ",     5) == 0 ||
           strncmp(s, "loop\n",    5) == 0 ||
           strncmp(s, "function ", 9) == 0 ||
           strncmp(s, "try",       3) == 0;
}

static bool closesBlock(const char* s) {
    return strcmp(s, "end") == 0;
}

static bool isCatch(const char* s) {
    return strncmp(s, "catch ", 6) == 0 || strcmp(s, "catch") == 0;
}

static void printHelp(void) {
    printf(NQ_COLOR_CYAN
        "  Commands:\n"
        "    exit / quit   -- leave the REPL\n"
        "    clear         -- clear the screen\n"
        "    help          -- show this help\n"
        "\n"
        "  Language quick reference:\n"
        "    let x = 10              declare variable\n"
        "    print x                 print value\n"
        "    function f(a) ... end   define function\n"
        "    if cond ... end         conditional\n"
        "    loop cond ... end       loop\n"
        "    try ... catch e ... end error handling\n"
        "    import \"stdlib/math\"    import module\n"
        "\n"
        "  Builtins: len push pop sort join split upper lower\n"
        "            floor ceil round abs sqrt pow min max\n"
        "            str num type input random rand_int\n"
        "            file_read file_write file_exists file_lines\n"
        "            assert clock mem_usage gc_collect error\n"
        NQ_COLOR_RESET "\n");
}

void runREPL(VM* vm) {
    printf(NQ_COLOR_CYAN NQ_COLOR_BOLD
        "\n  +--------------------------------------+\n"
        "  |  Nolqu %-8s  Interactive REPL  |\n"
        "  |  'help' for help  'exit' to quit    |\n"
        "  +--------------------------------------+\n"
        NQ_COLOR_RESET "\n",
        NQ_VERSION);

    char  line[REPL_LINE_MAX];
    char* carry = (char*)calloc(REPL_BUF_MAX, 1);
    if (!carry) { fprintf(stderr, "REPL: out of memory\n"); return; }
    int depth = 0;

    for (;;) {
        if (depth == 0) {
            printf(NQ_COLOR_GREEN "nq" NQ_COLOR_RESET NQ_COLOR_BOLD " > " NQ_COLOR_RESET);
        } else {
            for (int i = 0; i < depth; i++) printf("  ");
            printf(NQ_COLOR_YELLOW "... " NQ_COLOR_RESET);
        }
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) { printf("\n"); break; }

        int len = (int)strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';

        const char* t = line;
        while (*t == ' ' || *t == '\t') t++;

        if (strcmp(t, "exit") == 0 || strcmp(t, "quit") == 0) {
            printf(NQ_COLOR_CYAN "Goodbye!\n" NQ_COLOR_RESET);
            break;
        }
        if (strcmp(t, "help") == 0) { printHelp(); continue; }
        if (strcmp(t, "clear") == 0) { printf("\033[2J\033[H"); fflush(stdout); continue; }
        if (strcmp(t, "") == 0 && depth == 0) continue;

        // Track depth
        if (isCatch(t)) {
            /* net 0 — closes try body, opens catch body */
        } else if (opensBlock(t)) {
            depth++;
        } else if (closesBlock(t)) {
            if (depth > 0) depth--;
        }

        size_t cl = strlen(carry);
        if (cl + (size_t)len + 2 < (size_t)REPL_BUF_MAX) {
            memcpy(carry + cl, line, (size_t)len);
            carry[cl + len]     = '\n';
            carry[cl + len + 1] = '\0';
        }

        if (depth > 0) continue;

        Parser p;
        initParser(&p, carry, "<repl>");
        ASTNode* ast = parse(&p);

        if (p.had_error) { freeNode(ast); carry[0] = '\0'; depth = 0; continue; }

        CompileResult result = compile(ast, "<repl>");
        freeNode(ast);

        if (!result.had_error && result.function) {
            InterpretResult ir = runVM(vm, result.function, "<repl>");
            if (ir != INTERPRET_OK) {
                vm->stack_top   = vm->stack;
                vm->frame_count = 0;
                vm->try_depth   = 0;
                vm->thrown      = NIL_VAL;
            }
        }

        carry[0] = '\0';
        depth    = 0;
    }

    free(carry);
}
