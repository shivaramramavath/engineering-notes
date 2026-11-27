# Case Study 11: Workflow Engine

**Patterns Used**: **State Machine** (Step statuses: `PENDING`, `RUNNING`, `COMPLETED`, `FAILED`), **Command** (Step execution + compensation), **Template Method / Hooks** (`beforeStep`, `onError`), **Saga** (reverse compensation on failure), **Retry with Backoff**.

## Key Design Insight
Persist every step transition to durable storage before advancing, and require every step handler to be idempotent so worker crashes can resume from the last completed step without duplicating work.
