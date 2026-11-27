# Distributed Transactions: 2PC, Consensus & Sagas

## Metadata
| Attribute | Details |
|:---|:---|
| **Topic** | Distributed Systems: Atomic Commitment, 2PC, XA & The Saga Pattern |
| **Dialect** | Standard ANSI SQL / PostgreSQL 17+ Baseline |
| **Database Version** | PostgreSQL 17+, MySQL 8.4 LTS, CockroachDB, YugabyteDB, Spanner |
| **Tested with** | PostgreSQL 17.2, SQLite 3.50.4 |
| **Prerequisites** | `04-transactions-and-concurrency/01-acid.md`, `04-transactions-and-concurrency/08-select-for-update-skip-locked-and-queues.md` |
| **Complexity** | Advanced / Architectural |

---

## Table of Contents
1. [What & Why](#what--why)
2. [The Distributed Dilemma: Beyond Single-Node ACID](#the-distributed-dilemma-beyond-single-node-acid)
3. [Two-Phase Commit (2PC) Protocol](#two-phase-commit-2pc-protocol)
4. [The Fatal Flaw of 2PC: The Coordinator Crash](#the-fatal-flaw-of-2pc-the-coordinator-crash)
5. [XA Standard & SQL Syntax](#xa-standard--sql-syntax)
6. [Cross-Dialect Differences](#cross-dialect-differences)
7. [Distributed SQL Engines: Paxos, Raft & TrueTime](#distributed-sql-engines-paxos-raft--truetime)
8. [Practical Architecture: The Saga Pattern](#practical-architecture-the-saga-pattern)
9. [The Transactional Outbox Pattern + CDC](#the-transactional-outbox-pattern--cdc)
10. [Practical Examples](#practical-examples)
11. [Best Practices](#best-practices)
12. [Common Mistakes & Anti-Patterns](#common-mistakes--anti-patterns)
13. [Comparison Matrix: Distributed Consistency Models](#comparison-matrix-distributed-consistency-models)
14. [Real-World Interview Questions](#real-world-interview-questions)
15. [Practice Exercises](#practice-exercises)
16. [Summary & Further Reading](#summary--further-reading)

---

## What & Why

In a single-instance relational database, the engine guarantees ACID semantics with mathematical certainty: the buffer pool, lock manager, and Write-Ahead Log all operate within a single shared memory address space and flush to local non-volatile storage via a local kernel `fsync()`.

However, modern enterprise architectures frequently transcend the boundaries of a single database node:
- **Database Sharding**: Splitting customer data across 10 physical database clusters to scale write throughput.
- **Heterogeneous Databases**: Modifying a relational ledger in PostgreSQL while mutating a search index in Elasticsearch.
- **Microservice Architectures**: An e-commerce checkout that must coordinate the Order Service, Inventory Service, and Payment Service, each running its own independent database.

When an atomic business operation spans multiple independent network nodes, single-node transactions are powerless. If Node A commits successfully, but Node B crashes or drops the network packet, data consistency collapses. 

To solve this, distributed systems must implement **Distributed Atomic Commitment Protocols** (such as Two-Phase Commit) or re-architect the domain around **Compensating Workflows** (such as the Saga Pattern).

---

## The Distributed Dilemma: Beyond Single-Node ACID

In distributed environments, the system must navigate the **CAP Theorem** (Consistency, Availability, Partition Tolerance) and the **PACELC Theorem** (If Partitioned: Availability vs Consistency; Else: Latency vs Consistency):
- A network partition **will occur** eventually.
- If two database nodes cannot communicate, the system must choose between:
  - **Refusing the transaction** to preserve strict consistency (CP system).
  - **Allowing the transaction** to proceed on one node, accepting that data will temporarily diverge (AP system).

Traditional relational databases are designed as **CA** locally, but become strictly **CP** when coordinated across a network.

---

## Two-Phase Commit (2PC) Protocol

The canonical algorithm for achieving atomic commitment across multiple distributed database nodes is the **Two-Phase Commit (2PC)** protocol.

2PC involves two roles:
1. **The Coordinator**: The service or node orchestrating the global transaction.
2. **The Cohorts (Participants)**: The individual database nodes executing the local SQL operations.

```
+-----------------------------------------------------------------------------------------+
|                                TWO-PHASE COMMIT (2PC) TIMELINE                          |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|       COORDINATOR                       COHORT A (Database 1)     COHORT B (Database 2) |
|       ===========                       ====================     ====================   |
|            |                                     |                         |            |
|            | 1. Execute SQL (DML)                |                         |            |
|            |------------------------------------>|                         |            |
|            |-------------------------------------------------------------->|            |
|            |                                     |                         |            |
|            | [ PHASE 1: PREPARE ]                |                         |            |
|            | "Can you commit?" (PREPARE)         |                         |            |
|            |------------------------------------>|                         |            |
|            |-------------------------------------------------------------->|            |
|            |                                     |                         |            |
|            |                                     | (Writes WAL to disk)    | (Writes WAL)
|            |                                     | "VOTE YES / PREPARED"   | "VOTE YES" |
|            |<------------------------------------|                         |            |
|            |<--------------------------------------------------------------|            |
|            |                                     |                         |            |
|            | [ PHASE 2: COMMIT ]                 |                         |            |
|            | All voted YES -> "COMMIT PREPARED"  |                         |            |
|            |------------------------------------>|                         |            |
|            |-------------------------------------------------------------->|            |
|            |                                     |                         |            |
|            |                                     | (Commits transaction)   | (Commits)  |
|            |                                     | "ACK COMMITTED"         | "ACK"      |
|            |<------------------------------------|                         |            |
|            |<--------------------------------------------------------------|            |
|            v                                     v                         v            |
|      GLOBAL COMMIT COMPLETE!                                                            |
+-----------------------------------------------------------------------------------------+
```

### The Invariant of Phase 1
When a cohort votes **"PREPARED"**, it makes an unbreakable contractual promise to the coordinator:
- It has flushed all necessary WAL records to non-volatile disk.
- It has verified all schema constraints and row locks.
- Even if the cohort server loses power, catches fire, and reboots 10 hours later, it **guarantees** that it can still commit the transaction upon receiving the coordinator's command.

---

## The Fatal Flaw of 2PC: The Coordinator Crash

While 2PC guarantees atomicity, it is a **blocking protocol** that suffers from a devastating single point of failure:

```
Situation:
1. Coordinator sends PREPARE to Cohort A and Cohort B.
2. Both cohorts vote YES (Prepared).
3. Coordinator writes "GLOBAL COMMIT" to its local log.
4. COORDINATOR HARDWARE POWER DIES before sending "COMMIT" to the cohorts!
```

### The "In-Doubt" Crisis
- Cohort A and Cohort B are now stuck in the **IN-DOUBT (Prepared)** state.
- They **cannot unilaterally abort**: the coordinator may have sent "COMMIT" to another node before dying.
- They **cannot unilaterally commit**: another cohort may have voted "NO".
- **Result**: Both databases must hold all row locks, table locks, and snapshots open indefinitely.
- The held locks block all subsequent production transactions. If the coordinator cannot be recovered, a human database administrator must manually log into each database node and issue `ROLLBACK PREPARED` or `COMMIT PREPARED`.

Because of this blocking vulnerability and the extreme latency penalties of multi-node network round-trips, **2PC is widely rejected in modern cloud-native architectures**.

---

## XA Standard & SQL Syntax

The Open Group created the **XA Standard** (part of the Distributed Transaction Processing model, DTP) to formalize 2PC syntax in relational SQL engines:

### PostgreSQL 2PC Syntax
```sql
-- Step 1: Open transaction and execute work
BEGIN;
UPDATE customer_wallets SET balance = balance - 100.00 WHERE user_id = 42;

-- Step 2: Phase 1 (Prepare)
PREPARE TRANSACTION 'global_tx_transfer_42_to_99';
-- The transaction is detached from the current session and recorded in pg_prepared_xacts.
-- Locks remain held on user_id = 42!

-- Step 3: Phase 2 (Executed by Coordinator upon unanimous YES vote)
COMMIT PREPARED 'global_tx_transfer_42_to_99';

-- Or, if any node voted NO:
-- ROLLBACK PREPARED 'global_tx_transfer_42_to_99';
```

### MySQL XA Syntax
```sql
-- Phase 1
XA START 'global_tx_001';
UPDATE inventory SET stock = stock - 1 WHERE sku = 'MACBOOK_PRO';
XA END 'global_tx_001';
XA PREPARE 'global_tx_001';

-- Phase 2
XA COMMIT 'global_tx_001';
-- Or: XA ROLLBACK 'global_tx_001';
```

---

## Cross-Dialect Differences

```
+----------------------------------------------------------------------------------------------------+
| DIALECT COMPARISON: DISTRIBUTED TRANSACTION PRIMITIVES                                             |
+-------------------+--------------------------------------------------------------------------------+
| PostgreSQL 17+    | Supports PREPARE TRANSACTION / COMMIT PREPARED / ROLLBACK PREPARED. Must set   |
|                   | max_prepared_transactions > 0 (default is 0 / disabled!).                      |
+-------------------+--------------------------------------------------------------------------------+
| MySQL 8.4 (InnoDB)| Native XA transactions (XA START, XA END, XA PREPARE, XA COMMIT, XA ROLLBACK).  |
|                   | Used internally by MySQL replication and MySQL Shell / InnoDB Cluster.         |
+-------------------+--------------------------------------------------------------------------------+
| SQLite 3.50+      | Not supported. SQLite is an embedded single-machine database.                  |
+-------------------+--------------------------------------------------------------------------------+
| SQL Server 2022   | Supports Microsoft Distributed Transaction Coordinator (MSDTC) via 2PC.       |
|                   | High latency overhead; deprecated in modern cloud architectures.               |
+-------------------+--------------------------------------------------------------------------------+
```

---

## Distributed SQL Engines: Paxos, Raft & TrueTime

Modern NewSQL / Distributed SQL databases eliminate the coordinator bottleneck of 2PC by replacing fragile centralized coordinators with **distributed consensus algorithms**:

1. **Google Spanner**:
   - Uses **TrueTime API** (atomic clocks and GPS receivers in every Google datacenter) to guarantee bounded clock skew ($\le 7\text{ms}$).
   - Uses **Paxos** consensus for replicated state machines within each shard.
   - Uses 2PC *across* Paxos groups for cross-shard transactions, using TrueTime to achieve linearizable serializability worldwide.
2. **CockroachDB & YugabyteDB**:
   - Open-source distributed SQL engines compatible with the PostgreSQL wire protocol.
   - Replaces hardware atomic clocks with **Hybrid Logical Clocks (HLC)**.
   - Replaces centralized 2PC with **Raft consensus**: transaction status is recorded as a replicated record in a distributed Raft group, eliminating single-coordinator failure.

---

## Practical Architecture: The Saga Pattern

In microservices and decoupled enterprise architectures, cross-database 2PC is abandoned in favor of the **Saga Pattern**.

A **Saga** is a sequence of local database transactions. Each local transaction updates the database within a single service and publishes an event or message. If a local transaction fails along the way, the Saga executes a series of **Compensating Transactions** that undo the changes made by earlier transactions.

```
Normal Execution (Happy Path):
[ Create Order (Pending) ] ---> [ Deduct Payment ] ---> [ Reserve Inventory ] ---> [ Order Confirmed ]

Failure at Step 3 (Inventory Out of Stock):
[ Create Order (Pending) ] ---> [ Deduct Payment ] ---> [ Reserve Fails! ]
                                         |
                                         v (Trigger Compensation)
                                [ Refund Payment ] <--- [ Cancel Order ]
```

### Saga Flavors:
1. **Choreography**: Services listen to domain events on a message bus (Kafka/RabbitMQ) and independently trigger local transactions without a central controller.
2. **Orchestration**: A centralized Saga Orchestrator (e.g., Temporal, AWS Step Functions, or custom engine) coordinates the sequence, sends commands to services, and manages compensation trees if any step fails.

---

## The Transactional Outbox Pattern + CDC

To reliably publish domain events from a local database transaction to an event bus without dual-write bugs, the industry standard is **Change Data Capture (CDC)** paired with the **Transactional Outbox Pattern**:

```
+-----------------------------------------------------------------------------------------+
|                               TRANSACTIONAL OUTBOX WITH CDC                             |
+-----------------------------------------------------------------------------------------+
|                                                                                         |
|  1. APPLICATION SERVICE                                                                 |
|     BEGIN;                                                                              |
|     INSERT INTO orders (id, total) VALUES (101, 250.00);                                |
|     INSERT INTO outbox_events (event_id, payload) VALUES (UUID, '{"order": 101}');      |
|     COMMIT; (100% ACID Guaranteed Atomic locally!)                                      |
|                                                                                         |
|  2. CHANGE DATA CAPTURE (DEBEZIUM / POSTGRES WAL)                                       |
|     - Debezium continuously streams the PostgreSQL Write-Ahead Log (pg_wal).            |
|     - Extracts every new row inserted into outbox_events directly from WAL.             |
|     - Streams the event payload directly to Apache Kafka or RabbitMQ!                   |
|                                                                                         |
|  3. DOWNSTREAM MICROSERVICES                                                            |
|     - Inventory Service and Notification Service consume from Kafka.                    |
|     - Process locally with idempotency keys.                                            |
+-----------------------------------------------------------------------------------------+
```

---

## Practical Examples

### Example 1: Demonstrating PostgreSQL 2PC (Two-Phase Commit)
```sql
-- Prerequisites: max_prepared_transactions must be > 0 in postgresql.conf

-- On Database Node 1:
BEGIN;
UPDATE inventory SET stock = stock - 1 WHERE sku = 'SERVER_RACK_01';
PREPARE TRANSACTION 'tx_cluster_checkout_881';

-- On Database Node 2 (Concurrently):
BEGIN;
UPDATE user_balances SET balance = balance - 1200.00 WHERE user_id = 902;
PREPARE TRANSACTION 'tx_cluster_checkout_881';

-- Inspect held prepared transactions:
SELECT gid, prepared, owner, database FROM pg_prepared_xacts;

-- If both nodes prepared successfully, Coordinator issues:
-- Node 1:
COMMIT PREPARED 'tx_cluster_checkout_881';
-- Node 2:
COMMIT PREPARED 'tx_cluster_checkout_881';
```

### Example 2: Implementing a Compensating Transaction in SQL (Saga Step)
When a downstream step fails, the system executes an explicit compensating update:
```sql
-- Forward Transaction: Authorize and deduct balance
UPDATE user_accounts 
SET balance = balance - 50.00,
    version = version + 1
WHERE user_id = 7801 AND balance >= 50.00;

-- Downstream service fails: Inventory is out of stock!
-- Saga Compensating Transaction: Credit the exact deducted amount back with audit trail:
BEGIN;

UPDATE user_accounts 
SET balance = balance + 50.00,
    version = version + 1
WHERE user_id = 7801;

INSERT INTO ledger_audit (user_id, entry_type, amount, reason)
VALUES (7801, 'COMPENSATION_CREDIT', 50.00, 'Order 99401 cancelled: Inventory unavailable');

COMMIT;
```

### Example 3: Edge Case — Handling Abandoned In-Doubt Transactions
Detecting and safely terminating prepared transactions after a coordinator crash:
```sql
-- Query prepared transactions older than 30 minutes:
SELECT 
    gid,
    prepared,
    owner,
    database,
    CURRENT_TIMESTAMP - prepared AS in_doubt_age
FROM pg_prepared_xacts
WHERE prepared < CURRENT_TIMESTAMP - INTERVAL '30 minutes';

-- DBA manually resolves in-doubt transaction based on coordinator log:
ROLLBACK PREPARED 'tx_cluster_checkout_881';
```

### Example 4: Failure Mode — Microservices Dual-Write Anti-Pattern
```python
# CATASTROPHIC ANTI-PATTERN: Dual Write
def create_order(order_data):
    # Step 1: Write to local SQL database
    db.execute("INSERT INTO orders VALUES (...)")
    db.commit() # Database committed!
    
    # Step 2: Publish event to Kafka
    # NETWORK DROPS OR SERVER OOM-KILLED RIGHT HERE!
    kafka_producer.send("order_events", order_data) # NEVER RUNS!
    
    # Result: The Order exists in the database, but Payment and Inventory
    # services NEVER receive the event! Data is permanently corrupted!
```

### Example 5: Optimized Outbox Table Schema for High-Throughput CDC
```sql
CREATE TABLE transactional_outbox (
    event_id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    aggregate_type VARCHAR(64) NOT NULL,
    aggregate_id VARCHAR(128) NOT NULL,
    event_type VARCHAR(64) NOT NULL,
    payload JSONB NOT NULL,
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL
);

-- Note: In Debezium CDC setups, you do NOT update or delete from outbox immediately!
-- Debezium reads the WAL directly. A separate periodic truncation or partitioning
-- prunes old outbox records daily.
```

---

## Best Practices

1. **Avoid Distributed 2PC in Microservices**: Never use Two-Phase Commit across HTTP APIs or across microservice network boundaries. Network latency, coordinator crashes, and held locks will destroy system availability.
2. **Use the Transactional Outbox Pattern for Asynchronous Events**: Whenever database state changes must trigger downstream services, write the event into an `outbox` table in the same local ACID transaction.
3. **Design for Idempotency in All Consumers**: In distributed systems, messages are delivered "at-least-once". Every downstream consumer must use idempotency tokens or unique constraints to ignore duplicate deliveries.
4. **Always Provide Compensating Actions for Sagas**: Every forward business action (e.g., `DeductBalance`, `ReserveSeat`, `ChargeCard`) must have an engineered, tested compensating counter-action (`RefundBalance`, `ReleaseSeat`, `VoidCharge`).
5. **Set Alerting on `pg_prepared_xacts`**: If your architecture uses PostgreSQL 2PC (e.g., via Citus or an application coordinator), set continuous Prometheus alerts on `COUNT(*) FROM pg_prepared_xacts`. An in-doubt transaction held for more than 5 minutes indicates an orphaned coordinator that will cause severe vacuum bloat!

---

## Common Mistakes & Anti-Patterns

| Symptom | Root Cause | Correct Approach |
|:---|:---|:---|
| **Whole database freezes after microservice crash** | A 2PC coordinator crashed during Phase 1, leaving transactions stuck in `PREPARED` holding row locks forever. | Monitor and roll back abandoned prepared transactions; migrate to the Saga pattern. |
| **Order created, but payment never processed** | Attempting dual writes: updating SQL database and publishing to message broker in separate statements. | Use the Transactional Outbox Pattern with Debezium CDC. |
| **Saga compensation fails due to missing state** | Designing compensating transactions that assume initial data hasn't changed (e.g., balance was withdrawn in interim). | Implement Semantic Locks (e.g., holding funds in `PENDING` rather than immediate deduction). |
| **Catastrophic vacuum bloat on Postgres cluster** | `max_prepared_transactions` enabled, and an unmonitored prepared transaction pinned the `xmin` horizon for days. | Audit `pg_prepared_xacts` continuously; terminate stale prepared transactions. |
| **Clock drift causing data anomalies in Spanner clones** | Attempting to build a distributed SQL cluster using raw NTP without atomic clocks or hybrid logical clocks. | Use established NewSQL engines (CockroachDB, YugabyteDB) with Hybrid Logical Clocks. |
| **Out-of-order event processing in downstream consumers** | Microservices consuming concurrent outbox events without partition key ordering. | Partition outbox events by `aggregate_id` (e.g., `order_id`) to preserve strict FIFO processing per entity. |

---

## Performance & Security Notes

### Latency Math: Single-Node ACID vs Distributed 2PC
- **Single-Node PostgreSQL Commit**:
  - Local in-memory lock acquisition: $\approx 10\text{ nanoseconds}$.
  - Sequential SSD WAL `fsync()`: $\approx 50\text{ microseconds}$.
  - Total transaction latency: **$< 1\text{ millisecond}$**.
- **Distributed Two-Phase Commit (2 Nodes across AZs / Datacenters)**:
  - Round-trip ping between datacenters (Phase 1 Prepare): $\approx 20\text{ milliseconds}$.
  - Cohort WAL `fsync()` on both nodes: $\approx 5\text{ milliseconds}$.
  - Round-trip ping between datacenters (Phase 2 Commit): $\approx 20\text{ milliseconds}$.
  - Cohort commit acknowledgment: $\approx 5\text{ milliseconds}$.
  - Total transaction latency: **$50 - 100\text{ milliseconds}$** (100x slower!).
  - Because locks are held throughout the entire 100ms window, throughput on contentious rows drops from 20,000 tx/sec to 10 tx/sec!

### Security: Coordinator Impersonation in 2PC
In PostgreSQL and MySQL, any connection with sufficient privileges can issue `COMMIT PREPARED 'gid'` or `ROLLBACK PREPARED 'gid'`. In distributed clusters, network communication between coordinators and cohorts must be secured via mTLS, and prepared transaction identifiers must incorporate cryptographically secure nonces or HMAC signatures to prevent unauthorized transaction hijacking.

---

## Comparison Matrix: Distributed Consistency Models

| Architecture | Protocol | Consistency Guarantee | Availability Profile | Latency Profile | Best Use Case |
|:---|:---|:---|:---|:---|:---|
| **Single-Node RDBMS** | Local ACID (WAL + 2PL/MVCC) | Strict Serializability | CA (High until node dies) | Sub-millisecond | Standard enterprise OLTP |
| **Distributed 2PC** | Two-Phase Commit | Strict Serializability | CP (Blocks on coordinator failure) | Very High (100ms+) | Sharded financial databases |
| **Distributed SQL (NewSQL)**| Raft / Paxos + HLC / TrueTime | Strict Serializability / Linearizability | CP (Fault-tolerant consensus) | Medium (10-30ms) | Multi-region globally distributed DB |
| **The Saga Pattern** | Local ACID + Compensating Events | Eventual Consistency | AP (High availability) | Low (Asynchronous) | Microservices, cross-domain workflows |
| **Transactional Outbox** | Local ACID + CDC streaming | At-Least-Once Delivery | AP (Decoupled producers/consumers) | Low (Near-real-time) | Event-driven microservices |

---

## Real-World Interview Questions

<details>
<summary><strong>1. Why is Two-Phase Commit (2PC) considered a "blocking" protocol?</strong></summary>

**Answer:**
2PC is considered a blocking protocol because if the **coordinator crashes after the cohorts have voted "YES / PREPARED" in Phase 1**, but before sending the Phase 2 commit or abort decision, the cohorts cannot make a decision on their own.

A cohort that voted YES has promised not to abort, but it cannot commit without knowing if all other cohorts also voted YES. As a result, the cohorts must remain blocked in the "In-Doubt" state, holding all acquired row locks and resources indefinitely until the coordinator recovers or an administrator intervenes. These held locks can cause cascading lockups across the entire database.
</details>

<details>
<summary><strong>2. How does the Saga Pattern handle transaction failures compared to 2PC?</strong></summary>

**Answer:**
- In **2PC**, if any participant encounters an error during Phase 1, the entire distributed transaction is rolled back atomically before any changes are finalized. No intermediate state is ever made visible to the outside world.
- In the **Saga Pattern**, each step executes and **commits immediately** as a local ACID transaction in its respective service database. Intermediate state is immediately visible. If Step 4 fails, the Saga engine does not roll back the database; instead, it executes forward **Compensating Transactions** (e.g., calling `RefundPayment` to counteract `ChargePayment`). Sagas trade isolation for high availability and low latency, operating under eventual consistency.
</details>

<details>
<summary><strong>3. What is the Dual-Write Problem, and why is it impossible to solve with application-level try/catch blocks?</strong></summary>

**Answer:**
The Dual-Write Problem occurs when an application must update two distinct systems (e.g., a relational database and a Kafka event stream). 

An application cannot solve this with `try/catch` because network communication can fail between the two steps:
- If you write to the database first and the network drops before publishing to Kafka, the message is never sent.
- If you publish to Kafka first and the database transaction aborts due to a constraint violation, an event describing a non-existent state has already been broadcast.
- Even if both succeed, if the server process is killed (`SIGKILL` or power outage) between the two lines of code, the systems permanently desynchronize.
The only reliable solution is to write both records to the **same local database atomically** using the Transactional Outbox Pattern.
</details>

<details>
<summary><strong>4. How does Google Spanner achieve distributed serializability across datacenters worldwide?</strong></summary>

**Answer:**
Google Spanner combines three core technologies:
1. **TrueTime API**: A synchronized global clock backed by GPS receivers and atomic clocks in every Google datacenter, guaranteeing that clock uncertainty ($\epsilon$) is bounded (typically $< 7\text{ms}$).
2. **Paxos Consensus**: Data is sharded into splits, and each split is replicated across a Paxos group of nodes for fault tolerance.
3. **Commit Wait**: When a transaction commits, Spanner assigns it a timestamp $s = \text{TrueTime.now().latest}$. The coordinator deliberately waits for $2\epsilon$ time before acknowledging the commit to the client. This guarantees that any subsequent transaction anywhere in the world will receive a timestamp strictly greater than $s$, enforcing strict external consistency (linearizability) without cross-datacenter locking for reads.
</details>

<details>
<summary><strong>5. What is the difference between Saga Choreography and Saga Orchestration?</strong></summary>

**Answer:**
- **Saga Choreography**: There is no central controller. Each microservice completes its local transaction and publishes a domain event to an event broker. Downstream services listen for that event, execute their local transaction, and emit their own events. If an error occurs, services listen for failure events and trigger compensating transactions. Best for simple workflows with 2-3 steps.
- **Saga Orchestration**: A centralized Orchestrator service (or workflow engine like Temporal/Camunda) defines the entire state machine. The orchestrator explicitly sends commands to services, tracks completion, handles timeouts, and orchestrates the exact sequence of compensating transactions if a step fails. Best for complex, mission-critical enterprise workflows.
</details>

<details>
<summary><strong>6. In PostgreSQL, what happens to dead tuples when a PREPARE TRANSACTION is forgotten?</strong></summary>

**Answer:**
A prepared transaction behaves like an active, open transaction: it registers its `xmin` in shared memory. 
If an administrator or coordinator forgets to issue `COMMIT PREPARED` or `ROLLBACK PREPARED`, the prepared transaction **pins the cluster's global `xmin` horizon indefinitely**, even surviving database server reboots!

As a result, `VACUUM` is blocked from reclaiming dead tuples across all tables in the cluster, leading to catastrophic table bloat, index degradation, and eventually bringing the database down in an emergency TXID Wraparound shutdown.
</details>

<details>
<summary><strong>7. How does Debezium Change Data Capture (CDC) extract outbox events without impacting database read performance?</strong></summary>

**Answer:**
Debezium does **not** poll the database using `SELECT * FROM outbox`. Polling queries waste database CPU, trigger lock contention, and create dead tuple churn.

Instead, Debezium connects to PostgreSQL as a **logical replication client** and directly reads the sequential append-only **Write-Ahead Log (`pg_wal`)**. As transactions commit locally, PostgreSQL writes the outbox row insertions into the WAL stream. Debezium parses the WAL stream at the byte level, extracts the JSON payloads, and forwards them asynchronously to Kafka with near-zero latency and negligible database query overhead.
</details>

<details>
<summary><strong>8. What is a "Semantic Lock" in the Saga Pattern?</strong></summary>

**Answer:**
Because Sagas commit each local transaction immediately, they lack the **Isolation** property of ACID (anomalies like dirty reads and lost updates can occur at the business level).

A **Semantic Lock** is an application-level design pattern that prevents concurrent modifications during an active Saga. Instead of modifying live values directly, the entity is placed in a pending state:
- An order is marked `status = 'PENDING_APPROVAL'`.
- A bank balance deduction is marked `held_funds = held_funds + 50.00` rather than mutating `available_balance`.
Other operations check this flag and refuse to mutate the resource until the Saga completes or is compensated, recreating isolation at the application tier.
</details>

---

## Practice Exercises

<details>
<summary><strong>Exercise 1: Executing a Safe 2PC Prepared Transaction in PostgreSQL</strong></summary>

**Problem:** Using PostgreSQL, open a transaction, insert an audit record into a temporary table, prepare the transaction with identifier `'audit_check_01'`, inspect the system catalog `pg_prepared_xacts`, and then roll it back.

**Solution:**
```sql
-- Note: Requires max_prepared_transactions > 0 in postgresql.conf
CREATE TABLE audit_trail (id INT, note TEXT);

BEGIN;
INSERT INTO audit_trail VALUES (1, 'Testing 2PC');
PREPARE TRANSACTION 'audit_check_01';

-- Verify it appears in the system view:
SELECT gid, prepared, owner FROM pg_prepared_xacts;
-- Returns: gid = 'audit_check_01'

-- Roll it back:
ROLLBACK PREPARED 'audit_check_01';

SELECT COUNT(*) FROM audit_trail;
-- Returns 0!
```
</details>

<details>
<summary><strong>Exercise 2: Designing an Outbox Table Schema</strong></summary>

**Problem:** Write the PostgreSQL DDL for a production `outbox_events` table supporting high-throughput CDC with partition key, event type, JSONB payload, and indexing.

**Solution:**
```sql
CREATE TABLE outbox_events (
    event_id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    aggregate_type VARCHAR(64) NOT NULL,
    aggregate_id VARCHAR(128) NOT NULL,
    event_type VARCHAR(64) NOT NULL,
    payload JSONB NOT NULL,
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL
);

CREATE INDEX idx_outbox_ordering ON outbox_events(aggregate_id, created_at ASC);
```
</details>

<details>
<summary><strong>Exercise 3: Building a Saga Orchestrator Step Function in SQL</strong></summary>

**Problem:** Write a stored procedure that executes Step 1 of a Saga (Debit Wallet). If the balance is insufficient, return an error code; if successful, deduct balance and write an audit record with `status = 'PENDING'`.

**Solution:**
```sql
CREATE OR REPLACE PROCEDURE saga_step_debit_wallet(
    p_user_id INT, 
    p_amount NUMERIC, 
    OUT p_success BOOLEAN
) LANGUAGE plpgsql AS $$
BEGIN
    UPDATE user_wallets
    SET balance = balance - p_amount,
        held_balance = held_balance + p_amount
    WHERE user_id = p_user_id
      AND balance >= p_amount;
      
    IF FOUND THEN
        p_success := TRUE;
    ELSE
        p_success := FALSE;
    END IF;
END;
$$;
```
</details>

<details>
<summary><strong>Exercise 4: Auditing Orphaned Prepared Transactions</strong></summary>

**Problem:** Write an automated monitoring query in PostgreSQL that returns an alert if any prepared transaction has been in-doubt for longer than 15 minutes.

**Solution:**
```sql
SELECT 
    gid,
    prepared,
    owner,
    database,
    CURRENT_TIMESTAMP - prepared AS age,
    'CRITICAL: In-doubt transaction blocking vacuum' AS alert_level
FROM pg_prepared_xacts
WHERE prepared < CURRENT_TIMESTAMP - INTERVAL '15 minutes';
```
</details>

<details>
<summary><strong>Exercise 5: Implementing a Compensating Refund Step</strong></summary>

**Problem:** Write the SQL query to execute a compensating action for the wallet debit in Exercise 3, reversing the held balance and restoring available balance.

**Solution:**
```sql
UPDATE user_wallets
SET balance = balance + :refund_amount,
    held_balance = held_balance - :refund_amount
WHERE user_id = :user_id;
```
</details>

<details>
<summary><strong>Exercise 6: Calculating Two-Phase Commit Latency Cost</strong></summary>

**Problem:** If single-node commit latency is 0.5ms and cross-datacenter network round-trip time (RTT) is 25ms, calculate the total commit latency of a 2PC transaction involving 3 datacenters.

**Solution:**
In 2PC, the coordinator must communicate with all cohorts across two sequential phases:
1. **Phase 1 (Prepare)**: Coordinator broadcasts PREPARE to all datacenters and waits for the slowest cohort to respond: $1 \times \text{RTT} = 25\text{ms} + \text{fsync} (1\text{ms}) = 26\text{ms}$.
2. **Phase 2 (Commit)**: Coordinator broadcasts COMMIT to all datacenters and waits for acknowledgment: $1 \times \text{RTT} = 25\text{ms} + \text{fsync} (1\text{ms}) = 26\text{ms}$.
3. Total Latency $\approx 26\text{ms} + 26\text{ms} = \mathbf{52\text{ms}}$.
Compared to single-node 0.5ms, distributed 2PC is **over 100x slower**.
</details>

---

## Summary & Further Reading

- Single-node ACID cannot scale across network partitions; distributed systems must choose between availability and strict consistency (CAP theorem).
- **Two-Phase Commit (2PC)** guarantees distributed atomicity across database nodes, but is a **blocking protocol** that freezes resources if the coordinator crashes during Phase 1.
- Modern cloud systems replace 2PC with **Distributed SQL (NewSQL)** engines utilizing Raft/Paxos consensus (CockroachDB, YugabyteDB, Spanner).
- In microservices, distributed transactions are replaced by the **Saga Pattern** (local transactions + compensating actions) and the **Transactional Outbox Pattern** with Change Data Capture (CDC).
- **Module Conclusion**: This completes Module 04! Next, proceed to [Module 05: Indexing, Query Optimization & Tuning](../05-indexing-and-performance/README.md) to master physical B-Trees, execution plan cost modeling, and join algorithms.
