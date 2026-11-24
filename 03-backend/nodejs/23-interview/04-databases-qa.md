# Node.js Technical Interview: Databases, Caching & Scaling Q&A

> In-depth architectural questions and answers on PostgreSQL connection pooling, the N+1 problem, transaction isolation, Redis caching topologies, and distributed locking.

---

### Q1: How does Connection Pooling work in Node.js database drivers (`pg.Pool`)? What happens during pool exhaustion, and how should it be tuned?

#### In-Depth Answer
Because Node.js is asynchronous and single-threaded, a single Node.js process cannot execute multiple queries concurrently over a single TCP socket connection to PostgreSQL.
**`pg.Pool`** manages a cache of warm, pre-authenticated TCP connections:
1. When `pool.query()` or `pool.connect()` is called:
   - If an idle connection is available in the pool, it is leased immediately.
   - If no connection is idle and the total connections are below `max`, a new connection is negotiated.
   - If the pool has reached `max` (e.g. 20 connections), the query is pushed into an internal FIFO queue in Node.js memory.
2. When a query finishes, `client.release()` returns the connection to the idle pool, which immediately executes the next queued query.

#### Pool Exhaustion Failure Mode:
If queries run slowly (e.g. a table scan taking 5 seconds), all 20 connections become busy. Incoming HTTP requests queue up in memory until `connectionTimeoutMillis` is exceeded, throwing:
`Error: timeout exceeded when trying to connect`.
If incoming traffic continues, the in-memory queue grows until Node.js crashes with an Out-of-Memory (OOM) error.

#### Sizing Rule of Thumb:
$$\text{Pool Size} = ((\text{Core Count} \times 2) + \text{Effective Spindle Count})$$
In containerized environments (Kubernetes), setting `max: 10` to `max: 20` per container and scaling container replicas horizontally behind PgBouncer or Supabase connection poolers is standard.

---

### Q2: What is the N+1 Query Problem in GraphQL and REST APIs? How does DataLoader solve it at the event loop level?

#### In-Depth Answer

#### The Problem:
Fetching 50 blog posts and their authors:
- 1 query to fetch 50 posts: `SELECT * FROM posts LIMIT 50;`
- 50 separate queries executed by the author resolver: `SELECT * FROM authors WHERE id = 'auth_1';`, etc.
- Total queries: $1 + 50 = 51$. This destroys database performance.

#### The DataLoader Solution:
DataLoader leverages the Node.js event loop microtask queue to coalesce individual calls into a single batch query:
1. When `authorLoader.load(authorId)` is called, it does **not** execute a query immediately.
2. It returns a pending Promise and registers the requested key in an internal array.
3. DataLoader schedules a dispatch callback using `process.nextTick()`.
4. As the JavaScript call stack finishes executing all 50 author resolver functions within the current tick, the call stack empties.
5. In the microtask phase immediately following, DataLoader's batch function executes **exactly once**:
   ```sql
   SELECT * FROM authors WHERE id = ANY(ARRAY['auth_1', 'auth_2', ...]);
   ```
6. The single array result is resolved back to the 50 individual awaiting Promises.

---

### Q3: Explain Database Transaction Isolation Levels in PostgreSQL and when each is required in Node.js applications.

#### In-Depth Answer
ANSI SQL defines 4 isolation levels:
1. **Read Committed (Default in PostgreSQL)**:
   - A query sees only rows committed before the query began.
   - *Anomaly Prevented*: Dirty reads (reading uncommitted data).
   - *Allowed Anomalies*: Non-repeatable reads (re-reading the same row within the transaction can return different values if another transaction committed changes).
2. **Repeatable Read**:
   - A transaction sees a snapshot as of the *first non-transaction-control statement* in the transaction.
   - *Anomalies Prevented*: Dirty reads, non-repeatable reads.
   - *Allowed Anomalies*: Serialization anomalies (write skew: two transactions concurrently reading state and updating different rows based on that state, violating a cross-row invariant).
3. **Serializable (Highest Level of Isolation)**:
   - Guarantees that concurrent transactions produce the exact same outcome as if they had executed one after another sequentially.
   - Detects read/write conflicts; if a conflict occurs, PostgreSQL aborts the transaction with code `40001: could not serialize access due to read/write dependencies among transactions`.
   - **Application Requirement**: When using Serializable isolation in Node.js, the application **must implement an automatic retry loop** to retry aborted serialization failures!

---

### Q4: Compare Redis Caching Strategies: Cache-Aside, Write-Through, and Write-Behind.

#### In-Depth Answer

| Strategy | Read Path | Write Path | Pros / Cons |
| :--- | :--- | :--- | :--- |
| **Cache-Aside (Lazy Loading)** | Application checks Redis. If Miss: queries DB, writes to Redis, returns to client. | Application writes directly to DB, then deletes or invalidates the Redis key. | **Pros**: Only requested data is cached; cache misses are resilient.<br>**Cons**: Initial latency spike on miss; stale data risk if invalidation fails. |
| **Write-Through** | Application reads directly from cache. | Application writes to Cache; Cache component immediately writes synchronously to DB before returning. | **Pros**: Cache is never stale.<br>**Cons**: Higher write latency (two writes); unused data fills up cache. |
| **Write-Behind (Write-Back)** | Application reads from cache. | Application writes to Cache; Cache enqueues write and updates DB asynchronously in batches. | **Pros**: Ultra-fast write performance (sub-millisecond).<br>**Cons**: Potential data loss if cache instance crashes before flushing to DB. |

---

### Q5: What is the Redlock Algorithm? What are Martin Kleppmann's criticisms regarding distributed locks and fencing tokens?

#### In-Depth Answer

#### The Redlock Algorithm:
Proposed by Salvatore Sanfilippo (Redis creator) for acquiring distributed locks across $N$ independent Redis masters (typically 5 nodes):
1. Client gets current timestamp $T_1$.
2. Attempts to acquire lock in all $N$ instances using `SET key token NX PX timeout`.
3. If client acquires lock on majority ($\ge \frac{N}{2} + 1$) within a time less than lock validity, lock is granted.

#### Martin Kleppmann's Critique:
Kleppmann demonstrated that in distributed systems with **asynchronous clocks, garbage collection pauses, or network delays**, Redlock is insufficient for correctness:
1. Client A acquires lock with 10s TTL.
2. Client A experiences a long V8 Stop-The-World Garbage Collection pause (e.g. 15s).
3. Client A's lock expires on Redis.
4. Client B acquires the lock.
5. Client A wakes up from GC, believes it still holds the lock, and writes to the shared storage $\rightarrow$ **Split-Brain Data Corruption!**

#### The Solution: Fencing Tokens
To ensure absolute correctness, storage targets must enforce monotonic **fencing tokens**:
- The lock manager assigns an incrementing counter (Token 33, Token 34...).
- The database storage rejects any write containing a fencing token lower than the highest token it has already processed.
