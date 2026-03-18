# Nolqu Language Grammar — v1.1.1a4 (Alpha)

> [!NOTE]
> **Development documentation — Nolqu v1.1.x (alpha)**
> This grammar describes all syntax available in the current alpha.
> For v1.0.0 stable grammar see [`docs/stable/grammar.md`](../stable/grammar.md).

For a readable language reference see [language.md](language.md).
For VM internals see [vm_design.md](vm_design.md).

---

## Program Structure

```ebnf
program     → statement* EOF

statement   → let_stmt
            | assign_stmt
            | compound_assign_stmt
            | index_assign_stmt
            | print_stmt
            | if_stmt
            | loop_stmt
            | for_stmt
            | break_stmt
            | continue_stmt
            | function_decl
            | return_stmt
            | import_stmt
            | try_stmt
            | expr_stmt
            | NEWLINE
```

---

## Statements

```ebnf
let_stmt             → "let" IDENT "=" expr NEWLINE

assign_stmt          → IDENT "=" expr NEWLINE

compound_assign_stmt → IDENT ( "+=" | "-=" | "*=" | "/=" | "..=" ) expr NEWLINE

index_assign_stmt    → expr "[" expr "]" "=" expr NEWLINE

print_stmt           → "print" expr NEWLINE

if_stmt              → "if" expr NEWLINE
                         statement*
                       ( "else" NEWLINE statement* )?
                       "end" NEWLINE

loop_stmt            → "loop" expr NEWLINE
                         statement*
                       "end" NEWLINE

for_stmt             → "for" IDENT "in" expr NEWLINE
                         statement*
                       "end" NEWLINE

break_stmt           → "break" NEWLINE

continue_stmt        → "continue" NEWLINE

function_decl        → "function" IDENT "(" param_list? ")" NEWLINE
                         statement*
                       "end" NEWLINE

param_list           → IDENT ( "," IDENT )*

return_stmt          → "return" expr? NEWLINE

import_stmt          → "import" STRING NEWLINE

try_stmt             → "try" NEWLINE
                         statement*
                       "catch" IDENT NEWLINE
                         statement*
                       "end" NEWLINE

expr_stmt            → expr NEWLINE
```

---

## Expressions

Precedence (lowest → highest):

```ebnf
expr         → or_expr

or_expr      → and_expr ( "or" and_expr )*

and_expr     → not_expr ( "and" not_expr )*

not_expr     → "not" not_expr
             | equality

equality     → comparison ( ( "==" | "!=" ) comparison )*

comparison   → concat ( ( "<" | ">" | "<=" | ">=" ) concat )*

concat       → add ( ".." add )*

add          → mult ( ( "+" | "-" ) mult )*

mult         → unary ( ( "*" | "/" | "%" ) unary )*

unary        → "-" unary
             | "!" unary
             | call

call         → primary ( "(" arg_list? ")" | "[" expr "]" )*

primary      → NUMBER
             | STRING
             | "true" | "false" | "nil"
             | IDENT
             | "(" expr ")"
             | "[" ( expr ( "," expr )* )? "]"

arg_list     → expr ( "," expr )*
```

---

## Lexical Structure

### Identifiers
```
IDENT → [a-zA-Z_][a-zA-Z0-9_]*
```
Identifiers beginning with `_` suppress unused-variable warnings.

### Numbers
```
NUMBER → DIGIT+ ( "." DIGIT+ )?
DIGIT  → [0-9]
```
All numbers are 64-bit IEEE 754 doubles. Scientific notation is not supported in source literals.

### Strings
```
STRING            → '"' char* '"'
char              → any character except '"' and unescaped newline
                  | escape_sequence
escape_sequence   → "\n" | "\t" | "\r" | "\\" | "\""
```

### Comments
```
comment → "#" [^\n]* NEWLINE
```
Single-line only. Block comments are not supported.

### Keywords

```
let      print    if       else     loop     for      in
function return   import   try      catch    end
true     false    nil      and      or       not      break    continue
```

### Operators

```
Arithmetic:       +    -    *    /    %
Compound assign:  +=   -=   *=   /=   ..=
Comparison:       ==   !=   <    >    <=   >=
Assignment:       =
Concat:           ..
Prefix not:       !
Indexing:         [  ]
Grouping:         (  )
```

---

## Scoping Rules

- Variables declared with `let` at the top level → **global**
- Variables declared with `let` inside a function → **local to that function**
- Variables declared inside `if`, `loop`, `for`, `try`, or `catch` → **local to that block**
- Local variables shadow outer variables of the same name within their scope
- Catch variables are local to the catch block
- `for` loop variables are local to the `for` block
- `break` and `continue` are compile-time errors outside a loop

---

## Type System

Nolqu is **dynamically typed** with six types:

| Type | Values | Notes |
|---|---|---|
| `nil` | `nil` | Absence of value |
| `bool` | `true` `false` | |
| `number` | all numeric literals | 64-bit float, no integer type |
| `string` | `"..."` | Immutable, interned, UTF-8 bytes |
| `array` | `[...]` | Dynamic, heterogeneous |
| `function` | `function ... end` | First-class |

### Truthiness

`nil` and `false` are falsy. All other values are truthy (including `0` and `""`).

### Equality

- Values of different types are never equal (except `0 != false`).
- Strings compare by value (interning guarantees pointer equality).
- Arrays compare by identity — two arrays with identical contents are not equal unless they are the same object.

---

## Error Handling

`try`/`catch`/`end` catches any thrown value. The catch variable receives the
error message as a string.

Catchable errors include: `error()`, `assert()`, arithmetic errors, type mismatches,
index out of bounds, undefined variable access, wrong argument counts, file I/O failures.

Nesting: up to 64 `try` handlers active simultaneously.

---

## Module System

```nolqu
import "path/to/module"
```

- Path is relative to the **current working directory** when `nq` is invoked.
- `.nq` extension is added automatically.
- Each module is compiled and run **at most once** per compilation — re-importing is a no-op.
- No cycle detection — circular imports will loop forever.
- Top-level declarations in a module become **global** in the importing program.

---

## Notes and Limitations (v1.1.1a4)

- No closures — functions cannot capture variables from outer scopes.
- No hash maps — only arrays and strings are collection types.
- No integer type — all numbers are 64-bit floats.
- Single-pass compiler — forward references to functions in the same file are not supported.
- `chr()` supports ASCII only (code points 0–127).
- The `codegen` transpiler is experimental and does not support arrays, try/catch, or import.
