# Sharding & Partitioning

| Field | Value |
| :--- | :--- |
| **Category** | Distributed & Cloud Patterns (Data Scaling) |
| **Origin** | Distributed Databases & Consistent Hashing (Karger et al., 1997) |
| **Difficulty** | Intermediate → Advanced |
| **Prerequisite(s)** | [Space-Based & Cell-Based](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/11-space-based-and-cell-based.md) |
| **Status** | Complete |

> **TL;DR**: **Partitioning / Sharding** splits large datasets horizontally across multiple independent nodes by a **Shard Key** (`tenant_id`, `user_id`)—using **Consistent Hashing** (with virtual nodes) so adding or removing a node only moves $\approx 1/N$ of keys instead of rehashing everything.

---

## 1. Partitioning Strategies

| Strategy | How It Maps Keys | Pros | Cons |
| :--- | :--- | :--- | :--- |
| **Range Partitioning** | `A–F -> Shard 1`, `G–M -> Shard 2` | Fast range scans | Hotspots (e.g., today's date gets 100% of writes) |
| **Hash (`hash(key) % N`)** | Uniform distribution | Even write spread | Adding a node (`N -> N+1`) moves ~all keys! |
| **Consistent Hashing** | Keys & nodes on a $0..2^{32}-1$ ring + virtual nodes | Adding a node moves only $1/N$ keys | Range queries require scatter-gather |

---

## 2. Hot-Key / Celebrity Partition Mitigation

If Justin Bieber or a huge enterprise tenant generates 40% of traffic on one shard key (`user_id`), **salt the key** (`user_id + "_" + random(0..9)`) to spread writes across 10 shards and aggregate on read.

---

## 3. Interview Questions

<details>
<summary>1. Why does Consistent Hashing use Virtual Nodes (vnodes)?</summary>

With few physical nodes, random placement on the hash ring can be skewed. Assigning 100+ virtual nodes per physical server smooths key distribution and balances rebalancing when a node joins/leaves.

</details>

<details>
<summary>2. What makes a good Shard Key?</summary>

High cardinality, even write/read distribution, and alignment with the most common query patterns so queries hit **1 shard** rather than scatter-gathering across all shards.

</details>

---

## 4. Cheat Sheet

- **Consistent Hashing + vnodes** for elastic caches/shards.
- **Hot keys** → Key salting (`key_0..key_9`).

---

## 5. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Leader Election & Distributed Lock](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/16-leader-election-and-distributed-lock.md) | [Module 11](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) | [Health Check & Graceful Degradation](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/18-health-check-and-graceful-degradation.md) |
