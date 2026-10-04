# What Is AWS?

AWS (Amazon Web Services) is a collection of 200+ managed services (compute, storage, databases, networking, messaging, AI) that you rent over an API and pay for by usage. Instead of buying servers, racking them and running software on them, you call an API (or click in a console) and get a resource within seconds.

This note gives you the mental model for the rest of the repo: what "cloud" actually means, how AWS is organised, what it costs you in tradeoffs, and how it compares with Azure, GCP and running your own hardware.

---

## What "cloud" actually means

Three things make something "cloud" rather than "someone else's server":

1. **On-demand**: you provision in seconds or minutes, not weeks.
2. **Pay for use**: billed per hour, per second, per request, or per GB, not a fixed purchase.
3. **API-driven**: every resource is created, changed and deleted through an API. The console, CLI, CDK and Terraform are all just clients of that API.

Point 3 is the one that matters most for developers. It is what makes infrastructure *code*.

```bash
# The console is optional. This is the same API the console calls.
aws sts get-caller-identity          # who am I?
aws ec2 describe-regions --output table   # what regions exist?
```

### IaaS, PaaS, SaaS (and where AWS sits)

AWS spans all the layers. You choose how much you manage:

```
You manage more                                     AWS manages more
─────────────────────────────────────────────────────────────────────►
 EC2            ECS/EKS on EC2      Fargate / RDS      Lambda / S3 / DynamoDB
 (a VM)         (containers)        (managed runtime)  (just your code / data)
 IaaS                                PaaS-ish           "serverless"
```

Rule of thumb: **move right until something you need is no longer possible**. Every step right removes operational work and removes some control.

---

## How AWS is organised

```
AWS Account
 └── Region (e.g. ap-south-1 Mumbai, us-east-1 N. Virginia)
      └── Availability Zones (isolated data centres within the region)
           └── Resources (EC2 instances, subnets, ...)
```

- **Account**: the billing and security boundary. Not "your login": one person or company usually has several accounts.
- **Region**: a geographic area. Resources live in a region, and most services are regional. Regions are isolated from each other by design.
- **Availability Zone (AZ)**: one or more data centres in a region. Spreading across AZs is how you survive a data centre failure.
- **Global services** exist (IAM, Route 53, CloudFront), but they are the exception.

Details, including billing and free tier, are in [Accounts, Regions and Billing](./02-accounts-regions-and-billing.md).

### The service map

You will touch a small fraction of the catalogue. The core, and where each is covered in this repo:

| Need | Typical service |
|---|---|
| Run a server | EC2 |
| Run a function on events | Lambda |
| Run containers | ECS / Fargate |
| Store files | S3 |
| Key-value / NoSQL | DynamoDB |
| Relational DB | RDS / Aurora |
| Network isolation | VPC |
| DNS / CDN | Route 53 / CloudFront |
| Queues and events | SQS / SNS / EventBridge |
| Auth for your app's users | Cognito |
| Permissions | IAM |

---

## The shared responsibility model

The most important idea for security. AWS and you split the work:

```
┌──────────────────────────────────────────────────────┐
│  YOU: security IN the cloud                          │
│  your data, IAM policies, app code, OS patches (EC2),│
│  security groups, encryption choices                 │
├──────────────────────────────────────────────────────┤
│  AWS: security OF the cloud                          │
│  physical data centres, hardware, hypervisor,        │
│  managed-service internals                           │
└──────────────────────────────────────────────────────┘
```

The line moves with the service. On EC2 you patch the OS. On Lambda AWS does. On S3 AWS keeps the storage durable, but **a public bucket is still your mistake**. Most real-world AWS breaches are misconfiguration on the customer side, not AWS being hacked.

---

## How you pay

There is no single price. Each service has its own meters, and the bill is the sum:

- **Time**: EC2 and RDS are billed per instance-time, whether or not they are busy.
- **Requests / execution**: Lambda and API Gateway are billed per request and compute time. Idle costs roughly nothing.
- **Storage**: S3 and EBS are billed per GB-month.
- **Data transfer**: data *into* AWS is generally free; data *out* to the internet, and often *between* AZs or regions, is charged.

Hidden costs people trip over: NAT Gateways (hourly plus per-GB), cross-AZ traffic, idle load balancers, forgotten EBS volumes and Elastic IPs, and CloudWatch log volume. Set a billing alarm on day one. See [Accounts, Regions and Billing](./02-accounts-regions-and-billing.md).

---

## Pros and cons

**Strengths**

- **Breadth**: almost any building block exists as a managed service, so you rarely have to run something yourself.
- **Maturity and ecosystem**: the longest-running major provider (S3 and EC2 launched in 2006), with the largest community, documentation, tooling and hiring pool.
- **Elasticity**: scale up for a spike, scale to zero when idle (with the right services).
- **Global footprint**: many regions and AZs, useful for latency and compliance.
- **Reliability primitives**: multi-AZ, backups, managed failover are available off the shelf.

**Weaknesses**

- **Complexity**: the service count and the IAM model have a real learning curve. This repo exists partly because of that.
- **Cost is easy to get wrong**: usage-based pricing rewards attention and punishes neglect. At steady, predictable load, it can cost more than owned hardware.
- **Vendor lock-in**: proprietary services (DynamoDB, Lambda event wiring, Cognito) are hard to move. Portable pieces (containers, Postgres on RDS) reduce this.
- **Inconsistent developer experience**: services were built by different teams over 15+ years, so naming, APIs and console UX vary.
- **Shared responsibility is easy to misread**: managed does not mean secure by default.

---

## AWS vs Azure vs GCP vs self-hosting

| | AWS | Azure | GCP | Self-hosted |
|---|---|---|---|---|
| **Strongest at** | Breadth, ecosystem, default choice for startups and cloud-native work | Microsoft shops: Active Directory/Entra, .NET, Office 365, hybrid | Data/analytics (BigQuery), Kubernetes heritage, ML tooling | Predictable cost, full control, data residency |
| **Typical weakness** | Complexity, bill surprises | Console and naming can be confusing; breadth varies by area | Smaller market share and enterprise footprint | You own hardware, patching, scaling, on-call, and capacity planning |
| **Pick when** | No strong reason otherwise; widest hiring pool | Your org already runs on Microsoft | Your workload is data- or Kubernetes-centric | Stable load at scale, strict control or regulatory needs |

Honest take: for most teams the three big clouds are **capable of the same core things** (VMs, containers, object storage, managed databases, serverless). The decision usually comes down to existing contracts and skills, team familiarity, and one or two specific services, rather than a clear technical winner. Don't let anyone tell you one is categorically "better".

Self-hosting is not obsolete. If your load is flat and large, or you have hard data-location constraints, owning or colocating hardware can be cheaper. The cloud's real advantage is **not paying for capacity you don't use and not waiting to get it**.

---

## Common misconceptions

- **"The cloud is always cheaper."** Not at steady high utilisation. It is cheaper when load is spiky, uncertain, or small.
- **"Serverless means no servers."** Servers exist; you just don't manage them. You still deal with limits, cold starts, and cost per invocation.
- **"Managed means secure."** AWS secures the platform; configuration and access are yours.
- **"Multi-AZ means multi-region."** An AZ failure is survivable with multi-AZ. A whole-region outage needs a deliberate multi-region design, which is much harder.
- **"One account is enough."** Fine for learning. For real systems, separate accounts per environment (dev/prod) are a common isolation and blast-radius practice.
- **"I'll just click around in the console."** Fine to explore, but anything you want to repeat or review belongs in code. See [Infrastructure as Code](../06-operations/01-infrastructure-as-code.md).

---

## Quick Summary

- AWS = managed building blocks rented over an API, billed by usage.
- Hierarchy: **account → region → AZs → resources**. Most services are regional.
- Pick the level of abstraction deliberately: EC2 (control) → containers → Lambda/managed (less ops).
- **Shared responsibility**: AWS secures the platform; you secure your configuration, data and access.
- Costs come from many small meters; data transfer and idle resources are the usual surprises.
- AWS vs Azure vs GCP is mostly about existing skills and ecosystem, not raw capability. Self-hosting still wins in some steady-state cases.

**Next:** [Accounts, Regions and Billing](./02-accounts-regions-and-billing.md)