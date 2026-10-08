# Mongoose Setup

Setup has three pieces: the **root connection** (`forRootAsync`), per-module **model registration** (`forFeature`), and **injecting the model** into services. This note also covers the MongoDB-specific details that surprise people: replica sets, buffering errors, and index auto-creation.

Prerequisites: [Database connection](../02-database-foundations/02-database-connection.md), [dynamic modules](../../03-core-concepts/04-modules-and-di/03-dynamic-modules.md), [configuration](../../03-core-concepts/03-configuration/README.md).

## Install

```bash
npm i @nestjs/mongoose mongoose
```

Confirm that the `@nestjs/mongoose` version you install supports your Mongoose major (check its peer dependencies); mismatches show up as install warnings or runtime type errors.

## Root connection

Configure from validated config, asynchronously ([timing trap](../../03-core-concepts/03-configuration/01-configuration-basics.md)):

```ts
// app.module.ts
@Module({
  imports: [
    ConfigModule.forRoot({ isGlobal: true }),
    MongooseModule.forRootAsync({
      inject: [ConfigService],
      useFactory: (config: ConfigService) => ({
        uri: config.getOrThrow<string>('MONGODB_URI'),
        dbName: config.get<string>('MONGODB_DB'),
        maxPoolSize: 10,
        serverSelectionTimeoutMS: 5000,     // fail in 5s instead of hanging when MongoDB is unreachable
        autoIndex: config.get('NODE_ENV') !== 'production',
      }),
    }),
    UsersModule,
  ],
})
export class AppModule {}
```

```bash
# .env
MONGODB_URI=mongodb://localhost:27017/app?replicaSet=rs0
```

| Option | Notes |
|--------|-------|
| `uri` | `mongodb://...` or `mongodb+srv://...` (Atlas/SRV). Never hard-code credentials |
| `dbName` | Overrides the database named in the URI |
| `maxPoolSize` | Max concurrent connections per process (driver option); size using [pool sizing](../02-database-foundations/02-database-connection.md) |
| `serverSelectionTimeoutMS` | How long to wait to find a usable server; keep it short so failures are visible |
| `autoIndex` | Whether Mongoose builds schema indexes on startup. Disable in production ([indexes](./06-indexes.md)) |
| `retryAttempts` / `retryDelay` | `@nestjs/mongoose` startup retry options |
| `connectionFactory` | Hook to customize the connection (for example registering global plugins) |
| `connectionName` | Name for additional connections |

Other driver options (`authSource`, TLS settings, `readPreference`, `w`) can go in the URI or options; check the MongoDB driver docs.

## Registering models per module

```ts
// users/users.module.ts
@Module({
  imports: [MongooseModule.forFeature([{ name: User.name, schema: UserSchema }])],
  providers: [UsersService],
  exports: [UsersService],
})
export class UsersModule {}
```

```ts
// users/users.service.ts
import { InjectModel } from '@nestjs/mongoose';
import { Model } from 'mongoose';

@Injectable()
export class UsersService {
  constructor(@InjectModel(User.name) private readonly userModel: Model<User>) {}

  findAll() {
    return this.userModel.find().lean().exec();
  }
}
```

- `name: User.name` is the **model name** (the string `'User'`), which also determines the collection name (Mongoose lowercases and pluralizes it to `users`).
- `@InjectModel(User.name)` uses that name as the DI token. A typo, or a model missing from `forFeature`, gives `Nest can't resolve dependencies of the UsersService (?)... UserModel`.
- The schema classes (`User`, `UserSchema`) are defined in [Schemas and models](./02-schemas-and-models.md).

Also injectable: the connection itself, for transactions and low-level work:

```ts
constructor(@InjectConnection() private readonly connection: Connection) {}
```

## Multiple connections

```ts
MongooseModule.forRoot(uriA, { connectionName: 'primary' }),
MongooseModule.forRoot(uriB, { connectionName: 'analytics' }),

MongooseModule.forFeature([{ name: Event.name, schema: EventSchema }], 'analytics'),

constructor(@InjectModel(Event.name, 'analytics') private readonly events: Model<Event>) {}
constructor(@InjectConnection('analytics') private readonly conn: Connection) {}
```

Each connection has its own pool; count them in your connection budget.

## MongoDB must be a replica set for transactions

Multi-document transactions need a **replica set** (or sharded cluster), even on your laptop. For local development run a single-node replica set:

```yaml
# docker-compose.yml (sketch)
services:
  mongo:
    image: mongo:7
    command: ['--replSet', 'rs0', '--bind_ip_all']
    ports: ['27017:27017']
```

then initiate it once (`rs.initiate()` in `mongosh`) and add `?replicaSet=rs0` to the URI. Managed services like Atlas are replica sets by default. Details in [transactions](./07-transactions.md). If you never need multi-document transactions, a standalone server works.

## Startup and shutdown

- Mongoose connects when the module initializes; with `serverSelectionTimeoutMS` a bad URI/unreachable server fails startup instead of hanging.
- Enable [shutdown hooks](../../03-core-concepts/04-modules-and-di/09-application-lifecycle.md) (`app.enableShutdownHooks()`) so connections close cleanly on `SIGTERM`.
- Health checks: a Terminus MongoDB/Mongoose indicator or a `ping` command ([health checks](../../07-production/03-observability/04-health-checks.md)).

## Testing setup

- **Unit tests:** mock the model token. Query chains are awkward to mock (`find().lean().exec()`), which is one reason to hide Mongoose behind a [repository](./03-repositories.md):

```ts
import { getModelToken } from '@nestjs/mongoose';

{ provide: getModelToken(User.name), useValue: { find: jest.fn(), create: jest.fn() } }
```

- **Integration/E2E:** use a real MongoDB (Docker or Testcontainers), or an in-memory server such as `mongodb-memory-server` (its replica-set variant if you need transactions). Clean collections between tests, wait for index creation (`await Model.init()`), and close the connection in `afterAll` ([integration testing](../01-testing/05-integration-testing.md)).

## Common mistakes

- **Reading `process.env` at import time** inside `forRoot(...)`.
- **Forgetting `forFeature`** or mismatching the model `name` and `@InjectModel` argument.
- **Leaving `autoIndex` on in production**, triggering index builds on startup.
- **No `serverSelectionTimeoutMS`**, so queries buffer silently when the DB is down.
- **Expecting transactions on a standalone MongoDB.**
- **Forgetting that each extra connection has its own pool.**
- **Hard-coding credentials** or committing connection strings.
- **Unbounded pool sizes** multiplied across replicas.

## Debugging

- `MongooseServerSelectionError` / `ECONNREFUSED`: host, port, network, TLS, or credentials; remember containers can't use `localhost` for the host machine.
- `Operation users.find() buffering timed out after 10000ms`: Mongoose isn't connected (bad URI, DB down, or you used a model before the connection was established).
- `Authentication failed`: wrong credentials or `authSource` (often `admin`).
- `Nest can't resolve dependencies of the UsersService (?)... UserModel`: add `MongooseModule.forFeature([...])` to that module.
- `Transaction numbers are only allowed on a replica set member or mongos`: connect to a replica set ([transactions](./07-transactions.md)).
- Atlas connection refused: IP allowlist or network access configuration.

## Quick Summary

- `MongooseModule.forRootAsync` creates the connection; `forFeature([{ name, schema }])` registers models; `@InjectModel(Class.name)` injects them; `@InjectConnection()` gives the connection.
- Set `serverSelectionTimeoutMS`, a sensible `maxPoolSize`, and disable `autoIndex` in production.
- Transactions need a replica set, even locally.
- Mocking chained Mongoose queries is awkward; use repositories and real-database integration tests.
- Check `@nestjs/mongoose` compatibility with your Mongoose major version.

## Next

[Schemas and models →](./02-schemas-and-models.md)
