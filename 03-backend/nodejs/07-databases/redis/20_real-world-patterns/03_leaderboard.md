# Leaderboard

A leaderboard answers three questions fast: **who is on top, where do I rank, and who is around me?** Redis sorted sets were practically designed for this. Inserts, score updates and rank lookups are all O(log N), even with millions of players.

```
lb:global  (sorted set, score = points)

   rank 1  ada     9820
   rank 2  grace   9410
   rank 3  linus   9410   ◄── ties are ordered by member name unless you design otherwise
   ...
```

## The problem

| Requirement | Why a database struggles |
|-------------|--------------------------|
| Top N in real time | `ORDER BY score DESC LIMIT N` re-sorts or needs an index scan under heavy writes |
| "My rank" for any user | A `COUNT(*) WHERE score > mine` per request is expensive at scale |
| Scores change constantly | Index churn under thousands of updates per second |
| Daily, weekly and all-time boards | Several aggregations to keep consistent |

## Core commands

| Need | Command | Cost |
|------|---------|------|
| Set a score | `ZADD key score member` | O(log N) |
| Add to a score | `ZINCRBY key delta member` | O(log N) |
| Keep only the best score | `ZADD key GT score member` (Redis 6.2+) | O(log N) |
| Top N | `ZRANGE key 0 N-1 REV WITHSCORES` (6.2+) | O(log N + N) |
| My rank (0-based) | `ZREVRANK key member` | O(log N) |
| My score | `ZSCORE key member` | O(1) |
| Scores of many users | `ZMSCORE key m1 m2 ...` (6.2+) | O(N) given |
| Players with score in a range | `ZCOUNT key min max` | O(log N) |
| Total players | `ZCARD key` | O(1) |
| Keep only the top K | `ZREMRANGEBYRANK key 0 -(K+1)` | O(log N + removed) |
| Combine boards | `ZUNIONSTORE dest n k1 k2 ...` | O(N) plus sorting |

Ranks are **0-based** and ascending by default. `REV` (or the older `ZREVRANGE`/`ZREVRANK`) gives highest first.

## Implementation

```ts
import Redis from "ioredis";

export interface Entry { rank: number; userId: string; score: number }

// ioredis returns WITHSCORES results as a flat array: [member, score, member, score, ...]
function toPairs(flat: string[]): [string, number][] {
  const out: [string, number][] = [];
  for (let i = 0; i < flat.length; i += 2) out.push([flat[i], Number(flat[i + 1])]);
  return out;
}

export class Leaderboard {
  constructor(private redis: Redis, private key: string) {}

  /** Best-score board: only raises the score. Returns true if the score changed. */
  async submitBest(userId: string, score: number): Promise<boolean> {
    return (await this.redis.zadd(this.key, "GT", "CH", score, userId)) === 1;
  }

  /** Cumulative board: adds points and returns the new total. */
  async addPoints(userId: string, delta: number): Promise<number> {
    return Number(await this.redis.zincrby(this.key, delta, userId));
  }

  async top(n = 10, offset = 0): Promise<Entry[]> {
    const flat = await this.redis.zrange(this.key, offset, offset + n - 1, "REV", "WITHSCORES");
    return toPairs(flat).map(([userId, score], i) => ({ rank: offset + i + 1, userId, score }));
  }

  async rankOf(userId: string): Promise<Entry | null> {
    const [[, rank], [, score]] = (await this.redis
      .multi()
      .zrevrank(this.key, userId)
      .zscore(this.key, userId)
      .exec())! as [[Error | null, number | null], [Error | null, string | null]];
    if (rank === null || score === null) return null;               // not on the board
    return { rank: rank + 1, userId, score: Number(score) };
  }

  /** The player and their neighbors, for a "you are here" view. */
  async around(userId: string, radius = 3): Promise<Entry[]> {
    const rank = await this.redis.zrevrank(this.key, userId);
    if (rank === null) return [];
    const start = Math.max(0, rank - radius);
    const flat = await this.redis.zrange(this.key, start, rank + radius, "REV", "WITHSCORES");
    return toPairs(flat).map(([id, score], i) => ({ rank: start + i + 1, userId: id, score }));
  }

  async size(): Promise<number> {
    return this.redis.zcard(this.key);
  }

  /** Percentile: share of players you are ahead of (0 to 100). */
  async percentile(userId: string): Promise<number | null> {
    const [rank, total] = await Promise.all([this.redis.zrevrank(this.key, userId), this.size()]);
    if (rank === null || total === 0) return null;
    return Math.round(((total - 1 - rank) / Math.max(1, total - 1)) * 100);
  }

  /** Keep only the best `limit` players (run periodically). */
  async trim(limit: number) {
    return this.redis.zremrangebyrank(this.key, 0, -(limit + 1));
  }
}
```

```ts
const board = new Leaderboard(redis, "lb:game:42:all");

await board.addPoints("ada", 120);
await board.submitBest("grace", 9410);

await board.top(10);          // [{ rank: 1, userId: "ada", score: 9820 }, ...]
await board.rankOf("ada");    // { rank: 1, userId: "ada", score: 9820 }
await board.around("ada", 2); // two above, two below
```

`rankOf` uses `MULTI` so rank and score come from the same moment. Note that `zrevrank` returns `null` for non-members, which is different from rank 0.

### Add names and avatars

The sorted set holds IDs and scores only. Fetch display data for the visible page in one call:

```ts
async function topWithNames(board: Leaderboard, n = 10) {
  const entries = await board.top(n);
  const names = await redis.hmget("user:names", ...entries.map((e) => e.userId));   // HMGET: one round trip
  return entries.map((e, i) => ({ ...e, name: names[i] ?? "Unknown" }));
}
```

For richer profiles, use a pipeline of `HGETALL user:{id}` or cache the rendered top-N page for a second or two.

## Ties

A sorted set orders equal scores **lexicographically by member**, so `grace` outranks `linus` at the same score for no good reason. Usually you want "whoever reached the score first ranks higher". Encode that into the score:

```
composite = points × SCALE + (WINDOW_SECONDS − secondsSinceStart)
             └ main ordering ┘   └ earlier achievers get a larger tiebreak ┘
```

```ts
const SCALE = 1e7;                      // room for the tiebreak part
const SEASON_SECONDS = 30 * 86_400;     // must be < SCALE

function encode(points: number, nowSec: number, seasonStartSec: number) {
  const elapsed = Math.min(SEASON_SECONDS, nowSec - seasonStartSec);
  return points * SCALE + (SEASON_SECONDS - elapsed);
}

function decodePoints(composite: number) {
  return Math.floor(composite / SCALE);
}
```

Limits to respect: JavaScript numbers (and Redis scores, which are doubles) are exact up to 2^53 (about 9 × 10^15). With `SCALE = 1e7`, points must stay below roughly 9 × 10^8. Choose the scale from your real maximum. This works for **best-score** boards (`ZADD GT`). For cumulative boards, every `ZINCRBY` changes the tiebreak, so recompute the composite from the stored points (keep raw points in a hash) or accept member-name ordering.

## Time-windowed boards

Create **one key per period** and let old ones expire. No cleanup job needed.

```ts
const day = (d = new Date()) => d.toISOString().slice(0, 10);

function mondayOf(d = new Date()) {
  const x = new Date(Date.UTC(d.getUTCFullYear(), d.getUTCMonth(), d.getUTCDate()));
  x.setUTCDate(x.getUTCDate() - ((x.getUTCDay() + 6) % 7));
  return x.toISOString().slice(0, 10);
}

export async function recordPoints(gameId: string, userId: string, points: number) {
  const keys = [
    `lb:${gameId}:day:${day()}`,
    `lb:${gameId}:week:${mondayOf()}`,
    `lb:${gameId}:all`,
  ];
  const m = redis.multi();
  m.zincrby(keys[0], points, userId).expire(keys[0], 3 * 86_400, "NX");      // keep 3 days   (EXPIRE NX: Redis 7.0+)
  m.zincrby(keys[1], points, userId).expire(keys[1], 21 * 86_400, "NX");     // keep 3 weeks
  m.zincrby(keys[2], points, userId);                                         // no expiry
  await m.exec();
}
```

In a Redis Cluster, these keys can live on different nodes, so a `MULTI` across them fails with `CROSSSLOT`. Either use a hash tag (`lb:{game42}:day:...`, which keeps one game's boards on one node) or send separate commands.

### "Last 7 days" from daily boards

```ts
async function last7Days(gameId: string) {
  const keys = Array.from({ length: 7 }, (_, i) => `lb:${gameId}:day:${day(new Date(Date.now() - i * 86_400_000))}`);
  const dest = `lb:${gameId}:last7`;
  await redis.zunionstore(dest, keys.length, ...keys, "AGGREGATE", "SUM");
  await redis.expire(dest, 60);            // cache the merged board briefly
  return new Leaderboard(redis, dest);
}
```

`ZUNIONSTORE` is O(N) over all members, so do not run it per request on big boards. Compute it on a schedule or cache it for a minute. Missing keys are treated as empty. `WEIGHTS` lets you decay older days (for example newer days count more).

## Friends leaderboard

```ts
async function friendsBoard(userId: string, friendIds: string[]) {
  const ids = [userId, ...friendIds];
  const scores = await redis.zmscore("lb:game:42:all", ...ids);          // ZMSCORE: Redis 6.2+
  return ids
    .map((id, i) => ({ userId: id, score: scores[i] === null ? null : Number(scores[i]) }))
    .filter((e): e is { userId: string; score: number } => e.score !== null)
    .sort((a, b) => b.score - a.score)
    .map((e, i) => ({ rank: i + 1, ...e }));
}
```

Fine for friend lists of hundreds. For very large social graphs, maintain a per-user board for the people they follow, or limit the list.

## Keeping it bounded

| Technique | Command |
|-----------|---------|
| Keep only the top K of a huge board | `ZREMRANGEBYRANK key 0 -(K+1)` on a schedule |
| Expire period boards | `EXPIRE` when creating the key |
| Remove a banned player | `ZREM key userId` on every board |
| Drop an old season | `UNLINK key` (not `DEL`) |

Trimming to the top K means a player who falls out loses their rank view. Decide whether "rank 12,345,678" matters to anyone, and if not, trim.

## Real-time updates

Poll the top N every few seconds (cheap, cached) or push changes:

```ts
const { rank } = (await board.rankOf(userId))!;
await redis.publish("lb:updates:game42", JSON.stringify({ userId, rank, score }));
```

A gateway subscribes and forwards to WebSocket clients. Pub/Sub is best-effort, so clients should also refetch periodically (see [Pub/Sub Fundamentals](../09_pub-sub/01_pub-sub-fundamentals.md) and [Socket.IO with Redis](../09_pub-sub/03_socketio-with-redis.md)). Push only meaningful changes (a rank change in the top 100), not every point.

## Scaling

A sorted set with millions of members is fine on one node, memory permitting (measure with `MEMORY USAGE`). The limit you hit first is usually **one hot key on one shard** in Cluster, or a single-core ceiling.

Options, in order of effort:

1. Cache the top-N page for 1 to 2 seconds. Reads become nearly free
2. Batch score writes (aggregate points client-side for a second, then one `ZINCRBY`)
3. Shard the board by user hash into K sorted sets, and merge on read:

```ts
const SHARDS = 16;
const shardKey = (userId: string) => `lb:g42:s${crc32(userId) % SHARDS}`;

// global top N = merge of each shard's top N (the true top N is always inside the union)
async function globalTop(n: number) {
  const pages = await Promise.all(
    Array.from({ length: SHARDS }, (_, s) => redis.zrange(`lb:g42:s${s}`, 0, n - 1, "REV", "WITHSCORES")),
  );
  return pages.flatMap(toPairs).sort((a, b) => b[1] - a[1]).slice(0, n);
}

// a user's global rank = (number of players with a higher score, summed over all shards) + 1
async function globalRank(userId: string, score: number) {
  const counts = await Promise.all(
    Array.from({ length: SHARDS }, (_, s) => redis.zcount(`lb:g42:s${s}`, `(${score}`, "+inf")),
  );
  return counts.reduce((a, b) => a + b, 0) + 1;
}
```

Sharding makes "global rank" cost K commands, so only do it when one key truly cannot cope. (`crc32` stands for any stable hash function.)

## Source of truth

Treat the sorted set as a **serving layer**. If points come from events (matches played, purchases), keep those in a database or stream so you can **rebuild** the board:

```ts
async function rebuild(boardKey: string, rows: AsyncIterable<{ userId: string; points: number }>) {
  const tmp = `${boardKey}:rebuild`;
  for await (const r of rows) await redis.zincrby(tmp, r.points, r.userId);   // batch with a pipeline in practice
  await redis.rename(tmp, boardKey);                                            // atomic swap
}
```

`RENAME` swaps in the finished board atomically, so readers never see a half-built one. This also gives you a recovery path after data loss (see [Backup and Disaster Recovery](../19_redis-production/03_backup-and-disaster-recovery.md)).

## Testing

```ts
it("ranks by score, highest first", async () => {
  await board.addPoints("a", 10); await board.addPoints("b", 30); await board.addPoints("c", 20);
  expect((await board.top(3)).map((e) => e.userId)).toEqual(["b", "c", "a"]);
});

it("returns 1-based ranks and null for unknown users", async () => {
  await board.addPoints("a", 5);
  expect((await board.rankOf("a"))!.rank).toBe(1);
  expect(await board.rankOf("ghost")).toBeNull();
});

it("keeps only the best score with GT", async () => {
  await board.submitBest("a", 100);
  await board.submitBest("a", 50);
  expect((await board.rankOf("a"))!.score).toBe(100);
});

it("returns neighbors around a player, clamped at the top", async () => {
  for (const [id, s] of [["a", 5], ["b", 4], ["c", 3], ["d", 2], ["e", 1]] as const) await board.addPoints(id, s);
  const around = await board.around("b", 2);
  expect(around.map((e) => e.userId)).toEqual(["a", "b", "c", "d"]);
});

it("handles concurrent increments", async () => {
  await Promise.all(Array.from({ length: 100 }, () => board.addPoints("a", 1)));
  expect((await board.rankOf("a"))!.score).toBe(100);
});

it("breaks ties by earlier achievement", () => {
  const start = 1_000_000;
  expect(encode(100, start + 10, start)).toBeGreaterThan(encode(100, start + 20, start));
});
```

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Treating `ZRANK` as 1-based | Ranks are 0-based, so add 1 for display |
| Forgetting `REV` | Default order is ascending, so lowest first |
| `ZREVRANK` returning `null` handled as rank 0 | Check for `null` explicitly |
| Scores returned as strings | Convert with `Number()` |
| Float precision beyond 2^53 | Keep composite scores below about 9 × 10^15 |
| Ties resolved by member name | Encode a time-based tiebreak into the score |
| `ZADD` overwriting a better score | `ZADD ... GT` for best-score boards |
| Reading rank and score in separate moments | `MULTI` (or accept small skew) |
| `ZUNIONSTORE` on every request | Precompute and cache |
| Unbounded boards | Trim to top K, expire period boards |
| Deleting a huge board with `DEL` | `UNLINK` |
| One hot board key in Cluster | Cache reads, batch writes, or shard and merge |
| Trusting client-submitted scores | Compute points server-side from verified events |
| Redis as the only copy of the scores | Keep the events elsewhere so you can rebuild |

## Key takeaways

- Sorted sets give O(log N) updates and rank lookups, so one key serves top-N, "my rank" and "around me"
- Use `ZINCRBY` for cumulative boards and `ZADD GT` for best-score boards, and remember ranks are 0-based
- Encode tiebreaks into the score, within double precision limits
- One key per time window with a TTL gives daily, weekly and seasonal boards, and `ZUNIONSTORE` combines them (cache the result)
- Cache the top page, batch writes and shard only when a single key is truly the bottleneck
- Keep events as the source of truth so the board can be rebuilt

**Previous:** [Request Deduplication](./02_request-deduplication.md) | **Next:** [Notification System](./04_notification-system.md)
