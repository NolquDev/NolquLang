#include "table.h"
#include "memory.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* t) {
    t->count    = 0;
    t->capacity = 0;
    t->entries  = NULL;
}

void freeTable(Table* t) {
    FREE_ARRAY(Entry, t->entries, t->capacity);
    initTable(t);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t idx = key->hash % (uint32_t)capacity;
    Entry* tombstone = NULL;

    for (;;) {
        Entry* e = &entries[idx];
        if (e->key == NULL) {
            if (IS_NIL(e->value)) {
                // Empty slot — return tombstone if we found one, else this slot
                return tombstone ? tombstone : e;
            } else {
                // Tombstone
                if (!tombstone) tombstone = e;
            }
        } else if (e->key == key) {
            return e;
        }
        idx = (idx + 1) % (uint32_t)capacity;
    }
}

static void adjustCapacity(Table* t, int new_capacity) {
    Entry* new_entries = NQ_ALLOC(Entry, new_capacity);
    for (int i = 0; i < new_capacity; i++) {
        new_entries[i].key   = NULL;
        new_entries[i].value = NIL_VAL;
    }

    t->count = 0;
    for (int i = 0; i < t->capacity; i++) {
        Entry* src = &t->entries[i];
        if (!src->key) continue;
        Entry* dst = findEntry(new_entries, new_capacity, src->key);
        dst->key   = src->key;
        dst->value = src->value;
        t->count++;
    }

    FREE_ARRAY(Entry, t->entries, t->capacity);
    t->entries  = new_entries;
    t->capacity = new_capacity;
}

bool tableGet(Table* t, ObjString* key, Value* out) {
    if (t->count == 0) return false;
    Entry* e = findEntry(t->entries, t->capacity, key);
    if (!e->key) return false;
    *out = e->value;
    return true;
}

bool tableSet(Table* t, ObjString* key, Value value) {
    if (t->count + 1 > (int)(t->capacity * TABLE_MAX_LOAD)) {
        adjustCapacity(t, GROW_CAPACITY(t->capacity));
    }
    Entry* e = findEntry(t->entries, t->capacity, key);
    bool is_new = (e->key == NULL);
    if (is_new && IS_NIL(e->value)) t->count++;
    e->key   = key;
    e->value = value;
    return is_new;
}

bool tableDelete(Table* t, ObjString* key) {
    if (t->count == 0) return false;
    Entry* e = findEntry(t->entries, t->capacity, key);
    if (!e->key) return false;
    // Place a tombstone
    e->key   = NULL;
    e->value = BOOL_VAL(true); // non-nil sentinel
    return true;
}

void tableCopy(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* e = &from->entries[i];
        if (e->key) tableSet(to, e->key, e->value);
    }
}
