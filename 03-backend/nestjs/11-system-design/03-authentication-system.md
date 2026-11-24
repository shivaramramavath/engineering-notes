---
title: "System Design: Centralized Authentication & SSO System"
description: "Designing an enterprise Identity Provider with JWT token rotation, Redis blacklisting, and Passkeys/WebAuthn"
tags: [nestjs, system-design, auth, idp, jwt, sso]
---

# System Design: Centralized Authentication & SSO System

## Architecture
- **Token Strategy**: Asymmetric RS256 JWT access tokens (15m expiration) verified locally by resource servers via public JWKS.
- **Session & Revocation**: Refresh tokens hashed with Argon2id in PostgreSQL; immediate revocation enforced via distributed Redis blacklist.
