# Nolqu Language Grammar — v1.2.0 (Stable)

> [!NOTE]
> **Stable documentation — Nolqu v1.0.0**
> For the latest grammar see [`docs/dev/grammar.md`](../dev/grammar.md).

---

## Program Structure

```ebnf
program     → statement* EOF

statement   → let_stmt | assign_stmt | index_assign_stmt
            | print_stmt | if_stmt | loop_stmt
            | function_decl | return_stmt
            | import_stmt | try_stmt | expr_stmt | NEWLINE
```

## Statements

```ebnf
let_stmt         → "let" IDENT "=" expr NEWLINE
assign_stmt      → IDENT "=" expr NEWLINE
index_assign_stmt → expr "[" expr "]" "=" expr NEWLINE
print_stmt       → "print" expr NEWLINE
if_stmt          → "if" expr NEWLINE statement*
                   ( "else" NEWLINE statement* )? "end" NEWLINE
loop_stmt        → "loop" expr NEWLINE statement* "end" NEWLINE
function_decl    → "function" IDENT "(" param_list? ")" NEWLINE
                   statement* "end" NEWLINE
param_list       → IDENT ( "," IDENT )*
return_stmt      → "return" expr? NEWLINE
import_stmt      → "import" STRING NEWLINE
try_stmt         → "try" NEWLINE statement*
                   "catch" IDENT NEWLINE statement* "end" NEWLINE
expr_stmt        → expr NEWLINE
```

## Expressions (precedence, lowest → highest)

```ebnf
expr    → or_expr
or_expr → and_expr ( "or" and_expr )*
and_expr → not_expr ( "and" not_expr )*
not_expr → "not" not_expr | equality
equality → comparison ( ( "==" | "!=" ) comparison )*
comparison → concat ( ( "<" | ">" | "<=" | ">=" ) concat )*
concat  → add ( ".." add )*
add     → mult ( ( "+" | "-" ) mult )*
mult    → unary ( ( "*" | "/" | "%" ) unary )*
unary   → "-" unary | "!" unary | call
call    → primary ( "(" arg_list? ")" | "[" expr "]" )*
primary → NUMBER | STRING | "true" | "false" | "nil"
        | IDENT | "(" expr ")"
        | "[" ( expr ( "," expr )* )? "]"
```

## Lexical

```
IDENT  → [a-zA-Z_][a-zA-Z0-9_]*
NUMBER → DIGIT+ ( "." DIGIT+ )?
STRING → '"' char* '"'   (escape: \n \t \r \\ \")
comment → "#" [^\n]*
```

## Keywords

```
let  print  if  else  loop  function  return
import  try  catch  end
true  false  nil  and  or  not
```

> **Available in v1.2.0:** `break` `continue` `for` `in`

## Operators

```
+   -   *   /   %          arithmetic
==  !=  <   >   <=  >=     comparison
=                           assignment
..                          string concatenation
!                           prefix logical not
[   ]                       array indexing
```

> **Available in v1.2.0:** `+=` `-=` `*=` `/=` `..=`
