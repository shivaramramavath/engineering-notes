# 21 · Security and Hardening

Security in modern enterprise Java is not an afterthought; it is an architectural discipline woven into every layer of software development and operations. This module provides authoritative, production-grade security guidelines covering input validation, cryptographic primitives (JCA/JCE), password hashing standards, secrets lifecycle management, safe deserialization (JEP 290/415), software supply chain auditing (SBOM), and an audit-ready production security checklist.

## Contents

| # | Note | Focus |
|---|------|-------|
| 00 | [Input Validation and Injection Defense](00_input-validation-and-injection.md) | SQLi, Command Injection, Log Forging, Path Traversal, Bean Validation |
| 01 | [Cryptography in Java (JCA / JCE)](01_cryptography.md) | AES-256-GCM, Ed25519 signatures, SecureRandom, constant-time comparisons |
| 02 | [Password Hashing](02_password-hashing.md) | Argon2id, PBKDF2-HMAC-SHA512, memory-hard functions, salt & pepper |
| 03 | [Secrets Management](03_secrets-management.md) | Externalized secrets, char[] wiping, Vault, PKCS12 KeyStores, heap dump safety |
| 04 | [Insecure Deserialization](04_insecure-deserialization.md) | Gadget chains, RCE, JEP 290 allow-lists, JEP 415 filter factories, JSON migration |
| 05 | [Dependency Security & Supply Chain](05_dependency-security.md) | OWASP Dependency-Check, CycloneDX SBOMs, transitive CVE patching, typosquatting |
| 06 | [Production Security Checklist](06_security-checklist.md) | Non-root containers, JVM flags, TLS 1.3, HSTS, log masking, audit matrix |

---
**Next module:** [Production Engineering](../22-production-engineering/README.md)
