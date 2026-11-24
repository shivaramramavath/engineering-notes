# Module 03: NestJS Core Concepts

> Exhaustive guide to the NestJS request execution pipeline, validation engines, configuration namespaces, dynamic modules, and provider scopes.

---

## Sub-Modules & Topics

### 01. Request Pipeline ([01-request-pipeline](01-request-pipeline/README.md))
- **Pipeline Overview**: The deterministic execution order: `Middleware -> Guards -> Interceptors (Pre) -> Pipes -> Handler -> Interceptors (Post) -> Filters`.
- **Middleware**: Function & class-based middleware, route binding, and `MiddlewareConsumer`.
- **Pipes**: Parameter validation and transformation (`ValidationPipe`, `ParseIntPipe`, `ParseDatePipe`).
- **Guards**: Authentication and role authorization guards implementing `CanActivate`.
- **Interceptors**: RxJS streaming operators, response mapping, logging, caching, and timeouts.
- **Exception Filters**: Custom exception handling and RFC 7807 Problem Details serialization.
- **Execution Context & Custom Decorators**: `ExecutionContext`, `ArgumentsHost`, and metadata reflection via `Reflector`.

### 02. Validation & Serialization ([02-validation-and-serialization](02-validation-and-serialization/README.md))
- **Data Transfer Objects (DTOs)**: Class-based contracts and runtime validation schemas.
- **Class-Validator & Class-Transformer**: Whitelist filtering, non-whitelisted property rejection, and validation groups.
- **Zod-Based Validation**: Modern schema-first validation with `nestjs-zod`, `createZodDto`, and Swagger integration.
- **Response Serialization**: Excluding sensitive fields (passwords, salts) using `@Exclude()` and `ClassSerializerInterceptor`.

### 03. Configuration ([03-configuration](03-configuration/README.md))
- **ConfigModule**: Environment loading with `.env` files and hierarchical namespaces.
- **Validation**: Schema validation of environment variables via Joi and Zod.
- **Namespaced Configuration**: Type-safe `registerAs` configuration objects.

### 04. Modules & Dependency Injection ([04-modules-and-di](04-modules-and-di/README.md))
- **Dynamic Modules**: `forRoot`, `forRootAsync`, and `ConfigurableModuleBuilder` patterns.
- **Provider Scopes**: `DEFAULT` (Singleton), `REQUEST` (Per-request), and `TRANSIENT` lifecycles and performance implications.
- **Circular Dependencies**: Resolving cyclic dependencies safely using `forwardRef()`.
- **Module Lifecycle Hooks**: `OnModuleInit`, `OnApplicationBootstrap`, `OnModuleDestroy`, and reversed shutdown execution order in Nest 11.
