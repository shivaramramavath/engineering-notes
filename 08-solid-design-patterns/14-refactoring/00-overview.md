# Module 14 Overview: Refactoring

> *"Make the change easy (warning: this may be hard), then make the easy change."* — Kent Beck

## Refactoring vs Rewriting
- **Refactoring**: Behavior-preserving, tiny reversible steps, tests pass after every 2-minute edit.
- **Rewriting**: Throwing away code and starting over (loses edge-case bug fixes accumulated over years).

## Core Flow
1. **Build a Safety Net** (`01`, `04`): Automated tests / Characterization tests before touching structure.
2. **Apply Small Moves** (`02`, `03`, `06`): Extract, Inline, Replace Conditional, Refactor *towards* or *away from* patterns.
3. **Migrate Safely at Scale** (`05`, `07`): Branch by Abstraction + Hotspot-guided prioritization.
