# Module 22: JavaScript & Node.js Application Security

> A production-grade curriculum covering client-side and server-side web security, common exploit vectors, defensive programming techniques, and security engineering standards for modern JavaScript runtimes.

---

## Curriculum Overview

This module provides an exhaustive, practical breakdown of modern JavaScript and Node.js security vulnerabilities, their underlying runtime causes, and concrete mitigations aligned with OWASP Top 10, ASVS, and CWE benchmarks.

| # | Note | Focus Areas | Level |
| :- | :--- | :--- | :--- |
| **01** | [Cross-Site Scripting (XSS)](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/01_xss.md) | Stored, Reflected, DOM-based XSS; sanitization, DOMPurify, CSP nonces, context encoding. | Intermediate / Advanced |
| **02** | [Cross-Site Request Forgery (CSRF)](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/02_csrf.md) | Synchronizer token pattern, Double Submit Cookie, SameSite attributes, Lax GET risks. | Intermediate / Advanced |
| **03** | [Prototype Pollution](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/03_prototype-pollution.md) | `__proto__`, `constructor.prototype` tampering; object freezing, null prototypes, V8 hidden class deopt. | Advanced |
| **04** | [Input Validation & Sanitization](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/04_input-validation.md) | Schema validation (Zod/Valibot), ReDoS defense, SQL injection, NoSQL injection prevention. | Intermediate |
| **05** | [Dependency & Supply Chain Security](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/05_dependency-security.md) | Lockfile integrity, `npm audit`, Dependabot, typosquatting, build pipeline sandboxing. | Intermediate / Advanced |
| **06** | [Production Security Checklist](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security/06_security-checklist.md) | OWASP ASVS verification, Helmet headers, SSRF guards, crypto PRNG, constant-time checks. | Advanced / Production |

---

## Prerequisites & Cross-References

- **Language Core**: [04_scope-and-execution](file:///e:/Learning/engineering-notes/01-languages/javascript/04_scope-and-execution/README.md), [05_this-and-oop](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/README.md)
- **Node.js Runtime**: [16_nodejs](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/README.md)
- **Real-World Patterns**: [23_real-world-patterns](file:///e:/Learning/engineering-notes/01-languages/javascript/23_real-world-patterns/README.md)
- **Backend Architecture**: [03-backend/nodejs](file:///e:/Learning/engineering-notes/03-backend/nodejs/README.md), [06-project-patterns/auth](file:///e:/Learning/engineering-notes/06-project-patterns/auth/README.md)
