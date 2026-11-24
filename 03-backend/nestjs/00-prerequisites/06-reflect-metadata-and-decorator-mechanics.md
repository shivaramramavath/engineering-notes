# Module 00: Reflect Metadata & Decorator Mechanics in NestJS

> Deep architectural exploration of TypeScript decorators, metadata reflection via `reflect-metadata`, design-time type emission (`design:paramtypes`), and how the NestJS Dependency Injection container reads constructor metadata.

| Metadata | Details |
| :--- | :--- |
| **NestJS Version** | NestJS 11+ |
| **Target Runtime** | Node.js >= 20.0.0 (Node.js 22 LTS recommended) |
| **Core Dependency** | `reflect-metadata` |
| **TypeScript Flags** | `"experimentalDecorators": true`, `"emitDecoratorMetadata": true` |

---

## 1. Concept Overview (Why before How)

JavaScript is dynamically typed at runtime: when TypeScript compiles to JavaScript, all type annotations, interfaces, and type aliases are erased. Without a mechanism to persist type information into runtime bytecode, an Inversion of Control (IoC) container would have no way of knowing what class instances to pass to a constructor:

```typescript
// How does NestJS know to inject an instance of UsersService here?
@Controller('users')
export class UsersController {
  constructor(private readonly usersService: UsersService) {}
}
```

The answer lies in **Metadata Reflection**:
1. When `@Controller()` or `@Injectable()` is applied to a class, TypeScript inspects constructor parameter types.
2. If `"emitDecoratorMetadata": true` is enabled in `tsconfig.json`, TypeScript automatically emits a hidden metadata property named **`design:paramtypes`** containing an array of class references (`[UsersService]`).
3. The NestJS IoC container reads this array at startup using `Reflect.getMetadata('design:paramtypes', UsersController)` and automatically instantiates and injects the required dependencies.

---

## 2. Architecture & Data Flow Diagram

```
+----------------------------------------------------------------------------------------------------+
|                                TypeScript Decorator & Metadata Flow                                |
|                                                                                                    |
|  [ Source TS: constructor(private service: UsersService) ]                                         |
|                                │                                                                   |
|                                ▼ TypeScript Compilation (emitDecoratorMetadata: true)              |
|  +─────────────────────────────────────────────────────────────+                                   |
|  | Emit JS:                                                    |                                   |
|  | __decorate([ Controller() ], UsersController);              |                                   |
|  | __metadata("design:paramtypes", [ UsersService ]);          |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │                                                                   |
|                                ▼ Runtime Boot (NestFactory.create)                                 |
|  +─────────────────────────────────────────────────────────────+                                   |
|  | NestJS Container / ModuleCompiler                           |                                   |
|  |                                                             |                                   |
|  | const types = Reflect.getMetadata('design:paramtypes', Ctrl)|                                   |
|  | // types === [ [class UsersService] ]                       |                                   |
|  |                                                             |                                   |
|  | const instance = container.get(types[0]);                   |                                   |
|  | const controller = new UsersController(instance);           |                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
+----------------------------------------------------------------------------------------------------+
```

---

## 3. Core Mechanics & Deep Dive

### 3.1 The Three Magic Keys Emitted by TypeScript
When a class member has a decorator, TypeScript emits up to three standard metadata keys:
1. **`design:type`**: The type of a property or field (e.g. `String`, `Number`, `Boolean`, or custom class).
2. **`design:paramtypes`**: An array of types for the parameters of a function or class constructor.
3. **`design:returntype`**: The return type of a method.

### 3.2 Decorator Execution Order
Decorators execute in a strict, counter-intuitive evaluation order:
1. **Parameter Decorators**, followed by **Method / Property / Accessor Decorators** are applied for each instance member.
2. Parameter, Method, Property, Accessor decorators for each **static** member.
3. Parameter decorators for the **constructor**.
4. **Class Decorators** are applied last!

When multiple decorators decorate a single declaration, they evaluate from **top to bottom (factory creation)**, but execute from **bottom to top (reverse composition)**:
```typescript
@DecA()
@DecB()
class Example {}
// Evaluation: DecA() -> DecB()
// Execution: DecB implementation runs -> DecA implementation runs
```

---

## 4. Complete Code Examples (5 Levels)

### 4.1 Basic Example: Manual Reflection API Usage
```typescript
import 'reflect-metadata';

class DatabaseConnection {}

// 1. Manually define metadata on a target class
Reflect.defineMetadata('custom:role', 'ADMIN', DatabaseConnection);

// 2. Read metadata back
const role = Reflect.getMetadata('custom:role', DatabaseConnection);
console.log('Role metadata:', role); // 'ADMIN'
```

### 4.2 Real-World Production Example: Custom Auth Roles Decorator
```typescript
import { SetMetadata, createParamDecorator, ExecutionContext } from '@nestjs/common';
import { Reflector } from '@nestjs/core';

export const ROLES_KEY = 'roles';
export const Roles = (...roles: string[]) => SetMetadata(ROLES_KEY, roles);

// Custom parameter decorator reading claims from request
export const CurrentUser = createParamDecorator(
  (data: string | undefined, ctx: ExecutionContext) => {
    const request = ctx.switchToHttp().getRequest();
    const user = request.user;
    return data ? user?.[data] : user;
  }
);
```

### 4.3 Edge Case: Interface Erasure Trap
TypeScript interfaces **do not exist at runtime**. If you attempt to inject an interface without a token, `design:paramtypes` emits `Object`, causing DI resolution failure:
```typescript
// WRONG: Injects Object, Nest cannot determine what provider to instantiate!
// constructor(private repo: IUserRepository) {}

// CORRECT: Use an explicit string or symbol injection token with @Inject()
export const USER_REPOSITORY_TOKEN = Symbol('IUserRepository');

export class UsersService {
  constructor(
    @Inject(USER_REPOSITORY_TOKEN) private readonly repo: IUserRepository
  ) {}
}
```

### 4.4 Failure Case & Debugging: Missing `emitDecoratorMetadata`
```typescript
// In tsconfig.json:
// "emitDecoratorMetadata": false

// At runtime:
// Error: Nest can't resolve dependencies of the UsersController (?). Please make sure that the argument dependency at index [0] is available in the UsersModule context.
// Cause: Without emitDecoratorMetadata, design:paramtypes is undefined!
```

### 4.5 Idiomatic Production Recipe: Decorator Composition with `applyDecorators`
```typescript
import { applyDecorators, SetMetadata, UseGuards } from '@nestjs/common';
import { ApiBearerAuth, ApiUnauthorizedResponse } from '@nestjs/swagger';
import { JwtAuthGuard } from './jwt-auth.guard.js';
import { RolesGuard } from './roles.guard.js';

export function Auth(...roles: string[]) {
  return applyDecorators(
    SetMetadata('roles', roles),
    UseGuards(JwtAuthGuard, RolesGuard),
    ApiBearerAuth(),
    ApiUnauthorizedResponse({ description: 'Unauthorized access' })
  );
}

// Clean usage on controllers:
@Controller('billing')
export class BillingController {
  @Get('invoices')
  @Auth('ADMIN', 'ACCOUNTANT')
  getInvoices() {
    return [];
  }
}
```

---

## 5. Framework Internals (Under the Hood)
Inside `@nestjs/core`, the `Injector` class uses `Reflector` and native `Reflect`:
```typescript
// Conceptual implementation of NestJS parameter type inspection:
export class Injector {
  resolveConstructorParams(target: Function) {
    const paramTypes = Reflect.getMetadata('design:paramtypes', target) || [];
    return paramTypes.map((paramType) => {
      // Look up instance in IoC container registry
      return this.container.get(paramType);
    });
  }
}
```
If a parameter decorator like `@Inject('CUSTOM_TOKEN')` is used, Nest overrides the `paramTypes` lookup with the custom token stored under `self:paramtypes`.

---

## 6. Common Pitfalls & Mistakes Table

| Mistake | Why It Happens | Idiomatic Fix |
| :--- | :--- | :--- |
| **1. Injecting an Interface directly** | TypeScript interfaces are erased during compilation (`design:paramtypes` emits `Object`). | Use `@Inject('TOKEN')` or `@Inject(MySymbol)`. |
| **2. Missing `reflect-metadata` import** | In raw setups without Nest CLI, forgetting `import 'reflect-metadata'`. | Must be imported at the very top of `main.ts`. |
| **3. `emitDecoratorMetadata: false`** | TS compiler does not emit type information. | Ensure `"emitDecoratorMetadata": true` in `tsconfig.json`. |
| **4. Circular Reference Type Erasure** | Circular dependency causes type to evaluate as `undefined` at runtime. | Use `forwardRef(() => TargetService)` in constructor injection. |
| **5. Decorator Factory Invocation** | Writing `@Roles` instead of `@Roles('ADMIN')`. | Remember that decorators taking arguments are factories returning functions. |
| **6. Forgetting `@Injectable()` on Service** | Omitting decorator means TS will not emit constructor metadata for that service. | Always decorate all injectable classes with `@Injectable()`. |

---

## 7. Performance & Memory Impact
- **Metadata Storage**: Metadata is stored in a `WeakMap` inside the `reflect-metadata` polyfill. Target classes act as keys, allowing metadata to be garbage-collected if classes are disposed.
- **Reflection Cost**: Reflection lookups occur strictly during application bootstrap (`NestFactory.create`). During normal request execution, metadata lookups (e.g. in Guards via `Reflector`) are fast hash-table lookups ($O(1)$).

---

## 8. Security Considerations
- **Metadata Tampering**: Never expose metadata endpoints or accept user input that dynamically alters metadata keys at runtime.
- **Role Verification**: Always sanitize role arrays in `@Roles()` decorators to prevent unexpected authorization bypasses.

---

## 9. Testing Strategies (Unit & E2E)

```typescript
import { describe, it, expect } from 'vitest';
import { Reflector } from '@nestjs/core';
import { Roles, ROLES_KEY } from './roles.decorator.js';

class MockTarget {
  @Roles('ADMIN')
  testMethod() {}
}

describe('Roles Decorator Unit Test', () => {
  it('correctly attaches roles metadata to target method', () => {
    const reflector = new Reflector();
    const metadata = reflector.get(ROLES_KEY, MockTarget.prototype.testMethod);
    expect(metadata).toEqual(['ADMIN']);
  });
});
```

---

## 10. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: What are the prerequisites in `tsconfig.json` for NestJS decorators to function?</strong></summary>

**Answer:**  
Two compiler options are required: `"experimentalDecorators": true` (enables stage-2 decorator syntax support) and `"emitDecoratorMetadata": true` (causes the TypeScript compiler to emit `design:type`, `design:paramtypes`, and `design:returntype` metadata keys).
</details>

<details>
<summary><strong>Q2: Why can't NestJS inject an interface like `constructor(private repo: IUserRepository)`?</strong></summary>

**Answer:**  
TypeScript interfaces are pure design-time constructs that are completely removed during compilation to JavaScript. At runtime, TypeScript emits `Object` for the parameter type. Because `Object` is ambiguous, Nest cannot determine which concrete provider class to inject.
</details>

<details>
<summary><strong>Q3: How does NestJS resolve interface injection?</strong></summary>

**Answer:**  
By pairing the interface with an explicit runtime token (string or Symbol) using `@Inject('USER_REPO')` on the constructor parameter, and binding that token to a provider in the module (`{ provide: 'USER_REPO', useClass: UserRepository }`).
</details>

<details>
<summary><strong>Q4: In what order do class, method, property, and parameter decorators execute?</strong></summary>

**Answer:**  
Parameter decorators run first, followed by method/property/accessor decorators, and class decorators execute last.
</details>

<details>
<summary><strong>Q5: What is the purpose of `Reflector` in NestJS?</strong></summary>

**Answer:**  
`Reflector` is a built-in helper class in `@nestjs/core` that simplifies reading custom metadata attached to classes and methods via `Reflect.getMetadata()`. It powers Guards, Interceptors, and Filters when inspecting roles, permissions, or custom metadata tags.
</details>

<details>
<summary><strong>Q6: How does `applyDecorators` work under the hood?</strong></summary>

**Answer:**  
`applyDecorators` composes multiple decorator functions into a single decorator. It iterates through the provided decorators and applies each one to the target class, method, or property descriptor sequentially.
</details>

<details>
<summary><strong>Q7: Does NestJS support standard Stage 3 ECMAScript decorators?</strong></summary>

**Answer:**  
Modern NestJS 11 supports TypeScript 5+ Stage 3 decorators, but constructor parameter type reflection still relies on metadata emitted via legacy experimental decorators for dependency injection auto-wiring.
</details>

<details>
<summary><strong>Q8: What happens if two modules register different metadata under the exact same string key?</strong></summary>

**Answer:**  
String keys can collide if shared globally. Best practice is to use unique namespaced string constants (e.g. `auth:roles`) or JavaScript `Symbol` identifiers to guarantee uniqueness.
</details>

---

## 11. Hands-on Exercises

<details>
<summary><strong>Exercise 1 (Easy): Create a `@Public()` route decorator</strong></summary>

**Prompt:** Create a custom decorator that marks an endpoint as publicly accessible without requiring JWT auth.  
**Solution:**
```typescript
import { SetMetadata } from '@nestjs/common';
export const IS_PUBLIC_KEY = 'isPublic';
export const Public = () => SetMetadata(IS_PUBLIC_KEY, true);
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Read metadata in a Guard</strong></summary>

**Prompt:** In a guard's `canActivate`, check if the route or controller has the `@Public()` metadata flag.  
**Solution:**
```typescript
const isPublic = this.reflector.getAllAndOverride<boolean>(IS_PUBLIC_KEY, [
  context.getHandler(),
  context.getClass(),
]);
if (isPublic) return true;
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Custom Header Parameter Decorator</strong></summary>

**Prompt:** Implement `@UserAgent()` parameter decorator that extracts the `user-agent` header.  
**Solution:**
```typescript
import { createParamDecorator, ExecutionContext } from '@nestjs/common';
export const UserAgent = createParamDecorator((_data, ctx: ExecutionContext) => {
  return ctx.switchToHttp().getRequest().headers['user-agent'];
});
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Compose OpenAPI and Role Guard Decorator</strong></summary>

**Prompt:** Compose `@Roles('ADMIN')`, `@UseGuards(RolesGuard)`, and `@ApiResponse({ status: 403 })` into `@AdminOnly()`.  
**Solution:**
```typescript
import { applyDecorators, UseGuards } from '@nestjs/common';
import { ApiResponse } from '@nestjs/swagger';
export function AdminOnly() {
  return applyDecorators(
    Roles('ADMIN'),
    UseGuards(RolesGuard),
    ApiResponse({ status: 403, description: 'Forbidden: Admin access required' })
  );
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Typed Metadata Decorator with Generics</strong></summary>

**Prompt:** Build a type-safe metadata decorator that enforces permission objects matching `{ action: string; resource: string }`.  
**Solution:**
```typescript
export interface PolicyRule { action: string; resource: string }
export const CHECK_POLICIES_KEY = 'check_policy';
export const CheckPolicies = (...handlers: PolicyRule[]) =>
  SetMetadata(CHECK_POLICIES_KEY, handlers);
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build an IoC Container Constructor Inspector</strong></summary>

**Prompt:** Write a utility function that inspects a class constructor and returns the names of all injected parameter classes using `Reflect`.  
**Solution:**
```typescript
export function getInjectedParamNames(target: Function): string[] {
  const types = Reflect.getMetadata('design:paramtypes', target) || [];
  return types.map((t: any) => t.name || 'Unknown');
}
```
</details>

---

## 12. Version Differences & Migration Notes (Nest 10 vs Nest 11)
- **TypeScript 5.7+**: Nest 11 fully supports modern TypeScript reflection, preserving decorator metadata accurately under SWC compilation.
- **Fastify 5 Compatibility**: Decorators behave identically across Express 5 and Fastify 5 platforms.

---

## 13. Further Reading & References
- [TypeScript Decorators Handbook](https://www.typescriptlang.org/docs/handbook/decorators.html)
- [npm: reflect-metadata](https://www.npmjs.com/package/reflect-metadata)
- [NestJS Custom Decorators Guide](https://docs.nestjs.com/custom-decorators)
