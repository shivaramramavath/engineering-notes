# Technical Interview Mastery: FastAPI System Design Scenarios

## Overview & Evaluation Criteria

Senior architecture interviews pose open-ended, high-scale scenarios designed to test edge cases, trade-offs, technology choices, and failure mitigations.

---

## Technical Scenarios Deep Dive

<details>
<summary><strong>Scenario 1: Design an API Gateway handling 50,000 WebSocket connections with real-time push to individual users across multiple pods.</strong></summary>

### Architectural Solution:
1. **Connection Tier**: Horizontally scale 25 FastAPI pods behind an AWS ALB or Envoy proxy with WebSocket support. Each pod holds $\approx 2,000$ active WebSocket client sockets in an in-memory dictionary `dict[user_id, WebSocket]`.
2. **Cross-Pod Dispatcher (Pub/Sub)**:
   - A client on Pod 1 wants to message User B connected to Pod 18.
   - When Pod 1 receives the message, it does NOT know where User B is. It publishes an event to **Redis Pub/Sub** on channel `user:broadcast:{target_user_id}` or Kafka topic.
   - Every FastAPI pod subscribes to its local user base. Pod 18 intercepts the message, locates User B's local socket, and pushes the frame.
3. **Session State & Disconnects**:
   - Store active user pod mappings in Redis with 60-second TTL heartbeat.
   - On network drop, client reconnects to any available pod and resynchronizes un-delivered messages via cursor timestamp.

</details>

<details>
<summary><strong>Scenario 2: How would you prevent the "Thundering Herd" cache stampede problem on a high-traffic news article or product page?</strong></summary>

### Architectural Solution:
When an article visited by 100,000 users per minute expires in Redis, all 100,000 requests simultaneously miss the cache and hit the PostgreSQL database, crashing the connection pool.
1. **Distributed Mutex Lock**:
   - When a cache miss occurs, the first request attempts to acquire a short-lived Redis lock: `SET lock:article:123 "1" NX EX 5`.
   - The thread that acquires the lock queries the database and populates Redis.
   - All other 99,999 threads fail to acquire the lock and sleep for 50ms before retrying against Redis (which is now populated).
2. **Probabilistic Early Expiration (XFetch Algorithm)**:
   - Instead of waiting for hard TTL expiry, compute a probability of refreshing early as the key approaches expiration:
     $$\Delta \times \beta \times \ln(\text{random}()) > \text{remaining\_ttl}$$
   - A single background request silently re-computes and refreshes the cache before it ever expires for the general public.

</details>

<details>
<summary><strong>Scenario 3: Design a multi-stage file upload service where users upload 1GB raw video files and receive real-time encoding progress updates.</strong></summary>

### Architectural Solution:
1. **Ingest Phase**: Do NOT upload 1GB files through FastAPI. The client requests a **Presigned S3 Multipart Upload URL**. The browser uploads chunks directly to AWS S3.
2. **Notification Phase**: S3 emits an `ObjectCreated` event to AWS SQS / EventBridge upon multipart completion.
3. **Transcode Phase**: A pool of Celery/K8s FFmpeg worker pods dequeues the job and encodes 1080p/720p HLS chunks.
4. **Real-Time Progress**: The FFmpeg worker parses encoding percentage and publishes `{"video_id": "123", "progress": 42%}` to Redis Pub/Sub.
5. **FastAPI WebSocket**: The client maintains a WebSocket with FastAPI listening to `progress:{video_id}`, rendering a smooth progress bar in the UI.
</details>
