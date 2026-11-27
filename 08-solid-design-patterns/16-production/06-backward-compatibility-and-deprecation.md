# Backward Compatibility & Deprecation

> **TL;DR**: Follow **Postel's Law** (Tolerant Reader: ignore unknown JSON fields) and **Expand-and-Contract** for zero-downtime API and database migrations, announcing retirements with HTTP `Deprecation` and `Sunset` headers.

## Safe Deprecation Playbook
1. **Expand**: Add new field/endpoint alongside old one.
2. **Announce**: Add `Deprecation: true` and `Sunset: <date>` headers + monitor traffic on old endpoint.
3. **Migrate**: Help remaining clients switch.
4. **Contract**: Remove old field/endpoint only after usage hits 0%.
