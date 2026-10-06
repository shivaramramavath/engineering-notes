# HTTP Exceptions

In Nest you signal an HTTP error by **throwing** an exception. The framework catches it, and the exception layer turns it into a response. You never `return res.status(404)...` from a normal handler.

This note covers the exception classes themselves. How thrown errors become responses (and how to customize that) is in [Exception filters](./08-exception-filters.md).

## Core concept

`HttpException` is the base class. It carries a **response body** and a **status code**.

```ts
import { HttpException, HttpStatus } from '@nestjs/common';

throw new HttpException('Forbidden', HttpStatus.FORBIDDEN);
```

Response:

```json
{ "statusCode": 403, "message": "Forbidden" }
```

Signature:

```ts
new HttpException(
  response: string | Record<string, any>,
  status: number,
  options?: { cause?: unknown; description?: string },
);
```

- If `response` is a **string**, Nest builds `{ statusCode, message }`.
- If `response` is an **object**, it is used as the body **as-is** (you own the shape, including `statusCode`).
- `cause` is for logging/debugging and is **not** sent to the client.

```ts
try {
  await this.repo.save(user);
} catch (err) {
  throw new HttpException('Could not save user', HttpStatus.BAD_REQUEST, { cause: err });
}
```

## Built-in exceptions

Prefer these over raw `HttpException`. They set the status and a default message for you.

| Class | Status | Typical use |
|-------|--------|-------------|
| `BadRequestException` | 400 | Malformed or invalid input |
| `UnauthorizedException` | 401 | Missing/invalid credentials |
| `ForbiddenException` | 403 | Authenticated but not allowed |
| `NotFoundException` | 404 | Resource doesn't exist |
| `MethodNotAllowedException` | 405 | Wrong HTTP method |
| `NotAcceptableException` | 406 | Can't satisfy `Accept` header |
| `RequestTimeoutException` | 408 | Request took too long |
| `ConflictException` | 409 | Duplicate / state conflict |
| `GoneException` | 410 | Resource permanently removed |
| `PreconditionFailedException` | 412 | Conditional request failed |
| `PayloadTooLargeException` | 413 | Body too large |
| `UnsupportedMediaTypeException` | 415 | Unsupported content type |
| `UnprocessableEntityException` | 422 | Well-formed but semantically invalid |
| `InternalServerErrorException` | 500 | Unexpected failure |
| `NotImplementedException` | 501 | Feature not implemented |
| `BadGatewayException` | 502 | Upstream returned garbage |
| `ServiceUnavailableException` | 503 | Temporarily unavailable |
| `GatewayTimeoutException` | 504 | Upstream timed out |

All are exported from `@nestjs/common` and extend `HttpException`.

```ts
throw new NotFoundException(`User ${id} not found`);
```

```json
{ "message": "User 7 not found", "error": "Not Found", "statusCode": 404 }
```

Built-ins produce `{ message, error, statusCode }` where `error` is the status text. You can pass either a string or an object, plus options:

```ts
throw new BadRequestException('Invalid payload', { cause: err, description: 'Schema mismatch' });
// → { message: 'Invalid payload', error: 'Schema mismatch', statusCode: 400 }
```

With an object as the first argument, the object **replaces** the body:

```ts
throw new BadRequestException({ code: 'EMAIL_TAKEN', message: 'Email already registered' });
// → { code: 'EMAIL_TAKEN', message: '...' }   (no statusCode/error added)
```

## Custom exceptions

Extend `HttpException` (or a built-in) to attach domain meaning.

```ts
// email-taken.exception.ts
export class EmailTakenException extends ConflictException {
  constructor(email: string) {
    super({ code: 'EMAIL_TAKEN', message: `Email ${email} is already registered` });
  }
}

throw new EmailTakenException('a@b.com');
```

Keep the number of custom exceptions small. A stable `code` field in the body is often more useful to API clients than a new class per case.

## Where to throw

Two reasonable styles:

1. **Throw HTTP exceptions directly in services.** Simple, pragmatic, and fine for small and medium apps.
2. **Throw domain errors in services, map them in a filter.** Services stay transport-agnostic (important if the same service is called from a queue worker or a gRPC handler where an HTTP exception makes no sense).

```ts
// domain error, no HTTP knowledge
export class OrderAlreadyPaidError extends Error {}

// service
if (order.paid) throw new OrderAlreadyPaidError();

// filter maps it to 409, see exception filters
```

Pick one style per project and stay consistent. See [error handling principles](../../08-architecture-and-patterns/03-clean-code/04-error-handling-principles.md).

## Unknown errors

Anything that isn't an `HttpException` (a plain `Error`, a TypeORM/Prisma error, a bug) is treated as an unexpected failure:

```json
{ "statusCode": 500, "message": "Internal server error" }
```

The built-in handler also **logs** these unrecognized errors; recognized `HttpException`s are not logged by default. Details are deliberately hidden from the client. Don't "fix" this by leaking `err.message`; map known errors to proper exceptions instead.

## Important behavior

- `getStatus()` returns the status number; `getResponse()` returns the body (string or object); `cause` holds the original error.
- `ValidationPipe` failures throw `BadRequestException` with `message` as an **array** of messages. Clients must handle both string and array.
- Throwing inside an `async` handler works the same as in sync code. Make sure you `await` the promise. A floating promise that rejects won't be caught by Nest.
- An exception thrown in a guard, interceptor, pipe, or handler is handled identically.
- Errors in **background work** (queue processors, cron jobs, event listeners) aren't HTTP requests, so the HTTP exception layer never sees them. Handle those with their own error strategy ([retries and DLQs](../../05-advanced/02-background-processing/04-retries-and-dead-letter-queues.md)).

## Common mistakes

- **`throw new HttpException('msg')` without a status.** The status is required.
- **Using the wrong status**: 400 for "not found", 403 when 401 is correct, 500 for client mistakes. Pick the status that tells the client what to do next.
- **Returning an error instead of throwing it.** `return new NotFoundException()` sends a 200 with a weird body.
- **Swallowing errors** in `try/catch` and returning `null`, which turns failures into silent successes.
- **Leaking internals**: putting stack traces, SQL, or raw DB messages in the response body.
- **Catching `Error` and re-throwing as `InternalServerErrorException(err.message)`**, exposing details and losing the original type. Pass `{ cause: err }` instead.

## Debugging

- Need the real error behind a 500? Check server logs (the default handler logs unknown errors) or add a global filter that logs `exception` with its `cause`.
- Body shape not what you expected? Remember string vs object arguments and that objects replace the body.
- Status code wrong? Verify you're not throwing a custom exception that inherits a different base class.

## Quick Summary

- Throw exceptions; don't return error responses.
- `HttpException(response, status, { cause })` is the base; prefer built-ins like `NotFoundException`.
- String → `{ statusCode, message }`; object → used as the body verbatim.
- Non-HTTP errors become a generic 500 and are logged.
- Choose one policy: throw HTTP exceptions in services, or throw domain errors and map them in a filter.

## Next

[Exception filters →](./08-exception-filters.md)
