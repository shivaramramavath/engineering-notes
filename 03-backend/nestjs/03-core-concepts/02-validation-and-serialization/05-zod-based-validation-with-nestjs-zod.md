# Zod-Based Validation with nestjs-zod

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 03-core-concepts / 02-validation-and-serialization
- **Prerequisites**: [01-built-in-validation-pipe](./01-built-in-validation-pipe.md), [02-class-validator-deep-dive](./02-class-validator-deep-dive.md), [00-prerequisites/01-typescript-essentials](../../00-prerequisites/01-typescript-essentials.md)

---

## 1. Context & Purpose

For years, `class-validator` and `class-transformer` were the de facto validation tools in NestJS. However, `class-validator` relies heavily on experimental TypeScript decorator metadata, class prototype reflection, and mutable object mutations. Furthermore, sharing validation schemas between backend NestJS code and modern frontend frameworks (React, Next.js, Vue) is difficult because frontend apps prefer lightweight, schema-first validation.

**Zod** has emerged as the modern TypeScript schema standard: it offers zero-dependency schema declaration, static type inference (`z.infer<T>`), and composable schema transformations. Through **`nestjs-zod`**, developers can replace `class-validator` entirely, gaining schema-first DTOs, automated Swagger OpenAPI integration, and shared validation schemas between frontend and backend.

---

## 2. Theoretical Foundations

Comparing Class-Validator with Zod:

| Feature | `class-validator` + `class-transformer` | `nestjs-zod` (Zod Schema-First) |
|---|---|---|
| Approach | Class-based with property decorators (`@IsString()`) | Functional schema declaration (`z.string()`) |
| Type Inference | Manual duplicate TypeScript property declarations | Automatic via `createZodDto(schema)` |
| Runtime Overhead | Reflection & prototype instantiation | Direct pure JavaScript function validation |
| Frontend Sharing | Cumbersome (requires `reflect-metadata` bundle) | Trivial (pure JS/TS schema exportable anywhere) |
| OpenAPI Integration | Manual `@ApiProperty()` on every field | Automatic schema derivation from Zod constraints |

---

## 3. Architecture & Internal Mechanics

```
Incoming Request JSON
         │
         ▼
ZodValidationPipe (Global or Route)
         │
         ├─> Retrieves Zod Schema from DTO prototype
         │
         ▼
zodSchema.safeParseAsync(req.body)
         │
   ┌─────┴─────┐
   ▼           ▼
[Success]   [Failure]
   │           │
   │           ▼
   │     ZodValidationException
   │     - Maps issues to 400 Bad Request
   │     - Emits structured error list
   │
   ▼
Parsed & Strongly-Typed DTO Object
         │
         ▼
Controller Handler
```

`createZodDto` bridges Zod functional schemas to NestJS class expectations by generating a class whose prototype carries the Zod schema reference.

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save nestjs-zod zod
```

### 4.2 Defining Schema-First DTOs

```typescript
// src/users/dto/create-user.dto.ts
import { z } from 'zod';
import { createZodDto } from 'nestjs-zod';

// Define the source-of-truth Zod schema
export const CreateUserSchema = z.object({
  email: z.string().email({ message: 'Invalid email address' }),
  name: z.string().min(2, { message: 'Name must be at least 2 characters' }),
  age: z.coerce.number().int().min(18, { message: 'Must be at least 18 years old' }),
  role: z.enum(['USER', 'ADMIN']).default('USER'),
  tags: z.array(z.string()).default([]),
});

// Derive both the TypeScript class and the NestJS DTO
export class CreateUserDto extends createZodDto(CreateUserSchema) {}
```

---

### 4.3 Enabling Global Zod Validation Pipe

Configure `ZodValidationPipe` and exception filter in `main.ts`:

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { ZodValidationPipe } from 'nestjs-zod';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  // Apply Zod validation globally across all controllers
  app.useGlobalPipes(new ZodValidationPipe());

  await app.listen(3000);
}
bootstrap();
```

---

### 4.4 OpenAPI / Swagger Integration with `patchNestjsSwagger`

To automatically generate Swagger documentation from Zod schemas:

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { SwaggerModule, DocumentBuilder } from '@nestjs/swagger';
import { patchNestjsSwagger } from 'nestjs-zod';
import { AppModule } from './app.module';

// CRITICAL: Patch SwaggerModule before creating documents!
patchNestjsSwagger();

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  const config = new DocumentBuilder()
    .setTitle('Zod Powered API')
    .setVersion('1.0')
    .build();

  const document = SwaggerModule.createDocument(app, config);
  SwaggerModule.setup('api/docs', app, document);

  await app.listen(3000);
}
bootstrap();
```

---

### 4.5 Controller Consumption

```typescript
// src/users/users.controller.ts
import { Controller, Post, Body, Get, Param } from '@nestjs/common';
import { CreateUserDto } from './dto/create-user.dto';

@Controller('users')
export class UsersController {
  @Post()
  async createUser(@Body() dto: CreateUserDto) {
    // dto is fully type-safe and runtime-validated
    return {
      message: 'User created successfully',
      data: dto,
    };
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Route Parameter Coercion
Query parameters and URL path parameters always arrive from Express as strings. Standard `z.number()` fails on `@Param('id')`.
- **Handling**: Use `z.coerce.number()` or `z.coerce.date()` to automatically coerce string inputs into numbers or Date objects before validation.

### 5.2 Stripping Unknown Properties
By default, Zod objects strip unknown keys (`strip()`). If your application requires strict rejection of unexpected payload keys:
```typescript
export const StrictSchema = CreateUserSchema.strict();
```

---

## 6. Performance & Memory Implications

- **Validation Speed**: Zod validates flat JSON objects up to 2x faster than `class-validator`, which must perform dynamic metadata lookups on each property and instantiate empty class prototypes.
- **Bundle Size**: Zod has zero external dependencies, making shared schema libraries lightweight for browser bundles.

---

## 7. Anti-Patterns & Common Mistakes

- **Forgetting `patchNestjsSwagger()`**: Running Swagger without calling `patchNestjsSwagger()`. Swagger fails to inspect Zod schemas, resulting in empty Swagger models.
- **Duplicating Types with `type` and `class`**: Writing `type User = z.infer<typeof Schema>` and then manually writing `class UserDto` with duplicate properties. Always use `createZodDto(Schema)`.
- **Mixing `class-validator` and `nestjs-zod` Globally**: Registering both `ValidationPipe` and `ZodValidationPipe` globally without filtering can cause duplicate execution and conflicting error formats.

---

## 8. Production Checklist & Operational Readiness

- [ ] `patchNestjsSwagger()` called before `SwaggerModule.createDocument()`.
- [ ] Global `ZodValidationPipe` registered in `main.ts`.
- [ ] Coercion (`z.coerce`) configured for numeric query and path parameters.
- [ ] Shared schema files exported to `@app/contracts` or shared npm package.
- [ ] Custom `ZodValidationExceptionFilter` applied for standardized API error responses.

---

## 9. Migration & Version History

- **Zod 3.x**: De facto standard for TypeScript schemas.
- **nestjs-zod 3.x/4.x**: First-class NestJS 10 and 11 compatibility with Express 5 support.

---

## 10. Verification & Testing

Unit testing a Zod DTO validation directly:

```typescript
import { CreateUserSchema } from './create-user.dto';

describe('CreateUserSchema', () => {
  it('validates a correct payload', () => {
    const valid = {
      email: 'alex@example.com',
      name: 'Alex',
      age: 25,
    };
    const parsed = CreateUserSchema.safeParse(valid);
    expect(parsed.success).toBe(true);
  });

  it('rejects invalid email and underage users', () => {
    const invalid = {
      email: 'not-an-email',
      name: 'A',
      age: 15,
    };
    const parsed = CreateUserSchema.safeParse(invalid);
    expect(parsed.success).toBe(false);
    if (!parsed.success) {
      expect(parsed.error.issues.length).toBeGreaterThanOrEqual(2);
    }
  });
});
```

---

## 11. Comprehensive Examples

### Custom Structured Error Filter for Zod

```typescript
// src/common/filters/zod-exception.filter.ts
import { ExceptionFilter, Catch, ArgumentsHost, HttpStatus } from '@nestjs/common';
import { ZodValidationException } from 'nestjs-zod';
import { Response } from 'express';

@Catch(ZodValidationException)
export class ZodHttpExceptionFilter implements ExceptionFilter {
  catch(exception: ZodValidationException, host: ArgumentsHost) {
    const ctx = host.switchToHttp();
    const response = ctx.getResponse<Response>();
    const zodError = exception.getZodError();

    const formattedErrors = zodError.errors.map((err) => ({
      field: err.path.join('.'),
      message: err.message,
      rule: err.code,
    }));

    response.status(HttpStatus.BAD_REQUEST).json({
      statusCode: HttpStatus.BAD_REQUEST,
      error: 'Bad Request',
      message: 'Validation failed',
      errors: formattedErrors,
      timestamp: new Date().toISOString(),
    });
  }
}
```

---

## 12. Cross-References & Further Reading

- [01-built-in-validation-pipe.md](./01-built-in-validation-pipe.md) - Standard ValidationPipe
- [02-class-validator-deep-dive.md](./02-class-validator-deep-dive.md) - Class-validator mechanics
- [04-intermediate/09-openapi-and-swagger/01-swagger-setup.md](../../04-intermediate/09-openapi-and-swagger/01-swagger-setup.md) - Swagger setup
- [nestjs-zod GitHub Repository](https://github.com/risen228/nestjs-zod)
