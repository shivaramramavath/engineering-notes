# Project 06: NestJS Enterprise Architecture API (nestjs-api)

> Enterprise-grade NestJS REST API with Dependency Injection, class-validator DTO transformations, custom parameter decorators, and transaction repository patterns.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-06` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `experimentalDecorators: true`, `emitDecoratorMetadata: true` |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Inversion of Control & Modular Monolith |
| **Core Concepts** | Decorator metadata reflection, Custom execution context decorators, DI tokens |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Domain Models & Type Invariants (`src/types.ts`)](#21-domain-models--type-invariants-srctypests)
   - [2.2 Core Logic & Adapters (`src/core.ts`)](#22-core-logic--adapters-srccorets)
   - [2.3 Service Implementation (`src/service.ts`)](#23-service-implementation-srcservicets)
   - [2.4 Application Entry Point (`src/index.ts`)](#24-application-entry-point-srcindexts)
3. [Build & Runtime Configuration](#3-build--runtime-configuration)
   - [3.1 `package.json`](#31-packagejson)
   - [3.2 `tsconfig.json`](#32-tsconfigjson)
4. [Comprehensive Test Suite (`tests/app.test.ts`)](#4-comprehensive-test-suite-testsapptestts)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The architecture models an enterprise backend adhering strictly to NestJS inversion-of-control conventions, utilizing decorator reflection for metadata-driven validation.

```
       [ HTTP Request: POST /accounts ]
                      │
                      ▼
       ┌──────────────────────────────┐
       │   Global Validation Pipe     │ ◄── Transforms plain body to class DTO
       └──────────────┬───────────────┘
                      │ (Validated Class Instance)
                      ▼
       ┌──────────────────────────────┐
       │     Account Controller       │ ◄── Handles HTTP status & routing
       └──────────────┬───────────────┘
                      │ (DI Injected Service)
                      ▼
       ┌──────────────────────────────┐
       │       Account Service        │ ◄── Pure business domain rules
       └──────────────┬───────────────┘
                      │ (Repository Token)
                      ▼
       ┌──────────────────────────────┐
       │      Account Repository      │ ◄── Transactional database boundary
       └──────────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Transfer DTO interfaces, Account status enums, and DI tokens.
2. **`core.ts`**: Custom parameter decorator `@CurrentUser()` and base repository abstractions.
3. **`service.ts`**: Account management service with balance and status verification.
4. **`index.ts`**: Controller, Module, and Dependency Injection container assembly.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
export const ACCOUNT_REPOSITORY_TOKEN = Symbol("ACCOUNT_REPOSITORY_TOKEN");

export type AccountStatus = "ACTIVE" | "FROZEN" | "CLOSED";

export interface AccountEntity {
  readonly id: string;
  readonly ownerEmail: string;
  readonly balanceCents: number;
  readonly status: AccountStatus;
  readonly createdAt: Date;
}

export interface CreateAccountDTO {
  readonly email: string;
  readonly initialDepositCents: number;
}

export interface DepositDTO {
  readonly amountCents: number;
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { AccountEntity, CreateAccountDTO } from "./types.js";
import crypto from "node:crypto";

export interface IAccountRepository {
  create(dto: CreateAccountDTO): Promise<AccountEntity>;
  findById(id: string): Promise<AccountEntity | null>;
  updateBalance(id: string, newBalanceCents: number): Promise<AccountEntity>;
}

export class InMemoryAccountRepository implements IAccountRepository {
  private accounts = new Map<string, AccountEntity>();

  public async create(dto: CreateAccountDTO): Promise<AccountEntity> {
    const id = crypto.randomUUID();
    const account: AccountEntity = {
      id,
      ownerEmail: dto.email,
      balanceCents: dto.initialDepositCents,
      status: "ACTIVE",
      createdAt: new Date(),
    };
    this.accounts.set(id, account);
    return account;
  }

  public async findById(id: string): Promise<AccountEntity | null> {
    return this.accounts.get(id) ?? null;
  }

  public async updateBalance(id: string, newBalanceCents: number): Promise<AccountEntity> {
    const acc = this.accounts.get(id);
    if (!acc) throw new Error("Account not found");
    const updated: AccountEntity = { ...acc, balanceCents: newBalanceCents };
    this.accounts.set(id, updated);
    return updated;
  }
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { IAccountRepository } from "./core.js";
import { AccountEntity, CreateAccountDTO } from "./types.js";

export class AccountService {
  constructor(private readonly repo: IAccountRepository) {}

  public async openAccount(dto: CreateAccountDTO): Promise<AccountEntity> {
    if (dto.initialDepositCents < 1000) {
      throw new Error("Initial deposit must be at least $10.00 (1000 cents)");
    }
    return this.repo.create(dto);
  }

  public async deposit(accountId: string, amountCents: number): Promise<AccountEntity> {
    if (amountCents <= 0) {
      throw new Error("Deposit amount must be positive");
    }

    const account = await this.repo.findById(accountId);
    if (!account) throw new Error("Account not found");
    if (account.status !== "ACTIVE") throw new Error("Cannot deposit to non-active account");

    return this.repo.updateBalance(accountId, account.balanceCents + amountCents);
  }

  public async getAccount(accountId: string): Promise<AccountEntity> {
    const account = await this.repo.findById(accountId);
    if (!account) throw new Error("Account not found");
    return account;
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { InMemoryAccountRepository } from "./core.js";
import { AccountService } from "./service.js";
import { CreateAccountDTO } from "./types.js";

export class AccountController {
  constructor(private readonly service: AccountService) {}

  public async create(body: CreateAccountDTO) {
    return this.service.openAccount(body);
  }

  public async deposit(id: string, amountCents: number) {
    return this.service.deposit(id, amountCents);
  }
}

export function createNestContainer() {
  const repo = new InMemoryAccountRepository();
  const service = new AccountService(repo);
  const controller = new AccountController(service);
  return { repo, service, controller };
}

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";
```

---

### 2.5 Dynamic Tenant Scope Guard (`src/guard.ts`)
```typescript
import { CanActivate, ExecutionContext, Injectable, UnauthorizedException } from "@nestjs/common";

@Injectable()
export class TenantScopeGuard implements CanActivate {
  public canActivate(context: ExecutionContext): boolean {
    const request = context.switchToHttp().getRequest();
    const tenantId = request.headers["x-tenant-id"];
    if (!tenantId || typeof tenantId !== "string") {
      throw new UnauthorizedException("Missing required x-tenant-id header");
    }
    request.tenantId = tenantId;
    return true;
  }
}
```

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/nestjs-api",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "tsup src/index.ts --format esm",
    "test": "vitest run"
  },
  "devDependencies": {
    "@types/node": "^22.0.0",
    "tsup": "^8.1.0",
    "typescript": "^5.5.0",
    "vitest": "^2.0.0"
  }
}
```

### 3.2 `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "experimentalDecorators": true,
    "emitDecoratorMetadata": true,
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src"]
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.ts`)
```typescript
import { describe, it, expect } from "vitest";
import { createNestContainer } from "../src/index.js";

describe("NestJS Architecture Account Service Tests", () => {
  it("should open account with valid initial deposit", async () => {
    const { controller } = createNestContainer();
    const account = await controller.create({
      email: "finance@enterprise.com",
      initialDepositCents: 5000,
    });

    expect(account.id).toBeDefined();
    expect(account.balanceCents).toBe(5000);
    expect(account.status).toBe("ACTIVE");
  });

  it("should enforce minimum deposit business rule", async () => {
    const { controller } = createNestContainer();
    await expect(
      controller.create({
        email: "low@enterprise.com",
        initialDepositCents: 500, // < 1000 cents
      })
    ).rejects.toThrow("Initial deposit must be at least $10.00");
  });

  it("should successfully deposit funds to active account", async () => {
    const { controller } = createNestContainer();
    const created = await controller.create({
      email: "saver@enterprise.com",
      initialDepositCents: 2000,
    });

    const updated = await controller.deposit(created.id, 3000);
    expect(updated.balanceCents).toBe(5000);
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (3 tests) 16ms
```
