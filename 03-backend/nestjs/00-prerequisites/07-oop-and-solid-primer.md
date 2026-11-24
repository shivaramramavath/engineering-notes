# Module 00: OOP & SOLID Principles in NestJS

> Applying Object-Oriented Programming and the SOLID principles to enterprise NestJS architectures, dependency injection, and clean module boundaries.

| Metadata | Details |
| :--- | :--- |
| **NestJS Version** | NestJS 11+ |
| **Target Runtime** | Node.js >= 20.0.0 (Node.js 22 LTS recommended) |
| **Paradigm** | Object-Oriented Programming (OOP) & Dependency Injection |
| **Theory Link** | Cross-linked with [08-solid-design-patterns](../../08-solid-design-patterns/README.md) |

---

## 1. Concept Overview (Why before How)

Most Node.js backends struggle as codebases grow beyond 50,000 lines because JavaScript encourages procedural code, unstructured function imports, and tight coupling between database clients and HTTP handlers.

NestJS solves this by providing an opinionated, architecture-first framework rooted in **Object-Oriented Programming (OOP)** and the **SOLID principles**. Understanding how each SOLID principle maps directly to NestJS primitives is essential for designing testable, maintainable systems that scale over years.

---

## 2. Architecture & Data Flow Diagram

```
+----------------------------------------------------------------------------------------------------+
|                                    SOLID Architecture in NestJS                                    |
|                                                                                                    |
|  [ S: Single Responsibility ]                                                                      |
|  Controller (HTTP Only) ──> Service (Domain Logic Only) ──> Repository (Database Only)             |
|                                                                                                    |
|  [ O: Open / Closed ]                                                                              |
|  Add new features via Interceptors, Guards, or Event Listeners without altering existing Services  |
|                                                                                                    |
|  [ L: Liskov Substitution ]                                                                        |
|  MemoryCacheService and RedisCacheService both satisfy CacheService contract                       |
|                                                                                                    |
|  [ I: Interface Segregation ]                                                                      |
|  Pipes only implement PipeTransform; Guards only implement CanActivate                            |
|                                                                                                    |
|  [ D: Dependency Inversion ]                                                                       |
|  High-level Domain Service ──(Depends on Interface Port)──> [ UserRepository Interface ]           |
|                                                                      ▲                             |
|                                                                      │ Implements                  |
|                                                        TypeOrmUserRepository (Low-level Adapter)   |
+----------------------------------------------------------------------------------------------------+
```

---

## 3. Core Mechanics & Deep Dive

### 3.1 Single Responsibility Principle (SRP)
In NestJS:
- **Controllers**: Only handle HTTP concerns (deserializing params, validating DTOs, returning status codes). Zero database queries or heavy calculations.
- **Services**: Pure business domain logic. Zero knowledge of HTTP requests (`req`), headers, or response cookies.
- **Repositories**: Pure data access and query orchestration.

### 3.2 Open/Closed Principle (OCP)
Classes should be open for extension, but closed for modification:
- Extend controller behavior using **Interceptors** (`@UseInterceptors(LoggingInterceptor)`) without modifying controller methods.
- Extend authorization policies by attaching new **Guards** (`@UseGuards(RolesGuard)`) without modifying the handler logic.

### 3.3 Liskov Substitution Principle (LSP)
Subclasses or interface implementations must be substitutable for their base types without altering program correctness:
- The NestJS platform adapter architecture: swapping `@nestjs/platform-express` with `@nestjs/platform-fastify` does not break controller logic because both adhere to the `HttpServer` interface contract.

### 3.4 Interface Segregation Principle (ISP)
Clients should not be forced to depend on interfaces they do not use:
- NestJS defines small, single-method contracts:
  - `PipeTransform`: `transform(value, metadata)`
  - `CanActivate`: `canActivate(context)`
  - `NestInterceptor`: `intercept(context, next)`
  - `ExceptionFilter`: `catch(exception, host)`

### 3.5 Dependency Inversion Principle (DIP)
High-level modules should not depend on low-level modules; both should depend on abstractions (interfaces):
- Services depend on an abstract `NotificationPort` interface, while the low-level `TwilioSmsAdapter` implements the interface and is bound in the module via `{ provide: 'NOTIFICATION_PORT', useClass: TwilioSmsAdapter }`.

---

## 4. Complete Code Examples (5 Levels)

### 4.1 Basic Example: SRP Violation vs Clean Separation
```typescript
// BAD (Violates SRP: Controller makes direct database queries and calculates tax):
@Controller('orders')
export class BadOrderController {
  @Post()
  async createOrder(@Body() body: any) {
    const tax = body.price * 0.2; // Business logic in controller!
    return await rawDb.query('INSERT INTO orders VALUES ($1, $2)', [body.id, tax]); // DB in controller!
  }
}

// GOOD (Adheres to SRP: Clear separation of duties):
@Controller('orders')
export class GoodOrderController {
  constructor(private readonly orderService: OrderService) {}

  @Post()
  async createOrder(@Body() dto: CreateOrderDto) {
    return await this.orderService.createOrder(dto);
  }
}
```

### 4.2 Real-World Production Example: Dependency Inversion with Injection Tokens
```typescript
// 1. Abstraction (Port)
export interface EmailSender {
  sendEmail(to: string, subject: string, body: string): Promise<boolean>;
}
export const EMAIL_SENDER_TOKEN = Symbol('EmailSender');

// 2. Concrete Implementation (Adapter)
@Injectable()
export class SesEmailAdapter implements EmailSender {
  async sendEmail(to: string, subject: string, body: string): Promise<boolean> {
    console.log(`[SES] Sending email to ${to}: ${subject}`);
    return true;
  }
}

// 3. High-Level Domain Service (Depends on Abstraction)
@Injectable()
export class UserRegistrationService {
  constructor(
    @Inject(EMAIL_SENDER_TOKEN) private readonly emailSender: EmailSender
  ) {}

  async register(email: string) {
    // Business logic...
    await this.emailSender.sendEmail(email, 'Welcome', 'Welcome to our platform!');
  }
}

// 4. Module Configuration
@Module({
  providers: [
    UserRegistrationService,
    { provide: EMAIL_SENDER_TOKEN, useClass: SesEmailAdapter },
  ],
})
export class AuthModule {}
```

### 4.3 Edge Case: Strategy Pattern with Polymorphic Providers
```typescript
// Selecting payment provider dynamically at runtime adhering to OCP
export interface PaymentStrategy {
  charge(amountCents: number): Promise<string>;
}

@Injectable()
export class StripeStrategy implements PaymentStrategy {
  async charge(amountCents: number) { return `stripe_tx_${amountCents}`; }
}

@Injectable()
export class PayPalStrategy implements PaymentStrategy {
  async charge(amountCents: number) { return `paypal_tx_${amountCents}`; }
}

@Injectable()
export class PaymentContextService {
  private strategies = new Map<string, PaymentStrategy>();

  constructor(stripe: StripeStrategy, paypal: PayPalStrategy) {
    this.strategies.set('stripe', stripe);
    this.strategies.set('paypal', paypal);
  }

  getStrategy(provider: string): PaymentStrategy {
    const strategy = this.strategies.get(provider);
    if (!strategy) throw new Error(`Unsupported payment provider: ${provider}`);
    return strategy;
  }
}
```

### 4.4 Failure Case & Debugging: Tight Coupling Crash
```typescript
// Tight coupling failure: Instantiating dependencies directly with 'new'
@Injectable()
export class ReportService {
  private mailer: SmtpMailer;

  constructor() {
    this.mailer = new SmtpMailer(); // Tightly coupled! Impossible to mock in unit tests!
  }
}
// Fix: Inject mailer via constructor injection!
```

### 4.5 Idiomatic Production Recipe: Clean Ports & Adapters Architecture
```typescript
// Domain Port:
export interface OrderRepository {
  save(order: OrderEntity): Promise<OrderEntity>;
  findById(id: string): Promise<OrderEntity | null>;
}
export const ORDER_REPOSITORY = Symbol('OrderRepository');

// Application Service:
@Injectable()
export class PlaceOrderUseCase {
  constructor(
    @Inject(ORDER_REPOSITORY) private readonly orderRepo: OrderRepository
  ) {}

  async execute(command: PlaceOrderCommand): Promise<OrderEntity> {
    const order = new OrderEntity(command.items);
    order.calculateTotals();
    return await this.orderRepo.save(order);
  }
}
```

---

## 5. Framework Internals (Under the Hood)
The entire NestJS IoC engine is built around the **Dependency Inversion Principle**:
- When `NestFactory.create(AppModule)` runs, the framework traverses the dependency tree.
- For every class constructor, it analyzes the parameter types.
- The injector matches the required abstraction tokens with provider recipes (`useClass`, `useValue`, `useFactory`).
- This allows Nest to swap entire subsystems (e.g. replacing an in-memory test database with real PostgreSQL) by modifying only the provider configuration in the test module.

---

## 6. Common Pitfalls & Mistakes Table

| Mistake | Why It Happens | Idiomatic Fix |
| :--- | :--- | :--- |
| **1. Fat Controllers** | Writing SQL or business calculations directly in controller methods. | Delegate all business logic to services; keep controllers thin. |
| **2. Leaky Abstractions** | Passing raw HTTP `Request` or `Response` objects into domain services. | Extract values into pure DTOs or command objects before calling services. |
| **3. Hardcoding `new Service()`** | Instantiating classes manually instead of letting Nest inject them. | Declare dependencies in constructors and register them as providers. |
| **4. God Services** | Creating a single service with 50+ unrelated methods. | Break services into focused, cohesive classes (e.g. `UserAuthService`, `UserProfileService`). |
| **5. Modifying Existing Code for New Types** | Adding endless `if/else` checks for new payment methods (violates OCP). | Use the Strategy Pattern or polymorphic providers. |
| **6. Bloated Interfaces** | Creating a monolithic interface with 30 methods (violates ISP). | Deconstruct into focused role interfaces. |

---

## 7. Performance & Memory Impact
- **Singleton Efficiency**: Inverting dependencies via singletons (`Scope.DEFAULT`) ensures only one instance of each service exists in memory, eliminating garbage collection churn from repeated allocations.
- **Indirection Overhead**: Interface indirection and method dispatch in V8 has negligible (<1 nanosecond) overhead.

---

## 8. Security Considerations
- **Separation of Concerns for Security**: Decouple authorization logic from business logic by using Guards. This ensures security policies can be audited in a single place without auditing every domain service.

---

## 9. Testing Strategies (Unit & E2E)

```typescript
import { describe, it, expect, vi } from 'vitest';
import { UserRegistrationService, EmailSender } from './user-registration.service.js';

describe('UserRegistrationService SOLID Unit Test', () => {
  it('registers user and sends welcome email using mocked port', async () => {
    // Demonstrating DIP: Easy testing with mock implementation
    const mockEmailSender: EmailSender = {
      sendEmail: vi.fn().mockResolvedValue(true),
    };

    const service = new UserRegistrationService(mockEmailSender);
    await service.register('ada@example.com');

    expect(mockEmailSender.sendEmail).toHaveBeenCalledWith(
      'ada@example.com',
      'Welcome',
      expect.any(String)
    );
  });
});
```

---

## 10. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: How does NestJS enforce the Dependency Inversion Principle?</strong></summary>

**Answer:**  
NestJS decouples high-level domain components from low-level infrastructure by using an Inversion of Control (IoC) container. High-level services declare dependencies as abstract injection tokens (`@Inject('TOKEN')`), while low-level modules supply concrete implementations via provider bindings (`useClass`, `useFactory`), allowing adapters to be swapped without changing domain logic.
</details>

<details>
<summary><strong>Q2: Give an example of Open/Closed Principle in NestJS.</strong></summary>

**Answer:**  
Interceptors and Guards demonstrate OCP. If you need to add logging, performance timing, or rate limiting to a controller, you attach an Interceptor (`@UseInterceptors(LoggingInterceptor)`) or Guard without modifying a single line of the existing controller handler code.
</details>

<details>
<summary><strong>Q3: Why should NestJS services never accept the Express `Request` object?</strong></summary>

**Answer:**  
Accepting `Request` leaks low-level HTTP transport details into the business service, violating the Single Responsibility and Dependency Inversion principles. It also makes unit testing difficult (requiring complex HTTP mocks) and prevents the service from being reused across WebSockets, microservices, or CLI commands.
</details>

<details>
<summary><strong>Q4: How does NestJS implement the Interface Segregation Principle?</strong></summary>

**Answer:**  
NestJS breaks framework capabilities into tiny, single-method interfaces (`CanActivate`, `PipeTransform`, `NestInterceptor`, `ExceptionFilter`). A custom pipe only implements `transform()`, and a guard only implements `canActivate()`, ensuring classes only implement methods they actually need.
</details>

<details>
<summary><strong>Q5: What is the Liskov Substitution Principle as applied to NestJS platform adapters?</strong></summary>

**Answer:**  
NestJS defines an abstract `HttpServer` interface. Both `@nestjs/platform-express` and `@nestjs/platform-fastify` implement this interface. Because both adhere to the contract, an application can switch between Express and Fastify without modifying application-level controllers or services.
</details>

<details>
<summary><strong>Q6: How do you prevent "God Services" in NestJS?</strong></summary>

**Answer:**  
By adhering to the Single Responsibility Principle: decompose large domains into cohesive sub-services or Use Cases (e.g. `CreateOrderUseCase`, `CancelOrderUseCase`, `CalculateOrderTaxService`) rather than having a single 2,000-line `OrderService`.
</details>

<details>
<summary><strong>Q7: What is the difference between an Anemic Domain Model and a Rich Domain Model in NestJS?</strong></summary>

**Answer:**  
An anemic domain model treats entities as pure data structures (getters/setters only) and places all logic in services. A rich domain model embeds core invariants and state transformations directly inside entity classes (e.g. `order.cancel()`), adhering to true OOP principles.
</details>

<details>
<summary><strong>Q8: When should you use the Strategy Pattern in NestJS?</strong></summary>

**Answer:**  
When an operation has multiple interchangeable algorithms or external providers (e.g. multiple payment gateways, multiple authentication strategies in Passport, or multiple notification channels).
</details>

---

## 11. Hands-on Exercises

<details>
<summary><strong>Exercise 1 (Easy): Refactor a Controller violating SRP</strong></summary>

**Prompt:** Move password hashing logic out of a controller into an `AuthService`.  
**Solution:**
```typescript
@Injectable()
export class AuthService {
  async register(dto: RegisterDto) {
    const hash = await argon2.hash(dto.password);
    return await this.userRepo.create({ ...dto, passwordHash: hash });
  }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Define a Port Token with Symbol</strong></summary>

**Prompt:** Create a unique symbol token and interface for an SMS provider.  
**Solution:**
```typescript
export interface SmsPort { send(to: string, msg: string): Promise<void>; }
export const SMS_PORT = Symbol('SmsPort');
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Implement Mock Provider for Unit Testing</strong></summary>

**Prompt:** Write a test module replacing `SMS_PORT` with an in-memory spy.  
**Solution:**
```typescript
const module = await Test.createTestingModule({
  providers: [
    NotificationService,
    { provide: SMS_PORT, useValue: { send: vi.fn().mockResolvedValue(undefined) } },
  ],
}).compile();
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Decorator satisfying OCP</strong></summary>

**Prompt:** Create an `@AuditLog(action)` interceptor decorator that records method invocations without modifying business logic.  
**Solution:**
```typescript
export const AuditLog = (action: string) => applyDecorators(
  SetMetadata('audit_action', action),
  UseInterceptors(AuditLogInterceptor)
);
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Implement Factory Provider with Feature Flags</strong></summary>

**Prompt:** Provide `StripeService` or `MockPaymentService` based on environment configuration.  
**Solution:**
```typescript
export const PaymentProvider = {
  provide: 'PAYMENT_SERVICE',
  useFactory: (config: ConfigService) => {
    return config.get('MOCK_PAYMENTS') ? new MockPaymentService() : new StripeService();
  },
  inject: [ConfigService],
};
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build a Specification Pattern Evaluator</strong></summary>

**Prompt:** Implement a specification pattern in NestJS where rules (`IsCustomerEligible`, `HasSufficientCredit`) can be combined via `.and()` and `.or()`.  
**Solution:**
```typescript
export interface Specification<T> {
  isSatisfiedBy(candidate: T): boolean;
  and(other: Specification<T>): Specification<T>;
}
```
</details>

---

## 12. Version Differences & Migration Notes (Nest 10 vs Nest 11)
- **Express 5 Engine**: Stricter typing and promise forwarding in Nest 11 align cleanly with SRP by removing manual `try/catch` boilerplate from controllers.
- **DI Enhancements**: Nest 11 dynamic module object references improve startup times for applications adhering strictly to DIP and clean modular separation.

---

## 13. Further Reading & References
- [08-solid-design-patterns Curriculum](../../08-solid-design-patterns/README.md)
- [Clean Architecture by Robert C. Martin](https://blog.cleancoder.com/uncle-bob/2012/08/13/the-clean-architecture.html)
- [NestJS Architecture Concepts](https://docs.nestjs.com/)
