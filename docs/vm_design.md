# Nolqu Virtual Machine — Desain Bytecode

## Arsitektur Overview

```
Program .nq
    │
    ▼
┌─────────────┐
│   Lexer     │  Tokenisasi source code → stream of Token
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Parser    │  Recursive descent parser → AST
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Compiler  │  Tree-walk compiler → Bytecode Chunk
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Nolqu VM   │  Stack-based bytecode interpreter
└─────────────┘
```

---

## Stack-Based Virtual Machine

Nolqu VM menggunakan arsitektur **stack-based** (berbasis tumpukan):
- Setiap operasi membaca dari stack dan menulis ke stack
- Tidak ada register eksplisit
- Sederhana dan portabel

### Value Stack

```
Stack (bertumbuh ke atas):
┌──────────┐  ← stack_top (pointer ke slot kosong berikutnya)
│          │
│  val[n]  │  ← nilai teratas
│  val[n-1]│
│  ...     │
│  val[0]  │  ← dasar stack
└──────────┘
```

### Call Frames

Setiap pemanggilan fungsi membuat satu `CallFrame`:

```
CallFrame {
    function*  → ObjFunction yang sedang dieksekusi
    ip         → instruction pointer (posisi bytecode saat ini)
    slots*     → pointer ke base stack frame ini
}
```

Layout stack saat fungsi `add(5, 10)` dipanggil:

```
Stack:
┌──────────────┐  ← stack_top
│  local var n │  (variabel lokal di dalam fungsi)
│     ...      │
│  arg[1] = 10 │  ← slots[2]
│  arg[0] = 5  │  ← slots[1]
│  <fn add>    │  ← slots[0] (fungsi itu sendiri)
│  (caller...) │
└──────────────┘
```

---

## Instruction Set

### Format Instruksi

```
[OPCODE: 1 byte] [operand1: 1 byte] [operand2: 1 byte] ...
```

### Tabel Instruksi Lengkap

| Opcode             | Byte | Operand | Deskripsi                          |
|--------------------|------|---------|-------------------------------------|
| OP_CONST           | 0x00 | idx     | Push constants[idx] ke stack        |
| OP_NIL             | 0x01 | —       | Push nil                            |
| OP_TRUE            | 0x02 | —       | Push true                           |
| OP_FALSE           | 0x03 | —       | Push false                          |
| OP_POP             | 0x04 | —       | Pop dan buang top of stack          |
| OP_DUP             | 0x05 | —       | Duplikasi top of stack              |
| OP_DEFINE_GLOBAL   | 0x06 | name_idx| Pop → globals[name]                 |
| OP_GET_GLOBAL      | 0x07 | name_idx| Push globals[name]                  |
| OP_SET_GLOBAL      | 0x08 | name_idx| Peek → globals[name]                |
| OP_GET_LOCAL       | 0x09 | slot    | Push stack[frame.base + slot]       |
| OP_SET_LOCAL       | 0x0A | slot    | Peek → stack[frame.base + slot]     |
| OP_ADD             | 0x0B | —       | pop b, pop a → push a+b             |
| OP_SUB             | 0x0C | —       | pop b, pop a → push a-b             |
| OP_MUL             | 0x0D | —       | pop b, pop a → push a*b             |
| OP_DIV             | 0x0E | —       | pop b, pop a → push a/b             |
| OP_MOD             | 0x0F | —       | pop b, pop a → push a%b             |
| OP_NEGATE          | 0x10 | —       | pop a → push -a                     |
| OP_CONCAT          | 0x11 | —       | pop b, pop a → push a..b (string)   |
| OP_EQ              | 0x12 | —       | pop b, pop a → push a==b            |
| OP_NEQ             | 0x13 | —       | pop b, pop a → push a!=b            |
| OP_LT              | 0x14 | —       | pop b, pop a → push a<b             |
| OP_GT              | 0x15 | —       | pop b, pop a → push a>b             |
| OP_LTE             | 0x16 | —       | pop b, pop a → push a<=b            |
| OP_GTE             | 0x17 | —       | pop b, pop a → push a>=b            |
| OP_NOT             | 0x18 | —       | pop a → push !a                     |
| OP_PRINT           | 0x19 | —       | pop a, print a + '\n'               |
| OP_JUMP            | 0x1A | hi, lo  | ip += (hi<<8 \| lo)                 |
| OP_JUMP_IF_FALSE   | 0x1B | hi, lo  | if !peek: ip += offset (pop cond)   |
| OP_LOOP            | 0x1C | hi, lo  | ip -= (hi<<8 \| lo)                 |
| OP_CALL            | 0x1D | argc    | Call stack[-argc-1] dengan argc args|
| OP_RETURN          | 0x1E | —       | Pop hasil, pulihkan frame           |
| OP_HALT            | 0x1F | —       | Hentikan eksekusi                   |

---

## Contoh Bytecode

### `print 5 + 3`

```
OP_CONST   0    → push 5
OP_CONST   1    → push 3
OP_ADD          → pop 3, pop 5, push 8
OP_PRINT        → pop 8, print "8\n"
```

### `if x > 5` ... `end`

```
OP_GET_GLOBAL  [x]
OP_CONST       [5]
OP_GT               → push true/false
OP_JUMP_IF_FALSE  [jump_offset]   ← patch setelah blok
OP_POP              → pop true
... (then block) ...
[PATCH TARGET] ←
OP_POP              → pop false
```

### `loop i < 10` ... `end`

```
[LOOP_START]:
OP_GET_LOCAL   [i]
OP_CONST       [10]
OP_LT               → push true/false
OP_JUMP_IF_FALSE  [exit_offset]
OP_POP
... (body) ...
OP_LOOP        [back_to_LOOP_START]
[EXIT]:
OP_POP
```

---

## Manajemen Memori

### Heap Objects

Semua objek heap (string, fungsi) dilacak dalam linked list global:

```
nq_all_objects → ObjString → ObjFunction → ObjString → NULL
```

### String Interning

Semua string di-intern: string yang sama hanya ada satu salinan di memori.
Perbandingan string = perbandingan pointer (O(1)).

```
StringTable (hash table open-addressing):
"hello" → ptr_to_ObjString_hello
"world" → ptr_to_ObjString_world
```

### Global Variables

Hash table open-addressing:
```
globals:
ObjString* "x" → Value NUMBER(5)
ObjString* "f" → Value OBJ(ObjFunction*)
```

---

## Tipe Data Internal

```
Value (tagged union, 16 bytes):
  type: VAL_NIL | VAL_BOOL | VAL_NUMBER | VAL_OBJ
  as:
    boolean: bool
    number:  double (64-bit IEEE 754)
    obj:     Obj*

ObjString:
  Obj header (type, next, marked)
  length: int
  chars:  char*  (null-terminated)
  hash:   uint32_t (FNV-1a)

ObjFunction:
  Obj header
  arity: int           (jumlah parameter)
  chunk: Chunk*        (bytecode milik fungsi ini)
  name:  ObjString*    (nama fungsi, NULL untuk script)
```
