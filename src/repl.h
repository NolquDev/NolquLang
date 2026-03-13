/*
 * repl.h — Nolqu interactive Read-Eval-Print Loop
 *
 * runREPL() blocks until the user types "exit" or EOF.
 * The VM is shared across inputs so global variables persist
 * across lines.
 */

#ifndef NQ_REPL_H
#define NQ_REPL_H

#include "common.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

void runREPL(VM* vm);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NQ_REPL_H */
