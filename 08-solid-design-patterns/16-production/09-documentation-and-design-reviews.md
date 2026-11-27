# Documentation & Design Reviews

> **TL;DR**: Keep documentation close to code so it stays alive: **C4 diagrams** for architecture overview, **ADRs** for *why*, **RFCs** for upcoming cross-team designs, and **concise docstrings** on public contracts.

## Effective Design Reviews (RFCs)
- Start with **Problem & Non-Goals** before proposing solutions.
- Compare **at least 2 alternatives** with explicit trade-offs (latency, cost, ops complexity).
- Call out **Failure Modes, Rollback Plan, and Observability** upfront.
