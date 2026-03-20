#include "common.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "repl.h"
#include "object.h"
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

// ─────────────────────────────────────────────
//  File helpers
// ─────────────────────────────────────────────

/* Levenshtein distance for command suggestion (small strings only) */
static int cmdLevenshtein(const char* a, const char* b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la > 20 || lb > 20) return 99;
    int dp[21][21];
    for (int i = 0; i <= la; i++) dp[i][0] = i;
    for (int j = 0; j <= lb; j++) dp[0][j] = j;
    for (int i = 1; i <= la; i++)
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            dp[i][j] = dp[i-1][j-1] + cost;
            if (dp[i-1][j] + 1 < dp[i][j]) dp[i][j] = dp[i-1][j] + 1;
            if (dp[i][j-1] + 1 < dp[i][j]) dp[i][j] = dp[i][j-1] + 1;
        }
    return dp[la][lb];
}

/* Suggest the closest valid command if the input looks like a mistyped command
 * (no .nq extension, file does not exist, distance <= 2). */
static const char* suggestCommand(const char* input) {
    static const char* cmds[] = {
        "run", "check", "test", "version", "help", "repl", "compile", NULL
    };
    /* Only suggest if input has no .nq extension and no path separators */
    if (strstr(input, ".nq") || strchr(input, '/') || strchr(input, '\\'))
        return NULL;
    /* Only suggest if file does not exist */
    FILE* probe = fopen(input, "r");
    if (probe) { fclose(probe); return NULL; }

    const char* best = NULL;
    int best_dist = 3; /* threshold: only suggest if dist <= 2 */
    for (int i = 0; cmds[i]; i++) {
        int d = cmdLevenshtein(input, cmds[i]);
        if (d < best_dist) { best_dist = d; best = cmds[i]; }
    }
    return best;
}

static char* readFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr,
            NQ_COLOR_RED "[IOError]" NQ_COLOR_RESET
            " Cannot open file: %s\n"
            "  Hint: Check if the file exists and you have read permission.\n",
            path);
        return NULL;
    }

    /* Try seekable fast path first */
    if (fseek(f, 0, SEEK_END) == 0) {
        long size = ftell(f);
        rewind(f);
        if (size >= 0) {
            char* buf = (char*)malloc((size_t)(size + 1));
            if (!buf) {
                fprintf(stderr, NQ_COLOR_RED "[IOError]" NQ_COLOR_RESET " Out of memory.\n");
                fclose(f); return NULL;
            }
            size_t n = fread(buf, 1, (size_t)size, f);
            buf[n] = '\0'; fclose(f);
            return buf;
        }
        rewind(f);
    }

    /* Fallback: read incrementally (stdin / pipes) */
    size_t cap = 4096, len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) { fclose(f); return NULL; }
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            char* nb = (char*)realloc(buf, cap);
            if (!nb) { free(buf); fclose(f); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';
    fclose(f);
    return buf;
}

// ─────────────────────────────────────────────
//  Run a .nq file
// ─────────────────────────────────────────────
static int runFile(VM* vm, const char* path) {
    char* source = readFile(path);
    if (!source) return 1;

    Parser parser;
    initParser(&parser, source, path);
    ASTNode* ast = parse(&parser);
    free(source);

    if (parser.had_error) { freeNode(ast); return 1; }

    CompileResult result = compile(ast, path);
    freeNode(ast);
    if (result.had_error || !result.function) return 1;

    InterpretResult ir = runVM(vm, result.function, path);
    return (ir == INTERPRET_OK) ? 0 : 1;
}

// ─────────────────────────────────────────────
//  nq check — parse+compile only, no execution
// ─────────────────────────────────────────────
static int checkFile(const char* path) {
    char* source = readFile(path);
    if (!source) return 1;

    Parser parser;
    initParser(&parser, source, path);
    ASTNode* ast = parse(&parser);
    free(source);

    if (parser.had_error) {
        freeNode(ast);
        fprintf(stderr,
            NQ_COLOR_RED "[SyntaxError]" NQ_COLOR_RESET " %s: parse error\n", path);
        return 1;
    }

    CompileResult result = compile(ast, path);
    freeNode(ast);

    if (result.had_error || !result.function) {
        fprintf(stderr,
            NQ_COLOR_RED "[SyntaxError]" NQ_COLOR_RESET " %s: compile error\n", path);
        return 1;
    }

    printf(NQ_COLOR_GREEN "[OK]" NQ_COLOR_RESET " %s\n", path);
    return 0;
}

// ─────────────────────────────────────────────
//  nq compile — transpile .nq → C
//
//  Usage:
//    nq compile foo.nq            → foo.c
//    nq compile foo.nq -o bar.c   → bar.c
// ─────────────────────────────────────────────
static int compileToC(int argc, char* argv[]) {
    // Parse args: nq compile <src.nq> [-o <out.c>]
    const char* src_path = NULL;
    const char* out_path = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (!src_path) {
            src_path = argv[i];
        }
    }

    if (!src_path) {
        fprintf(stderr,
            NQ_COLOR_RED "[ compile ]" NQ_COLOR_RESET
            " Usage: nq compile <source.nq> [-o output.c]\n");
        return 1;
    }

    // Auto-generate output path if not given
    char auto_out[1024];
    if (!out_path) {
        size_t len = strlen(src_path);
        if (len > 3 && strcmp(src_path + len - 3, ".nq") == 0) {
            snprintf(auto_out, sizeof(auto_out), "%.*s.c", (int)(len - 3), src_path);
        } else {
            snprintf(auto_out, sizeof(auto_out), "%s.c", src_path);
        }
        out_path = auto_out;
    }

    char* source = readFile(src_path);
    if (!source) return 1;

    Parser parser;
    initParser(&parser, source, src_path);
    ASTNode* ast = parse(&parser);
    free(source);

    if (parser.had_error) {
        freeNode(ast);
        fprintf(stderr,
            NQ_COLOR_RED "[ compile ]" NQ_COLOR_RESET " Parse error in %s\n", src_path);
        return 1;
    }

    FILE* out = fopen(out_path, "w");
    if (!out) {
        freeNode(ast);
        fprintf(stderr,
            NQ_COLOR_RED "[ compile ]" NQ_COLOR_RESET
            " Cannot write to: %s\n", out_path);
        return 1;
    }

    CodeGen cg;
    codegen_init(&cg, out);
    codegen_emit(&cg, ast);
    fclose(out);
    freeNode(ast);

    if (cg.had_error) {
        fprintf(stderr,
            NQ_COLOR_RED "[ compile ]" NQ_COLOR_RESET " Codegen error: %s\n",
            cg.error_msg);
        return 1;
    }

    printf(NQ_COLOR_GREEN "[ compile ]" NQ_COLOR_RESET
        " %s -> %s\n"
        "  " NQ_COLOR_BOLD "Build with:" NQ_COLOR_RESET
        "  gcc -O2 %s -o program -lm\n",
        src_path, out_path, out_path);
    return 0;
}

// ─────────────────────────────────────────────
//  nq test — run *_test.nq / test_*.nq files
// ─────────────────────────────────────────────
static bool isTestFile(const char* name) {
    size_t len = strlen(name);
    if (len < 8) return false;
    return (strcmp(name + len - 8, "_test.nq") == 0)
        || (strncmp(name, "test_", 5) == 0 && len > 7)
        || (strstr(name, ".test.nq") != NULL);
}

static int runTestFile(VM* vm, const char* path) {
    vm->stack_top   = vm->stack;
    vm->frame_count = 0;
    vm->try_depth   = 0;
    vm->thrown      = NIL_VAL;

    char* source = readFile(path);
    if (!source) return 1;

    Parser parser;
    initParser(&parser, source, path);
    ASTNode* ast = parse(&parser);
    free(source);

    if (parser.had_error) { freeNode(ast); return 1; }

    CompileResult result = compile(ast, path);
    freeNode(ast);
    if (result.had_error || !result.function) return 1;

    InterpretResult ir = runVM(vm, result.function, path);
    return (ir == INTERPRET_OK) ? 0 : 1;
}

static int runTests(const char* path) {
    VM vm; initVM(&vm);
    int passed = 0, failed = 0;

    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        // Single file
        printf(NQ_COLOR_BOLD "Running:" NQ_COLOR_RESET " %s\n\n", path);
        int r = runTestFile(&vm, path);
        if (r == 0) { printf(NQ_COLOR_GREEN "  PASS" NQ_COLOR_RESET " %s\n", path); passed++; }
        else         { printf(NQ_COLOR_RED   "  FAIL" NQ_COLOR_RESET " %s\n", path); failed++; }
    } else {
        // Directory — scan for test files
        DIR* dir = opendir(path);
        if (!dir) {
            fprintf(stderr,
                NQ_COLOR_RED "[ test ]" NQ_COLOR_RESET " Cannot open: %s\n", path);
            freeVM(&vm); return 1;
        }

        // Collect names and sort for deterministic order
        char names[256][256]; int nfiles = 0;
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL && nfiles < 256) {
            if (isTestFile(entry->d_name)) {
                size_t dlen = strlen(entry->d_name);
                if (dlen > 255) dlen = 255;
                memcpy(names[nfiles], entry->d_name, dlen);
                names[nfiles++][dlen] = '\0';
            }
        }
        closedir(dir);

        // Simple insertion sort (memcpy — no format-truncation risk)
        for (int i = 1; i < nfiles; i++) {
            char tmp[256];
            memcpy(tmp, names[i], 256);
            int j = i - 1;
            while (j >= 0 && strcmp(names[j], tmp) > 0) {
                memcpy(names[j+1], names[j], 256); j--;
            }
            memcpy(names[j+1], tmp, 256);
        }

        printf(NQ_COLOR_BOLD "Running %d test(s) in: %s\n\n" NQ_COLOR_RESET,
               nfiles, path);

        for (int i = 0; i < nfiles; i++) {
            char full[1280];
            snprintf(full, sizeof(full), "%s/%s", path, names[i]);
            int r = runTestFile(&vm, full);
            if (r == 0) { printf(NQ_COLOR_GREEN "  PASS" NQ_COLOR_RESET " %s\n", names[i]); passed++; }
            else         { printf(NQ_COLOR_RED   "  FAIL" NQ_COLOR_RESET " %s\n", names[i]); failed++; }
        }
    }

    printf("\n");
    if (failed == 0) {
        printf(NQ_COLOR_GREEN NQ_COLOR_BOLD
            "  All %d test(s) passed. " NQ_COLOR_RESET "\n\n", passed + failed);
    } else {
        printf(NQ_COLOR_RED NQ_COLOR_BOLD
            "  %d/%d test(s) failed.\n" NQ_COLOR_RESET "\n", failed, passed + failed);
    }

    freeVM(&vm);
    return failed > 0 ? 1 : 0;
}

// ─────────────────────────────────────────────
//  Help & Version
// ─────────────────────────────────────────────
static void printVersion(void) {
    printf(NQ_COLOR_BOLD NQ_COLOR_CYAN
        "Nolqu " NQ_VERSION NQ_COLOR_RESET "\n"
        "A simple, fast programming language with C/C++ interop\n"
        "Runtime: nq  |  File extension: .nq  |  Transpile: nq compile\n");
}

static void printHelp(void) {
    printVersion();
    printf("\n");
    printf(NQ_COLOR_BOLD "Commands:" NQ_COLOR_RESET "\n");
    printf("  nq <file.nq>                    Run a Nolqu program\n");
    printf("  nq run <file.nq>                Run a Nolqu program\n");
    printf("  nq check <file.nq>              Parse+compile only (no run)\n");
    printf("  nq compile <file.nq> [-o out.c] Transpile to C source\n");
    printf("  nq test <file|dir>              Run test files (*_test.nq)\n");
    printf("  nq repl                         Start interactive REPL\n");
    printf("  nq version                      Show version\n");
    printf("  nq help                         Show this help\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "C/C++ Interop:" NQ_COLOR_RESET "\n");
    printf("  Transpile:  nq compile hello.nq && gcc hello.c -lm -o hello\n");
    printf("  Embed API:  #include \"nolqu.h\"  —  nq_open / nq_dostring / nq_close\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Built-in functions:" NQ_COLOR_RESET "\n");
    printf("  I/O:      input([prompt])  print\n");
    printf("  Type:     str(v)  num(v)  bool(v)  type(v)  error_type(e)\n");
    printf("            is_nil  is_num  is_str  is_bool  is_array\n");
    printf("            ord(ch)  chr(n)\n");
    printf("  Math:     sqrt  floor  ceil  round  abs  pow  min  max\n");
    printf("  String:   upper  lower  slice  trim  replace  split  join\n");
    printf("            startswith  endswith  index  repeat  len\n");
    printf("  Array:    len  push  pop  remove  contains  sort\n");
    printf("  Random:   random()  rand_int(lo, hi)\n");
    printf("  File:     file_read  file_write  file_append  file_exists  file_lines\n");
    printf("  Error:    error(msg)  assert(cond [, msg])\n");
    printf("  Time:     clock()\n");
    printf("  Memory:   mem_usage()  gc_collect()\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Stdlib (import \"stdlib/name\"):" NQ_COLOR_RESET "\n");
    printf("  math      clamp  lerp  sign  PI  TAU  E  sin  cos  tan  log  gcd  lcm\n");
    printf("  array     map  filter  reduce  reverse  sum  flatten  zip  chunk  unique\n");
    printf("  string    is_empty  lines  words  replace_all  pad_left  center  title_case\n");
    printf("  path      path_join  basename  dirname  ext  strip_ext  normalize\n");
    printf("  json      json_object  json_set  json_get  json_stringify  json_parse\n");
    printf("  time      now  millis  elapsed  sleep  format_duration  benchmark\n");
    printf("  io        read_file  write_file  read_lines  write_lines  copy_file\n");
    printf("  os        env_or  path_exists  read_lines  write_lines  file_size  touch\n");
    printf("  fmt       fmt  printf  fmt_num  fmt_pad  fmt_table\n");
    printf("  test      suite  expect  expect_eq  expect_err  done\n");
    printf("  file      read_or_default  write_lines  count_lines\n");
    printf("\n");
    printf(NQ_COLOR_BOLD "Syntax quick-ref:" NQ_COLOR_RESET "\n");
    printf("  let x = 42            # declare variable\n");
    printf("  const MAX = 100           # immutable binding\n");
    printf("  x += 10               # compound assign: += -= *= /= ..=\n");
    printf("  if x > 10  ...  else  ...  end\n");
    printf("  loop i < 10  ...  end              # while-style\n");
    printf("  for item in array  ...  end        # range-based\n");
    printf("  break  continue                    # loop control\n");
    printf("  function greet(name = \"world\")  return \"Hi, \" .. name  end\n");
    printf("  try  error(\"oops\")  catch err  print error_type(err)  end\n");
    printf("  arr[1:3]  s[:5]  s[-2:]            # slice\n");
  printf("  1 < x < 10                         # comparison chaining\n");
  printf("  from stdlib/math import PI, sin    # selective import\n");
    printf("\n");
}

// ─────────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc == 1) {
        VM vm; initVM(&vm);
        runREPL(&vm);
        freeVM(&vm);
        return 0;
    }

    const char* cmd = argv[1];

    // No-arg commands
    if (strcmp(cmd, "repl") == 0) {
        VM vm; initVM(&vm); runREPL(&vm); freeVM(&vm); return 0;
    }
    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0 ||
        strcmp(cmd, "-v")      == 0) { printVersion(); return 0; }
    if (strcmp(cmd, "help")    == 0 || strcmp(cmd, "--help")    == 0 ||
        strcmp(cmd, "-h")      == 0) { printHelp();    return 0; }

    // Two-argument commands — check that a filename was provided
    if (strcmp(cmd, "run") == 0 || strcmp(cmd, "check") == 0 ||
        strcmp(cmd, "test") == 0 || strcmp(cmd, "compile") == 0) {
        if (argc < 3) {
            fprintf(stderr,
                NQ_COLOR_RED "[UsageError]" NQ_COLOR_RESET
                " Missing filename.\n"
                "  Usage:  nq %s <file.nq>\n"
                "  Run  " NQ_COLOR_BOLD "nq help" NQ_COLOR_RESET
                "  for all commands.\n\n", cmd);
            return 1;
        }
        if (strcmp(cmd, "run") == 0) {
            VM vm; initVM(&vm);
            int r = runFile(&vm, argv[2]);
            freeVM(&vm); return r;
        }
        if (strcmp(cmd, "check") == 0)   return checkFile(argv[2]);
        if (strcmp(cmd, "test")  == 0)   return runTests(argv[2]);
        if (strcmp(cmd, "compile") == 0) return compileToC(argc - 2, argv + 2);
    }

    // Bare filename or unknown command
    if (argc == 2) {
        /* Check if it looks like a mistyped command before trying to run as file */
        const char* suggestion = suggestCommand(cmd);
        if (suggestion) {
            fprintf(stderr,
                NQ_COLOR_RED "[UsageError]" NQ_COLOR_RESET
                " Unknown command: %s\n"
                "  Did you mean: " NQ_COLOR_BOLD "%s" NQ_COLOR_RESET " ?\n\n",
                cmd, suggestion);
            return 1;
        }
        VM vm; initVM(&vm);
        int r = runFile(&vm, cmd);
        freeVM(&vm); return r;
    }

    fprintf(stderr, NQ_COLOR_RED "[UsageError]" NQ_COLOR_RESET " Invalid usage.\n\n");
    printHelp();
    return 1;
}
