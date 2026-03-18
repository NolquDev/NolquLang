# Security Policy

## Supported Versions

Only the latest stable release receives security fixes.

| Version | Supported |
|---------|-----------|
| 1.x (latest) | ✅ Yes |
| < 1.0 | ❌ No |

---

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues.**

If you discover a security vulnerability in Nolqu — whether in the runtime,
the VM, the standard library, or the embed API — please report it privately so
we can address it before public disclosure.

### How to report

Send an email to **nolqucontact@gmail.com** with:

- Subject: `[SECURITY] Brief description`
- A description of the vulnerability
- Steps to reproduce (a minimal `.nq` program or C code if applicable)
- The version of Nolqu affected (`nq version`)
- Your assessment of the impact (what can an attacker do?)
- Whether you have a proposed fix (optional but welcome)

We will acknowledge your report within **72 hours** and aim to provide
a fix or mitigation within **14 days** for critical issues.

---

## Scope

The following are considered in scope for security reports:

### Runtime / VM
- **Memory safety:** buffer overflows, use-after-free, out-of-bounds reads/writes
  in the VM, GC, allocator, or any C/C++ runtime code
- **Stack overflow / infinite recursion** that crashes the process without
  a clean error message
- **Integer overflow or underflow** in internal counters that can be triggered
  by a crafted `.nq` program
- **Arbitrary code execution** through a crafted `.nq` program or embedded API call

### Embed API (`nolqu.h`)
- Memory corruption reachable via `nq_dostring`, `nq_dofile`, or `nq_register`
- A crafted Nolqu script escaping the VM sandbox in an embedded context

### Standard Library
- File I/O functions (`file_read`, `file_write`, etc.) performing operations
  beyond what was requested — e.g. path traversal issues
- `stdlib/json.nq` parser entering infinite loops or crashing on
  adversarial input

---

## Out of Scope

The following are **not** considered security vulnerabilities:

- Denial-of-service via infinite loops in `.nq` programs — Nolqu has no
  execution timeout by design; the host application is responsible for limits
- Resource exhaustion (large allocations, deep recursion) from trusted scripts
- Bugs in the experimental `codegen` / transpiler component
- Issues only reproducible in a `make debug` build (ASan instrumented)
- Theoretical vulnerabilities without a proof-of-concept

---

## Disclosure Policy

We follow **coordinated disclosure**:

1. You report privately to nolqucontact@gmail.com
2. We confirm receipt within 72 hours
3. We investigate and develop a fix
4. We release the fix and credit you in the CHANGELOG (unless you prefer to remain anonymous)
5. You may publish details 30 days after the fix is released, or sooner with our agreement

---

## Credits

We are grateful to everyone who has responsibly disclosed vulnerabilities.
Contributors will be acknowledged in the release notes unless they request anonymity.
