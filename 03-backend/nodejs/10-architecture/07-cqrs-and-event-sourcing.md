# CQRS & Event Sourcing in Node.js

> Command Query Responsibility Segregation (CQRS) and Event Sourcing: Auditable append-only event streams and read-optimized query projections.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Patterns** | CQRS, Event Store, Read Model Projections |

---

## 1. What is CQRS?

In standard architectures, the same data model handles both reads and writes.
**CQRS segregates them into two distinct pipelines**:
- **Commands (Mutations)**: Express intent to change state (`TransferFundsCommand`). They validate business rules and emit events.
- **Queries (Reads)**: Retrieve pre-aggregated, read-optimized views (`GetAccountBalanceQuery`). They perform zero business validation and bypass complex relational joins.

---

## 2. What is Event Sourcing?

Instead of storing only the *current state* of an entity in a mutable SQL row, **Event Sourcing stores every state change as an immutable sequence of events**:

```
State = Events.reduce(applyEvent, InitialState)
```

### Event Sourced Bank Account Example
```typescript
interface AccountEvent {
  type: 'AccountOpened' | 'MoneyDeposited' | 'MoneyWithdrawn';
  accountId: string;
  amount?: number;
  timestamp: Date;
}

export class BankAccountAggregate {
  private id: string;
  private balance: number = 0;

  // Replay past events to reconstruct current state
  static fromHistory(events: AccountEvent[]): BankAccountAggregate {
    const account = new BankAccountAggregate();
    for (const event of events) {
      account.apply(event);
    }
    return account;
  }

  private apply(event: AccountEvent) {
    switch (event.type) {
      case 'AccountOpened':
        this.id = event.accountId;
        this.balance = 0;
        break;
      case 'MoneyDeposited':
        this.balance += event.amount!;
        break;
      case 'MoneyWithdrawn':
        this.balance -= event.amount!;
        break;
    }
  }

  // Business command execution
  withdraw(amount: number): AccountEvent {
    if (this.balance < amount) {
      throw new Error('Insufficient funds');
    }
    const event: AccountEvent = {
      type: 'MoneyWithdrawn',
      accountId: this.id,
      amount,
      timestamp: new Date(),
    };
    this.apply(event);
    return event;
  }
}
```
Complete auditability: you can reconstruct account state at any timestamp in history.
