# AWS Interview Prep

Questions grouped by topic, each with a compact answer that shows *reasoning*, not just recall. Interviewers rarely want a service definition; they want to hear **trade-offs, failure modes and what you'd check first**. Each section links to the note that has the depth.

**How to answer well**

- Lead with the one-sentence answer, then the trade-off ("X, *unless* Y, because Z").
- Name the failure mode. "At-least-once delivery, so the consumer must be idempotent" beats "SQS is a queue".
- Quantify when you can (RTO/RPO, cost shape, limits), and say when you'd **verify** a number.
- For design questions: clarify requirements, state assumptions, draw the happy path, then walk through failure, security, cost.
- It's fine to say "I'd check the docs for the current limit." It's not fine to guess confidently.

> Limits and prices change. Treat numbers as "roughly, and I'd confirm".

---

## 1. Foundations

**What is the shared responsibility model?**
AWS secures the cloud itself (data centres, hardware, hypervisor, managed-service internals); you secure what you put *in* it: IAM, data, configuration, OS patching on EC2, security groups, encryption choices. The line moves with the service: Lambda removes OS patching from you; a public S3 bucket is still your mistake. ([What is AWS](../01-foundations/01-what-is-aws.md))

**Region vs Availability Zone? How do they affect design?**
A region is an isolated geographic area; an AZ is one or more separate data centres within it. Spread across 2+ AZs to survive a data-centre failure. Multi-AZ is *not* multi-region: a regional event needs a deliberately designed multi-region setup. Most services are regional, so "where did my resources go?" is usually the wrong region. ([Accounts](../01-foundations/02-accounts-regions-and-billing.md))

**Why shouldn't you use the root user?**
It can do everything, including closing the account, and can't be restricted by IAM policies. Enable MFA, delete/never create its keys, and use an admin identity (ideally IAM Identity Center) for daily work. Keep root for the few tasks that require it.

**A budget is set to $100. You hit $100. What happens?**
Nothing stops. Budgets *alert*; they don't cap spend. The protection is noticing early (actual + forecast alerts, anomaly detection) and having an owner who acts.

**What are the common surprise costs?**
NAT Gateway (hourly + per GB), idle load balancers, forgotten EC2/RDS/EBS/snapshots, public IPv4 addresses, cross-AZ and internet data transfer, CloudWatch Logs ingestion with infinite retention, and leaked credentials. ([Accounts](../01-foundations/02-accounts-regions-and-billing.md))

---

## 2. IAM

**How does IAM decide whether a request is allowed?**
Default deny. An explicit Deny anywhere wins. Otherwise there must be an Allow from an applicable policy, *and* no guardrail (SCP, permission boundary, session policy) may block it. Guardrails only limit; they never grant. ([IAM](../01-foundations/03-iam.md))

**User vs role vs group vs policy?**
A *user* is a long-lived identity; a *role* is assumed and yields temporary credentials; a *group* just bundles users for attaching policies (it isn't a principal); a *policy* is the JSON that allows/denies actions on resources. Prefer roles everywhere.

**What are the two policies on a role?**
The *trust policy* (who may assume it) and the *permissions policy* (what it may do once assumed). Most "can't assume role" bugs are trust-policy bugs; most "AccessDenied after assuming" bugs are permissions bugs.

**Identity-based vs resource-based policies? Cross-account access?**
Identity-based attach to a principal; resource-based attach to the resource (bucket policy, queue policy, trust policy, KMS key policy). Same account: an Allow in either is generally enough. Cross-account: **both** sides must allow (and KMS too, if encrypted).

**How do you give a Lambda access to DynamoDB?**
Via its **execution role** with a policy allowing the specific actions on the specific table ARN (and index ARNs if needed): no keys. In CDK, `table.grantReadWriteData(fn)` generates that. ([IaC](../06-operations/01-infrastructure-as-code.md))

**Explain least privilege in practice.**
Start narrow, add specific permissions as `AccessDenied` shows what's missing, then tighten using IAM Access Analyzer (policy generation from CloudTrail, unused-access findings). Scope `Resource` to ARNs; one role per workload; no `*:*` on application roles.

**You get `AccessDenied`. How do you debug?**
`aws sts get-caller-identity` (right identity/account?), read the error (action, resource, principal), check for explicit denies (SCP, boundary, bucket policy), check the *resource* policy and **KMS** for encrypted data, use the policy simulator, and look at the CloudTrail event.

**How should humans and CI authenticate?**
Humans: IAM Identity Center with MFA and short-lived credentials. CI: OIDC federation to a role (no stored keys). Workloads: roles. IAM user keys are a liability. ([CI/CD](../06-operations/02-ci-cd-to-aws.md))

**Cognito user pool vs identity pool vs IAM?**
User pool = directory + login + JWTs for *your app's users*. Identity pool = swaps a login for temporary AWS credentials (rarely needed). IAM/Identity Center = *your engineers'* access to AWS. ([Cognito](../05-app-services/04-cognito.md))

---

## 3. Compute

**EC2 vs Lambda vs Fargate: how do you choose?**
Lambda for event-driven, spiky or short (≤15 min) work with minimal ops and ~zero idle cost; Fargate for containers, steady services, long-running or >15 min work without managing servers; EC2 when you need OS/host control, special hardware, or a large steady fleet where you'll optimise packing. Move toward managed until a requirement stops you.

**What is a Lambda cold start, and how do you reduce its impact?**
A new execution environment must start, load code and run init code before the first invocation. Reduce with small packages, lighter init, avoiding heavy work outside the handler, arm64/faster runtimes, SnapStart where supported, or provisioned concurrency (pay to keep environments warm). Create SDK clients outside the handler so warm invocations reuse them. ([Lambda](../02-compute/02-lambda.md))

**Lambda invokes your function twice for one SQS message. Why, and what do you do?**
Standard SQS and most event sources are at-least-once, and the visibility timeout may expire mid-processing. Make the handler idempotent (idempotency key, conditional write), size the queue's visibility timeout relative to the function timeout, and report partial batch failures.

**Sync vs async vs poll-based Lambda invocation?**
Sync: caller waits and handles retries (API Gateway, ALB). Async: Lambda queues the event, retries on failure, then DLQ/destination (S3, SNS, EventBridge). Poll-based: Lambda polls (SQS, Kinesis, DynamoDB Streams) and retry behaviour depends on the source. Who retries and what "failure" means differs per mode.

**Why is Lambda memory a cost lever?**
Memory also scales CPU. More memory can finish faster, so total GB-seconds (cost) can *drop*. Measure with the REPORT log line or a power-tuning tool rather than guessing.

**A Lambda in a VPC can't reach the internet. Why?**
Attaching to a VPC drops default internet access; you need a NAT Gateway (or VPC endpoints for AWS services). Only attach to a VPC when you need private resources. ([VPC](../04-networking/01-vpc.md))

**ECS task role vs task execution role?**
Execution role is used by ECS to launch the task (pull the image, write logs, fetch secrets). Task role is used by your application code for its AWS calls. Mixing them up causes "image won't pull" vs "app gets AccessDenied". ([ECS](../02-compute/03-ecs-fargate.md))

**An ECS service keeps restarting tasks. What do you check?**
Service events and `describe-tasks` stop reasons first; then ALB health check path/port; the app listening on `0.0.0.0` (not `127.0.0.1`); memory limits (exit 137); slow startup (health check grace period); image architecture vs task CPU architecture; CloudWatch logs.

**EC2 instance is "stopped": are you still paying?**
Compute stops billing, but EBS volumes, snapshots, and Elastic IPs keep billing. Terminate deletes the root volume by default, which is irreversible. ([EC2](../02-compute/01-ec2.md))

**How do you get a shell on an EC2 instance securely?**
SSM Session Manager: no open port 22, no key pairs to manage, IAM-controlled and logged. Requires the SSM agent, an instance role with the managed-instance-core policy, and a network path to SSM endpoints.

**What is IMDSv2 and why enforce it?**
The instance metadata service hands out the instance role's temporary credentials. IMDSv1 can be read with a simple GET, which makes SSRF vulnerabilities dangerous; IMDSv2 requires a session token. Set `HttpTokens=required`.

---

## 4. Storage and databases

**S3 is a file system, right?**
No. It's a flat key-value object store; `/` is just a character. Objects are immutable (replace, don't edit), "rename" is copy + delete, and listing isn't a query. It does offer strong read-after-write consistency. ([S3](../03-storage-and-databases/01-s3.md))

**How do you let a browser upload a file directly to S3 securely?**
Your backend generates a short-lived **presigned PUT URL** (scoped to a key and content type); the browser uploads straight to S3 so bytes never pass through your server/Lambda. Configure CORS on the bucket. The bucket stays private.

**How do you serve a private S3 bucket as a website?**
CloudFront with Origin Access Control: the bucket is never public; a bucket policy allows only that distribution. Add `index.html` handling for SPA routes. Not the S3 website endpoint, which needs public access. ([CloudFront](../04-networking/02-route53-and-cloudfront.md))

**Your policy allows `s3:GetObject` but it still fails. Why?**
Common causes: the resource ARN lacks `/*`; an explicit deny (bucket policy/SCP); Block Public Access; the objects use SSE-KMS and the caller lacks KMS permissions; cross-account needs both sides; wrong identity/region.

**How do you control S3 cost?**
Lifecycle rules (transition to cheaper classes, expire noncurrent versions, abort incomplete multipart uploads), Intelligent-Tiering for unknown patterns, a gateway VPC endpoint to avoid NAT charges, CloudFront for egress, and Storage Lens/Cost Explorer to find growth.

**DynamoDB: Query vs Scan?**
Query reads one partition by key (plus an optional sort-key condition), so cost tracks the items read. Scan reads the whole table and filters afterward, so you pay for everything read. Scans in a request path signal a modelling problem. ([DynamoDB](../03-storage-and-databases/02-dynamodb.md))

**How do you model data in DynamoDB?**
Start from access patterns, not entities. Choose partition/sort keys so the common reads are single `Get`/`Query` calls, add GSIs for others, denormalise freely, and keep items small (400 KB limit; blobs go to S3). Single-table design is an option for well-known patterns, not a requirement.

**What is a hot partition?**
Traffic concentrated on a few partition-key values (a celebrity user, a low-cardinality `status`, a date-only key) throttles even when table capacity looks fine. Use high-cardinality, evenly accessed keys, or shard keys with a suffix.

**GSI vs LSI?**
LSI: same partition key, different sort key, created only at table creation, supports strong consistency, shares the table's capacity. GSI: any keys, creatable anytime, own capacity, **eventually consistent only**.

**How do you do an idempotent create or optimistic locking in DynamoDB?**
`ConditionExpression` (`attribute_not_exists(pk)` for create-once; compare a `version` attribute for optimistic locking). A failed condition throws `ConditionalCheckFailedException`, a normal outcome to handle.

**On-demand vs provisioned capacity?**
On-demand: pay per request, great for new/spiky/unknown traffic. Provisioned (with autoscaling): cheaper for steady predictable load. Start on-demand, measure, switch if the numbers justify it.

**RDS Multi-AZ vs read replica vs backup?**
Multi-AZ = availability (standby + automatic failover; not readable in the classic setup). Read replicas = read scaling, asynchronous so reads can be stale. Backups/PITR = recovery from mistakes. Multi-AZ doesn't protect against `DROP TABLE` and doesn't scale reads. ([RDS/Aurora](../03-storage-and-databases/03-rds-and-aurora.md))

**RDS vs Aurora vs DynamoDB?**
Relational with joins/transactions/ad-hoc SQL → RDS or Aurora (Aurora for faster failover, many readers, serverless scaling, global DB). Known key-based access patterns at massive/spiky scale with serverless ops → DynamoDB. Test Aurora's cost for your workload rather than assuming it's cheaper.

**Lambda keeps exhausting your database connections. Fix?**
Each concurrent execution opens its own connections. Use RDS Proxy (or another pooler), keep per-environment pool sizes tiny, cap concurrency (reserved/max concurrency or a queue in front), and create the client outside the handler.

**How do you make a database encrypted after the fact?**
You can't enable encryption in place on an unencrypted RDS instance: snapshot, copy the snapshot with encryption, restore. Better: enable at creation.

---

## 5. Networking

**What makes a subnet public?**
Routing: its route table has `0.0.0.0/0 → Internet Gateway`. A resource there also needs a public IP to be reachable. A private subnet has no such route; outbound internet goes via NAT, or AWS services via VPC endpoints. ([VPC](../04-networking/01-vpc.md))

**Security group vs NACL?**
Security group: stateful, allow-only, attached to the network interface, can reference other security groups. NACL: stateless, allow and deny, numbered rules, applied at the subnet boundary. Use security groups as the main control.

**Connection timed out vs connection refused?**
Timed out = the packet never got there or the reply never returned: security group, route, NACL, no NAT/IGW. Refused = the path works but nothing is listening on that port/interface.

**Design a secure 3-tier app network.**
VPC with public subnets (ALB only) and private subnets (app, then data) across 2+ AZs; `alb-sg` allows 443 from the internet; `app-sg` allows the app port from `alb-sg`; `db-sg` allows the DB port from `app-sg`; NAT (or endpoints) for outbound; S3/DynamoDB gateway endpoints; no public IPs on app/data; SSM instead of SSH.

**Why add an S3 gateway endpoint?**
It's free, keeps S3 traffic off the NAT Gateway (saving per-GB processing charges) and off the public internet, and supports endpoint policies.

**Alias record vs CNAME in Route 53?**
A CNAME can't sit at the zone apex; an alias record (Route 53 extension) points at AWS resources, works at the apex, follows target IP changes, and queries to AWS targets are free. Use alias for CloudFront/ALB/API Gateway targets. ([Route 53](../04-networking/02-route53-and-cloudfront.md))

**Why must the CloudFront certificate be in `us-east-1`?**
CloudFront only reads ACM certificates from that region, regardless of where your origin lives. Also add the domain as an alternate domain name on the distribution.

**How do you avoid stale content on CloudFront?**
Fingerprint static assets (`app.3f9c1a.js`) with long TTLs, keep `index.html` short-lived/no-cache, and invalidate only specific paths when needed. Keep the cache key minimal so hit ratio stays high.

**ALB vs NLB vs API Gateway?**
ALB: L7 routing for services you run. NLB: L4, static IPs, TCP/UDP, extreme throughput. API Gateway: managed API front door (auth, throttling, CORS), typically for Lambda. Rule of thumb: ALB for services you run, API Gateway for APIs you expose. ([Load Balancing](../04-networking/03-load-balancing-and-api-gateway.md))

**ALB returns 502, 503, 504: meanings?**
502: the target sent an invalid response or closed the connection (crash, keep-alive mismatch, malformed Lambda response). 503: no healthy targets. 504: the target didn't respond in time.

**API Gateway HTTP API vs REST API?**
HTTP API: simpler, cheaper, lower latency, JWT authorizers. REST API: API keys/usage plans, request validation, caching, private APIs, VTL transformations, WAF integration. Default to HTTP unless you need a REST-only feature. Remember REST needs an explicit deployment after changes.

**A request needs 60 seconds of work behind API Gateway. How?**
Integration timeouts are ~29 s by default. Return `202 Accepted` with a job id, enqueue the work ([SQS](../05-app-services/01-sqs.md)), process asynchronously, and let the client poll or receive a callback/WebSocket push.

---

## 6. Messaging and app services

**SQS visibility timeout: what is it and what goes wrong?**
After a receive, the message is invisible for the timeout; you must delete it on success or it reappears. Too short → duplicate processing while the first attempt still runs; for Lambda, make it comfortably longer than the function timeout. ([SQS](../05-app-services/01-sqs.md))

**Standard vs FIFO queue?**
Standard: very high throughput, at-least-once, best-effort ordering. FIFO: strict order per `MessageGroupId`, deduplication, lower throughput; a failing message blocks its group. Default to Standard with idempotent consumers.

**Why always configure a DLQ?**
Poison messages otherwise retry forever, wasting compute and hiding behind healthy traffic. DLQ + alarm on depth > 0 + redrive after fixing the bug.

**SQS vs SNS vs EventBridge vs Kinesis?**
SQS: one consumer per message, buffering. SNS: push fan-out to many (often SNS → SQS). EventBridge: content-based routing, AWS/SaaS events, schedules, cross-account buses. Kinesis: ordered replayable streams with multiple readers re-reading history. ([SNS/EventBridge](../05-app-services/02-sns-and-eventbridge.md))

**SNS publishes but your SQS queue receives nothing. Why?**
The queue's access policy must allow `sns.amazonaws.com` (conditioned on the topic ARN); check the subscription is confirmed, filter policies, and KMS permissions if encrypted. Also decide on raw message delivery, or you'll receive SNS's JSON envelope.

**An EventBridge rule matches but the target never runs?**
Missing target permission (role or resource-based policy on the target), target errors landing in a DLQ, or a mismatch between expected and actual event shape. Test patterns with `test-event-pattern`.

**How do you get your emails out of spam with SES?**
Verify the domain; set up DKIM, SPF/custom MAIL FROM and DMARC with alignment; include a text part; send only to opted-in recipients; handle bounces/complaints through configuration-set events and stop sending to those addresses; warm up volume; separate transactional and marketing. Request production access (sandbox) per region. ([SES](../05-app-services/03-ses-email.md))

**How do you verify a Cognito JWT?**
Check the signature against the pool's JWKS, `iss`, `exp`, `aud`/`client_id`, and `token_use`; use `sub` as the stable user key. Libraries like `aws-jwt-verify` do this. Authentication isn't authorisation: enforce permissions with groups/scopes in your API. ([Cognito](../05-app-services/04-cognito.md))

**You're building an LLM feature on Bedrock. What do you watch?**
Use Converse; inference profiles and IAM for profile + model ARNs; keep model IDs in config (deprecation); check `stopReason` and `usage`; backoff on throttling; stream for latency; cap tokens for cost; validate structured output; treat tool calls and retrieved text as untrusted (prompt injection); build evals before changing prompts or models. ([Bedrock](../05-app-services/05-bedrock.md))

---

## 7. Operations

**CDK vs SAM vs Terraform?**
CDK: real programming languages, grants, reuse; outputs CloudFormation. SAM: compact serverless YAML with great local tooling. Terraform: multi-cloud with its own state. On AWS-only TypeScript teams, CDK is a strong default; SAM for mostly-Lambda apps. ([IaC](../06-operations/01-infrastructure-as-code.md))

**What can go wrong when you rename a CDK construct?**
The logical ID changes, so CloudFormation deletes the old resource and creates a new one, a data-loss risk for stateful resources. Read `cdk diff`, set `RemovalPolicy.RETAIN` on data, use `cdk refactor` for planned moves/renames, and never hard-code physical names.

**What does `cdk bootstrap` do?**
Creates the toolkit stack in an account/region (S3 bucket and ECR repo for assets, IAM roles for deployment). Required once per environment before the first deploy.

**How do you deploy from GitHub Actions without AWS keys?**
OIDC federation: create an IAM OIDC provider for GitHub, a role whose trust policy pins `aud` and a specific `sub` (repo + branch/environment), grant `id-token: write` in the workflow, and use the credentials action. Use one role per environment and a read-only role for PR checks. ([CI/CD](../06-operations/02-ci-cd-to-aws.md))

**What's wrong with `"sub": "repo:my-org/*"` in the trust policy?**
Any repository in the org can assume the role. Pin the exact repo and branch or environment; make environment protection rules double as AWS access control.

**Secrets Manager vs Parameter Store?**
Secrets Manager: built-in rotation (RDS etc.), versioning with staging labels, replication, per-secret + per-call cost. Parameter Store: free standard tier, great for config and simple secrets, no built-in rotation. Pass ARNs, not values; cache reads. ([Security](../06-operations/03-security-and-secrets.md))

**You committed an access key to a public repo. What now?**
Treat as compromised: deactivate the key immediately, rotate anything it could read, review CloudTrail across all regions for activity, look for persistence (new users/keys/roles, instances, trust-policy changes), check billing, fix the root cause, and add secret scanning.

**CloudWatch vs CloudTrail?**
CloudWatch = operational telemetry (logs, metrics, alarms, traces) about your *application and resources*. CloudTrail = audit log of *API calls* ("who did what"). The default 90-day event history isn't an audit log. Create an organisation trail to a protected bucket.

**What do you alarm on?**
User-visible symptoms: edge error rate and p99 latency, Lambda errors/throttles, DLQ depth and queue age, running vs desired tasks, DB storage/connections, plus "the service has gone silent". Choose `treat-missing-data` deliberately and make every alarm actionable. ([Observability](../06-operations/04-observability.md))

**X-Ray vs OpenTelemetry today?**
The X-Ray service is still supported and gaining features, but the X-Ray SDKs and Daemon went into maintenance mode on 25 Feb 2026. Instrument new work with OpenTelemetry (ADOT or the CloudWatch Agent), with CloudWatch Application Signals for SLOs and Lambda Active tracing as a quick win.

**Why are high-cardinality dimensions on custom metrics dangerous?**
Each unique dimension-value combination is a separate billed metric, so `userId` as a dimension creates unbounded cost. Keep dimensions low-cardinality and put identifiers in log fields.

---

## 8. Reliability and cost

**Define RTO and RPO. How do they drive DR strategy?**
RTO = how long you can be down; RPO = how much data you can lose. Tight RTO/RPO pushes toward warm standby or active-active (more cost/complexity); loose targets allow backup-and-restore or pilot light. Most systems are well served by multi-AZ plus tested backups. ([Reliability and Cost](../06-operations/05-reliability-and-cost.md))

**How do you make a service reliable against a flaky dependency?**
Timeouts everywhere, retries with exponential backoff + jitter (bounded, and only for safe operations), idempotency, circuit breakers/fail-fast, backpressure via queues and concurrency limits, graceful degradation, DLQs, and alarms. Avoid stacking retries across layers (retry storms).

**What causes most outages, and how do you reduce that risk?**
Changes, not hardware. Deploy via CI with reviewed diffs, roll out gradually (rolling with circuit breaker, weighted aliases, canaries, feature flags), gate on alarms, keep a rehearsed rollback, and make DB migrations backward-compatible.

**Your backups exist. Are you safe?**
Only if you've restored from them. Run restore drills, copy backups cross-account/region with immutability (vault lock/Object Lock), and rehearse DNS failover and runbooks.

**A health check design question: deep or shallow?**
Load balancer checks should usually be shallow (is the process up?). A deep check on a shared dependency can fail the entire fleet at once when that dependency blips, turning a partial degradation into a full outage.

**How would you cut an AWS bill by 30%?**
Get visibility first (tags, Cost Explorer by service → usage type, anomaly detection). Then: delete idle/forgotten resources and schedule dev/test off-hours; right-size (Compute Optimizer); Graviton; storage lifecycle; fix NAT/data transfer (gateway endpoints, CloudFront, same-AZ traffic); log retention and sampling; then commit steady baselines with Savings Plans; Spot for fault-tolerant work. Don't trade away redundancy you need.

**Savings Plans before or after right-sizing?**
After. Committing to oversized or soon-to-change resources locks in waste. Right-size, confirm the baseline is steady, then commit to what you're sure about.

---

## 9. Scenario / design prompts

For each: clarify requirements, then walk the same checklist: **entry point → compute → data → async work → auth → network → observability → failure → cost**.

### A. Serverless REST API for a mobile app (CRUD + notifications)
- **Auth:** Cognito user pool → API Gateway HTTP API JWT authorizer.
- **Compute/data:** Lambda (per route or small router) → DynamoDB (keys from access patterns, PITR on, `RETAIN`).
- **Async:** order events → SNS/EventBridge → SQS → Lambda workers (idempotent, DLQs) → SES/SNS push for notifications.
- **Uploads:** presigned S3 URLs.
- **Ops:** IaC (CDK), CI with OIDC, structured logs, alarms on 5XX/latency/DLQ, tracing sampled, budgets.
- **Failure:** at-least-once → idempotency; throttling → backoff; API timeouts → async pattern for long jobs.

### B. Containerised web app with a relational database
- CloudFront (optional) → ALB (public subnets, 2+ AZs) → ECS Fargate tasks (private subnets) → Aurora/RDS Multi-AZ (private).
- Security groups chain by reference; secrets via Secrets Manager injection; task role for S3/SQS; ECR with SHA-tagged images.
- Deployments: rolling + circuit breaker; migrations expand→migrate→contract.
- Scaling: target tracking on CPU/request count; RDS Proxy if connection-heavy.
- Cost/DR: right-size, Savings Plans after baseline, PITR + cross-region snapshot copies for DR, tested restore.

### C. Static site / SPA with a backend API
- S3 private bucket + CloudFront (OAC), ACM cert in `us-east-1`, Route 53 alias; error-page rewrite for SPA routes; fingerprinted assets.
- `/api/*` behaviour → API Gateway or ALB with caching disabled; WAF at CloudFront.
- CI: build → sync → invalidate `index.html` only.

### D. Image/document processing pipeline
- S3 upload → event → SQS (buffer) → Lambda/Fargate workers → results to S3 + DynamoDB metadata → notification via SNS/EventBridge.
- Idempotency on object key/version; DLQ + alarm; avoid recursive triggers (separate prefixes); concurrency caps; Lambda memory tuned; large files → Fargate or multipart.

### E. Multi-account, secure-by-default setup
- AWS Organizations: separate prod/non-prod/log/security accounts; SCP guardrails (region restrictions, protect CloudTrail/GuardDuty); IAM Identity Center for people; OIDC roles for CI; org-wide CloudTrail to a log account; GuardDuty/Security Hub/Config; account-level S3 Block Public Access; cost tags and budgets per account.

### F. RAG chatbot on Bedrock
- Docs in S3 → Knowledge Base (or custom chunk + embed + pgvector/OpenSearch/S3 Vectors) → retrieve top-k → Converse with citations; Guardrails; streaming to the UI (not behind a 29 s synchronous API call).
- Cost: token caps, prompt caching, smaller models for easy tasks; evals; prompt-injection defences; per-tenant authorisation on retrieval (filter by metadata).

---

## 10. Rapid-fire: true or false?

| Statement | Answer |
|---|---|
| Multi-AZ RDS gives you read scaling. | **False** (classic standby isn't readable); that's read replicas |
| Multi-AZ replaces backups. | **False**: a bad write replicates instantly |
| An AWS Budget will stop your spending at the limit. | **False**: it alerts only |
| S3 listings may be eventually consistent. | **False** today: strong read-after-write consistency |
| An S3 bucket name is private to your account. | **False**: globally unique and discoverable |
| A DynamoDB `FilterExpression` reduces the read capacity you consume. | **False**: it filters after reading |
| GSI reads can be strongly consistent. | **False**: eventually consistent only |
| FIFO queues remove the need for idempotent consumers. | **False** |
| Lambda can run for 30 minutes. | **False**: 15-minute max |
| A security group can contain deny rules. | **False**: allow-only (NACLs deny) |
| VPC peering is transitive. | **False** |
| Stopping an EC2 instance stops all charges. | **False**: EBS, EIPs, snapshots |
| Public ACM certs for CloudFront can live in any region. | **False**: `us-east-1` |
| A CNAME is fine at the zone apex. | **False**: use an alias record |
| Access keys in CI are fine if stored as secrets. | **False**: prefer OIDC roles |
| CloudTrail's event history is a complete audit log. | **False**: 90 days; create a trail |
| `cdk deploy --hotswap` is fine for production. | **False**: dev only; causes drift |
| API keys on API Gateway are a security mechanism. | **False**: metering, not authentication |
| The X-Ray SDK is the recommended way to instrument new apps. | **False** today: OpenTelemetry |
| Encrypting data means only authorised principals can read it. | **False**: authorised-but-wrong principals decrypt fine |

---

## 11. Questions to ask the interviewer

Good questions signal operational maturity:

- How do you deploy to production, and how fast can you roll back?
- What are your SLOs, and who gets paged when they burn?
- How is AWS access managed: Identity Center, OIDC for CI, any long-lived keys left?
- How do you handle account structure, cost ownership and tagging?
- When did you last test a restore or run a game day?
- What's the biggest operational pain in the current AWS setup?

---

## Study map

| If you're weak on… | Re-read |
|---|---|
| Permissions and `AccessDenied` | [IAM](../01-foundations/03-iam.md), [Security](../06-operations/03-security-and-secrets.md) |
| Networking / timeouts | [VPC](../04-networking/01-vpc.md), [Load Balancing](../04-networking/03-load-balancing-and-api-gateway.md) |
| Event-driven design | [SQS](../05-app-services/01-sqs.md), [SNS/EventBridge](../05-app-services/02-sns-and-eventbridge.md), [Lambda](../02-compute/02-lambda.md) |
| Data modelling | [DynamoDB](../03-storage-and-databases/02-dynamodb.md), [RDS/Aurora](../03-storage-and-databases/03-rds-and-aurora.md) |
| Operating in production | [Observability](../06-operations/04-observability.md), [Reliability and Cost](../06-operations/05-reliability-and-cost.md) |
| Quick lookups | [Cheatsheet](./cheatsheet.md) |