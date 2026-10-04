# Accounts, Regions and Billing

Before you create a single resource you need three things straight: what an AWS **account** is, where your resources physically live (**regions** and **AZs**), and how not to get a surprise bill. This note covers all three, because the mistakes in each are the most common ones beginners make.

---

## The AWS account

An account is the **billing and security boundary** in AWS. Resources, IAM users and costs all belong to exactly one account. It is not the same thing as "your login".

When you sign up you create:

- an **account ID** (12 digits, appears in every ARN), and
- a **root user**, the email + password you signed up with, which can do *everything*, including closing the account.

### Root user rules

1. Enable **MFA** on the root user immediately.
2. Do not create access keys for root. Ever.
3. Do not use root for daily work. Create an admin identity (preferably through IAM Identity Center, see [IAM](./03-iam.md) and [AWS CLI](./04-aws-cli.md)) and use that.
4. Keep root for the handful of tasks that genuinely need it (closing the account, changing the support plan, some billing settings).

### One account or many?

Fine to start with one for learning. For real systems, teams commonly use **separate accounts per environment** (dev / staging / prod), managed under **AWS Organizations**. Reasons: a mistake or compromise in dev cannot touch prod, billing is separated per account, and permissions are easier to reason about. This is a convention, not a requirement, but it is the standard isolation tool in AWS.

---

## Regions and Availability Zones

```
Region: ap-south-1 (Mumbai)
 ├── AZ  ap-south-1a   ← one or more data centres
 ├── AZ  ap-south-1b
 └── AZ  ap-south-1c
```

- A **region** is a geographic area with a code like `us-east-1`, `eu-west-1`, `ap-south-1`. Regions are **isolated** from each other: resources and data do not leave a region unless you move them.
- An **Availability Zone** is a physically separate location inside a region, connected to the others by low-latency links. Deploying across 2+ AZs is how you survive a single data-centre failure.
- Most services are **regional**: an S3 bucket, a DynamoDB table or a Lambda function exists in one region. A few are **global** (IAM, Route 53, CloudFront control plane).

### Choosing a region

In rough priority order:

1. **Compliance / data residency**: some data must stay in a country or jurisdiction.
2. **Latency** to your users.
3. **Service availability**: new services and features often launch in a few regions first.
4. **Price**: varies by region, sometimes noticeably.

### Things that catch people

- **"Where did my resources go?"** The console shows one region at a time. Switch the region selector (top right) and your EC2 instances "disappear". They didn't; you're looking at the wrong region. Same with the CLI: set `--region`, `AWS_REGION`, or the profile's `region`.
- **AZ names are not stable across accounts.** `us-east-1a` in your account may map to a different physical zone than in someone else's. When coordinating across accounts, use **AZ IDs** (e.g. `use1-az1`).
- **Opt-in regions.** Regions launched in recent years are disabled by default and must be enabled per account before use.
- **Some things are pinned to `us-east-1`.** Certificates for CloudFront (ACM) must be requested there, and the CloudWatch billing metrics live there. Remember this when you reach [Route 53 and CloudFront](../04-networking/02-route53-and-cloudfront.md).
- **Multi-AZ is not multi-region.** Multi-AZ protects against a zone failure. A regional outage needs a deliberately designed multi-region setup, which is much harder.

```bash
aws ec2 describe-regions --output table                 # enabled regions
aws ec2 describe-availability-zones --region ap-south-1 \
  --query 'AvailabilityZones[].[ZoneName,ZoneId]' --output table
```

---

## Free tier

The free tier changed for accounts created on or after **15 July 2025**. Check the [official Free Tier page](https://aws.amazon.com/free/) before relying on any detail here, as it has been revised before.

For new accounts you choose a plan at sign-up:

| | Free plan | Paid plan |
|---|---|---|
| Credits | $100 at sign-up, up to $100 more by completing onboarding activities | Same credits |
| Duration | Ends after 6 months, or when credits run out, whichever is first | No expiry |
| When credits run out | Account stops being usable (there is a grace period to upgrade to keep resources) rather than billing you | Normal pay-as-you-go billing starts |
| Service access | Some services are restricted | Full catalogue |

Plus, on both plans, a set of **Always Free** services with monthly allowances (30+ at the time of writing).

Older accounts (created before that date) may still be on the previous model: specific allowances for 12 months. Don't mix up the two when reading old tutorials, because "750 hours of t2.micro free for 12 months" advice is stale for new accounts.

**Misconception:** "Free tier means nothing can cost me money." On the Paid plan, once credits are spent, you pay normally. Also, credits only cover what they cover; always check what a service costs before leaving it running.

---

## Billing alarms: set them on day one

Cost data is **delayed** (it can lag by hours) and AWS will **not** stop your resources when you hit a limit. Budgets *alert*; they do not *cap*. The protection is you noticing early.

### 1. AWS Budgets (use this first)

Console: **Billing → Budgets → Create budget → Cost budget**, set a monthly amount, and add email alerts at, say, 50%, 80% and 100% of **actual** spend plus one on **forecasted** spend.

The same thing from the CLI:

```json
// budget.json
{
  "BudgetName": "monthly-cap",
  "BudgetLimit": { "Amount": "10", "Unit": "USD" },
  "TimePeriodUnit": "MONTHLY",
  "BudgetType": "COST"
}
```

```json
// notifications.json
[
  {
    "Notification": {
      "NotificationType": "ACTUAL",
      "ComparisonOperator": "GREATER_THAN",
      "Threshold": 80,
      "ThresholdType": "PERCENTAGE"
    },
    "Subscribers": [
      { "SubscriptionType": "EMAIL", "Address": "you@example.com" }
    ]
  }
]
```

```bash
aws budgets create-budget \
  --account-id 111122223333 \
  --budget file://budget.json \
  --notifications-with-subscribers file://notifications.json
```

### 2. Cost Explorer and cost anomaly detection

Cost Explorer shows spend by service, region and tag. When a bill looks wrong, group by **service** first, then by **usage type**. Cost Anomaly Detection can email you when spend deviates from the pattern.

### 3. CloudWatch billing alarm (older approach)

An alarm on the `EstimatedCharges` metric. It requires enabling billing alerts in Billing preferences and works only in `us-east-1`. Budgets are simpler and more flexible, so prefer them.

### Typical sources of surprise cost

| Culprit | Why it bites |
|---|---|
| NAT Gateway | Billed per hour **and** per GB processed, even when idle |
| Idle load balancers | Hourly charge whether or not they receive traffic |
| Forgotten EC2 / RDS | Billed while running; stopped instances still pay for their EBS volumes |
| Unattached EBS volumes, old snapshots | Storage keeps billing after the instance is gone |
| Public IPv4 addresses | Charged per address, including ones attached to running resources |
| Cross-AZ and internet egress traffic | Data transfer out is metered |
| CloudWatch Logs | Ingestion volume adds up, and retention defaults to **never expire** |
| Leaked credentials | Someone else mining crypto on your account is the worst-case bill |

A sensible habit while learning: **end each session by deleting what you created**, ideally by tearing down the whole stack if you built it with [infrastructure as code](../06-operations/01-infrastructure-as-code.md).

---

## Common mistakes

- Working daily as root.
- No MFA, or MFA only on IAM users but not root.
- Creating resources in the wrong region and then not finding them (or leaving them running unnoticed in a region you never look at).
- Assuming a budget will shut things off.
- Following old free-tier tutorials that no longer match new accounts.
- Setting billing alerts only after the first unexpected bill.

---

## Quick Summary

- An **account** is the billing + security boundary; protect **root** with MFA and stop using it.
- **Region** = geographic, isolated; **AZ** = separate data centre(s) within it. Spread across AZs for resilience; multi-AZ ≠ multi-region.
- Most services are regional. Always know which region you're in (console and CLI).
- New accounts (since 15 July 2025) get a credit-based **Free plan** (6 months) or **Paid plan**, plus Always Free services. Verify on the official page.
- **Budgets alert, they don't cap.** Create one on day one, and know the usual cost traps: NAT Gateway, idle load balancers, forgotten volumes, log retention, data transfer.

**Next:** [IAM](./03-iam.md)