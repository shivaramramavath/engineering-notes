# Execution Context

Guards, interceptors, and exception filters don't receive `req` and `res` directly. They receive an **`ArgumentsHost`** or **`ExecutionContext`**: a transport-agnostic wrapper around "whatever arguments this handler is being called with". This is what lets the same guard or interceptor work for HTTP, WebSockets, microservices, and GraphQL.

## Why it exists

Nest handlers aren't always `(req, res)`:

| Transport | Handler arguments |
|-----------|-------------------|
| HTTP (Express) | `[req, res, next]` |
| HTTP (Fastify) | `[request, reply]` |
| WebSocket | `[client, data]` |
| Microservice (RPC) | `[data, context]` |
| GraphQL | `[root, args, context, info]` |

`ArgumentsHost` hides the array and gives typed accessors, so framework-level code doesn't hard-code a transport.

## ArgumentsHost

Available in **exception filters** (and as the base of `ExecutionContext`).

```ts
interface ArgumentsHost {
  getArgs<T extends any[] = any[]>(): T;
  getArgByIndex<T = any>(index: number): T;
  switchToHttp(): HttpArgumentsHost;   // getRequest(), getResponse(), getNext()
  switchToRpc(): RpcArgumentsHost;     // getData(), getContext()
  switchToWs(): WsArgumentsHost;       // getData(), getClient(), getPattern()
  getType<TContext extends string = 'http' | 'ws' | 'rpc'>(): TContext;
}
```

```ts
const http = host.switchToHttp();
const req = http.getRequest<Request>();   // pass a generic for typing
const res = http.getResponse<Response>();
```

Prefer the `switchTo*` helpers over `getArgs()[0]`; the helpers survive changes in argument layout.

## ExecutionContext

Available in **guards** and **interceptors**. It extends `ArgumentsHost` with two methods:

```ts
interface ExecutionContext extends ArgumentsHost {
  getClass<T = any>(): Type<T>;   // the controller class
  getHandler(): Function;         // the route handler method about to be invoked
}
```

These are the hooks that make metadata-driven behavior possible: you know *which* controller and method is being targeted.

```ts
const handler = context.getHandler();       // e.g. CatsController.prototype.create
const controller = context.getClass();      // CatsController
console.log(`${controller.name}.${handler.name}`);  // "CatsController.create"
```

`getHandler()` returns the method reference, not the name. Use `.name` if you want a string, and don't call it expecting to execute the route.

## Reading metadata with `Reflector`

`Reflector` (from `@nestjs/core`) reads metadata set by decorators like `@SetMetadata`, `@Roles`, `@Public`.

```ts
const roles = this.reflector.getAllAndOverride<string[]>('roles', [
  context.getHandler(),
  context.getClass(),
]);
```

| Method | Use |
|--------|-----|
| `get(key, target)` | Single target |
| `getAll(key, targets)` | Array of values, one per target |
| `getAllAndOverride(key, targets)` | First non-`undefined` value (route overrides controller) |
| `getAllAndMerge(key, targets)` | Merge arrays/objects from all targets |

Targets are checked in the order given; list the handler first so it overrides the class. Creating the metadata decorators themselves is covered in [Custom decorators](./10-custom-decorators.md).

## Writing code that works across transports

Use `getType()` to branch.

```ts
@Injectable()
export class AuthGuard implements CanActivate {
  canActivate(context: ExecutionContext): boolean {
    switch (context.getType()) {
      case 'http': {
        const req = context.switchToHttp().getRequest();
        return this.validate(req.headers.authorization);
      }
      case 'ws': {
        const client = context.switchToWs().getClient();
        return this.validate(client.handshake?.auth?.token);
      }
      case 'rpc': {
        const meta = context.switchToRpc().getContext();
        return this.validate(meta?.get?.('authorization')?.[0]); // shape depends on the transport
      }
      default:
        return false;
    }
  }
  private validate(token?: string) { /* ... */ return !!token; }
}
```

The exact shape of the `rpc` and `ws` context objects depends on the transporter and socket library, so verify against what you actually use ([WebSocket auth](../../05-advanced/04-realtime/03-websocket-authentication.md), [gRPC auth](../../05-advanced/06-microservices/09-grpc-authentication.md)).

## GraphQL

In GraphQL resolvers the "request" isn't in the usual place. Use `GqlExecutionContext`:

```ts
import { GqlExecutionContext } from '@nestjs/graphql';

canActivate(context: ExecutionContext) {
  const gqlCtx = GqlExecutionContext.create(context);
  const { req } = gqlCtx.getContext();   // depends on how you configured `context` in GraphQLModule
  return !!req.user;
}
```

`context.getType<GqlContextType>()` returns `'graphql'`. Where `req` lives depends on your `GraphQLModule` configuration. See [GraphQL auth and security](../../05-advanced/05-graphql/03-graphql-auth-and-security.md).

## Typing the request

`getRequest()` returns `any` unless you pass a generic. For custom properties like `user`, extend the type once instead of casting everywhere:

```ts
// types/express.d.ts
declare global {
  namespace Express {
    interface Request {
      user?: { id: string; roles: string[] };
    }
  }
}
export {};
```

(Fastify has its own augmentation mechanism via module declaration.)

## Common mistakes

- **Assuming HTTP everywhere.** A global guard or interceptor also runs for WebSocket and RPC handlers in hybrid apps. Check `getType()` if you touch `req`.
- **Using `getArgs()[0]`** instead of `switchToHttp().getRequest()`.
- **Forgetting `getClass()`** when reading metadata, so controller-level decorators get ignored.
- **Reading `req.body` in a guard and trusting its shape.** Pipes haven't run yet.
- **Calling `context.getHandler()()`** to invoke the route. It's a reference for introspection only.
- **Expecting `ExecutionContext` in pipes or middleware.** They don't receive it.

## Debugging

- Undefined `req.user` in a guard or decorator? The authenticating guard hasn't run yet (check order), or the request is non-HTTP.
- `Reflector` returns `undefined`? Check the metadata key, the target order, and where the decorator was applied.
- Log `context.getType()`, `context.getClass().name`, and `context.getHandler().name` to see exactly what a guard or interceptor is executing for.

## Quick Summary

- `ArgumentsHost` = transport-agnostic access to handler arguments (filters); `ExecutionContext` adds `getClass()` / `getHandler()` (guards, interceptors).
- Use `switchToHttp()`, `switchToWs()`, `switchToRpc()`; branch on `getType()` for multi-protocol code.
- `Reflector` + `getHandler()`/`getClass()` is the foundation of roles, public routes, and per-route options.
- GraphQL needs `GqlExecutionContext`; request typing needs a one-time declaration merge.

## Next

[Custom decorators →](./10-custom-decorators.md)
