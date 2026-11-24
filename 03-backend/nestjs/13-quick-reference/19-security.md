# Security & OWASP Cheat Sheet

- **HSTS**: `maxAge: 31536000; includeSubDomains; preload`
- **CSP**: Restrict script-src, object-src 'none'
- **Hashing**: `argon2.hash(password, { type: argon2.argon2id })`
- **Encryption**: AES-256-GCM with 96-bit random IV
