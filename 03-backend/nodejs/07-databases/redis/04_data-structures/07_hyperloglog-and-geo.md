# HyperLogLog and Geo

Two specialized structures: **HyperLogLog** for approximate unique counts in tiny memory, and **Geo** for location queries.

---

## Part 1: HyperLogLog

HyperLogLog (HLL) estimates the number of **distinct** elements using at most about **12 KB**, no matter how many elements you add. The trade-off is a **standard error of about 0.81%**.

### Command tour

```ts
await redis.pfadd("uv:2026-09-30", "u1", "u2", "u3", "u1");   // 1 if the estimate changed, else 0
await redis.pfcount("uv:2026-09-30");                          // ~3
await redis.pfcount("uv:2026-09-29", "uv:2026-09-30");         // ~ unique across both (union)
await redis.pfmerge("uv:week", "uv:2026-09-29", "uv:2026-09-30"); // merge into a new HLL
```

You can add IDs, emails, IPs or anything else. Redis hashes the element and does not store it.

### Pattern: unique visitors

```ts
async function trackVisit(pageId: string, visitorId: string) {
  const day = new Date().toISOString().slice(0, 10);
  await redis.pfadd(`uv:${pageId}:${day}`, visitorId);
}

async function uniqueVisitors(pageId: string, days: string[]) {
  return redis.pfcount(...days.map((d) => `uv:${pageId}:${d}`));   // union across days
}
```

### HLL vs Set vs Bitmap

| | HyperLogLog | Set | Bitmap |
|---|-------------|-----|--------|
| Memory | ≤ 12 KB fixed | Grows with members | Grows with max ID |
| Accuracy | ~99.2% | Exact | Exact |
| Can list members | No | Yes | Only by scanning bits |
| Can test membership | No | Yes | Yes |
| Element type | Anything | Strings | Dense integers |
| Union across keys | Yes (`PFCOUNT`, `PFMERGE`) | Yes (`SUNION`) | Yes (`BITOP OR`) |

Use HLL when you need **counts of millions of uniques** and can accept a small error. Use a set or bitmap when you need exactness or membership checks.

### HLL notes

- Small HLLs use a **sparse** encoding and switch to a 12 KB **dense** one as they grow
- It is a regular string under the hood, so `TYPE` may show `string`
- Errors don't compound badly when merging, but counts are always estimates
- `PFCOUNT` with multiple keys is O(N) in the number of keys and computes a temporary merge

---

## Part 2: Geo

Geo commands index **longitude/latitude** points inside a **sorted set** (the coordinates are encoded into the score as a geohash).

### Adding points

Note the order: **longitude first, then latitude**.

```ts
await redis.geoadd(
  "stores",
  78.4867, 17.3850, "hyderabad",
  77.5946, 12.9716, "bengaluru",
  80.2707, 13.0827, "chennai",
  80.6480, 16.5062, "vijayawada",
);
```

Valid longitudes are -180 to 180, and valid latitudes are about ±85.05112878.

### Reading points

```ts
await redis.geopos("stores", "hyderabad");            // [["78.48669...", "17.38500..."]]
await redis.geodist("stores", "hyderabad", "chennai", "km");   // "515.9..." (string)
await redis.geohash("stores", "hyderabad");           // geohash string
```

Units: `m`, `km`, `mi`, `ft`.

### Searching

`GEOSEARCH` (Redis 6.2+) replaces the deprecated `GEORADIUS` family.

```ts
// within 300 km of a coordinate, nearest first, with distances
await redis.geosearch(
  "stores",
  "FROMLONLAT", 78.4867, 17.3850,
  "BYRADIUS", 300, "km",
  "ASC", "COUNT", 5,
  "WITHDIST"
);
// [["hyderabad","0.0000"], ["vijayawada","2xx.xxxx"]]

// around an existing member
await redis.geosearch("stores", "FROMMEMBER", "hyderabad", "BYRADIUS", 300, "km", "ASC");

// inside a bounding box
await redis.geosearch("stores", "FROMLONLAT", 78.4, 17.4, "BYBOX", 200, 200, "km", "ASC");
```

Other reply options: `WITHCOORD`, `WITHHASH`. Store results in another key with `GEOSEARCHSTORE`.

### Pattern: nearest stores

```ts
async function nearestStores(lon: number, lat: number, radiusKm = 25, limit = 10) {
  const res = (await redis.geosearch(
    "stores", "FROMLONLAT", lon, lat,
    "BYRADIUS", radiusKm, "km", "ASC", "COUNT", limit, "WITHDIST"
  )) as [string, string][];

  return res.map(([id, dist]) => ({ id, distanceKm: Number(dist) }));
}
```

### Pattern: live driver locations

```ts
await redis.geoadd("drivers:online", lon, lat, driverId);       // update as they move
await redis.zrem("drivers:online", driverId);                   // remove when offline
```

For expiring stale drivers, keep a parallel sorted set of `driverId → lastSeen` and trim it, then remove the same IDs from the Geo key.

### Geo facts

- It is **a sorted set**, so `ZREM`, `ZCARD`, `ZRANGE` and `TYPE` (`zset`) all work
- Precision is roughly 0.6 m, and distances assume a sphere (small error, fine for most apps)
- There is no per-member TTL, so clean up stale entries yourself
- Shard by region if a single key gets huge, since search cost grows with results and area

## Complexity

| Command | Cost |
|---------|------|
| `PFADD` | O(1) per element |
| `PFCOUNT` (single key) | O(1) approx |
| `PFMERGE` | O(N) keys |
| `GEOADD` | O(log N) per item |
| `GEODIST`, `GEOPOS` | O(1) / O(log N) |
| `GEOSEARCH` | O(N + log M) (N = items in the shape area, M = items in it) |

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Expecting exact counts from HLL | Use sets or bitmaps if exactness matters |
| Trying to list members of an HLL | Not possible. Store IDs elsewhere |
| Swapped lat/lon in `GEOADD` | Longitude first |
| Huge search radius returning thousands of items | Always pass `COUNT` |
| Stale points in a Geo key | Timestamp set plus cleanup job |
| Using deprecated `GEORADIUS` | `GEOSEARCH` |

## Key takeaways

- **HyperLogLog**: about 12 KB, about 0.81% error, counts distinct items but can't list them
- **Geo**: a sorted set of points, with longitude before latitude
- Prefer `GEOSEARCH`, always limit results, and clean up stale points yourself

**Next:** [Streams Overview](./08_streams-overview.md)
