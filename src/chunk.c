#include "chunk.h"
#include "memory.h"
#include "value.h"

void initChunk(Chunk* chunk) {
    chunk->count    = 0;
    chunk->capacity = 0;
    chunk->code     = NULL;
    chunk->lines    = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code,  chunk->capacity);
    FREE_ARRAY(int,     chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->count >= chunk->capacity) {
        int old = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old);
        chunk->code  = GROW_ARRAY(uint8_t, chunk->code,  old, chunk->capacity);
        chunk->lines = GROW_ARRAY(int,     chunk->lines, old, chunk->capacity);
    }
    chunk->code[chunk->count]  = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void patchJump(Chunk* chunk, int offset) {
    int jump = chunk->count - offset - 2;
    if (jump > 0xFFFF) {
        fprintf(stderr,
            NQ_COLOR_RED "error:" NQ_COLOR_RESET
            " Jump target out of range.\n");
        exit(1);
    }
    chunk->code[offset]     = (jump >> 8) & 0xFF;
    chunk->code[offset + 1] = jump & 0xFF;
}

// ─────────────────────────────────────────────
//  Disassembler
// ─────────────────────────────────────────────
static int simpleInstruction(const char* name, int offset) {
    printf("%-20s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-20s %4d\n", name, slot);
    return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
    int target = offset + 3 + sign * jump;
    printf("%-20s %4d -> %04d\n", name, offset, target);
    return offset + 3;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint16_t idx = (uint16_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
    printf("%-20s %4d  (", name, idx);
    printValue(chunk->constants.values[idx]);
    printf(")\n");
    return offset + 3;   /* opcode + 2 index bytes */
}

void disassembleChunk(Chunk* chunk, const char* name) {
    printf(NQ_COLOR_CYAN "=== Bytecode: %s ===" NQ_COLOR_RESET "\n", name);
    for (int offset = 0; offset < chunk->count;)
        offset = disassembleInstruction(chunk, offset);
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
        printf("   | ");
    else
        printf("%4d ", chunk->lines[offset]);

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONST:         return constantInstruction("OP_CONST",        chunk, offset);
        case OP_NIL:           return simpleInstruction("OP_NIL",                   offset);
        case OP_TRUE:          return simpleInstruction("OP_TRUE",                  offset);
        case OP_FALSE:         return simpleInstruction("OP_FALSE",                 offset);
        case OP_POP:           return simpleInstruction("OP_POP",                   offset);
        case OP_DUP:           return simpleInstruction("OP_DUP",                   offset);
        case OP_DEFINE_GLOBAL: return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_GET_GLOBAL:    return constantInstruction("OP_GET_GLOBAL",    chunk, offset);
        case OP_SET_GLOBAL:    return constantInstruction("OP_SET_GLOBAL",    chunk, offset);
        case OP_GET_LOCAL:     return byteInstruction("OP_GET_LOCAL",         chunk, offset);
        case OP_SET_LOCAL:     return byteInstruction("OP_SET_LOCAL",         chunk, offset);
        case OP_ADD:           return simpleInstruction("OP_ADD",                   offset);
        case OP_SUB:           return simpleInstruction("OP_SUB",                   offset);
        case OP_MUL:           return simpleInstruction("OP_MUL",                   offset);
        case OP_DIV:           return simpleInstruction("OP_DIV",                   offset);
        case OP_MOD:           return simpleInstruction("OP_MOD",                   offset);
        case OP_NEGATE:        return simpleInstruction("OP_NEGATE",                offset);
        case OP_CONCAT:        return simpleInstruction("OP_CONCAT",                offset);
        case OP_EQ:            return simpleInstruction("OP_EQ",                    offset);
        case OP_NEQ:           return simpleInstruction("OP_NEQ",                   offset);
        case OP_LT:            return simpleInstruction("OP_LT",                    offset);
        case OP_GT:            return simpleInstruction("OP_GT",                    offset);
        case OP_LTE:           return simpleInstruction("OP_LTE",                   offset);
        case OP_GTE:           return simpleInstruction("OP_GTE",                   offset);
        case OP_NOT:           return simpleInstruction("OP_NOT",                   offset);
        case OP_PRINT:         return simpleInstruction("OP_PRINT",                 offset);
        case OP_JUMP:          return jumpInstruction("OP_JUMP",          1, chunk, offset);
        case OP_JUMP_IF_FALSE: return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:          return jumpInstruction("OP_LOOP",         -1, chunk, offset);
        case OP_BUILD_ARRAY:   return byteInstruction("OP_BUILD_ARRAY",        chunk, offset);
        case OP_GET_INDEX:     return simpleInstruction("OP_GET_INDEX",                offset);
        case OP_SET_INDEX:     return simpleInstruction("OP_SET_INDEX",                offset);
        case OP_SLICE:         return simpleInstruction("OP_SLICE",                    offset);
        case OP_TRY:           return jumpInstruction("OP_TRY",          1, chunk, offset);
        case OP_TRY_END:       return simpleInstruction("OP_TRY_END",                  offset);
        case OP_THROW:         return simpleInstruction("OP_THROW",                    offset);
        case OP_CALL:          return byteInstruction("OP_CALL",              chunk, offset);
        case OP_RETURN:        return simpleInstruction("OP_RETURN",                offset);
        case OP_HALT:          return simpleInstruction("OP_HALT",                  offset);
        default:
            printf("??? unknown instruction: %d\n", instruction);
            return offset + 1;
    }
}
