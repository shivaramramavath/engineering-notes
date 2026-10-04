# Reliability and Cost

Two sides of the same engineering coin. **Reliability** is making sure the system keeps working when parts of it fail (and they will). **Cost** is making sure you're not paying for more than you need. They pull against each other: every extra AZ, replica or standby costs money, so the real skill is deciding *how much reliability a given system is worth*, then buying exactly that.

This is the capstone of the repo. It leans on everything before it, and links back rather than repeating details.

Prerequisites: [Observability](./04-observability.md), [VPC](../04-networking/01-vpc.md), [RDS and Aurora](../03-storage-and-databases/03-rds-and-aurora.md), [Accounts and Billing](../01-foundations/02-accounts-regions-and-billing.md).

---

# Part 1: Reliability

## Start with targets, not tools

You can't design for "reliable"; you can design for numbers:

| Term | Meaning | Example |
|---|---|---|
| **SLO** | Target for user-visible behaviour | 99.9% of requests succeed in < 500 ms over 30 days |
| **Error budget** | The allowed failure (100% − SLO) | 99.9% ≈ 43 minutes of downtime per 30 days |
| **RTO** | Recovery Time Objective: how long you can be down | 1 hour |
| **RPO** | Recovery Point Objective: how much data you can lose | 5 minutes |

Availability is multiplicative: a request that depends on three components each at 99.9% is at about 99.7%. Every extra synchronous dependency lowers it. Decide per system. A marketing site and a payments ledger deserve different answers.

## Design principles that actually matter

1. **Expect failure.** Disks, instances, AZs, dependencies and your own deployments fail. Design so a single failure isn't an outage.
2. **Remove single points of failure.** One instance, one NAT Gateway, one AZ, one database primary without failover: each is a single point.
3. **Keep services stateless;** put state in managed stores (S3, DynamoDB, RDS) so any instance can die and be replaced.
4. **Automate recovery.** Auto Scaling, ECS service scheduling, health checks and managed failover beat human reaction time.
5. **Limit blast radius.** Separate accounts and stacks per environment, cell-based or per-tenant isolation, throttling, and gradual rollouts.
6. **Test failure** instead of assuming it works.

### Within a region: multi-AZ is the baseline

| Layer | Resilience move |
|---|---|
| Compute | Spread across **2+ AZs**: Auto Scaling group, ECS service with tasks in several subnets ([EC2](../02-compute/01-ec2.md), [ECS](../02-compute/03-ecs-fargate.md)); Lambda is multi-AZ by default |
| Load balancing | ALB/NLB enabled in 2+ AZs with health checks ([Load Balancing](../04-networking/03-load-balancing-and-api-gateway.md)) |
| Database | **RDS Multi-AZ** or Aurora with a replica in another AZ; DynamoDB and S3 are multi-AZ by design |
| Network | A NAT Gateway per AZ (or the regional mode) rather than one shared ([VPC](../04-networking/01-vpc.md)) |
| Storage | EBS is single-AZ, so snapshot it or avoid depending on it for state |

Remember: **Multi-AZ is not backup, and not multi-region.** It protects against an AZ failure, not against a bad deployment, a `DROP TABLE`, or a regional event.

### Handling failing dependencies

Most outages in distributed systems are cascades: one slow dependency exhausts threads/connections everywhere. Defences:

- **Timeouts everywhere.** Every network call needs one. Defaults are often infinite or far too long. A caller's timeout must be **shorter than its own caller's**, or you waste work.
- **Retries with exponential backoff and jitter**, only for idempotent or safely repeatable operations, with a **cap**. Retrying aggressively during an outage multiplies load ("retry storms"). The AWS SDKs have retry behaviour built in, so know its limits and don't stack layers of retries.
- **Idempotency** (idempotency keys, conditional writes) so retries and at-least-once delivery are safe ([SQS](../05-app-services/01-sqs.md), [DynamoDB](../03-storage-and-databases/02-dynamodb.md)).
- **Circuit breakers / fail fast:** stop calling a failing dependency for a while instead of piling on.
- **Backpressure and load shedding:** queue work ([SQS](../05-app-services/01-sqs.md)), limit concurrency (Lambda reserved/max concurrency), return `429`/`503` early rather than collapsing.
- **Graceful degradation:** serve cached or reduced functionality when a non-critical dependency is down (recommendations missing is better than checkout broken).
- **Dead-letter queues** so poison messages don't block healthy ones, with alarms on them.
- **Health checks that mean something:** a *shallow* check (process is up) for load balancers; a *deep* check on dependencies only where failure should remove the instance, since a deep check on a shared dependency can take your whole fleet out of rotation at once.

### Quotas and limits

AWS enforces **Service Quotas** (concurrency, API rates, instances, addresses). Hitting one during a traffic spike or recovery is a common self-inflicted outage. Know the limits for your critical services (Lambda concurrency, Bedrock tokens per minute, SES sending rate, API throttles), request increases **before** you need them, and alarm on usage approaching the limit. Throttling (`429`, `ThrottlingException`) is normal behaviour to design for, with backoff.

### Deployments are the biggest source of outages

Many incidents are caused by changes, not hardware. So:

- Deploy through **CI with a reviewed `diff`** ([IaC](./01-infrastructure-as-code.md), [CI/CD](./02-ci-cd-to-aws.md)).
- Prefer **gradual rollout**: ECS rolling with circuit breaker, Lambda weighted aliases, canaries, feature flags.
- Keep **fast, rehearsed rollback** (redeploy the previous artifact).
- Make DB migrations **backward-compatible** (expand → migrate → contract).
- Use **alarms as deployment gates**: auto-roll back when error rate rises.

## Backup and disaster recovery

**Backups are not DR until you've restored from them.** A backup you've never tested is a hope, not a plan.

| Strategy | What runs in the DR region | RTO | RPO | Relative cost |
|---|---|---|---|---|
| **Backup & restore** | Nothing; only backups copied across region/account | Hours+ | Hours (backup interval) | $ |
| **Pilot light** | Core data replicated; compute off/minimal | Tens of minutes–hours | Minutes | $$ |
| **Warm standby** | Scaled-down full copy running | Minutes | Seconds–minutes | $$$ |
| **Active-active (multi-site)** | Full capacity in 2+ regions | Near zero | Near zero | $$$$ |

Choose by RTO/RPO and budget. Most systems are fine at multi-AZ plus tested backups. Multi-region is expensive and complex (data replication, conflict handling, DNS failover, testing), so do it only when the business requires it.

Practical building blocks:

- **AWS Backup** centrally schedules and retains backups across RDS, DynamoDB, EBS, EFS, S3 and more, including **cross-region and cross-account copies** and **vault lock** (immutability against ransomware/deletion).
- **RDS automated backups + PITR**, snapshots copied cross-region; **DynamoDB PITR** and global tables; **S3 versioning, replication, Object Lock**.
- **Cross-account backup copies**, so a compromised account can't delete its own backups.
- **Infrastructure as code** makes rebuilding an environment in another region a deployment, not an archaeology project.
- **DNS failover** with Route 53 health checks ([Route 53](../04-networking/02-route53-and-cloudfront.md)). Rehearse it, because failover paths that are never exercised tend not to work.
- Be careful about **control-plane dependencies** during a regional event. Prefer failover mechanisms that rely on data-plane actions you've pre-provisioned over ones that need to create resources at the worst moment.

### Test it

- **Restore drills:** regularly restore a backup into a scratch environment and verify the data.
- **Game days / fault injection:** deliberately kill instances, fail an AZ's subnets, block a dependency, and confirm alarms fire and recovery happens. **AWS Fault Injection Service (FIS)** runs controlled experiments.
- **Load tests** before launches, including at the quota limits you depend on.
- Write **runbooks** and run **blameless post-incident reviews** that produce concrete fixes.

The **AWS Well-Architected Framework** (and its Reliability pillar, plus the free Well-Architected Tool) gives structured review questions if you want a checklist beyond this note.

---

# Part 2: Cost

## You can't optimise what you can't see

Set up visibility **before** optimising:

1. **Budgets and alerts** ([Accounts and Billing](../01-foundations/02-accounts-regions-and-billing.md)): alert on actual and forecast spend.
2. **Cost Anomaly Detection:** automatic alerts when spend deviates from the pattern.
3. **Tagging:** consistent tags (`app`, `env`, `owner`, `cost-center`), **activated as cost allocation tags** in Billing, so Cost Explorer can group spend by team and service. Untagged spend can't be assigned to anyone. Enforce tags in IaC (CDK `Tags.of(app).add(...)`) and with tag policies.
4. **Cost Explorer:** group by **service → usage type → region → tag**. For deep analysis, export to S3 (**Data Exports / Cost and Usage Report**) and query with Athena.
5. **Separate accounts** per environment/team: the cleanest cost boundary there is.

## Where the money usually goes, and the levers

Compute, data transfer, databases, storage and logging typically dominate. Attack the biggest line items first.

| Area | Levers |
|---|---|
| **Idle and forgotten resources** | Delete unused EC2/RDS/EBS volumes/snapshots/Elastic IPs/load balancers/NAT Gateways; shut down dev/test outside working hours; use TTLs on temporary environments. This is often the quickest saving. |
| **Right-sizing** | Match instance sizes to measured CPU/memory. **Compute Optimizer** recommends changes for EC2, EBS, Lambda and ECS-on-Fargate. Tune Lambda memory by measuring ([Lambda](../02-compute/02-lambda.md)). |
| **Pricing models** | Commit steady baselines with **Savings Plans** (flexible across instance families/services) or Reserved Instances; **Spot** for fault-tolerant work ([EC2](../02-compute/01-ec2.md)). Commit *after* right-sizing and only to what's truly steady. |
| **Architecture fit** | Spiky/idle workloads → **Lambda, Fargate, DynamoDB on-demand, Aurora Serverless**; steady high-throughput → provisioned/containers/instances often cheaper. Compare at your real traffic. |
| **Graviton (ARM)** | Typically cheaper for the same performance across EC2, Lambda, Fargate, RDS if your software supports `arm64`. |
| **Storage** | S3 **lifecycle rules** and Intelligent-Tiering; expire old versions and incomplete multipart uploads; `gp3` over older EBS types; delete old snapshots ([S3](../03-storage-and-databases/01-s3.md)). |
| **Data transfer** | Keep chatty services in the **same AZ/region** where possible; add **S3/DynamoDB gateway endpoints**; use **CloudFront** (origin-to-CloudFront transfer is free); avoid pointless NAT traffic; watch cross-AZ and cross-region charges ([VPC](../04-networking/01-vpc.md)). |
| **Databases** | Right-size instances; reserve steady ones; use Aurora Serverless for variable load; read replicas only if needed; delete old snapshots; consider DynamoDB table class / capacity mode ([RDS](../03-storage-and-databases/03-rds-and-aurora.md), [DynamoDB](../03-storage-and-databases/02-dynamodb.md)). |
| **Observability** | Log **retention**, log level discipline, sampling traces, low-cardinality metrics ([Observability](./04-observability.md)). |
| **Public IPv4** | Billed hourly per address: remove unneeded ones, put services behind load balancers, adopt IPv6 where possible. |
| **AI/Bedrock** | Cap `maxTokens`, trim history, cache prompts, route simple work to smaller models, use batch inference for offline jobs ([Bedrock](../05-app-services/05-bedrock.md)). |

(For the common surprise-bill culprits, see the table in [Accounts and Billing](../01-foundations/02-accounts-regions-and-billing.md); it's not repeated here.)

## The cost/reliability trade-offs, made explicit

| Decision | Reliability gain | Cost impact |
|---|---|---|
| RDS **Multi-AZ** | Automatic failover | Roughly doubles instance cost |
| NAT Gateway **per AZ** | No cross-AZ dependency | More hourly charges (the regional mode simplifies this; check pricing) |
| **Aurora** replicas / readers | Fast failover, read scaling | Extra instance cost |
| **Warm standby / multi-region** | Lower RTO and RPO | Duplicate infrastructure and data transfer |
| Longer **backup/log retention** | Better recovery/forensics | Storage cost |
| **Provisioned concurrency**, over-provisioned capacity | Predictable latency, headroom | Pay for idle |
| **Savings Plans / RIs** | None | Lower unit cost, but a commitment risk |
| **Spot** | None (adds interruption risk) | Large discount |

Write the choice down with the reason ("Multi-AZ on prod DB only; dev single-AZ"). Apply **production-grade reliability to production and cheaper settings to dev/test**: that distinction alone often cuts non-prod spend dramatically.

## FinOps habits

- **Review cost monthly** (even 30 minutes): top services, biggest movers, anomalies, untagged spend.
- **Give teams their own cost visibility** and ownership via tags/accounts; whoever deploys should see what it costs.
- **Estimate before building** (the AWS Pricing Calculator), and re-check after launch.
- **Automate cleanup** (lifecycle rules, TTLs, scheduled shutdowns, ephemeral PR environments that tear down).
- Treat **cost as a metric** you alarm on, alongside latency and errors.
- Prefer **IaC** so environments are reproducible and disposable.
- Beware **false economy**: saving money by removing redundancy or monitoring just before an outage is expensive. Optimise waste, not resilience you actually need.

---

## Production-readiness checklist

Before calling a service production-ready:

- [ ] SLO and RTO/RPO defined and agreed
- [ ] Multi-AZ for compute, load balancer, database; no single points of failure
- [ ] Timeouts, bounded retries with backoff/jitter, idempotent operations, DLQs
- [ ] Alarms on user-visible symptoms, routed to a human; runbooks linked ([Observability](./04-observability.md))
- [ ] Backups configured, **restore tested**, cross-account/region copy if required
- [ ] Deploys via CI, with diff review, gradual rollout and a practised rollback
- [ ] Quotas checked for peak load; load tested
- [ ] Least-privilege roles, secrets managed, encryption on ([Security and Secrets](./03-security-and-secrets.md))
- [ ] Tags applied, budget and anomaly alerts set, log retention configured
- [ ] Non-prod environments sized and scheduled to cost less than prod

---

## Common mistakes

**Reliability**
- Treating **Multi-AZ as backup** or as multi-region protection.
- **Untested backups** and never-rehearsed failover.
- **No timeouts**, or unbounded retries that amplify outages.
- **Non-idempotent** operations behind at-least-once delivery.
- A **single NAT Gateway / single AZ / single instance** in production.
- **Deep health checks on shared dependencies**, removing the whole fleet at once.
- Discovering **service quotas** during an incident.
- **Big-bang deployments** with no rollback plan.
- Over-engineering **multi-region** when multi-AZ plus backups meets the requirement.

**Cost**
- **No tags**, so nobody owns the bill.
- **Optimising before measuring**, or committing to **Savings Plans before right-sizing**.
- Leaving **dev/test running 24/7** at prod size.
- Ignoring **data transfer**, **NAT**, and **log** costs.
- Buying **RIs/Savings Plans** for a workload that's about to change.
- Cutting **redundancy or observability** to save money, then paying for the outage.
- Never **reviewing the bill** until it's a surprise.

---

## Quick Summary

- **Define targets first**: SLO, RTO, RPO. Reliability is bought deliberately, not maximised blindly.
- **Multi-AZ is the baseline**, but it isn't backup or multi-region. Remove single points of failure; keep services stateless.
- Survive bad dependencies with **timeouts, backoff + jitter, idempotency, circuit breaking, backpressure, DLQs, graceful degradation**.
- Most outages come from **changes**: use CI, diffs, gradual rollout, alarms as gates, and a practised rollback.
- **Backups don't count until you've restored them.** Choose a DR strategy (backup/restore → pilot light → warm standby → active-active) by RTO/RPO and budget; test with restore drills and fault injection.
- **Cost: visibility first**: budgets, anomaly detection, **tags**, Cost Explorer. Then cut **idle waste → right-size → commit (Savings Plans) → fit architecture → storage/transfer/logs**.
- Make cost/reliability trade-offs **explicit and per environment**: prod gets resilience, dev gets frugality.
- Review cost monthly and treat it as an operational metric.

**Next:** the reference section: [Cheatsheet](../reference/cheatsheet.md) and [Interview prep](../reference/interview.md).
