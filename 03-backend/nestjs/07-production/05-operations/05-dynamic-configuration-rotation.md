---
title: "Dynamic Configuration and Secret Rotation in NestJS"
description: "Rotating database credentials, API tokens, and feature flags at runtime without restarting NestJS application containers"
tags: [nestjs, operations, secret-rotation, dynamic-config, feature-flags, zero-downtime]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Dynamic Configuration and Secret Rotation in NestJS

## Conceptual Overview
Compliance standards (PCI-DSS, SOC 2, ISO 27001) mandate periodic rotation of cryptographic keys and database credentials. In zero-downtime production environments, restarting hundreds of container pods to pick up a changed password is slow and risky.

Dynamic secret rotation architectures:
1. Maintain dual active credentials during rotation windows (Old and New key both valid).
2. Polling or webhook triggers to update connection pools in-flight.
3. Feature flags using LaunchDarkly or Unleash.

---

## Key Takeaways & Cheat Sheet

- Support dual-key verification during cryptographic token rotation.
- Use dynamic configuration stores (Consul, AWS AppConfig) for feature flags.
- Re-authenticate database connection pools gracefully upon credential update events.

---

## Cross-References
- [06-secrets-management.md](../01-security/06-secrets-management.md) - Secrets management
- [01-production-configuration.md](../04-deployment/01-production-configuration.md) - Production configuration
