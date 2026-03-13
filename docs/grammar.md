# Nolqu Language Grammar
## Formal Grammar Specification (EBNF)

---

## Program Structure

```
program     → statement* EOF

statement   → let_stmt
            | assign_stmt
            | print_stmt
            | if_stmt
            | loop_stmt
            | function_decl
            | return_stmt
            | import_stmt
            | expr_stmt
```

---

## Statements

```
let_stmt     → "let" IDENT "=" expr NEWLINE

assign_stmt  → IDENT "=" expr NEWLINE

print_stmt   → "print" expr NEWLINE

if_stmt      → "if" expr NEWLINE
                   block
               ( "else" NEWLINE block )?
               "end" NEWLINE

loop_stmt    → "loop" expr NEWLINE
                   block
               "end" NEWLINE

function_decl → "function" IDENT "(" params? ")" NEWLINE
                    block
                "end" NEWLINE

return_stmt  → "return" expr? NEWLINE

import_stmt  → "import" STRING NEWLINE

expr_stmt    → expr NEWLINE

block        → statement*

params       → IDENT ( "," IDENT )*
args         → expr ( "," expr )*
```

---

## Expressions (by precedence, lowest to highest)

```
expr         → logic_or

logic_or     → logic_and ( "or"  logic_and )*

logic_and    → equality  ( "and" equality  )*

equality     → comparison ( ( "==" | "!=" ) comparison )*

comparison   → addition ( ( "<" | ">" | "<=" | ">=" ) addition )*

addition     → multiply ( ( "+" | "-" | ".." ) multiply )*

multiply     → unary ( ( "*" | "/" | "%" ) unary )*

unary        → ( "-" | "not" ) unary
             | call

call         → primary ( "(" args? ")" )*

primary      → NUMBER
             | STRING
             | "true"
             | "false"
             | "nil"
             | IDENT
             | "(" expr ")"
```

---

## Lexical Elements

```
NUMBER      → DIGIT+ ( "." DIGIT+ )?
STRING      → '"' ( [^"\\] | ESCAPE )* '"'
ESCAPE      → '\' ( 'n' | 't' | 'r' | '"' | '\' )
IDENT       → ALPHA ( ALPHA | DIGIT | '_' )*
COMMENT     → '#' [^\n]*
NEWLINE     → '\n'
WHITESPACE  → ' ' | '\t' | '\r'   (ignored, not significant)

ALPHA       → [a-zA-Z_]
DIGIT       → [0-9]
```

---

## Keywords

| Keyword    | Kegunaan                     |
|------------|------------------------------|
| `let`      | Deklarasi variabel           |
| `print`    | Mencetak ke output           |
| `if`       | Kondisi percabangan          |
| `else`     | Alternatif kondisi           |
| `loop`     | Perulangan (while-style)     |
| `function` | Deklarasi fungsi             |
| `return`   | Mengembalikan nilai dari fungsi |
| `import`   | Mengimpor file lain          |
| `end`      | Menutup blok (if/loop/function) |
| `true`     | Nilai boolean benar          |
| `false`    | Nilai boolean salah          |
| `nil`      | Nilai kosong                 |
| `and`      | Logika AND                   |
| `or`       | Logika OR                    |
| `not`      | Negasi logika                |

---

## Operators

| Operator | Jenis       | Contoh           |
|----------|-------------|------------------|
| `+`      | Penjumlahan | `5 + 3`          |
| `-`      | Pengurangan | `10 - 4`         |
| `*`      | Perkalian   | `4 * 7`          |
| `/`      | Pembagian   | `10 / 2`         |
| `%`      | Modulo      | `10 % 3`         |
| `..`     | Concat teks | `"a" .. "b"`     |
| `==`     | Sama dengan | `x == 5`         |
| `!=`     | Tidak sama  | `x != 0`         |
| `<`      | Lebih kecil | `x < 10`         |
| `>`      | Lebih besar | `x > 0`          |
| `<=`     | Kurang sama | `x <= 100`       |
| `>=`     | Lebih sama  | `x >= 1`         |
| `and`    | Logika AND  | `x > 0 and y > 0`|
| `or`     | Logika OR   | `x == 1 or y == 1`|
| `not`    | Negasi      | `not false`      |
| `-`      | Negasi angka| `-x`             |

---

## Tipe Data

| Tipe       | Contoh                    | Keterangan                     |
|------------|---------------------------|--------------------------------|
| `angka`    | `42`, `3.14`, `-7`        | Bilangan 64-bit floating point |
| `teks`     | `"Halo"`, `"Nolqu"`       | String Unicode, immutable      |
| `boolean`  | `true`, `false`           | Nilai logika                   |
| `nil`      | `nil`                     | Tidak ada nilai                |
| `fungsi`   | `function add(a, b) ...`  | Fungsi                         |

---

## Aturan Scoping

- Variabel yang dideklarasikan di level atas bersifat **global**
- Variabel yang dideklarasikan di dalam fungsi bersifat **lokal**
- Parameter fungsi bersifat lokal
- Blok `if` dan `loop` **tidak** membuat scope baru (menggunakan scope fungsi/global)

---

## Truthiness

| Nilai   | Truthy? |
|---------|---------|
| `nil`   | ❌ Falsy |
| `false` | ❌ Falsy |
| `0`     | ✅ Truthy |
| `""`    | ✅ Truthy |
| semua lainnya | ✅ Truthy |

---

## Catatan Khusus

- **Satu statement per baris** — tidak ada semicolon
- **Komentar** dimulai dengan `#`
- **Konkatenasi teks** menggunakan `..` (bukan `+`)
- **Pengecekan kesamaan** menggunakan `==` (bukan `=`)
- **Penugasan** menggunakan `=` (untuk `let` atau reassign)
- Indentasi **tidak wajib** tapi dianjurkan untuk keterbacaan
