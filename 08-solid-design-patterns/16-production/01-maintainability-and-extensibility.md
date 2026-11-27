# Maintainability & Extensibility

| Field | Value |
| :--- | :--- |
| **Category** | Production Design |

> **TL;DR**: The most maintainable code is **easy to delete**. Optimize for **low cognitive load** (clear names, shallow nesting, vertical feature slices) rather than clever abstractions.

## Three Laws of Maintainable Code
1. **Design for Deletion**: Modular code with clean interfaces can be deleted or replaced in 1 PR without untangling 20 files.
2. **Local Reasoning**: You should understand what a function does by reading that file—without jumping through 6 levels of inheritance or global state.
3. **Rule of Three**: Don't build a plugin framework for 1 use case; wait until 3 real variations emerge.
