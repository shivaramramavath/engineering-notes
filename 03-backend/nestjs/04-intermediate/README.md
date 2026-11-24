# Module 04: NestJS Intermediate & Data Architecture

> In-depth guide to enterprise testing, database persistence engines (TypeORM, Prisma, Mongoose, MikroORM, Drizzle), authentication, authorization, and OpenAPI generation.

---

## Sub-Modules & Topics

### 01. Testing ([01-testing](01-testing/README.md))
- **Unit Testing**: Testing services and controllers using `Test.createTestingModule` and mock providers.
- **E2E Testing**: Full application integration testing using Supertest.
- **Database Testing with Testcontainers**: Testing real PostgreSQL, Redis, and MongoDB instances in disposable Docker containers.
- **SWC & Vitest**: Fast native compilation and parallel test runners.

### 02. Database Foundations & Multi-ORM ([02-database-foundations](02-database-foundations/README.md))
- **Relational Architecture**: Connection pooling, N+1 query problem, transactions, and migration strategies.
- **MikroORM**: Unit of Work, Identity Map, and Data Mapper patterns for Domain-Driven Design.
- **Drizzle ORM**: Zero-overhead, high-performance TypeScript SQL query builder (`@nestjs/drizzle`).
- **Kysely**: Type-safe query building with typed SQL queries.

### 03. TypeORM ([03-typeorm](03-typeorm/README.md))
- **Entities & Repositories**: Active Record vs Data Mapper, relations, indexes, and custom repositories.
- **Transactions & Subscribers**: `QueryRunner` ACID transactions, database event subscribers, and soft deletes.

### 04. Prisma ([04-prisma](04-prisma/README.md))
- **Prisma Integration**: Wrapping `PrismaClient` in `@Injectable()` services with `OnModuleInit`.
- **Client Extensions**: Utilizing `$extends` for custom computed fields, query logging, and Row-Level Security (RLS).

### 05. Mongoose ([05-mongoose](05-mongoose/README.md))
- **MongoDB in NestJS**: Schemas, models, subdocuments, population, and multi-document transactions.

### 06. Authentication ([06-authentication](06-authentication/README.md))
- **Passport Strategies**: Local strategy, JWT bearer strategy, refresh token rotation, and cookie sessions.
- **Modern Standards**: OAuth2 / OIDC, Two-Factor Authentication (TOTP), and Passkeys / WebAuthn.

### 07. Authorization ([07-authorization](07-authorization/README.md))
- **Role-Based Access Control (RBAC)**: Role guards and metadata decorators.
- **Attribute-Based Access Control (ABAC) & CASL**: Subject-attribute policy evaluation and resource ownership guards.

### 08. API Design ([08-api-design](08-api-design/README.md))
- **REST Best Practices**: Resource design, API versioning (URI, header, media-type), cursor pagination, and filtering DSLs.
- **Resilience**: Idempotency keys (`Idempotency-Key` header) and reliable Webhook dispatchers.

### 09. OpenAPI & Swagger ([09-openapi-and-swagger](09-openapi-and-swagger/README.md))
- **SwaggerModule**: Automatic OpenAPI 3.1 specification generation, CLI plugins, and client SDK generation.
