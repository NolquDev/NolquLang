# Contributing to Nolqu

Thank you for your interest in contributing to Nolqu!
This document explains how to get involved — whether that means fixing a bug,
improving documentation, or proposing a new language feature.

---

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Ways to Contribute](#ways-to-contribute)
3. [Getting Started](#getting-started)
4. [Development Workflow](#development-workflow)
5. [Code Style](#code-style)
6. [Commit Messages](#commit-messages)
7. [Pull Request Process](#pull-request-process)
8. [Reporting Bugs](#reporting-bugs)
9. [Suggesting Features](#suggesting-features)
10. [Questions](#questions)

---

## Code of Conduct

This project follows a [Code of Conduct](CODE_OF_CONDUCT.md).
By participating you agree to uphold it.
Please report unacceptable behaviour to **nolquteam@gmail.com**.

---

## Ways to Contribute

You do not need to write code to contribute. Here are all the ways to help:

| Type | Examples |
|---|---|
| **Bug reports** | Wrong output, crash, memory error |
| **Documentation** | Typos, unclear examples, missing sections |
| **Standard library** | New `stdlib/*.nq` modules |
| **Examples** | Real-world scripts in `examples/` |
| **Language features** | Propose via GitHub Issues before implementing |
| **Runtime / VM** | Performance, safety, correctness fixes |
| **Tests** | Expand `examples/tests/` coverage |
| **Tooling** | Editor plugins, syntax highlighting, formatters |

---

## Getting Started

### 1. Fork and clone

```bash
git clone https://github.com/Nadzil123/Nolqu.git
cd Nolqu
```

### 2. Build

```bash
make          # release build → ./nq
make debug    # debug + ASan/UBSan → ./nq-debug
```

### 3. Run the tests

```bash
make test
```

All tests must pass before submitting a pull request.

### 4. Run under AddressSanitizer (for runtime changes)

```bash
make debug
./nq-debug examples/tests/math_test.nq
./nq-debug examples/tests/array_test.nq
./nq-debug examples/tests/error_test.nq
./nq-debug examples/tests/string_test.nq
```

No memory errors should appear.

---

## Development Workflow

```
main branch  — always stable, tagged releases only
feature/xyz  — your working branch
```

1. Create a branch from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. Make your changes.

3. Add or update tests for anything you changed.

4. Build and test:
   ```bash
   make clean && make && make test
   ```

5. Push and open a Pull Request.

---

## Code Style

### C (runtime core — `memory.c`, `value.c`, `chunk.c`, `object.c`, `table.c`, `gc.c`, `vm.c`)

- Standard: **C11**
- Indentation: **4 spaces** (no tabs)
- Max line length: **100 characters**
- Naming: `snake_case` for functions and variables, `UPPER_CASE` for macros
- Every public function has a brief comment above it
- No global mutable state outside of the documented globals (`nq_all_objects`, `nq_gc_vm`, etc.)
- **Never** call `malloc`/`realloc`/`free` directly — always use `nq_realloc`, `NQ_ALLOC`, `FREE_ARRAY`
- Zero warnings with `-Wall -Wextra -Wpedantic`

### C++ (tooling layer — `lexer.cpp`, `parser.cpp`, `compiler.cpp`, etc.)

- Standard: **C++17**
- Same indentation and naming as the C layer
- Prefer `std::vector` over raw arrays for variable-length internal collections
- No exceptions — error handling goes through the existing compiler error mechanism
- No RTTI
- Zero warnings with `-Wall -Wextra`

### Nolqu (stdlib — `stdlib/*.nq`)

- Follow the style of existing modules (`stdlib/math.nq`, `stdlib/array.nq`)
- Every exported function has a leading comment that explains what it does,
  its parameters, and includes at least one usage example
- Helper functions that are not part of the public API are prefixed with `_`
- Keep functions short and focused — one function, one job
- No global mutable state in stdlib except where explicitly required (e.g. `stdlib/json.nq` parser state)

---

## Commit Messages

Use the [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>: <short summary>

[optional body]
[optional footer]
```

**Types:**

| Type | Use for |
|---|---|
| `feat` | New feature or stdlib function |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `refactor` | Code change with no behaviour change |
| `test` | Adding or fixing tests |
| `perf` | Performance improvement |
| `chore` | Build system, dependencies, housekeeping |

**Examples:**

```
feat: add break and continue to loop statements

fix: vm.c native string functions used malloc instead of nq_realloc
causing GC byte counter underflow

docs: expand stdlib.md with stdlib/json examples

test: add error_test.nq nested try/catch coverage
```

---

## Pull Request Process

1. **One PR per change** — keep PRs focused. A bug fix and a new feature belong in separate PRs.

2. **Fill out the PR template** — describe what changed and why.

3. **Tests required** — every bug fix should have a test that would have caught it. Every new feature needs tests.

4. **Documentation required** for:
   - New stdlib functions → update `docs/stdlib.md`
   - New language features → update `docs/language.md` and `docs/grammar.md`
   - New CLI commands → update `README.md` and `docs/language.md`

5. **Breaking changes** — changes that break existing `.nq` programs or the embed API are only considered for a new major version. Discuss on an Issue first.

6. A maintainer will review and merge. Response time is typically within a few days.

---

## Reporting Bugs

Use the **Bug Report** issue template on GitHub.

Include:
- Nolqu version (`nq version`)
- Operating system and architecture
- The `.nq` program that reproduces the bug (as small as possible)
- The output you got vs. the output you expected
- Whether the bug is reproducible with `./nq-debug` (AddressSanitizer build)

For **security vulnerabilities** please do **not** open a public issue.
See [SECURITY.md](SECURITY.md) instead.

---

## Suggesting Features

Open a **Feature Request** issue on GitHub before writing any code.

A good feature request explains:
- What problem you are trying to solve
- What syntax or API you are proposing
- How it fits the Nolqu philosophy (simple, readable, practical)
- What you would **not** want it to do

Features that add complexity without clear scripting value are unlikely to be accepted.
Nolqu prioritises a **small, stable core** over a large feature set.

---

## Questions

- **General questions:** open a GitHub Discussion
- **Team contact:** nolquteam@gmail.com
