# Lava Flow & Dead Code

| Field | Value |
| :--- | :--- |
| **Category** | Anti-Pattern (Codebase Entropy) |
| **Fix** | Git History Trust + Automated Dead-Code Linters |

> **TL;DR**: **Lava Flow** is old, half-abandoned experimental code, unused feature flags, and commented-out blocks left in production because *"nobody knows if it's still needed, so nobody dares delete it."*

## Why It Hurts
- Slows builds, confuses new hires, inflates static analysis noise, and keeps security vulnerabilities alive in unused dependencies.

## Fix
- **Delete dead code boldly**—Git remembers everything if you ever need it back.
- Use linters/IDE unused-symbol checks (`ts-prune` / `knip`, `vulture`, IntelliJ unused inspection) and delete expired Feature Flags.
