# Module 13 Overview: Anti-Patterns & Code Smells

- **Code Smell** (Martin Fowler / Kent Beck): A surface-level symptom (`Long Method`, `Primitive Obsession`, `Feature Envy`) suggesting something *might* be wrong structurally.
- **Anti-Pattern** (Brown et al., 1998): A commonly reinvented, seductive "solution" (`God Object`, `Singleton Abuse`, `Big Ball of Mud`) that creates far more problems than it solves.

## Diagnostic Triage

1. **Architecture-Level**: Big Ball of Mud, Golden Hammer, Cargo Cult, Leaky Abstractions.
2. **Class/Module-Level**: God Object, Shotgun Surgery, Divergent Change, Deep Inheritance, Singleton/Service Locator Abuse.
3. **Method/Expression-Level**: Long Method, Primitive Obsession, Feature Envy, Magic Numbers, Boolean Flags.
