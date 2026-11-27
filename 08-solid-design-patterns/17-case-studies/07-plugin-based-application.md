# Case Study 07: Plugin-Based Application (Microkernel)

**Patterns Used**: **Microkernel**, **Plugin & Registry**, **Facade** (stable `PluginContext` API exposed to plugins), **Mediator / Event Bus** (plugins communicate without importing each other).

## Key Design Insight
Pass a narrow `PluginContext` Facade to each plugin's `activate(ctx)` hook rather than exposing internal core classes—enforcing versioned contracts and clean `deactivate()` cleanup.
