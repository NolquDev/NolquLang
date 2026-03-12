# Nolqu Programming Language

```
 _   _       _
| \ | | ___ | | __ _ _   _
|  \| |/ _ \| |/ _` | | | |
| |\  | (_) | | (_| | |_| |
|_| \_|\___/|_|\__, |\__,_|
               |___/
Versi 0.1.0
```

Nolqu adalah bahasa pemrograman yang dirancang agar kode mudah dibaca seperti
membaca kalimat biasa. Tujuannya satu: menulis program tanpa harus berjuang
melawan syntax. Cocok untuk belajar konsep pemrograman dari nol, maupun untuk
developer yang ingin bahasa yang tidak bawel.

---

## Instalasi

```bash
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu
make
sudo make install   # pasang ke /usr/local/bin/nq
```

---

## Penggunaan

```bash
nq program.nq          # Jalankan program
nq run program.nq      # Jalankan program (alternatif)
nq repl                # Mode interaktif (REPL)
nq version             # Tampilkan versi
nq help                # Tampilkan bantuan
```

---

## Sintaks Dasar

### Variabel

```nolqu
let nama = "Budi"
let umur = 25
let aktif = true
let kosong = nil

print nama
print umur
```

### Operasi Aritmatika

```nolqu
let x = 10
let y = 3

print x + y    # 13
print x - y    # 7
print x * y    # 30
print x / y    # 3.33333...
print x % y    # 1
```

### Konkatenasi Teks

```nolqu
let salam = "Halo, " .. "Dunia" .. "!"
print salam    # Halo, Dunia!

let angka = 42
print "Jawabannya adalah: " .. angka
```

### Kondisi

```nolqu
let nilai = 75

if nilai >= 90
  print "Nilai A"
else
  if nilai >= 75
    print "Nilai B"
  else
    print "Nilai C"
  end
end
```

### Perulangan

```nolqu
let i = 1
loop i <= 5
  print i
  i = i + 1
end
```

### Fungsi

```nolqu
function sapa(nama)
  return "Halo, " .. nama .. "!"
end

print sapa("Dunia")

function faktorial(n)
  if n <= 1
    return 1
  end
  return n * faktorial(n - 1)
end

print faktorial(5)    # 120
```

### Logika Boolean

```nolqu
let a = 10
let b = 20

if a > 0 and b > 0
  print "Keduanya positif"
end

if a > 100 or b > 10
  print "Salah satu benar"
end

if not false
  print "Ini selalu tampil"
end
```

---

## Tipe Data

| Tipe      | Contoh                | Keterangan                 |
|-----------|-----------------------|----------------------------|
| `angka`   | `42`, `3.14`, `-5`    | 64-bit floating point      |
| `teks`    | `"Halo Dunia"`        | String Unicode immutable   |
| `boolean` | `true`, `false`       | Nilai logika               |
| `nil`     | `nil`                 | Tidak ada nilai            |
| `fungsi`  | `function nama() ...` | Fungsi                     |

---

## Keyword

```
let       — deklarasi variabel
print     — cetak ke output
if        — kondisi
else      — alternatif kondisi
loop      — perulangan (while-style)
function  — deklarasi fungsi
return    — kembalikan nilai
import    — impor file
end       — tutup blok
true      — nilai boolean benar
false     — nilai boolean salah
nil       — nilai kosong
and       — logika AND
or        — logika OR
not       — negasi logika
```

---

## Contoh Program Lengkap

### Deret Fibonacci

```nolqu
function fibonacci(n)
  if n <= 1
    return n
  end
  return fibonacci(n - 1) + fibonacci(n - 2)
end

let i = 0
loop i < 10
  print fibonacci(i)
  i = i + 1
end
```

### FizzBuzz

```nolqu
let n = 1
loop n <= 20
  if n % 15 == 0
    print "FizzBuzz"
  else
    if n % 3 == 0
      print "Fizz"
    else
      if n % 5 == 0
        print "Buzz"
      else
        print n
      end
    end
  end
  n = n + 1
end
```

---

## Arsitektur

```
source.nq
    │
    ▼ Lexer         → Token stream
    ▼ Parser        → AST (Abstract Syntax Tree)
    ▼ Compiler      → Bytecode Chunk
    ▼ Nolqu VM      → Eksekusi (stack-based)
```

- **Lexer** — Tokenisasi kode sumber
- **Parser** — Recursive descent, menghasilkan AST
- **Compiler** — Tree-walk compiler, emits bytecode
- **VM** — Stack-based bytecode interpreter
- **Runtime** — Binary `nq` mandiri (C99)

### Struktur Project

```
nolqu/
├── src/
│   ├── common.h      — Common headers & macros
│   ├── memory.h/.c   — Memory management
│   ├── value.h/.c    — Value types (nil, bool, number, obj)
│   ├── chunk.h/.c    — Bytecode chunk + disassembler
│   ├── object.h/.c   — Heap objects (string, function)
│   ├── table.h/.c    — Hash table (globals, string interning)
│   ├── lexer.h/.c    — Lexer / tokenizer
│   ├── ast.h/.c      — AST node types
│   ├── parser.h/.c   — Recursive descent parser
│   ├── compiler.h/.c — AST → Bytecode compiler
│   ├── vm.h/.c       — Nolqu Virtual Machine
│   ├── repl.h/.c     — Interactive REPL
│   └── main.c        — CLI entry point
├── examples/
│   ├── hello.nq
│   ├── fibonacci.nq
│   ├── functions.nq
│   └── counter.nq
├── docs/
│   ├── grammar.md    — Formal grammar (EBNF)
│   └── vm_design.md  — VM & bytecode design
├── Makefile
└── README.md
```

---

## Build

```bash
make          # Release build → ./nq
make debug    # Debug build   → ./nq-debug
make test     # Jalankan contoh program
make clean    # Hapus build artifacts
```

---

## Error Messages

Nolqu dirancang untuk memberikan pesan error yang jelas:

```
[ Error Runtime ] program.nq:5
  Variabel 'x' tidak ditemukan.
  Petunjuk: Apakah kamu sudah mendeklarasikannya dengan 'let x = ...'?
```

```
[ Error Runtime ] program.nq:8
  Pembagian dengan nol tidak diizinkan!
  Petunjuk: Periksa nilai pembagi sebelum melakukan pembagian.
```

```
[ Error Runtime ] program.nq:12
  Fungsi 'tambah' memerlukan 2 argumen, tapi diberikan 1.
```

---

## Lisensi

MIT License — bebas digunakan dan dimodifikasi.
# Nolqu
