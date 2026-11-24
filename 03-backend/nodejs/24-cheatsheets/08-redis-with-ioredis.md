# Cheatsheet 08: Redis with ioredis Reference Card

> Production syntax reference for Redis operations using `ioredis` in Node.js 22 LTS, covering data structures, atomic scripts, Pub/Sub, and pipelining.

---

### 1. Connection & Sentinel / Cluster Configuration
```javascript
import Redis from 'ioredis';

export const redis = new Redis(process.env.REDIS_URL, {
  maxRetriesPerRequest: 3,
  retryStrategy: (times) => Math.min(times * 100, 3000), // Exponential backoff
  enableReadyCheck: true,
});
```

---

### 2. Common Data Structures & Commands

#### Strings & TTL
```javascript
// Set with TTL (EX: seconds, PX: milliseconds)
await redis.set('session:123', JSON.stringify({ userId: 1 }), 'EX', 3600);
const data = await redis.get('session:123');

// Atomic Counter
const views = await redis.incr('page:views:home');
const attempts = await redis.incrby('rate:login:user1', 1);

// Set if Not Exists (Locking primitive)
const acquired = await redis.set('lock:order:99', 'token_abc', 'NX', 'EX', 30); // Returns 'OK' or null
```

#### Hashes (Objects)
```javascript
await redis.hset('user:100', { name: 'Ada', email: 'ada@example.com', visits: 5 });
const name = await redis.hget('user:100', 'name');
const allFields = await redis.hgetall('user:100'); // Returns JS object
await redis.hincrby('user:100', 'visits', 1);
```

#### Lists (Queues / Stacks)
```javascript
await redis.lpush('tasks', 'task_1', 'task_2'); // Push left
const item = await redis.rpop('tasks');          // Pop right (FIFO Queue)
const items = await redis.lrange('tasks', 0, 10);// Slice without removal
```

#### Sets (Unique Items)
```javascript
await redis.sadd('online_users', 'usr_1', 'usr_2');
const isOnline = await redis.sismember('online_users', 'usr_1'); // 1 or 0
const allOnline = await redis.smembers('online_users');
await redis.srem('online_users', 'usr_1');
```

#### Sorted Sets (Leaderboards / Sliding Windows)
```javascript
// ZADD key score member
await redis.zadd('leaderboard', 1500, 'player_one', 2300, 'player_two');
// Fetch top 10 highest scores
const topPlayers = await redis.zrevrange('leaderboard', 0, 9, 'WITHSCORES');
```

---

### 3. Pipelining & Multi/Exec Transactions
```javascript
// Pipeline (Batch commands into a single network packet without round-trips):
const pipeline = redis.pipeline();
pipeline.set('k1', 'v1');
pipeline.incr('counter');
pipeline.get('k1');
const results = await pipeline.exec(); // [[null, 'OK'], [null, 1], [null, 'v1']]

// Multi / Exec (Atomic Transaction):
const multi = redis.multi();
multi.decrby('account:from', 50);
multi.incrby('account:to', 50);
await multi.exec();
```

---

### 4. Pub/Sub Messaging
```javascript
const pub = new Redis();
const sub = new Redis(); // Subscriptions require dedicated client connection

await sub.subscribe('notifications');
sub.on('message', (channel, message) => {
  console.log(`Received message on ${channel}:`, message);
});

await pub.publish('notifications', JSON.stringify({ alert: 'System maintenance' }));
```
