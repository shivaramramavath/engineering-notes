# Database Connection

Opening a database connection is expensive (TCP, TLS, authentication). Apps therefore keep a **pool** of connections open and lend them out per query. Configuring that pool, and making sure it starts, fails, and stops cleanly, is most of what "database connection" means in a Nest app.

Prerequisites: [Database architecture](./01-database-architecture.md), [configuration](../../03-core-concepts/03-configuration/README.md), [dynamic modules](../../03-core-concepts/04-modules-and-di/03-dynamic-modules.md).

## What a pool does

```text
 request A ──┐                 ┌─ conn 1 ─┐
 request B ──┼─► pool (max N) ─┼─ conn 2 ─┼──► database
 request C ──┘   queue if full └─ conn 3 ─┘
```

- A query borrows a connection and returns it when done.
- If all `max` connections are busy, further queries **wait** in a queue (with a timeout). A leaked or slow connection looks like requests hanging.
- The database itself has a **connection limit** (`max_connections` in PostgreSQL). Exceeding it produces "too many clients" errors.

## Wiring it in Nest

The connection is root infrastructure: configure it **asynchronously** from validated config, never by reading `process.env` in a decorator ([timing trap](../../03-core-concepts/03-configuration/01-configuration-basics.md)).

### TypeORM

```ts
TypeOrmModule.forRootAsync({
  inject: [ConfigService],
  useFactory: (config: ConfigService) => ({
    type: 'postgres',
    url: config.getOrThrow<string>('DATABASE_URL'),
    autoLoadEntities: true,
    synchronize: false,                       // schema comes from migrations
    extra: { max: 10 },                       // driver pool option (pg): max connections
    retryAttempts: 5,
    retryDelay: 3000,
  }),
});
```

`extra` passes options straight to the underlying driver, so pool option names depend on the driver (`pg` uses `max`, `idleTimeoutMillis`, `connectionTimeoutMillis`). Setup details: [TypeORM setup](../03-typeorm/01-setup.md).

### Prisma

```ts
@Injectable()
export class PrismaService extends PrismaClient implements OnModuleInit, OnModuleDestroy {
  async onModuleInit() {
    await this.$connect();      // fail fast at startup instead of on the first query
  }
  async onModuleDestroy() {
    await this.$disconnect();   // release the pool on shutdown
  }
}
```

The pool is configured through the datasource URL (for example the `connection_limit` and `pool_timeout` parameters for relational databases; check the Prisma docs for your version). Prisma's exact recommended shutdown handling has changed between major versions, so follow the current Nest + Prisma recipe. Setup details: [Prisma setup](../04-prisma/01-setup.md).

### Mongoose

```ts
MongooseModule.forRootAsync({
  inject: [ConfigService],
  useFactory: (config: ConfigService) => ({
    uri: config.getOrThrow<string>('MONGODB_URI'),
    maxPoolSize: 10,
    serverSelectionTimeoutMS: 5000,   // fail in 5s instead of hanging when the DB is unreachable
  }),
});
```

Setup details: [Mongoose setup](../05-mongoose/01-setup.md).

## Configuration worth getting right

| Setting | Why |
|---------|-----|
| Connection URL from env | Different per environment; never hard-coded, never committed |
| **Pool max** | Bounds concurrency to the DB; see sizing below |
| **Connect/acquire timeout** | Turns "hang forever" into a clear error |
| **Statement/query timeout** | Stops runaway queries from holding connections (for PostgreSQL, `statement_timeout`) |
| **SSL/TLS** | Required by most managed databases; verify certificates in production |
| **Startup retries** | Containers often start before the DB is ready |
| **Synchronize off** | Schema changes go through [migrations](./05-migrations.md) |

Validate the URL and required settings at startup with [configuration validation](../../03-core-concepts/03-configuration/02-configuration-validation.md) so a missing `DATABASE_URL` fails immediately.

## Sizing the pool

The number that matters is the **total across all running instances**:

```text
instances × pool max  ≤  database max_connections  (minus headroom for admin, migrations, other services)
```

Example: 10 app replicas × `max: 20` = 200 connections, which exceeds PostgreSQL's default of 100. Symptoms: intermittent "too many clients already", worse during deploys when old and new instances overlap.

Guidance:

- A bigger pool isn't faster. Past a point, extra connections add contention on the database. Start small (around 10 per instance) and measure.
- Pool size bounds **concurrent queries**, not concurrent requests. A request that holds a connection while calling an external API wastes capacity.
- With many instances or serverless functions, put a **connection pooler** in front (such as PgBouncer or a managed equivalent) so thousands of clients share a small number of real connections. Check your ORM's docs for pooler compatibility (for example, transaction-mode poolers and prepared statements).

## Startup, readiness, and shutdown

- **Fail fast at startup** (`$connect()` in `onModuleInit`, retry options, or the ORM's initialization) so a misconfigured deploy never reports healthy.
- **Health checks** should verify the DB is reachable (`SELECT 1`) without exhausting the pool ([health checks](../../07-production/03-observability/04-health-checks.md)).
- **Shutdown:** close the pool so in-flight work finishes and connections are released. Enable [shutdown hooks](../../03-core-concepts/04-modules-and-di/09-application-lifecycle.md) so `SIGTERM` triggers `onModuleDestroy` ([graceful shutdown](../../07-production/04-deployment/02-graceful-shutdown-and-process-management.md)).

## Connections you hold manually

Anything that checks out a dedicated connection must return it:

```ts
const runner = dataSource.createQueryRunner();
await runner.connect();
try {
  // ...
} finally {
  await runner.release();     // always, or the pool slowly drains
}
```

Leaked query runners and abandoned transactions are the classic cause of "everything works until it suddenly hangs". See [transactions](./04-transactions.md).

## Common mistakes

- **Reading `process.env` at import time** instead of `forRootAsync` with `ConfigService`.
- **Pool size chosen per instance** without multiplying by instance count.
- **No timeouts**, so an unreachable DB makes every request hang.
- **`synchronize: true` outside throwaway local use.**
- **Forgetting to release** manually acquired connections.
- **Holding a transaction/connection open** across slow external calls.
- **Creating a new client per request or per module** (multiple pools) instead of one shared instance.
- **Skipping TLS** on managed databases, or disabling certificate verification "to make it work".
- **Not closing the pool in tests**, so Jest hangs ([integration testing](../01-testing/05-integration-testing.md)).

## Debugging

- `ECONNREFUSED`: wrong host/port, DB not up yet, or networking/firewall; check startup retries.
- `too many clients already` / `remaining connection slots are reserved`: total connections exceed the DB limit; reduce pool max, reduce instances, or add a pooler.
- Requests hang then time out with "timeout acquiring connection": pool exhausted. Look for leaked connections, long transactions, or slow queries holding connections.
- Authentication/SSL errors: URL credentials, `sslmode`, or certificate configuration.
- Works locally, fails in the container: `localhost` inside a container isn't the host; use the service name.
- Enable query/connection logging temporarily in non-production to see what's happening.

## Quick Summary

- Apps use a **pool** of reusable connections; total connections across instances must stay under the database limit.
- Configure the connection asynchronously from validated config; set pool max, timeouts, TLS, and startup retries.
- Connect at startup to fail fast; close the pool on shutdown with lifecycle hooks.
- Always release manually acquired connections; never hold them across slow external calls.
- Use a connection pooler for many instances or serverless.

## Next

[Repository pattern →](./03-repository-pattern.md)
