---
title: "Layered Architecture in NestJS"
description: "Designing classical N-tier enterprise systems: Presentation, Application/Business Logic, Domain, and Infrastructure layers in NestJS"
tags: [nestjs, architecture, layered-architecture, n-tier, separation-of-concerns]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Layered Architecture in NestJS

## Conceptual Overview
The Layered (N-Tier) Architecture is the most common structural paradigm in enterprise backend development. It organizes a codebase into horizontal tiers where each layer has a distinct responsibility and dependencies flow strictly downward:
- **Presentation Layer (Controllers, Gateways, DTOs)**: Handles HTTP/WebSocket/gRPC protocols, deserialization, and authentication.
- **Application Layer (Services, Use Cases)**: Coordinates business workflows, manages transactions, and orchestrates domain models.
- **Domain Layer (Entities, Value Objects)**: Contains core enterprise business rules, validations, and invariants.
- **Infrastructure Layer (Repositories, Adapters, Third-Party Clients)**: Handles database persistence (TypeORM, Prisma), external APIs (Stripe, Twilio), and cache interactions.

```
+-------------------------------------------------------------------------------+
|                       Classic Layered Architecture                            |
|                                                                               |
|  [ Presentation Layer ]  --> Controllers, DTOs, Guards, Interceptors          |
|            |                                                                  |
|            v (Calls)                                                          |
|  [ Application Layer ]   --> Application Services, Use Cases, Command Handlers|
|            |                                                                  |
|            v (Operates on)                                                    |
|  [ Domain Layer ]        --> Entities, Value Objects, Domain Exceptions       |
|            |                                                                  |
|            v (Persisted by)                                                   |
|  [ Infrastructure Layer ]--> Repositories, Database Drivers, External APIs   |
+-------------------------------------------------------------------------------+
```

---

## Core Mechanisms & Architecture

### 1. The Strict Layering Rule
In a strict layered architecture, Layer $N$ may only interact with Layer $N-1$:
- Controllers must **never** access database drivers, run raw SQL, or manipulate TypeORM repositories directly.
- Domain entities must not depend on HTTP requests or Express primitives.
- Infrastructure dependencies must be injected into application services via abstract interfaces.

---

## Implementation Details & NestJS APIs

### 1. Presentation Layer (Controller)

```typescript
// src/users/presentation/users.controller.ts
import { Controller, Post, Body, HttpCode, HttpStatus } from '@nestjs/common';
import { CreateUserDto } from './dtos/create-user.dto';
import { UserResponseDto } from './dtos/user-response.dto';
import { CreateUserUseCase } from '../application/create-user.use-case';

@Controller('users')
export class UsersController {
  constructor(private readonly createUserUseCase: CreateUserUseCase) {}

  @Post()
  @HttpCode(HttpStatus.CREATED)
  async create(@Body() dto: CreateUserDto): Promise<UserResponseDto> {
    const user = await this.createUserUseCase.execute(dto.email, dto.password);
    return UserResponseDto.fromEntity(user);
  }
}
```

### 2. Application Layer (Use Case)

```typescript
// src/users/application/create-user.use-case.ts
import { Injectable, ConflictException, Inject } from '@nestjs/common';
import { USERS_REPOSITORY, IUsersRepository } from '../domain/users-repository.interface';
import { User } from '../domain/user.entity';
import { HashService } from '../../common/crypto/hash.service';

@Injectable()
export class CreateUserUseCase {
  constructor(
    @Inject(USERS_REPOSITORY)
    private readonly usersRepository: IUsersRepository,
    private readonly hashService: HashService,
  ) {}

  async execute(email: string, plainPass: string): Promise<User> {
    const existing = await this.usersRepository.findByEmail(email);
    if (existing) {
      throw new ConflictException(`User with email ${email} already exists`);
    }

    const passwordHash = await this.hashService.hash(plainPass);
    const user = new User(email, passwordHash);

    return this.usersRepository.save(user);
  }
}
```

---

## Key Takeaways & Cheat Sheet

- Keep controllers thin: routing, DTO validation, and response formatting only.
- Encapsulate business operations in application use-case services.
- Never inject database repositories directly into controllers.
- Invert infrastructure dependencies using TypeScript interfaces and NestJS custom providers.

---

## Cross-References
- [02-modular-monolith.md](./02-modular-monolith.md) - Modular monolith architecture
- [03-clean-and-hexagonal-architecture.md](./03-clean-and-hexagonal-architecture.md) - Hexagonal architecture
