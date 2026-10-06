# Circular Dependencies

A circular dependency means A needs B and B needs A. Nest can't construct either first, so it fails or hands you `undefined`. Nest provides an escape hatch (`forwardRef`), but the better answer is almost always to **change the design** so the cycle disappears.

Prerequisites: [Feature and shared modules](./01-feature-and-shared-modules.md), [dependency injection](../../02-fundamentals/05-dependency-injection.md).

## Three different problems that look alike

Mixing these up is the main reason people struggle with this error.

| Kind | Where | Typical symptom |
|------|-------|-----------------|
| **File import cycle** | TypeScript `import` statements (often via barrel `index.ts` files) | A class is `undefined` at decoration time; error mentions "undefined" at index `[N]` of the `imports` array, or a decorator receives `undefined` |
| **Provider cycle** | Constructor injection: `AuthService` ↔ `UsersService` | `Nest can't resolve dependencies of the AuthService (?)`, or a message about a possible circular dependency |
| **Module cycle** | `imports` arrays: `AuthModule` ↔ `UsersModule` | Same family of errors, reported at module level |

A provider cycle usually comes with a module cycle too, since the two services live in different modules that import each other.

## Example of a provider cycle

```ts
@Injectable()
export class UsersService {
  constructor(private readonly auth: AuthService) {}    // needs AuthService
}

@Injectable()
export class AuthService {
  constructor(private readonly users: UsersService) {}  // needs UsersService
}
```

Neither can be created before the other.

## The escape hatch: `forwardRef`

`forwardRef(() => X)` says "resolve this reference later, once both classes exist". It must be applied on **both** sides, at both levels where applicable.

**Providers:**

```ts
@Injectable()
export class UsersService {
  constructor(
    @Inject(forwardRef(() => AuthService))
    private readonly auth: AuthService,
  ) {}
}

@Injectable()
export class AuthService {
  constructor(
    @Inject(forwardRef(() => UsersService))
    private readonly users: UsersService,
  ) {}
}
```

**Modules:**

```ts
@Module({
  imports: [forwardRef(() => AuthModule)],
  providers: [UsersService],
  exports: [UsersService],
})
export class UsersModule {}

@Module({
  imports: [forwardRef(() => UsersModule)],
  providers: [AuthService],
  exports: [AuthService],
})
export class AuthModule {}
```

If only one side uses `forwardRef`, it still fails.

### Limits of `forwardRef`

- **Don't use the injected dependency inside the constructor.** Nest may not have finished wiring it yet. Use it in methods called later.
- Class instantiation order becomes **non-obvious**, which complicates debugging and lifecycle hooks.
- The cycle is still there: tests, refactors, and extraction into a package all get harder.
- Interaction with [request-scoped providers](./07-scopes-and-request-context.md) can be problematic; avoid cycles among scoped providers.

Treat `forwardRef` as a **last resort** or a temporary patch.

## Fixing the design instead

A cycle usually means two units know too much about each other, or a third concept is hiding inside both.

### 1. Extract the shared piece into a third provider

```text
Before:  UsersService ⇄ AuthService
After:   UsersService → PasswordService ← AuthService
```

If both only need password hashing or a user-lookup helper, move that into its own provider (or a lower-level module) that neither depends back on.

### 2. Invert one direction with events

When A only needs to **notify** B, don't inject B into A; emit an event.

```ts
// OrdersService no longer depends on NotificationsService
this.events.emit('order.created', order);

// NotificationsService listens
@OnEvent('order.created')
handle(order: Order) { /* ... */ }
```

See [EventEmitter](../../05-advanced/03-events-and-messaging/01-event-emitter.md). This also loosens coupling between domains.

### 3. Depend on an abstraction owned by the consumer

Let `AuthModule` define what it needs (`UserLookup` interface/token) and let `UsersModule` implement it. Dependency direction flips and the cycle breaks ([tokens](./06-injection-tokens-and-optional-dependencies.md), [clean architecture](../../08-architecture-and-patterns/01-architecture/03-clean-and-hexagonal-architecture.md)).

### 4. Move logic up into an orchestrator

If `A` and `B` need each other to complete one use case, create a higher-level service (`RegistrationService`) that depends on both; they no longer depend on each other.

### 5. Merge them

If two services can't function apart, they may be one concept. Merge into one module/service rather than keeping an artificial boundary.

### 6. Resolve lazily

For genuinely optional or rare interactions, `ModuleRef` can fetch a provider at call time instead of injecting at construction ([ModuleRef](./08-module-ref-and-lazy-loading.md)). It hides the dependency, so use it sparingly.

## File-level cycles (barrel files)

```ts
// users/index.ts
export * from './users.service';
export * from './users.module';
```

If `users.service.ts` imports from `../auth` (its barrel), and `auth` imports from `../users` (its barrel), you get an import cycle that executes in an unlucky order. Decorators then receive `undefined` instead of a class.

Fixes:

- Import **directly from the file** (`'../auth/auth.service'`) rather than through barrels, especially inside the same feature area.
- Move shared types/constants to a leaf file that imports nothing from either side.
- Use `forwardRef` only if the cycle is a real DI cycle, **not** to paper over a file cycle.

## Detecting cycles

```bash
# madge: finds circular imports in your source
npx madge --circular --extensions ts src
```

There are also ESLint rules (e.g. `import/no-cycle`). Add one to CI so new cycles don't creep in.

## Common mistakes

- **`forwardRef` on one side only.**
- **Using the dependency in the constructor** after `forwardRef`.
- **Forgetting that module cycles need `forwardRef` in `imports`** in addition to provider-level `forwardRef`.
- **Treating `forwardRef` as the fix for file import cycles.** It can't repair `undefined` classes caused by barrel files.
- **Reaching for `forwardRef` first**, then accumulating cycles until the architecture is a knot.
- **Declaring the provider in both modules** to avoid importing (creates duplicates, not a fix).

## Debugging

1. Read the error: which provider/module is named, and at which index?
2. Is the thing `undefined` at *import* time (file cycle), or unresolved at *injection* time (DI cycle)?
3. Draw the arrows (who injects whom, who imports whom). The cycle is usually obvious on paper.
4. For file cycles, run `madge --circular` and look for barrel files in the chain.
5. After refactoring, remove `forwardRef`s you no longer need.

## Quick Summary

- Circular dependencies come in three forms: file imports, provider injection, and module imports.
- `forwardRef(() => X)` on **both** sides (and in `imports`) fixes DI cycles but not file cycles, and you can't use the dependency in the constructor.
- Prefer redesign: extract a shared provider, use events, invert via an abstraction, orchestrate above, or merge.
- Avoid barrel-file cycles by importing from specific files.
- Detect cycles in CI (`madge`, `import/no-cycle`).

## Next

[Custom providers →](./05-custom-providers.md)
