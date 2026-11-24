# Technical Interview Mastery: REST API Design, Idempotency & OpenAPI Standards

## Overview & Evaluation Criteria

Senior API engineers must demonstrate fluency in REST constraints, semantic HTTP status codes, URL modeling, cursor vs. offset pagination, idempotent mutations, and OpenAPI 3.1 contract versioning.

---

## Technical Q&A Deep Dive

<details>
<summary><strong>Q1: What is the formal difference between HTTP PUT and HTTP PATCH? When should each be used according to RFC 9110?</strong></summary>

### Answer:
- **`PUT` (Idempotent Replacement)**:
  - Replaces the *entire target resource* with the request payload.
  - If optional fields are omitted from the payload, they should be set to null or default values.
  - Executing `PUT` 10 times consecutively produces identical state on the server (idempotent).
- **`PATCH` (Partial Mutation)**:
  - Applies a *partial set of modifications* to the target resource without overwriting unmentioned fields.
  - Not strictly required to be idempotent by HTTP spec (though often designed to be in RESTful APIs).
- **In FastAPI & Pydantic v2**:
  - Implement `PATCH` by setting all fields in the request schema to optional (`| None = None`) and using `req.model_dump(exclude_unset=True)` to apply only fields explicitly provided by the caller.

</details>

<details>
<summary><strong>Q2: Why is Offset Pagination (<code>LIMIT 50 OFFSET 100000</code>) a severe performance anti-pattern on large tables? How does Cursor Pagination resolve it?</strong></summary>

### Answer:
- **Offset Flaw (B-Tree Scan Waste)**:
  - When PostgreSQL executes `LIMIT 50 OFFSET 100000`, the database engine must traverse the index, read 100,050 row pointers, and discard the first 100,000 rows. Query latency grows linearly $O(N)$ with offset depth.
  - **Pagination Drift**: If a row is inserted on page 1 while a user is browsing page 2, rows shift and duplicate across pages.
- **Cursor-Based Pagination (Keyset Pagination)**:
  - Instead of offsets, client passes the ID/timestamp of the last seen item: `WHERE (created_at, id) < (:last_ts, :last_id) ORDER BY created_at DESC, id DESC LIMIT 50`.
  - The B-Tree index jumps directly to the cursor position in $O(\log N)$ time, regardless of whether you are on the 1st or 1,000,000th page.
  - Immune to page drift.

</details>

<details>
<summary><strong>Q3: How do you design an API endpoint to be strictly idempotent using the <code>Idempotency-Key</code> header? Walk through the distributed race conditions.</strong></summary>

### Answer:
- **Design Pattern**:
  1. Client sends a unique UUID in `Idempotency-Key` header with every mutation (`POST /charges`).
  2. Gateway/App attempts to acquire a distributed Redis lock: `SET lock:idem:{key} "PROCESSING" NX EX 60`.
  3. If lock is not acquired, another request with the identical key is actively running; return `HTTP 409 Conflict` or poll.
  4. If key already exists in the completed cache (`idem:result:{key}`), replay the previously cached response immediately without re-executing business logic.
  5. After successful execution and DB commit, write the final response payload into `idem:result:{key}` with a 24-48 hour TTL and release the lock.
</details>
