# RDS and Aurora: Managed Relational Databases

RDS (Relational Database Service) runs standard relational engines (PostgreSQL, MySQL, MariaDB, Oracle, SQL Server, Db2) for you: provisioning, patching, backups, failover and monitoring are handled by AWS, but it is still **a real database server that you size, connect to and tune**. Aurora is AWS's own cloud-rebuilt MySQL- and PostgreSQL-compatible engine with a different storage layer.

Use a relational database when you need joins, transactions across rows/tables, ad-hoc queries, constraints and a flexible schema. If your access patterns are known, key-based and extremely high-scale, [DynamoDB](./02-dynamodb.md) may fit better; if you need both flexibility and scale, relational is usually the safer starting point.

Prerequisites: [VPC](../04-networking/01-vpc.md) basics (subnets, security groups) and [IAM](../01-foundations/03-iam.md).

---

## What "managed" does and doesn't mean

| AWS handles | You still handle |
|---|---|
| Hardware, OS patching, engine installation | Schema design, indexes, query performance |
| Automated backups, point-in-time restore | Choosing instance size, storage, and cost |
| Multi-AZ failover (if enabled) | Connection management and pooling |
| Minor-version patching (in maintenance windows) | **Major** version upgrades, planning and testing |
| Monitoring metrics | Security groups, credentials, encryption choices |

You **cannot** SSH into an RDS instance or access the host OS. Tuning is via **parameter groups**.

---

## RDS vs Aurora

```
RDS (standard engines)                    Aurora
┌──────────┐    EBS volume                ┌────────┐ ┌────────┐ ┌────────┐
│ instance │ ──► (one AZ; standby         │ writer │ │ reader │ │ reader │  ← compute instances
└──────────┘     copy if Multi-AZ)        └───┬────┘ └───┬────┘ └───┬────┘
                                              └──────────┼──────────┘
                                         shared distributed storage volume
                                         (copies spread across 3 AZs, auto-grows)
```

| | RDS (e.g. PostgreSQL/MySQL) | Aurora |
|---|---|---|
| Engines | Many, including Oracle/SQL Server | Aurora MySQL- and PostgreSQL-compatible only |
| Storage | EBS volume per instance (you set size; autoscaling available) | Shared cluster volume across 3 AZs, grows automatically |
| Read scaling | Up to a handful of read replicas (each with its own copy) | Many reader instances on the **same** storage; fast replica lag |
| Failover | Multi-AZ standby (typically around a minute or two) | Promotes a reader; usually faster |
| Cost | Cheaper at small/steady scale | Higher instance price; often better value for demanding or HA-critical workloads |
| Special options | Broadest engine/licence support | Serverless v2, Global Database, I/O-Optimized pricing, Limitless Database |

**Rule of thumb:** start with **RDS PostgreSQL** (or Aurora PostgreSQL) for a typical app; pick Aurora when you need faster failover, many read replicas, serverless scaling, or cross-region replication; pick other RDS engines when licensing or compatibility demands it. Test Aurora's cost for *your* workload rather than assuming it's cheaper. It bills compute, storage and I/O (or the I/O-Optimized model).

### Aurora Serverless v2

Compute that scales in **ACU (Aurora Capacity Unit)** steps between a minimum and maximum you configure. Since late 2024 it can scale to **0 ACUs** and auto-pause when idle, on supported engine versions, which is useful for dev/test and sporadic workloads. A paused database needs a few seconds to resume on the first connection, so don't use scale-to-zero where that latency is unacceptable. Check the docs for the version prerequisites and restrictions.

### Aurora DSQL (a different thing)

**Aurora DSQL** (generally available since May 2025) is a separate, serverless **distributed SQL** database with PostgreSQL compatibility, active-active multi-region, and scale-to-zero. It supports a *subset* of PostgreSQL features and uses a different concurrency model, so it is not "Aurora PostgreSQL with another billing model". Evaluate compatibility before choosing it.

---

## Creating a database

```bash
aws rds create-db-instance \
  --db-instance-identifier app-db \
  --engine postgres \
  --db-instance-class db.t4g.micro \
  --allocated-storage 20 --storage-type gp3 \
  --master-username appadmin \
  --manage-master-user-password \
  --db-subnet-group-name private-db-subnets \
  --vpc-security-group-ids sg-0db1234 \
  --no-publicly-accessible \
  --storage-encrypted \
  --backup-retention-period 7 \
  --multi-az \
  --deletion-protection
```

Choices that matter:

- **`--manage-master-user-password`**: RDS stores and rotates the admin password in **Secrets Manager** for you, so no password in your repo. See [Security and Secrets](../06-operations/03-security-and-secrets.md).
- **DB subnet group**: the set of subnets (in at least two AZs) where the database may live. Use **private** subnets.
- **`--no-publicly-accessible`** + a security group that allows the DB port **only from the app's security group**. The database should not be reachable from the internet.
- **`--storage-encrypted`**: set at creation. You can't flip encryption on for an existing unencrypted instance directly. The route is snapshot → copy the snapshot with encryption → restore.
- **`--multi-az`** for production.
- **`--deletion-protection`** plus a final snapshot when deleting.

Instance classes: `db.t*` burstable (dev/small), `db.m*` general purpose, `db.r*` memory-optimised. Graviton (`g`) classes are usually cheaper where supported.

---

## High availability, read scaling and backups: don't confuse them

| Feature | Purpose | Notes |
|---|---|---|
| **Multi-AZ** | **Availability**: synchronous standby in another AZ; automatic failover | The standby (in a classic Multi-AZ instance) **isn't readable** and doesn't add read capacity. There is also a Multi-AZ *cluster* option with readable standbys for some engines. |
| **Read replicas** | **Read scaling** (and DR options) | Asynchronous, so reads can be **stale** (replica lag). Route read-only queries there deliberately. |
| **Automated backups + PITR** | **Recovery** from mistakes | Restore to any second within the retention window. Restore creates a **new** database (new endpoint). |
| **Manual snapshots** | Long-term keep / pre-change safety | Persist until you delete them; keep costing storage. |

The classic misconception: "I have Multi-AZ, so I have backups and read scaling." No: Multi-AZ doesn't protect against `DROP TABLE` (the drop replicates instantly), and it doesn't scale reads.

For Aurora, you connect via **cluster endpoints**: the **writer** endpoint always points to the current primary; the **reader** endpoint load-balances across replicas. Use endpoints, not instance hostnames, so failover is transparent.

---

## Connecting from your application

- The database lives in private subnets, so your app (EC2, ECS task, Lambda in the VPC) must be in the same VPC (or connected to it) and allowed by the security group.
- Fetch credentials from Secrets Manager at runtime, not from env files in git.
- Use **TLS** to connect (RDS provides CA certificates).
- **Connection limits are real.** Each connection uses memory on the database, and `max_connections` depends on instance size. Apps with many processes, containers or **Lambda functions** can exhaust connections quickly.

### RDS Proxy

A managed connection pooler between your app and the database. It multiplexes many client connections onto fewer database connections, speeds up failover handling, and can use IAM auth and Secrets Manager. It's particularly useful with **Lambda**, where each concurrent execution environment would otherwise open its own connection (see [Lambda](../02-compute/02-lambda.md)). It adds cost and some feature caveats (for example connection "pinning" with certain session state), so use it when you actually have a connection-count problem. A pooler like PgBouncer inside your own infrastructure is the alternative.

```ts
// Reuse the client across warm Lambda invocations: create it OUTSIDE the handler.
import { Pool } from "pg";
const pool = new Pool({ host: process.env.DB_HOST, max: 2 /* keep tiny in Lambda */, ssl: true });
```

(Note that many production setups use IAM database authentication or fetch the secret at init time instead of embedding passwords.)

---

## Operating it

- **Parameter groups** hold engine settings (e.g. `max_connections`, logging, `shared_buffers`-type settings). Some changes need a reboot.
- **Performance Insights** and **Enhanced Monitoring** show load by SQL statement, wait events and OS metrics. These are the first tools when things are slow. Enable the slow query log for your engine.
- **Maintenance windows**: minor patches happen automatically in a window you choose; plan major upgrades deliberately. **Blue/Green deployments** let you stage a copy (e.g. a new version or schema change), test it, then switch over with little downtime.
- **Storage autoscaling** can grow storage when it fills. Set a maximum so a runaway job doesn't grow it unbounded. Storage can't be shrunk in place.
- **Stopped RDS instances restart automatically after 7 days.** AWS limits how long you can leave one stopped. Storage and backups continue to bill while stopped.
- **Schema changes** are your responsibility: manage migrations with a tool (Flyway, Liquibase, Prisma/Drizzle migrations, Alembic) in CI, not by hand.

---

## Costs

Instance hours (× 2 for Multi-AZ) + storage (and provisioned IOPS if used) + backup storage beyond the free allowance + data transfer + extras (Proxy, Performance Insights retention, cross-region replication). For Aurora: compute (or ACU-hours) + storage + I/O (unless I/O-Optimized). Savings levers: right-size, Reserved Instances / Database Savings Plans for steady load, Serverless v2 for spiky or dev workloads, stop or delete unused dev databases, and clean up old snapshots. Check the pricing pages. Rates and options change.

---

## Common mistakes

- **Publicly accessible database** with a wide-open security group.
- Opening the DB port to `0.0.0.0/0` instead of the app's security group.
- Thinking Multi-AZ = backups = read scaling.
- **Lambda → database without pooling**, leading to "too many connections".
- Not enabling encryption at creation (painful to retrofit).
- Hardcoding the master password, or using the master user for the application. Create a limited app user.
- Using instance hostnames instead of cluster endpoints with Aurora.
- Forgetting that restore-from-backup creates a **new** instance and endpoint.
- Undersized `t` (burstable) instances in production, exhausting CPU credits.
- Leaving dev databases running 24/7.
- Skipping `deletion protection` and a final snapshot.
- Missing indexes: the most common cause of "the database is slow" is a query doing a sequential scan, not insufficient hardware.

---

## Debugging

| Symptom | Check |
|---|---|
| **Timeout** connecting | Security group inbound rule (from the *client's* SG), subnet routing, NACLs, client actually in the VPC, correct endpoint and port |
| Can connect locally but not from Lambda/ECS | Lambda attached to the VPC? Security groups on both sides? Endpoints/NAT if it also needs to reach the internet |
| `password authentication failed` / auth errors | Wrong secret, rotated secret not refreshed, IAM auth token expired, wrong user/database |
| `too many connections` | Pool sizes × instances × concurrency; add RDS Proxy or limit pool size |
| Sudden slowness | Performance Insights (top SQL, waits), missing index, CPU credits exhausted on `t` class, lock contention, replica lag |
| Storage full / read-only | Storage autoscaling max, runaway logs/temp, large transactions; free space or increase storage |
| Failover happened | Events in RDS console; apps must reconnect (DNS changes), so use endpoints and retry logic |
| Replica returns stale data | Replica lag: read-your-writes needs the writer |
| Can't enable encryption | Snapshot → encrypted copy → restore |

```bash
aws rds describe-db-instances --db-instance-identifier app-db \
  --query 'DBInstances[0].[DBInstanceStatus,Endpoint.Address,MultiAZ,PubliclyAccessible]'
aws rds describe-events --source-type db-instance --source-identifier app-db --duration 1440
```

---

## Choosing between the data stores

| Need | Choose |
|---|---|
| Files/blobs, static assets, data lake | [S3](./01-s3.md) |
| Known key-based access patterns, huge scale, serverless, spiky | [DynamoDB](./02-dynamodb.md) |
| Joins, transactions, ad-hoc SQL, familiar tooling | **RDS PostgreSQL/MySQL** |
| Relational + fast failover/many readers/serverless scaling | **Aurora** (Serverless v2 for variable load) |
| Globally distributed SQL, active-active | Aurora DSQL (check feature compatibility) or Aurora Global Database |

---

## Quick Summary

- RDS = managed standard engines; Aurora = AWS-built MySQL/PostgreSQL-compatible engine with shared storage across AZs. You still own schema, queries, sizing and connections.
- **Multi-AZ = availability; read replicas = read scaling; backups/PITR = recovery.** They are different tools.
- Put databases in **private subnets**, reachable only from the app's **security group**; **encrypt at creation**; use `--manage-master-user-password` / Secrets Manager; enable deletion protection.
- Use **cluster endpoints** (Aurora) and design for reconnects after failover.
- Connection exhaustion is the classic Lambda/containers problem: pool carefully or use **RDS Proxy**.
- Aurora Serverless v2 can scale to 0 ACUs on supported versions; Aurora DSQL is a distinct distributed SQL service with a feature subset.
- Debug slowness with **Performance Insights** and indexes first.

**Next:** [VPC](../04-networking/01-vpc.md)