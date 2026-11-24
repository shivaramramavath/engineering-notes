# Project 12: CQRS & Event-Sourced Banking Engine (`12-cqrs-event-sourced-bank`)

> Financial transaction engine built with `@nestjs/cqrs`, event sourcing, state stream replay, snapshots, and immutable append-only event logs.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-12-cqrs-event-sourced-bank` |
| **Framework** | `@nestjs/cqrs` |
| **Event Storage** | PostgreSQL JSONB Event Store + Snapshots Table |
| **Auditing** | 100% Cryptographically Traceable Audit Trail |
| **Complexity Level** | Enterprise Production Grade |

---

## 1. Bank Account Aggregate & Events

```typescript
// src/bank/aggregates/account.aggregate.ts
import { AggregateRoot } from '@nestjs/cqrs';
import { AccountOpenedEvent } from '../events/account-opened.event';
import { MoneyDepositedEvent } from '../events/money-deposited.event';
import { MoneyWithdrawnEvent } from '../events/money-withdrawn.event';

export class AccountAggregate extends AggregateRoot {
  private balance: number = 0;
  private isClosed: boolean = false;

  constructor(private readonly accountId: string) {
    super();
  }

  openAccount(initialDeposit: number) {
    this.apply(new AccountOpenedEvent(this.accountId, initialDeposit));
  }

  deposit(amount: number) {
    if (this.isClosed) throw new Error('Account is closed');
    this.apply(new MoneyDepositedEvent(this.accountId, amount));
  }

  withdraw(amount: number) {
    if (this.balance < amount) throw new Error('Insufficient funds');
    this.apply(new MoneyWithdrawnEvent(this.accountId, amount));
  }

  // Event Replay Handlers
  onAccountOpenedEvent(event: AccountOpenedEvent) {
    this.balance = event.initialDeposit;
  }
  onMoneyDepositedEvent(event: MoneyDepositedEvent) {
    this.balance += event.amount;
  }
  onMoneyWithdrawnEvent(event: MoneyWithdrawnEvent) {
    this.balance -= event.amount;
  }
}
```
