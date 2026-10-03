# Backup and Disaster Recovery

Redis can lose data in many ways: a crash between snapshots, a failover that drops unreplicated writes, a bad deploy that deletes keys, a disk failure, a human running `FLUSHALL`, an attacker. Backup and recovery planning is how you decide **how much loss you can accept** and **how fast you must be back**.

```
Persistence   survives a restart on the same node          (RDB, AOF)
Replication   survives loss of a node                       (replicas, Sentinel, Cluster)
Backups       survive bad writes, corruption and disasters   (copies, offsite, tested)
```

All three are needed for data that matters. None replaces the others.

## Key terms

| Term | Meaning |
|------|---------|
| **RPO** (recovery point objective) | How much data you can afford to lose, measured in time |
| **RTO** (recovery time objective) | How long you can afford to be down |
| **Persistence** | Redis writing its dataset to disk |
| **Replication** | Copies on other nodes (asynchronous by default) |
| **Backup** | An independent, versioned copy kept elsewhere |
| **DR** (disaster recovery) | Restoring service after losing a node, zone or region |

Write RPO and RTO down per dataset, and design to them:

| Dataset | Example RPO | Example RTO | Approach |
|---------|-------------|-------------|----------|
| API cache | n/a (rebuildable) | Minutes | No backup. Protect the database from a cold start |
| Rate limits, counters | Minutes | Minutes | Replica plus RDB |
| Sessions | About 1 second | Under 1 minute | AOF `everysec`, replicas across zones |
| Job queues | Seconds | Minutes | AOF, replicas, `noeviction`, idempotent jobs |
| Data only in Redis | Near zero | Per business need | Strongest durability, frequent backups, drills, and a hard look at whether it belongs in Redis |

## What each mechanism protects against

| Failure | Persistence | Replica + failover | Backups |
|---------|:-----------:|:------------------:|:-------:|
| Process crash or restart | Yes | Yes | Yes |
| Host or disk loss | No (disk gone) | Yes | Yes |
| Zone outage | No | Yes, if replicas span zones | Yes, if copied offsite |
| `FLUSHALL` or bad deploy | **No** (persisted too) | **No** (replicated instantly) | **Yes** |
| Data corruption | No | Often replicated | **Yes** |
| Ransomware or malicious delete | No | No | Yes, if immutable and separate |
| Region outage | No | Only with cross-region replication | Yes, if copied cross-region |

## Persistence options

Details in [Persistence](../02_redis-fundamentals/04_persistence.md). The essentials for production:

| | RDB snapshots | AOF (append-only file) |
|--|---------------|------------------------|
| What | Point-in-time dump of the dataset | Log of every write |
| Loss window | Everything since the last snapshot (minutes) | About 1 second with `everysec`, near zero with `always` |
| Restore speed | Fast | Slower (replays the log, mitigated by the RDB preamble) |
| File size | Compact | Larger, rewritten periodically |
| Good for | Backups, fast restarts | Durability |

A sound default for important data is **both**:

```bash
save 3600 1 300 100 60 10000        # periodic snapshots (backup source, fast restart)
appendonly yes
appendfsync everysec                # lose at most about 1 second
aof-use-rdb-preamble yes            # faster AOF loading
```

Redis 7 stores the AOF as several files in a directory (`appenddirname`, default `appendonlydir`): a base file, incremental files and a manifest. Treat that directory as one unit.

For a pure cache you can turn persistence off entirely (`save ""`, `appendonly no`). That also removes fork cost and disk I/O.

### Know what "durable" means here

- `everysec` can lose about a second on a crash
- Replication is asynchronous: if the primary dies, writes that had not reached a replica are lost after failover
- `WAIT <replicas> <timeout>` blocks until replicas acknowledge, which narrows the window but does not guarantee no loss
- `min-replicas-to-write` makes the primary refuse writes when too few replicas are in sync, trading availability for safety

```ts
await redis.set("order:42", payload);
const acked = await redis.call("WAIT", "1", "100");   // replicas that acknowledged within 100 ms
```

## Backups

### Take a consistent RDB snapshot

Back up from a **replica** where possible, so the primary does not pay for the fork and the file copy.

```bash
#!/usr/bin/env bash
# redis-backup.sh: snapshot Redis and ship it offsite
set -euo pipefail

export REDISCLI_AUTH="${REDIS_BACKUP_PASSWORD}"          # keeps the password off the command line
CLI="redis-cli --user backup -h ${REDIS_HOST:-127.0.0.1} -p ${REDIS_PORT:-6379}"
DATA_DIR="${REDIS_DIR:-/var/lib/redis}"                   # only valid when running on the Redis host
OUT="${BACKUP_DIR:-/backups}"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"

before="$($CLI LASTSAVE)"
$CLI BGSAVE | grep -Eq 'Background saving started|already in progress' || { echo "BGSAVE refused"; exit 1; }

# wait for the save to finish
until [ "$($CLI LASTSAVE)" != "$before" ]; do sleep 1; done
status="$($CLI INFO persistence | tr -d '\r' | awk -F: '/^rdb_last_bgsave_status/{print $2}')"
[ "$status" = "ok" ] || { echo "BGSAVE failed: $status"; exit 1; }

cp "${DATA_DIR}/dump.rdb" "${OUT}/dump-${STAMP}.rdb"
redis-check-rdb "${OUT}/dump-${STAMP}.rdb"                # verify the file is readable
gzip -9 "${OUT}/dump-${STAMP}.rdb"
sha256sum "${OUT}/dump-${STAMP}.rdb.gz" > "${OUT}/dump-${STAMP}.rdb.gz.sha256"

# ship offsite (example: object storage with versioning, encryption and retention rules)
# aws s3 cp "${OUT}/dump-${STAMP}.rdb.gz" "s3://my-redis-backups/prod/" --sse
echo "backup ok: dump-${STAMP}.rdb.gz"
```

Notes:

- If a snapshot is already running (or an AOF rewrite is in progress), `BGSAVE` may report that. Handle it and retry rather than copying a half-written file
- `redis-check-rdb` catches corrupt files early, so you do not discover it during a disaster
- Run from cron, a scheduler or a Kubernetes `CronJob`, and **alert if the job fails or the newest backup is too old**
- If you cannot access the data directory (managed service, remote host), `redis-cli --rdb /path/dump.rdb` can pull a snapshot over the network. It needs permissions for replication-style commands, so check your ACL

### Backing up the AOF

The AOF directory changes continuously and is rewritten from time to time, so copying it blindly can capture an inconsistent set of files. The simplest reliable approach is to back up the **RDB snapshot** (as above). If you also need the AOF, follow the Redis documentation for your version, which describes pausing automatic rewrites (`CONFIG SET auto-aof-rewrite-percentage 0`) while the directory is copied and re-enabling them afterwards.

### Cluster backups

Each shard is backed up separately, and the snapshots are **not** one atomic point-in-time for the whole cluster. Also save each node's `nodes.conf` and your topology notes (which node held which slots). Restore shard by shard, then verify with `redis-cli --cluster check`.

### Where and how to store them

| Practice | Why |
|----------|-----|
| Copy offsite (another zone, ideally another region or account) | A disaster takes out the primary site |
| Encrypt at rest and in transit | Backups contain every key and value |
| Restrict who can read and delete them | Backups are a prime target |
| Versioning or immutability (object lock, write-once) | Protection against ransomware and accidental deletes |
| Retention policy, for example hourly for 24 hours, daily for 7 to 30 days | Lets you go back past a slow-burning corruption |
| Separate credentials from the Redis admin account | A compromised Redis host should not be able to erase backups |

## Restore procedures

### Restore from an RDB file

When AOF is enabled, Redis prefers the AOF files at startup. Restoring a bare `dump.rdb` into a server that has AOF enabled can therefore start from the wrong (empty or stale) data. The safe procedure:

```bash
# 1. stop writers (maintenance mode) and stop Redis
sudo systemctl stop redis

# 2. keep what is there for forensics, even if it looks broken
sudo mv /var/lib/redis /var/lib/redis.broken.$(date +%s) && sudo mkdir /var/lib/redis

# 3. place the backup
gunzip -c dump-20261003T020000Z.rdb.gz | sudo tee /var/lib/redis/dump.rdb > /dev/null
sudo chown -R redis:redis /var/lib/redis

# 4. start with AOF OFF so Redis loads the RDB
sudo redis-server /etc/redis/redis.conf --appendonly no     # or temporarily set it in the config, then use systemctl start

# 5. verify before reopening to traffic
redis-cli DBSIZE
redis-cli INFO keyspace
redis-cli --scan --count 100 | head          # spot-check keys, run known application queries

# 6. re-enable AOF, which rewrites it from the current dataset
redis-cli CONFIG SET appendonly yes
redis-cli INFO persistence | grep aof_rewrite_in_progress     # wait until 0
# and set appendonly yes in redis.conf so it survives restarts
```

Then bring replicas back (they perform a full sync from the restored primary), re-enable clients gradually and watch metrics.

### With Sentinel or Cluster

- **Sentinel**: while you restore the primary, Sentinel may promote a replica or an empty node. Stop or pause failover (stop the sentinels, or work with a clear maintenance procedure) so an empty node is never promoted over good data
- **Cluster**: restore the failed shard's primary and replicas, confirm slot ownership with `CLUSTER NODES`, and run `redis-cli --cluster check`

### Loading time

Restore speed depends on dataset size, disk and CPU. During loading, clients get `LOADING` errors.

```bash
redis-cli INFO persistence | egrep 'loading|loading_eta_seconds|loading_loaded_perc'
```

Measure it in a drill, because it is the biggest part of your real RTO. Make sure clients retry with backoff and your readiness checks handle `LOADING`.

### Partial restores and surgical recovery

You can restore a backup onto a **separate instance**, then copy only the keys you need back to production (`MIGRATE ... COPY`, `DUMP`/`RESTORE`, or a script). This is the usual answer to "someone deleted one customer's data".

```bash
redis-cli -h restored-instance --scan --pattern 'session:user:42:*' | while read k; do
  redis-cli -h restored-instance MIGRATE prod-redis 6379 "$k" 0 5000 COPY AUTH2 app "$APP_PASSWORD"
done
```

Review keys and permissions carefully before you do it against production.

## Verify backups: test restores

A backup is only real once you have restored it.

| Test | Frequency | What you learn |
|------|-----------|----------------|
| `redis-check-rdb` on each new file | Every backup | File is not corrupt |
| Automated restore into a scratch instance, with a sanity check (`DBSIZE`, known keys) | Daily or weekly | The restore procedure actually works |
| Full DR drill: rebuild service from backups in a clean environment | Quarterly | Real RTO, missing steps, missing access |
| Restore with the **current** Redis version | After every upgrade | Compatibility |

```ts
// post-restore sanity check
export async function verifyRestore(redis: Redis, expectedMinKeys: number) {
  const size = await redis.dbsize();
  if (size < expectedMinKeys) throw new Error(`restore too small: ${size} keys`);

  const canary = await redis.get("canary:last-write");        // a key your app updates regularly
  if (!canary) throw new Error("canary key missing");
  const ageSec = (Date.now() - Number(canary)) / 1000;
  return { size, canaryAgeSec: ageSec };                      // compare against your RPO
}
```

A **canary key** that the application writes every minute makes it easy to measure the age of the restored data, which is your effective RPO.

## High availability and failover drills

Backups cover disasters. Replication and failover cover everyday failures. Rehearse both.

```bash
# Sentinel: trigger a controlled failover
redis-cli -p 26379 SENTINEL failover mymaster

# Cluster: on a replica of the shard you want to move
redis-cli -p 6379 CLUSTER FAILOVER

# Verify afterwards
redis-cli INFO replication
redis-cli -p 26379 SENTINEL get-master-addr-by-name mymaster
```

A good drill answers:

- How long were writes failing or timing out?
- Did all clients reconnect on their own (including workers and subscribers)?
- Did any acknowledged write disappear?
- Did alerts fire, and did they point to the right runbook?
- Did the old primary rejoin as a replica cleanly?

Run these in staging first, then in production during a quiet window with the team watching. Also try the nastier cases: kill the primary process, block its network, fill its disk.

## Failure scenarios and responses

| Scenario | Detection | Response |
|----------|-----------|----------|
| Redis process crashes | `RedisDown`, restart count | Supervisor restarts it. With AOF it reloads. Expect `LOADING` during recovery |
| Primary host fails | Sentinel/cluster failover events, alerts | Automatic failover. Verify clients, rebuild the lost node as a replica |
| Replica fails | `connected_slaves` drops | Replace and resync. Keep an eye on primary load during the full sync |
| Disk full or failing | `MISCONF` errors, persistence alerts | Free space or replace the disk, run `BGSAVE`, check for lost writes |
| Zone outage | Multiple alerts at once | Fail over to replicas in surviving zones, confirm capacity |
| Memory exhausted | OOM errors or kernel OOM kill | Free memory, fix runaway keys, adjust `maxmemory`, scale |
| Accidental `FLUSHALL` or mass delete | App errors, key count collapse | Stop writers. Restore from backup (partial restore if possible). Do not rely on replicas, they copied the flush |
| Data corruption or bad deploy writing garbage | Validation errors, user reports | Roll back code. Restore affected keys from a point before the change |
| Malicious access or ransomware | Security alerts, `ACL LOG`, unexpected commands | Isolate the instance, rotate credentials, restore from immutable backups on clean hosts |
| Region outage | Region-level monitoring | Execute the DR runbook: restore in another region, repoint clients |
| Cold cache after restart | Database load spike, hit ratio collapse | Rate-limit rebuilds, request coalescing, gradual traffic shift |

## Protect against the human error

- Deny `FLUSHALL`, `FLUSHDB`, `DEBUG`, `CONFIG` and similar commands for application users (see [Authentication and ACL](../17_security/01_authentication-and-acl.md))
- Use separate credentials and visibly different hostnames or prompts for production
- Require review for any script that deletes by pattern
- Keep production access to a small group, with audit logs
- Rely on **ACLs**, not renamed commands, as the control that limits who can run destructive commands

## Cross-region and cross-site

| Option | Behavior | Notes |
|--------|----------|-------|
| Asynchronous replica in another region | Warm standby, some lag | Manual or scripted promotion. Expect to lose recent writes |
| Backups copied cross-region | Cold standby | Slowest RTO, cheapest, simplest |
| Active-active products (vendor features) | Multi-writer with conflict resolution | Provider-specific. Understand conflict semantics before relying on it |

Decide the **trigger** and **owner** for declaring a disaster and switching regions. A plan nobody is authorized to start is useless. Test client behavior too (DNS TTLs, endpoint configuration, secrets in the second region).

## Rebuild instead of restore

For rebuildable data, the DR plan can be "recreate it from the source of truth":

- Document the rebuild: scripts, order, expected duration and load on source systems
- Test it in staging with production-sized data
- Make sure the rebuild cannot overload the database (throttle, batch)
- Prefer this for caches, derived views and leaderboards, which keeps backups small and simple

## Disaster recovery runbook template

```
Title:        Restore Redis "sessions" from backup
Owner:        Platform team (on-call: #redis-oncall)
Trigger:      Data loss/corruption confirmed, or primary unrecoverable
RPO / RTO:    1 s / 15 min (target), last drill: 2026-09-12, measured RTO 11 min

Before you start
 - Declare the incident, stop writers (maintenance flag), stop Sentinel failover if restoring in place
 - Identify the target backup (latest good, or point before the incident): s3://.../prod/

Steps
 1. Provision or reuse target host (same Redis version, same config repo tag)
 2. Fetch backup, verify checksum, run redis-check-rdb
 3. Restore with AOF off, verify DBSIZE and canary key age
 4. Enable AOF, wait for rewrite, set appendonly yes in config
 5. Reattach replicas, wait for full sync
 6. Repoint clients (endpoint/DNS), re-enable writers gradually
 7. Watch hit ratio, DB load, error rate for 30 minutes

Verification:  canary age within RPO, error rate normal, replicas in sync
Rollback:      keep the broken data dir; if restore is worse, revert endpoint
After:         timeline, root cause, update this runbook, add a regression test or alert
```

Keep runbooks next to the code, review them after every incident and drill.

## Managed services

- Enable automatic snapshots and understand their frequency, retention and restore time
- Check whether **point-in-time recovery** exists, and how you trigger it
- Take your own export on a schedule if the provider's retention is short, and store it in a separate account
- Test restoring a snapshot into a new instance, and time it
- Verify cross-region copies and what happens to endpoints and credentials after a restore

## Pitfalls

| Pitfall | Fix |
|---------|-----|
| Treating replicas as backups | A flush or corruption replicates immediately. Keep real backups |
| Never restoring a backup | Schedule automated restore tests and quarterly drills |
| Backups only on the same host or zone | Copy offsite, ideally cross-region |
| Restoring an RDB into a server with AOF enabled | Start with `appendonly no`, then re-enable after loading |
| Copying `dump.rdb` while a save is in progress | Wait for `BGSAVE` completion and check the status first |
| Copying a live AOF directory carelessly | Back up the RDB, or pause AOF rewrites while copying |
| Backing up from the primary under load | Back up from a replica |
| No alert on backup failure or staleness | Alert when the newest backup is older than expected |
| Unencrypted, widely readable backups | Encrypt, restrict, make immutable |
| Sentinel promoting an empty node during a restore | Control failover during the procedure |
| Assuming `everysec` or replication means zero loss | Document the real loss window, use `WAIT` or `min-replicas-to-write` where needed |
| Ignoring restore time | Measure it. It dominates RTO |
| Persistence enabled on a cache that does not need it | Turn it off to remove fork and disk overhead |
| Unpracticed failover | Run failover drills in staging and production |

## Key takeaways

- Persistence, replication and backups solve different problems, so important data needs all three
- Define RPO and RTO per dataset, and be honest about each mechanism's real loss window
- Use RDB for backups and AOF for durability. Take snapshots from a replica, verify and ship them offsite, encrypted and immutable
- Restore carefully: AOF-enabled servers prefer the AOF, so load the RDB with AOF off, then re-enable it
- Test restores and failovers regularly. Measured RTO beats assumed RTO
- Guard against human error with ACLs and separate credentials, because replicas copy mistakes instantly
- For rebuildable data, a tested rebuild procedure can replace backups

**Previous:** [Monitoring and Logging](./02_monitoring-and-logging.md) | **Next:** [Production Checklist](./04_production-checklist.md)
