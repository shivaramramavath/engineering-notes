# NestJS TypeScript Architecture: Decorators, DTOs, and Metadata Reflection

> Comprehensive guide to NestJS typing: metadata reflection, custom param decorators, ExecutionContext typing, Class-Validator vs Zod DTOs, and Exception Filters.

| Metadata | Details |
| :--- | :--- |
| **Category** | Node.js Backend Typing |
| **Difficulty** | Advanced |
| **Prerequisites** | [05-classes/02-decorators.md](../05-classes/02-decorators.md), [17-design-patterns/05-dependency-injection.md](../17-design-patterns/05-dependency-injection.md) |
| **TypeScript Baseline** | TS 5.0+, NestJS v10/v11 |
| **Tested tsconfig** | `"experimentalDecorators": true, "emitDecoratorMetadata": true` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

**NestJS** is an enterprise-grade TypeScript backend framework that adopts modular architectural conventions inspired by Angular and Spring Boot: controllers, providers, modules, guards, pipes, and interceptors.

Unlike Express or Fastify, NestJS relies heavily on **TypeScript Decorator Metadata Reflection**:
- Classes are annotated with `@Injectable()`, `@Controller()`, and `@Get()`.
- The TypeScript compiler emits type metadata (`design:paramtypes`) into the compiled JavaScript.
- The NestJS IoC container reads this metadata at runtime, automatically wiring dependencies without manual registration code.

Understanding the typing mechanics of NestJS is essential to prevent common runtime traps:
- Incomplete metadata emit due to missing tsconfig flags (`emitDecoratorMetadata`).
- Interface types being used as DI tokens (which fail because interfaces are erased at compile time).
- ExecutionContext narrowing across HTTP, WebSockets, and Microservices transports.

```
+-----------------------------------------------------------+
|                   NestJS Reflection Engine                |
|                                                           |
|  @Controller('users')                                     |
|  export class UserController {                            |
|    constructor(private userService: UserService) {}       |
|  }                                                        |
|                                                           |
|  TypeScript Compiler emits:                               |
|  __metadata("design:paramtypes", [UserService])           |
|                                                           |
|  NestJS IoC Container reads paramtypes at startup,         |
|  instantiates UserService, and injects it!                |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Mandatory `tsconfig.json` Configuration
NestJS requires legacy experimental decorators and decorator metadata:
```json
{
  "compilerOptions": {
    "experimentalDecorators": true,
    "emitDecoratorMetadata": true,
    "target": "ES2021"
  }
}
```

### 2. Classes as DTOs (Data Transfer Objects)
In NestJS, DTOs are declared as **Classes**, not interfaces:
```typescript
import { IsString, IsEmail, MinLength } from 'class-validator';

export class CreateUserDto {
  @IsString()
  @MinLength(2)
  name!: string;

  @IsEmail()
  email!: string;
}
```
Why classes? Because classes exist at runtime as constructor functions, allowing `ValidationPipe` to inspect metadata and run validation!

### 3. ExecutionContext Narrowing
`ExecutionContext` is a polymorphic interface used across HTTP, WebSockets, and RPC:
```typescript
import { ExecutionContext, CanActivate } from '@nestjs/common';

export class AuthGuard implements CanActivate {
  canActivate(context: ExecutionContext): boolean {
    const http = context.switchToHttp();
    const req = http.getRequest<Request>();
    return Boolean(req.user);
  }
}
```

---

## 3. How It Works Internally

1. When a controller method has a parameter `@Body() dto: CreateUserDto`, NestJS's `ValidationPipe` reads `Reflect.getMetadata('design:paramtypes', target, key)`.
2. It obtains the `CreateUserDto` class constructor.
3. It converts the plain JSON body into an instance of `CreateUserDto` via `class-transformer`.
4. It executes `validate()` from `class-validator`. If validation passes, the typed class instance is passed to your controller method.

---

## 4. Syntax & API Reference

### Custom Parameter Decorator
```typescript
import { createParamDecorator, ExecutionContext } from '@nestjs/common';

export const CurrentUser = createParamDecorator(
  (data: unknown, ctx: ExecutionContext) => {
    const request = ctx.switchToHttp().getRequest();
    return request.user;
  }
);
```

---

## 5. Practical Examples

### Example 1: Basic Typed NestJS Controller
```typescript
import { Controller, Get, Post, Body, Param } from '@nestjs/common';

export class CreateItemDto {
  name!: string;
}

@Controller('items')
export class ItemsController {
  @Get(':id')
  findOne(@Param('id') id: string) {
    return { id, name: 'Item Name' };
  }

  @Post()
  create(@Body() dto: CreateItemDto) {
    return { created: true, name: dto.name };
  }
}
```

### Example 2: Real-World Global Exception Filter with RFC 7807 Details
```typescript
import { ExceptionFilter, Catch, ArgumentsHost, HttpException, HttpStatus } from '@nestjs/common';
import { Response, Request } from 'express';

@Catch()
export class AllExceptionsFilter implements ExceptionFilter {
  catch(exception: unknown, host: ArgumentsHost) {
    const ctx = host.switchToHttp();
    const response = ctx.getResponse<Response>();
    const request = ctx.getRequest<Request>();

    const status =
      exception instanceof HttpException
        ? exception.getStatus()
        : HttpStatus.INTERNAL_SERVER_ERROR;

    const message =
      exception instanceof HttpException
        ? exception.getResponse()
        : 'Internal server error';

    response.status(status).json({
      type: 'about:blank',
      title: 'HTTP Error',
      status,
      detail: message,
      instance: request.url,
      timestamp: new Date().toISOString()
    });
  }
}
```

### Example 3: Edge Case - Zod DTO Validation Pipe in NestJS
```typescript
import { PipeTransform, ArgumentMetadata, BadRequestException } from '@nestjs/common';
import { ZodSchema } from 'zod';

export class ZodValidationPipe implements PipeTransform {
  constructor(private schema: ZodSchema) {}

  transform(value: unknown, metadata: ArgumentMetadata) {
    const parseResult = this.schema.safeParse(value);
    if (!parseResult.success) {
      throw new BadRequestException(parseResult.error.flatten().fieldErrors);
    }
    return parseResult.data;
  }
}
```

### Example 4: Failure Case - Using Interface as Injection Token
```typescript
// ARCHITECTURAL VIOLATION:
// interface IUserService {}
// @Controller()
// class BrokenController {
//   constructor(private s: IUserService) {} // Fails at runtime!
// }
// NestJS Error: Nest can't resolve dependencies of BrokenController (?).
// Interfaces disappear at compile time; Nest cannot read metadata!
```

### Example 5: Idiomatic Custom Param Decorator with User Extraction
```typescript
import { createParamDecorator, ExecutionContext } from '@nestjs/common';

export interface AuthenticatedUser { id: string; email: string; }

export const AuthUser = createParamDecorator(
  (data: keyof AuthenticatedUser | undefined, ctx: ExecutionContext) => {
    const req = ctx.switchToHttp().getRequest();
    const user = req.user as AuthenticatedUser;
    return data ? user?.[data] : user;
  }
);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Interface as Injection Token**:
   TypeScript interfaces do NOT emit runtime JavaScript. Injecting `constructor(private repo: IUserRepository)` fails with `Nest can't resolve dependencies`. Use abstract classes (`abstract class IUserRepository {}`) or string/symbol tokens with `@Inject('USER_REPO')`.
2. **Circular Dependency Between Services**:
   When Service A injects Service B, and Service B injects Service A, startup fails with `Nest can't resolve dependencies`. Use `forwardRef(() => ServiceB)` in both constructors.
3. **`class-transformer` Stripping Defaults**:
   When using `ValidationPipe({ whitelist: true })`, any property sent in the HTTP request that lacks a decorator in the DTO class is stripped. If a field has no `@IsString()` or `@Allow()`, NestJS drops it silently!
4. **Fastify vs Express Adapter Typing**:
   NestJS supports both Express and Fastify. If your code calls `ctx.getRequest<Request>()` using Express types, running under Fastify (`FastifyAdapter`) causes type mismatches.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Interface as DI Token** | `constructor(s: IService)` | `constructor(@Inject('SERVICE') s: IService)` | `Nest can't resolve dependencies` error |
| **Missing `emitDecoratorMetadata`** | `"emitDecoratorMetadata": false` | Set `true` in `tsconfig.json` | DI container fails to discover dependencies |
| **DTO as Interface** | `interface CreateDto {}` | `class CreateDto {}` | Class-validator cannot inspect metadata |
| **Missing `whitelist: true`** | No ValidationPipe options | `ValidationPipe({ whitelist: true })` | Allows mass assignment vulnerabilities |
| **Un-awaited Interceptors** | Forgetting `next.handle()` returns Observable | Use RxJS operators (`map`, `tap`) | Request hangs or returns empty response |

---

## 8. Performance & Type-Checker Complexity

- **Cold Startup Overhead**: Reading decorator metadata and resolving dependency trees in large NestJS apps (50+ modules) takes ~500ms during application bootstrap. Runtime request execution runs at native Express/Fastify speeds.
- **Type Checking**: Decorator types evaluate cleanly in TypeScript.

---

## 9. Compiler Internals

When `"emitDecoratorMetadata": true` is enabled:
For any class with a decorator, the TypeScript compiler emits:
`__metadata("design:paramtypes", [Type1, Type2])`
into the generated JavaScript constructor function.

---

## 10. Best Practices & Design Guidelines

1. **Use Classes for DTOs**: Required for runtime validation reflection.
2. **Use Abstract Classes as Interfaces**: Gives both compile-time typing and runtime injection tokens.
3. **Enable `ValidationPipe({ whitelist: true, forbidNonWhitelisted: true })`**: Reject unknown payload keys.
4. **Centralize Custom Decorators**: Create clear param decorators (`@CurrentUser()`).

---

## 11. Real-World Case Study

### Role-Based Authorization Guard in NestJS
Enforcing RBAC using custom SetMetadata decorators and Reflector:

```typescript
import { Injectable, CanActivate, ExecutionContext, SetMetadata } from '@nestjs/common';
import { Reflector } from '@nestjs/core';

export const Roles = (...roles: string[]) => SetMetadata('roles', roles);

@Injectable()
export class RolesGuard implements CanActivate {
  constructor(private reflector: Reflector) {}

  canActivate(context: ExecutionContext): boolean {
    const requiredRoles = this.reflector.get<string[]>('roles', context.getHandler());
    if (!requiredRoles) return true; // No roles required

    const request = context.switchToHttp().getRequest();
    const user = request.user;
    if (!user || !user.role) return false;

    return requiredRoles.includes(user.role);
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. Why does NestJS require classes instead of TypeScript interfaces for DTOs?</summary>

TypeScript interfaces are completely erased during compilation and do not exist at runtime. Classes exist as JavaScript constructor functions at runtime. NestJS's `ValidationPipe` and `class-transformer` inspect the class constructor via metadata reflection to validate input data and instantiate objects.
</details>

<details>
<summary>2. Why does injecting an interface in a constructor fail in NestJS without `@Inject()`?</summary>

Because interfaces do not survive compilation into JavaScript, the compiler emits `undefined` in `design:paramtypes` for that parameter. At startup, NestJS cannot determine what dependency to instantiate. You must use `@Inject('TOKEN_NAME')` with a string/symbol token or inject an abstract class.
</details>

<details>
<summary>3. What is `ExecutionContext` in NestJS?</summary>

`ExecutionContext` extends `ArgumentsHost`. It provides information about the current execution pipeline and allows extracting request/response objects across different transport layers (HTTP via `.switchToHttp()`, WebSockets via `.switchToWs()`, and Microservices via `.switchToRpc()`).
</details>

<details>
<summary>4. What do `experimentalDecorators` and `emitDecoratorMetadata` do in `tsconfig.json`?</summary>

`experimentalDecorators` enables Stage 2 decorator syntax. `emitDecoratorMetadata` instructs TypeScript to emit type metadata (`design:type`, `design:paramtypes`, `design:returntype`) using `Reflect.metadata`, which NestJS uses for automated dependency injection.
</details>

<details>
<summary>5. What does the `whitelist: true` option in `ValidationPipe` do?</summary>

It automatically strips any property sent in the incoming request body that does not have an explicit validation decorator in the DTO class, preventing mass-assignment vulnerabilities.
</details>

<details>
<summary>6. How do you resolve circular dependencies between two NestJS services?</summary>

Use `forwardRef()`:
`constructor(@Inject(forwardRef(() => ServiceB)) private b: ServiceB)` in both service constructors.
</details>

<details>
<summary>7. What is an Exception Filter in NestJS?</summary>

An Exception Filter intercepts unhandled exceptions thrown across the application, translating them into appropriate HTTP status codes and structured response bodies (e.g. RFC 7807 Problem Details).
</details>

<details>
<summary>8. Can you use Zod instead of `class-validator` in NestJS?</summary>

Yes, by creating a custom `ZodValidationPipe` that implements `PipeTransform` and invokes `schema.parse(value)` in the `transform` method.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic NestJS DTO Class</summary>

**Problem:** Create a `CreateProductDto` class with `title: string` and `price: number`.

**Solution:**
```typescript
export class CreateProductDto {
  title!: string;
  price!: number;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Custom Controller Route</summary>

**Problem:** Write a NestJS controller with a `@Get('health')` method returning `{ status: 'ok' }`.

**Solution:**
```typescript
import { Controller, Get } from '@nestjs/common';

@Controller()
export class HealthController {
  @Get('health')
  check() {
    return { status: 'ok' };
  }
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Abstract Class DI Token</summary>

**Problem:** Create an abstract class `LoggerPort` and a service that injects it.

**Solution:**
```typescript
import { Injectable } from '@nestjs/common';

export abstract class LoggerPort {
  abstract log(message: string): void;
}

@Injectable()
export class AppService {
  constructor(private logger: LoggerPort) {}
  doWork() { this.logger.log('Working...'); }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Header Param Decorator</summary>

**Problem:** Create a custom param decorator `@ClientIp()` extracting `req.ip`.

**Solution:**
```typescript
import { createParamDecorator, ExecutionContext } from '@nestjs/common';

export const ClientIp = createParamDecorator(
  (data: unknown, ctx: ExecutionContext) => {
    const req = ctx.switchToHttp().getRequest();
    return req.ip;
  }
);
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Custom Pipe with Zod</summary>

**Problem:** Implement a generic `ZodValidationPipe` that accepts any Zod schema and transforms incoming query parameters.

**Solution:**
```typescript
import { PipeTransform, ArgumentMetadata, BadRequestException } from '@nestjs/common';
import { ZodSchema } from 'zod';

export class ZodPipe<T> implements PipeTransform {
  constructor(private schema: ZodSchema<T>) {}

  transform(value: unknown, metadata: ArgumentMetadata): T {
    const result = this.schema.safeParse(value);
    if (!result.success) {
      throw new BadRequestException(result.error.errors);
    }
    return result.data;
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): ExecutionContext Transport Checker</summary>

**Problem:** Write a guard that verifies the incoming request is strictly an HTTP request, throwing an error if invoked in a WebSocket or RPC context.

**Solution:**
```typescript
import { CanActivate, ExecutionContext, Injectable, BadRequestException } from '@nestjs/common';

@Injectable()
export class HttpOnlyGuard implements CanActivate {
  canActivate(context: ExecutionContext): boolean {
    if (context.getType() !== 'http') {
      throw new BadRequestException('Endpoint only accessible via HTTP');
    }
    return true;
  }
}
```
</details>
