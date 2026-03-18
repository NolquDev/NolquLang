# Nolqu Language Grammar — v1.0.0

> [!NOTE]
> **Development documentation — Nolqu v1.1.x (alpha)**
> This document describes features in the current alpha series.
> Some features listed here do not exist in the v1.0.0 stable release.
> For stable documentation see [`docs/stable/`](../stable/).


Formal EBNF grammar specification for the Nolqu programming language.

For a readable, example-based language reference see [language.md](language.md).
For VM internals and bytecode see [vm_design.md](vm_design.md).

---

## Program Structure

```ebnf
program     → statement* EOF

statement   → let_stmt
            | assign_stmt
            | index_assign_stmt
            | print_stmt
            | if_stmt
            | loop_stmt
            | function_decl
            | return_stmt
            | import_stmt
            | try_stmt
            | break_stmt
            | continue_stmt
            | for_stmt
            | compound_assign_stmt
            | expr_stmt
            | NEWLINE
```

---

## Statements

```ebnf
let_stmt         → "let" IDENT "=" expr NEWLINE

assign_stmt      → IDENT "=" expr NEWLINE

index_assign_stmt → expr "[" expr "]" "=" expr NEWLINE

print_stmt       → "print" expr NEWLINE

if_stmt          → "if" expr NEWLINE
                     statement*
                   ( "else" NEWLINE statement* )?
                   "end" NEWLINE

loop_stmt        → "loop" expr NEWLINE
                     statement*
                   "end" NEWLINE

function_decl    → "function" IDENT "(" param_list? ")" NEWLINE
                     statement*
                   "end" NEWLINE

param_list       → IDENT ( "," IDENT )*

return_stmt      → "return" expr? NEWLINE

import_stmt      → "import" STRING NEWLINE

try_stmt         → "try" NEWLINE
                     statement*
                   "catch" IDENT NEWLINE
                     statement*
                   "end" NEWLINE

break_stmt       → "break" NEWLINE

continue_stmt    → "continue" NEWLINE

for_stmt         → "for" IDENT "in" expr NEWLINE
                     statement*
                   "end" NEWLINE

compound_assign_stmt → IDENT ( "+=" | "-=" | "*=" | "/=" | "..=" ) expr NEWLINE

expr_stmt        → expr NEWLINE
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

concat       → add ( ".." add )*             (* string concatenation *)

add          → mult ( ( "+" | "-" ) mult )*

mult         → unary ( ( "*" | "/" | "%" ) unary )*

unary        → "-" unary
             | "!" unary                     (* prefix bang = logical not *)
             | call

call         → primary ( "(" arg_list? ")" | "[" expr "]" )*

primary      → NUMBER
             | STRING
             | "true" | "false" | "nil"
             | IDENT
             | "(" expr ")"
             | "[" ( expr ( "," expr )* )? "]"   (* array literal *)

arg_list     → expr ( "," expr )*
```

---

## Lexical Structure

### Identifiers
```
IDENT       → [a-zA-Z_][a-zA-Z0-9_]*
```
Identifiers beginning with `_` suppress unused-variable warnings.

### Numbers
```
NUMBER      → DIGIT+ ( "." DIGIT+ )?
DIGIT       → [0-9]
```
All numbers are 64-bit IEEE 754 double-precision floats.

### Strings
```
STRING      → '"' char* '"'
char        → any character except '"' and unescaped newline
            | escape_sequence

escape_sequence → "\n"   newline
                | "\t"   tab
                | "\r"   carriage return
                | "\\"   backslash
                | "\""   double quote
```

### Comments
```
comment     → "#" [^\n]* NEWLINE
```
Comments run from `#` to the end of the line. Block comments are not supported.

### Keywords
```
let  print  if  else  loop  for  in  function  return
import  try  catch  end
true  false  nil
and  or  not
break  continue
```

### Operators
```
+   -   *   /   %           arithmetic
==  !=  <   >   <=  >=      comparison
=                           assignment
+=   -=   *=   /=          compound assignment (arithmetic)
..=                         compound assignment (concat)
..                          string concatenation
!                           prefix logical not (same as 'not')
[   ]                       array indexing
```

### Whitespace & Newlines
- Spaces and tabs between tokens are ignored.
- `NEWLINE` (`\n`) is the statement terminator.
- Blank lines are allowed anywhere.

---

## Scoping Rules

- Variables declared with `let` at the top level are **global**.
- Variables declared with `let` inside a function body are **local**.
- Variables declared inside `if`, `loop`, or `try` blocks are **local to that block**.
- Local variables shadow outer variables of the same name within their scope.
- Catch variables (`catch err`) are local to the catch block.

---

## Type System

Nolqu is **dynamically typed** with four primitive types:

| Type       | Literal examples              | Notes                        |
|------------|-------------------------------|------------------------------|
| `nil`      | `nil`                         | Represents absence of value  |
| `bool`     | `true` `false`                |                              |
| `number`   | `0` `3.14` `-5` `1e6`        | Always 64-bit float          |
| `string`   | `"hello"` `"line\n"`         | Immutable, interned          |

And two compound types:

| Type       | Example                       | Notes                        |
|------------|-------------------------------|------------------------------|
| `array`    | `[1, "two", true]`            | Dynamic, heterogeneous       |
| `function` | `function f(x) ... end`       | First-class value            |

### Truthiness
- `nil` and `false` are falsy.
- Everything else (including `0` and `""`) is truthy.

### Equality
- Two values are equal if they have the same type and the same content.
- Strings are compared by value (interning guarantees pointer equality).
- Arrays are compared by identity (two arrays with the same elements are not equal unless they are the same object).

---

## Error Handling

```nolqu
try
  # code that might throw
  error("something went wrong")
catch variableName
  # variableName holds the error message (string)
  print "Caught: " .. variableName
end
```

- `try`/`catch`/`end` blocks can be nested up to 64 levels.
- The following operations throw catchable errors:
  - `error(msg)` — user-defined error
  - `assert(cond, msg?)` — assertion failure
  - Division and modulo by zero
  - Arithmetic on non-numbers
  - Array/string index out of bounds
  - Wrong index type (non-number)
  - File I/O failures (`file_read`, `file_write`, `file_append`, `file_lines`)
  - Calling a non-function value
  - `pop` on empty array
  - `len` on wrong type

---

## Module System

```nolqu
import "path/to/module"
```

- The path is relative to the current working directory (not the script file).
- File extension `.nq` is added automatically.
- Importing a module executes it in the global scope — all top-level declarations become global.
- Re-importing a module that has already been loaded is a **no-op** — each module is compiled and run at most once per program execution (deduplication added in v1.1.1a2).

**Built-in modules (in `stdlib/`):**
```nolqu
import "stdlib/math"    # clamp  lerp  sign
import "stdlib/array"   # map  filter  reduce  reverse
import "stdlib/file"    # read_or_default  write_lines  count_lines
```

---

## Notes and Limitations (v1.0.0)

- No closures — functions cannot capture variables from outer scopes.
- No first-class arrays in stdlib `map`/`filter`/`reduce` — callback must be a named function.
- No string mutation — all string operations return new strings.
- Numbers are always floats — there is no distinct integer type.
- `import` has no cycle detection; circular imports will infinite-loop.
- The transpiler (`nq compile`) is experimental and does not support `import`, `try/catch`, or arrays.
