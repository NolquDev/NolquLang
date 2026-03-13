/*
 * table.h — Nolqu hash table (open addressing, Robin Hood probing)
 *
 * Used for the global variable store (VM.globals).
 * Keys are always interned ObjString pointers — pointer equality is
 * sufficient for comparison (no strcmp needed).
 *
 * Tombstone strategy: deleted entries set key=NULL, value=BOOL_VAL(true)
 * so probe chains are not broken during lookup.
 */

#ifndef NQ_TABLE_H
#define NQ_TABLE_H

#include "common.h"
#include "value.h"
#include "object.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Entry ───────────────────────────────────────────────────────── */
typedef struct {
    ObjString* key;     /* NULL = empty slot or tombstone             */
    Value      value;   /* tombstone: key==NULL && !IS_NIL(value)     */
} Entry;

/* ── Table ───────────────────────────────────────────────────────── */
typedef struct {
    int    count;       /* live entries (tombstones not counted)      */
    int    capacity;
    Entry* entries;
} Table;

/* ── Table API ───────────────────────────────────────────────────── */
void  initTable  (Table* t);
void  freeTable  (Table* t);

/* Returns true and sets *out if key exists; false otherwise. */
bool  tableGet   (Table* t, ObjString* key, Value* out);
/* Returns true if key was newly inserted; false if it already existed. */
bool  tableSet   (Table* t, ObjString* key, Value value);
/* Returns true if key was found and removed (replaced with tombstone). */
bool  tableDelete(Table* t, ObjString* key);
/* Copy all entries from 'from' into 'to'. */
void  tableCopy  (Table* from, Table* to);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_TABLE_H */
