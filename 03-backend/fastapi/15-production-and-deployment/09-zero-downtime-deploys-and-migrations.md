---
title: "Zero-Downtime Deploys and Database Migrations for FastAPI"
description: "Blue-green and canary deployments, rolling updates, expand-contract database migrations with Alembic, backward compatibility, and connection draining."
fastapi_version: "0.115+"
alembic_version: "1.14+"
python_version: "3.12+"
free_threading_compatible: true
tested_with: "Alembic 1.14.0, SQLAlchemy 2.0.36, FastAPI 0.115.6"
---

# Zero-Downtime Deploys and Database Migrations for FastAPI

## 1. Conceptual Foundation & Mental Model

Modern web applications must remain available 24/7/365. Traditional maintenance windows—where traffic is halted while the database schema is updated and new application binaries are deployed—are unacceptable in modern high-availability distributed systems.

Achieving true zero-downtime deployments requires orchestrating two distinct systems:
1. **Application Deployment Strategies**:
   - **Rolling Update**: Incrementally replaces old container pods with new ones. At peak transition, both Version $N$ and Version $N+1$ run concurrently and receive live traffic.
   - **Blue-Green Deployment**: Provisions a complete parallel stack (Green). Once validated, the load balancer switches 100% of traffic from Blue to Green instantaneously.
   - **Canary Deployment**: Routes a small percentage (e.g. 5%) of live traffic to the new version, monitoring error rates before rolling out cluster-wide.
2. **Expand-Contract (Parallel Change) Database Migrations**:
   Because Version $N$ and Version $N+1$ execute simultaneously against the same database cluster, **every database change must remain fully backward-compatible with the old application version**. Schema changes must never rename columns or drop columns in a single step!

```
Expand-Contract Migration Pattern:
Phase 1: EXPAND (Backward Compatible)
   - Add new column `full_name` (nullable or with default)
   - Keep old columns `first_name`, `last_name` intact
   - Deploy App V1: Writes to both old and new columns; reads old

Phase 2: BACKFILL & TRANSITION
   - Background job migrates historical data into `full_name`
   - Deploy App V2: Reads from `full_name`; writes to both

Phase 3: CONTRACT (Cleanup)
   - Deploy App V3: Reads and writes only `full_name`
   - Drop old columns `first_name` and `last_name` in DB
```

---

## 2. Architecture & Internal Mechanics

```
+---------------------------------------------------------------+
|                      Application Load Balancer                |
|                    Traffic Split / Ingress Router             |
+-------------------------------+-------------------------------+
                                │
         ┌──────────────────────┴──────────────────────┐
         ▼ (90% Live Traffic)                          ▼ (10% Canary)
+--------------------------------+            +--------------------------------+
|       FastAPI App: Version N   |            |      FastAPI App: Version N+1  |
|  - Queries legacy column 'name'|            |  - Queries new 'display_name'  |
+----------------+---------------+            +----------------+---------------+
                 │                                             │
                 └──────────────────────┬──────────────────────┘
                                        ▼
+---------------------------------------------------------------+
|                    Shared PostgreSQL Cluster                  |
|  Table: users                                                 |
|  - id: integer                                                |
|  - name: varchar(255)         (Kept for Version N)            |
|  - display_name: varchar(255)  (Added for Version N+1)         |
|  - Trigger / Dual-write syncing both columns                  |
+---------------------------------------------------------------+
```

### The Breaking Lock Hazard in Relational Databases
In PostgreSQL, executing `ALTER TABLE orders ADD COLUMN status varchar(50) NOT NULL;` without a default value or prior to PG 11 forces a table rewrite and acquires an `ACCESS EXCLUSIVE` lock. This lock blocks all incoming `SELECT`, `INSERT`, `UPDATE`, and `DELETE` queries. Under high traffic, this locks up all database connections and crashes the application.

**Safe PostgreSQL Migration Rules**:
- Always add columns as `NULLABLE` first, or with a default on PostgreSQL 11+ (`DEFAULT 'pending'`).
- Always create indexes concurrently: `CREATE INDEX CONCURRENTLY idx_orders_status ON orders(status);`.

---

## 3. Comprehensive Syntax & Core Mechanics

### Alembic Multi-Step Safe Migration Script
```python
"""Safe Add Column with Concurrent Index

Revision ID: 9a8b7c6d5e4f
Revises: 1a2b3c4d5e6f
Create Date: 2026-11-24
"""
from alembic import op
import sqlalchemy as sa

revision = '9a8b7c6d5e4f'
down_revision = '1a2b3c4d5e6f'
branch_labels = None
depends_on = None

def upgrade() -> None:
    # 1. Add column as NULLABLE (Zero table lock duration)
    op.add_column(
        'users',
        sa.Column('phone_number_e164', sa.String(length=20), nullable=True)
    )

    # 2. In PostgreSQL, create index concurrently outside transaction
    # (Requires autocommit_block in alembic)
    with op.get_context().autocommit_block():
        op.execute(
            "CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_users_phone "
            "ON users (phone_number_e164)"
        )

def downgrade() -> None:
    with op.get_context().autocommit_block():
        op.execute("DROP INDEX CONCURRENTLY IF EXISTS idx_users_phone")
    op.drop_column('users', 'phone_number_e164')
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: Dual-Write Compatibility Layer in SQLAlchemy Repository
```python
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import insert, update
from models import UserModel

class UserRepository:
    def __init__(self, session: AsyncSession):
        self.session = session

    async def create_user(self, first_name: str, last_name: str) -> UserModel:
        combined_name = f"{first_name} {last_name}".strip()
        
        # Dual-writing during Expand Phase: Write to both old and new columns!
        stmt = insert(UserModel).values(
            first_name=first_name,         # Old column
            last_name=last_name,           # Old column
            full_name=combined_name        # New column
        ).returning(UserModel)
        
        result = await self.session.execute(stmt)
        await self.session.commit()
        return result.scalar_one()
```

### Scenario 2: Blue-Green Deployment Routing via AWS Route53 Weighted Records
```yaml
# CloudFormation / Route53 Weighted Record Configuration
Type: AWS::Route53::RecordSetGroup
Properties:
  HostedZoneName: example.com.
  RecordSets:
    - Name: api.example.com.
      Type: A
      SetIdentifier: BlueStack
      Weight: 100 # Change to 0 after Green is validated
      AliasTarget:
        DNSName: !GetAtt BlueALB.DNSName
        HostedZoneId: !GetAtt BlueALB.CanonicalHostedZoneID
    - Name: api.example.com.
      Type: A
      SetIdentifier: GreenStack
      Weight: 0   # Change to 100 to switch 100% of traffic
      AliasTarget:
        DNSName: !GetAtt GreenALB.DNSName
        HostedZoneId: !GetAtt GreenALB.CanonicalHostedZoneID
```

### Scenario 3: Kubernetes RollingUpdate Deployment with Zero Dropped Requests
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: fastapi-production
spec:
  replicas: 6
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 2         # Add up to 2 extra pods during rollout
      maxUnavailable: 0   # Never drop below 6 healthy pods!
  template:
    spec:
      containers:
        - name: api
          image: myregistry.internal/fastapi:v2.1.0
          lifecycle:
            preStop:
              exec:
                command: ["sleep", "5"] # Allow iptables routing updates to settle
          readinessProbe:
            httpGet:
              path: /health/ready
              port: 8000
            initialDelaySeconds: 5
            periodSeconds: 2
```

### Scenario 4: Asynchronous Historical Data Backfill Job
```python
import asyncio
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker
from sqlalchemy import select, update
from models import UserModel

# Run out-of-band to migrate legacy data without blocking API traffic
async def backfill_historical_names():
    engine = create_async_engine("postgresql+asyncpg://app:secret@db.internal:5432/app")
    Session = async_sessionmaker(engine, expire_on_commit=False)
    
    batch_size = 500
    last_id = 0
    
    while True:
        async with Session() as session:
            # Query un-migrated records in indexed keyset batches
            stmt = select(UserModel).where(
                UserModel.id > last_id,
                UserModel.full_name.is_(None)
            ).order_by(UserModel.id.asc()).limit(batch_size)
            
            result = await session.execute(stmt)
            users = result.scalars().all()
            if not users:
                break
                
            for u in users:
                u.full_name = f"{u.first_name} {u.last_name}".strip()
                last_id = u.id
                
            await session.commit()
            print(f"Migrated batch up to user ID {last_id}")
            await asyncio.sleep(0.1) # Sleep to prevent DB replication lag
            
    print("Backfill complete!")
```

### Scenario 5: Canary Deployment Verification Script
```python
import time
import httpx

def verify_canary_health(canary_url: str, duration_seconds: int = 60) -> bool:
    print(f"Monitoring canary deployment at {canary_url}...")
    start_time = time.time()
    error_count = 0
    success_count = 0
    
    with httpx.Client(timeout=2.0) as client:
        while time.time() - start_time < duration_seconds:
            try:
                resp = client.get(f"{canary_url}/health/ready")
                if resp.status_code == 200:
                    success_count += 1
                else:
                    error_count += 1
            except Exception:
                error_count += 1
            time.sleep(0.5)
            
    error_rate = error_count / (success_count + error_count)
    print(f"Canary test finished. Error rate: {error_rate:.2%}")
    return error_rate < 0.01 # Pass if error rate < 1%
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Renaming a Column in a Single Alembic Revision**:
   Executing `op.alter_column('table', 'old_name', new_column_name='new_name')` is an immediate breaking change. Active Version $N$ pods will immediately fail with `ColumnDoesNotExists` on every subsequent query. Columns must be added, duplicated via dual-writing, and only dropped after all old pods have been retired.

2. **Foreign Key Constraint Validation Locks**:
   Adding a `FOREIGN KEY` constraint to a table with millions of rows locks the entire table while PostgreSQL verifies all existing rows. Instead, add the constraint with `NOT VALID` (which takes milliseconds) and subsequently run `VALIDATE CONSTRAINT` concurrently.

3. **Replication Lag Triggered by Large Batch Updates**:
   Updating 1,000,000 rows in a single SQL statement (`UPDATE users SET ...`) floods the PostgreSQL WAL (Write-Ahead Log), causing multi-minute replication lag on read replicas and locking tables. Always chunk updates in small batches (e.g. 500 rows) with sleep intervals.

4. **Dropping Tables Before Code Retirement**:
   Dropping legacy database objects while even a single instance of Version $N$ is running will cause that instance to crash. Only execute the Contract/Cleanup phase days or weeks after the new version is verified stable.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Renaming a database column directly in SQL | Crashes existing application pods immediately | Use Expand-Contract (add new column, dual-write) |
| Adding a `NOT NULL` column without a default | Forces full table lock and blocks all active queries | Add column as nullable first, backfill, then set NOT NULL |
| Running `CREATE INDEX` without `CONCURRENTLY` | Locks table for all writes while index builds | Always use `CREATE INDEX CONCURRENTLY` |
| Setting `maxUnavailable > 0` in rolling updates with few pods | Capacity drops during deployment, causing latency spikes | Set `maxUnavailable: 0` and `maxSurge: 25%` |
| Running massive `UPDATE` queries in a single transaction | Generates massive WAL replication lag and locks rows | Chunk updates in batches of 500-1000 with sleeps |
| Dropping columns immediately after rolling out new code | Prevents instant rollback if bugs are discovered | Delay column deletion until next release cycle |

---

## 7. Performance & Scalability Considerations

- **Fast Rollback Capability**: When following the Expand-Contract pattern, rolling back application code from Version $N+1$ to Version $N$ requires zero database changes because the database schema remains fully compatible with both versions.
- **Connection Draining**: Configure load balancers with a 30 to 60-second connection draining period to allow in-flight requests on retiring pods to complete gracefully.

---

## 8. Security & OWASP Guardrails

- **Database Migration Credentials**: Run Alembic migrations in CI/CD using an administrative migration user with DDL privileges (`ALTER`, `CREATE`), while the runtime FastAPI application connects using a restricted user with DML-only privileges (`SELECT`, `INSERT`, `UPDATE`, `DELETE`).
- **Canary Blast Radius Limitation**: Limit initial canary traffic to internal users or a small percentage (1-5%) of external users to minimize user impact if errors occur.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from sqlalchemy import text
from database import engine

@pytest.mark.asyncio
async def test_verify_backward_compatible_schema():
    # Verify both old and new columns coexist during expand phase
    async with engine.connect() as conn:
        result = await conn.execute(text("""
            SELECT column_name 
            FROM information_schema.columns 
            WHERE table_name = 'users' AND column_name IN ('first_name', 'full_name');
        """))
        columns = [row[0] for row in result.fetchall()]
        
        # In Expand phase, both columns MUST exist simultaneously
        assert "first_name" in columns, "Legacy column missing (breaks old pods)!"
        assert "full_name" in columns, "New column missing (breaks new pods)!"
```

---

## 10. Real-World Production Incident

**Incident**: A SaaS invoicing platform suffered a 40-minute global outage when an engineer renamed a column from `amount` to `amount_cents` in the `invoices` table.

**Root Cause**: The engineer used Alembic to rename the column in a single migration script and triggered a rolling Kubernetes update. As soon as the migration executed on PostgreSQL, the 20 existing pods running Version $N$ began throwing HTTP 500 errors on every billing request because `SELECT amount FROM invoices` failed. When the team attempted to roll back the deployment, the rollback failed because Version $N$ code could not run against the newly altered database schema.

**Resolution**: The database administrator manually renamed the column back to restore service. The team adopted the Expand-Contract rulebook: all future renames must:
1. Add the new column as nullable.
2. Dual-write in code.
3. Backfill data in background batches.
4. Switch reads to the new column.
5. Drop the old column in a subsequent release cycle.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the Expand-Contract (Parallel Change) pattern in database migrations?</summary>

The Expand-Contract pattern is an architectural technique for safely making breaking database changes without downtime. It splits changes into three distinct phases:
1. **Expand**: Add the new schema elements (e.g. new column) alongside existing ones. Both old and new versions of the application can run simultaneously.
2. **Transition**: Application dual-writes to both columns, background workers backfill historical data, and reads switch to the new column.
3. **Contract**: Once all old application versions have been retired, the legacy schema elements (old columns/tables) are safely dropped.
</details>

<details>
<summary>2. Why does <code>CREATE INDEX CONCURRENTLY</code> in PostgreSQL require special handling in Alembic?</summary>

Standard `CREATE INDEX` takes a `SHARE` lock on the table, blocking all concurrent `INSERT`, `UPDATE`, and `DELETE` queries until the entire index is built. `CREATE INDEX CONCURRENTLY` builds the index without locking writes, but it cannot run inside a multi-statement transaction block. In Alembic, concurrent index operations must be executed inside `with op.get_context().autocommit_block():` to prevent Alembic from wrapping the statement in a transaction.
</details>

<details>
<summary>3. What is the difference between a Blue-Green deployment and a Rolling deployment?</summary>

- **Rolling Deployment**: Incrementally replaces pods in the same cluster. During the deployment, pods running Version $N$ and Version $N+1$ run concurrently and share traffic.
- **Blue-Green Deployment**: Provisions a complete parallel stack running Version $N+1$ (Green) alongside Version $N$ (Blue). Traffic is switched all at once at the load balancer or DNS layer, providing instant rollback capability by switching back.
</details>

<details>
<summary>4. Why should you configure <code>maxUnavailable: 0</code> in Kubernetes RollingUpdate deployments?</summary>

Setting `maxUnavailable: 0` ensures that Kubernetes never terminates an existing healthy pod until a replacement pod is completely initialized, has passed its readiness probes, and is actively receiving traffic. This guarantees that total cluster serving capacity never dips below 100% during a deployment.
</details>

<details>
<summary>5. How do you safely add a NOT NULL column to a table with 50 million rows in PostgreSQL?</summary>

1. In PostgreSQL 11+: Add the column with a constant default: `ALTER TABLE tbl ADD COLUMN col integer NOT NULL DEFAULT 0;` (metadata-only update, executed in milliseconds).
2. Prior to PG 11:
   - Add the column as `NULLABLE` without default.
   - Backfill historical rows in small batches with sleep intervals.
   - Add a `CHECK (col IS NOT NULL) NOT VALID;` constraint (instant).
   - Run `ALTER TABLE tbl VALIDATE CONSTRAINT ...;` (validates without blocking writes).
</details>

<details>
<summary>6. What is a Canary deployment and how does it reduce deployment risk?</summary>

A Canary deployment routes a small percentage of real production traffic (e.g., 2-5%) to the new version while the remaining 95% remains on the stable version. Automated monitoring tracks latency, error rates, and exceptions on the canary pods. If any anomalies occur, traffic is instantly rolled back, affecting only a tiny fraction of users.
</details>

<details>
<summary>7. Why must database migrations be decoupled from container startup commands?</summary>

Running `alembic upgrade head` in the container startup script (`ENTRYPOINT` or `CMD`) causes multiple pods starting in parallel to execute migrations concurrently. This leads to migration lock collisions, duplicate index errors, and race conditions. Migrations should run as a standalone, single-instance Kubernetes `Job` or CI/CD deployment step *before* updating application containers.
</details>

<details>
<summary>8. How does dual-writing protect against failed deployments?</summary>

During the transition phase of a migration, the application writes data to both the old column and the new column. If the new release exhibits a critical bug and must be rolled back to the previous version, the previous version can resume immediately without data loss because the legacy columns were kept synchronized throughout the release.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Non-Breaking Column Addition in Alembic
**Task**: Write an Alembic `upgrade()` function that adds a nullable column `email_verified` of type `Boolean` to table `accounts`.
<details>
<summary>Solution</summary>

```python
from alembic import op
import sqlalchemy as sa

def upgrade():
    op.add_column(
        'accounts',
        sa.Column('email_verified', sa.Boolean(), nullable=True, server_default=sa.text('false'))
    )

def downgrade():
    op.drop_column('accounts', 'email_verified')
```
</details>

### Exercise 2 (Easy): Safe Index Creation in Alembic
**Task**: Write an Alembic snippet to create an index on `users(email)` concurrently using `autocommit_block()`.
<details>
<summary>Solution</summary>

```python
from alembic import op

def upgrade():
    with op.get_context().autocommit_block():
        op.execute("CREATE INDEX CONCURRENTLY IF NOT EXISTS idx_users_email ON users (email)")

def downgrade():
    with op.get_context().autocommit_block():
        op.execute("DROP INDEX CONCURRENTLY IF EXISTS idx_users_email")
```
</details>

### Exercise 3 (Medium): Kubernetes RollingUpdate Manifest
**Task**: Write a Kubernetes Deployment strategy block ensuring zero unavailable pods and a 25% surge during updates.
<details>
<summary>Solution</summary>

```yaml
strategy:
  type: RollingUpdate
  rollingUpdate:
    maxSurge: 25%
    maxUnavailable: 0
```
</details>

### Exercise 4 (Medium): Backward-Compatible Pydantic Schema
**Task**: Define a Pydantic model that accepts both legacy `name` and new `display_name` fields, mapping them to `display_name`.
<details>
<summary>Solution</summary>

```python
from pydantic import BaseModel, model_validator
from typing import Optional

class UserProfileDTO(BaseModel):
    display_name: str

    @model_validator(mode="before")
    def handle_legacy_name(cls, values):
        if isinstance(values, dict):
            if "display_name" not in values and "name" in values:
                values["display_name"] = values["name"]
        return values

# Validates with either new or legacy key!
assert UserProfileDTO(display_name="Alice").display_name == "Alice"
assert UserProfileDTO(name="Bob").display_name == "Bob"
```
</details>

### Exercise 5 (Hard): Asynchronous Keyset Migration Backfiller
**Task**: Write an async Python script that migrates 10,000 records from `temp_token` to `hashed_token` in batches of 200 rows with a 50ms pause between batches.
<details>
<summary>Solution</summary>

```python
import asyncio
import hashlib
from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker
from sqlalchemy import select, update
from models import TokenModel

async def backfill_tokens():
    engine = create_async_engine("postgresql+asyncpg://app:secret@localhost:5432/app")
    Session = async_sessionmaker(engine, expire_on_commit=False)
    
    last_id = 0
    batch_size = 200

    while True:
        async with Session() as session:
            stmt = select(TokenModel).where(
                TokenModel.id > last_id,
                TokenModel.hashed_token.is_(None)
            ).order_by(TokenModel.id.asc()).limit(batch_size)
            
            result = await session.execute(stmt)
            tokens = result.scalars().all()
            if not tokens:
                break

            for t in tokens:
                t.hashed_token = hashlib.sha256(t.temp_token.encode()).hexdigest()
                last_id = t.id

            await session.commit()
            print(f"Backfilled up to ID {last_id}")
            await asyncio.sleep(0.05) # Prevent DB replication lag

    print("Backfill completed successfully.")
```
</details>

### Exercise 6 (Hard): Kubernetes Pre-Sync Migration Job
**Task**: Write a complete Kubernetes `Job` manifest designed for ArgoCD or Helm hooks to run `alembic upgrade head` before a deployment rollout begins.
<details>
<summary>Solution</summary>

```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: fastapi-db-migration
  annotations:
    "helm.sh/hook": pre-install,pre-upgrade
    "helm.sh/hook-weight": "1"
    "helm.sh/hook-delete-policy": before-hook-creation,hook-succeeded
spec:
  backoffLimit: 2
  template:
    spec:
      restartPolicy: Never
      containers:
        - name: migration
          image: myregistry.internal/fastapi-app:v2.0.0
          command: ["uv", "run", "alembic", "upgrade", "head"]
          envFrom:
            - secretRef:
                name: fastapi-production-secrets
```
</details>
