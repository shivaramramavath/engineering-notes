# 22 · Security

Security fundamentals for JavaScript applications, in the browser and on Node. The focus is on understanding **why** each vulnerability exists so you can spot it in code review, not memorizing payloads.

## Learning Order

| # | Note | What you'll learn |
|---|---|---|
| 01 | [XSS](./01_xss.md) | How untrusted data becomes script in the browser; output encoding, sanitizing, CSP |
| 02 | [CSRF](./02_csrf.md) | Forged requests via automatic cookies; SameSite, tokens, Origin checks |
| 03 | [Prototype Pollution](./03_prototype-pollution.md) | Poisoning `Object.prototype` through unsafe merges; safe patterns |
| 04 | [Input Validation](./04_input-validation.md) | Schema validation, injection defenses, limits, config validation |
| 05 | [Dependency Security](./05_dependency-security.md) | Lockfiles, `npm audit`, supply-chain risks, automated updates |
| 06 | [Security Checklist](./06_security-checklist.md) | A review checklist tying everything together |

## Prerequisites

- [Prototypes and the prototype chain](../05_this-and-oop/03_prototypes-and-prototype-chain.md): needed for prototype pollution
- [DOM and browser](../14_dom-and-browser/README.md): needed for XSS and CSRF
- [Networking](../15_networking/README.md): HTTP, headers, and CORS
- [Node.js](../16_nodejs/README.md): servers, env, filesystem, child processes
- [Testing](../21_testing/README.md): to lock in security fixes with regression tests

## Suggested Paths

- **Frontend developer:** 01 → 02 → 04 → 06
- **Backend/Node developer:** 04 → 02 → 03 → 05 → 06
- **Interview prep:** XSS types and defenses, CSRF vs CORS, prototype pollution mechanics, parameterized queries, "how do you secure an Express app?" (06)

## Core Principles

1. **Never trust input**: validate on the server, at the boundary, with an allowlist.
2. **Encode for the output context**: validation is not a substitute for escaping.
3. **Defense in depth**: combine controls (e.g. SameSite + token + Origin check; encoding + CSP).
4. **Least privilege**: for users, tokens, database accounts, and processes.
5. **Fail safely**: default deny, generic error messages, no secrets in logs.
6. **Automate**: audits, lint rules, and security-focused tests in CI.

## Related Topics

- [Error handling in production](../10_error-handling/05_production-error-handling.md)
- [Rate limiting](../23_real-world-patterns/09_rate-limiting.md)
- [Browser storage](../14_dom-and-browser/08_browser-storage.md): what is safe to store client-side
