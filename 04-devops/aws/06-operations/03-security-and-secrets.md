# Security and Secrets

Security on AWS comes down to a handful of habits applied consistently: **know who can do what** (IAM), **keep secrets out of code**, **encrypt**, **log everything that matters**, and **shrink what's exposed**. This note covers secrets handling in depth (Secrets Manager, Parameter Store, KMS) and then a practical hardening checklist.

Remember the [shared responsibility model](../01-foundations/01-what-is-aws.md): AWS secures the platform, but misconfiguration (open buckets, leaked keys, over-broad roles) is yours, and it's behind the large majority of real incidents.

Prerequisites: [IAM](../01-foundations/03-iam.md); useful: [Lambda](../02-compute/02-lambda.md), [ECS](../02-compute/03-ecs-fargate.md).

---

## What counts as a secret, and where it must never be

**Secrets:** database passwords, API keys, OAuth client secrets, signing keys, tokens, private certificates.
**Not secrets:** region names, table names, feature flags, public client IDs. Use plain config for those.

A secret must never appear in:

- source control (including `.env` files, Dockerfiles, CDK/CloudFormation templates, `cdk.context.json`)
- container images or build logs
- plain Lambda/ECS **environment variables** that are visible in the console, in `describe-*` output, or in templates
- client-side code or mobile apps (assume anything shipped to a client is public)
- chat, tickets, or screenshots

The goal: the secret exists in **one managed place**, is fetched at runtime by an identity that's *allowed* to read it, and can be **rotated** without redeploying code.

---

## Secrets Manager vs Parameter Store

Both store encrypted values, retrieved via IAM-controlled API calls. They overlap, but each has a sweet spot.

| | **Secrets Manager** | **SSM Parameter Store** |
|---|---|---|
| Purpose | Secrets that need lifecycle management | Configuration, plus simple secrets |
| **Automatic rotation** | **Yes** (built-in for RDS/Aurora, Redshift, DocumentDB; Lambda-based for custom secrets) | No built-in rotation (you'd build it) |
| Cost | Per secret per month **+ per API call** | **Standard parameters are free**; advanced tier is paid |
| Size | Up to 64 KB | 4 KB (standard) / 8 KB (advanced) |
| Versioning | Yes, with staging labels (`AWSCURRENT`, `AWSPREVIOUS`) | Yes (versions and labels) |
| Cross-account / replication | Resource policies; **multi-region replication** | Cross-account sharing for advanced parameters; no built-in replication |
| Encryption | Always, via KMS | `SecureString` via KMS; `String`/`StringList` are plaintext |
| Best for | DB credentials, third-party API keys that rotate | App config (`/myapp/prod/feature-x`), low-cost non-rotating secrets |

Rule of thumb: **Secrets Manager for credentials you want rotated; Parameter Store for configuration and for cheap, rarely-changed secrets** (as `SecureString`). Using **Parameter Store hierarchies** (`/myapp/prod/db-host`) lets one IAM policy grant "everything under `/myapp/prod/`" and `GetParametersByPath` fetch a whole group.

### Reading a secret

```ts
import { SecretsManagerClient, GetSecretValueCommand } from "@aws-sdk/client-secrets-manager";

const sm = new SecretsManagerClient({});
let cached: { host: string; username: string; password: string } | undefined;

async function getDbCreds() {
  if (cached) return cached;                      // cache per execution environment
  const res = await sm.send(new GetSecretValueCommand({ SecretId: process.env.DB_SECRET_ARN! }));
  cached = JSON.parse(res.SecretString!);
  return cached;
}
```

```bash
aws secretsmanager get-secret-value --secret-id prod/db --query SecretString --output text
aws ssm get-parameter --name /myapp/prod/api-key --with-decryption --query Parameter.Value --output text
```

**Pass the secret's ARN/name to your app, never its value.** Then fetch at runtime.

**Cache the value.** Each API call costs money and adds latency. In Lambda, create the client and cache **outside the handler** (see [Lambda](../02-compute/02-lambda.md)), with a short TTL so rotation is picked up. The **AWS Parameters and Secrets Lambda Extension** does caching for you through a local HTTP endpoint, and **Powertools for AWS Lambda** has a parameters utility with caching.

### Injecting into containers

[ECS](../02-compute/03-ecs-fargate.md) can inject secrets as environment variables **at task start** from Secrets Manager or Parameter Store (`secrets` in the container definition). The **task execution role** needs `secretsmanager:GetSecretValue` (and KMS decrypt if using a customer key). The value appears as an env var inside the container, but not in the task definition. Rotation requires restarting tasks to take effect, so fetch at runtime from code if you need live rotation.

### Rotation

- **RDS/Aurora:** simplest path is `--manage-master-user-password` at creation, so RDS keeps the admin credential in Secrets Manager and rotates it ([RDS](../03-storage-and-databases/03-rds-and-aurora.md)). For app users, enable managed or Lambda-based rotation on the secret.
- Rotation works by creating a new credential, testing it, and promoting it (`AWSPENDING` → `AWSCURRENT`). Your app must **re-fetch** rather than hold the old password forever. Handle auth failures by refreshing the secret once and retrying.
- Use **alternating-users** rotation if brief downtime during a single-user password change is unacceptable.

### Secrets and infrastructure as code

CloudFormation/CDK can **reference** secrets without embedding them via **dynamic references**:

```ts
// CDK: generate a secret, never see its value in code or the template
const dbSecret = new secretsmanager.Secret(this, "DbSecret", {
  generateSecretString: {
    secretStringTemplate: JSON.stringify({ username: "app" }),
    generateStringKey: "password",
    excludePunctuation: true,
  },
});
// Grant a function read access; pass the ARN, not the value
dbSecret.grantRead(fn);
fn.addEnvironment("DB_SECRET_ARN", dbSecret.secretArn);
```

Avoid `SecretValue.unsafePlainText(...)` and plaintext values in `cdk.context.json` or stack parameters. Template parameters and outputs are **visible** in the console.

---

## KMS: the encryption layer under everything

**AWS Key Management Service (KMS)** holds the keys that encrypt your S3 objects, EBS volumes, RDS storage, secrets, SQS queues and more.

| Key type | Notes |
|---|---|
| **AWS owned keys** | Used invisibly by some services; nothing to manage; no audit trail for you |
| **AWS managed keys** (`aws/s3`, `aws/secretsmanager`…) | Created per service in your account; free to hold; **can't be customised** (policy is fixed) |
| **Customer managed keys (CMKs)** | You control the **key policy**, rotation, grants, cross-account use; small monthly fee + per-request cost |

Use **customer managed keys** when you need to: restrict decryption to specific roles, share encrypted data across accounts, audit key usage, or revoke access by disabling a key. Otherwise the default service encryption is usually sufficient and far simpler.

Key points that cause real-world pain:

- Access to encrypted data needs **both** the service permission **and** `kms:Decrypt` (and friends) on the key, and the **key policy** must allow the principal (directly, or by delegating to IAM). "I can read the S3 object in IAM but get AccessDenied" is very often a **KMS** problem.
- **Cross-account** encrypted data requires the key policy *and* the caller's IAM policy to allow it.
- Deleting a key is scheduled (7–30 days wait). Once gone, **everything encrypted under it is unrecoverable**. Disable first; delete only deliberately.
- Enable **automatic key rotation** on customer managed symmetric keys.
- Envelope encryption (the pattern services use, and the one you should use for app-level encryption): KMS encrypts a small **data key**, which encrypts the bulk data. Don't send large payloads to KMS directly.

---

## Hardening checklist

Roughly in order of payoff:

### Identity and access
- **Root user:** MFA on, no access keys, not used daily ([Accounts](../01-foundations/02-accounts-regions-and-billing.md)).
- Humans via **IAM Identity Center** with MFA; no long-lived access keys. CI via **OIDC** ([CI/CD](./02-ci-cd-to-aws.md)). Services via **roles**.
- **Least privilege**, one role per workload; no `Action: "*"`/`Resource: "*"` on application roles. Review with **IAM Access Analyzer** (unused access, external access, policy generation).
- Use **Organizations + SCPs** as guardrails (deny disabling CloudTrail/GuardDuty, restrict regions, block leaving the org).
- Separate **accounts** per environment and for logs/security tooling.

### Data
- **S3 Block Public Access** at the **account** level; bucket policies enforce TLS ([S3](../03-storage-and-databases/01-s3.md)).
- **Encrypt at rest**: default on for S3 (SSE-S3); enable **default EBS encryption** for the account/region; encrypt RDS **at creation**; DynamoDB is always encrypted.
- **TLS in transit** everywhere (ALB/CloudFront certificates from ACM; `sslmode=require` to databases).
- **Backups** protected from deletion (S3 Object Lock / vault lock in AWS Backup) as ransomware defence.

### Network
- Databases and internal services in **private subnets**; security groups reference **other security groups**, not `0.0.0.0/0`. No SSH/RDP open to the world; use **SSM Session Manager** ([EC2](../02-compute/01-ec2.md), [VPC](../04-networking/01-vpc.md)).
- **VPC endpoints** to keep AWS API traffic off the public internet where it matters.
- **WAF** (on CloudFront/ALB/API Gateway) for web-facing apps; rate limiting; AWS Shield Standard is automatic.
- **IMDSv2 required** on EC2 (`HttpTokens=required`) to blunt SSRF credential theft.

### Detection and audit
- **CloudTrail:** an **organization trail** to a protected S3 bucket in a separate log account (the default 90-day Event history is not an audit log). Turn on **log file validation**; add data events for sensitive S3/Lambda where justified.
- **GuardDuty** (threat detection from CloudTrail/VPC flow/DNS logs), **Security Hub** (aggregated findings and standards checks), **AWS Config** (resource configuration history and compliance rules), **Inspector** (vulnerability scanning for EC2/ECR/Lambda), **Macie** (sensitive data in S3).
- Alarms for high-risk events: root login, IAM policy changes, console login without MFA, security group opened to the world, CloudTrail stopped.

### Application and supply chain
- **Dependency and image scanning** in CI (ECR scanning, SCA tools); keep base images and Lambda runtimes current.
- **Secret scanning** in the repo and CI (e.g. gitleaks, git-secrets, GitHub secret scanning with push protection).
- **Validate all input**; treat model output, queue messages and webhook payloads as untrusted ([Bedrock](../05-app-services/05-bedrock.md), [SQS](../05-app-services/01-sqs.md)).
- Pin and review third-party GitHub Actions and CDK constructs.

---

## If a key leaks (runbook)

1. **Deactivate** the key immediately (IAM → user → Security credentials → *Make inactive*), then delete after investigation. For role credentials, **revoke sessions** (attach a deny policy conditioned on `aws:TokenIssueTime`) and fix the source.
2. **Rotate** anything else the exposed identity could read (secrets, DB passwords).
3. **Investigate with CloudTrail**: what did that identity do, from which IPs, in which regions (check *every* region, including ones you don't use)?
4. **Look for persistence**: new IAM users/keys/roles, changed trust policies, new access keys, Lambda functions, EC2 instances (crypto mining), modified security groups.
5. Check **billing** for unexpected usage. AWS Support can also help, especially if you were notified of exposure.
6. **Fix the cause** (secrets in repo, over-broad role), add secret scanning, and write down what you learned.

Pushing a key to a public repo gets harvested by bots within minutes, so treat any such exposure as a confirmed compromise even if "it was only up for a minute".

---

## Common mistakes

- **Secrets in git, images, templates or plain env vars.**
- **Long-lived IAM user keys** for CI or humans.
- **Wildcard IAM policies** on application roles.
- **Public S3 buckets / open security groups** (SSH, databases) "temporarily".
- **No rotation story**, so a leaked DB password lives forever.
- **Fetching a secret on every request** (cost, latency, throttling) instead of caching with a TTL.
- **Forgetting KMS permissions** when enabling customer-managed key encryption.
- **Scheduling a KMS key for deletion** without checking what depends on it.
- Treating **CloudTrail's default history** as a sufficient audit trail.
- **Not enabling** GuardDuty/Config/Security Hub, so nobody notices misconfigurations.
- Logging **secrets or PII** in application logs ([Observability](./04-observability.md)).
- Putting secrets in **client-side code**.
- Assuming **encryption = access control**. An authorised-but-wrong principal still decrypts happily.

---

## Debugging

| Symptom | Check |
|---|---|
| `AccessDeniedException` on `GetSecretValue` | Role lacks `secretsmanager:GetSecretValue` on that ARN; **resource policy** on the secret; **KMS** decrypt permission if a CMK encrypts it; correct region (secrets are regional) |
| `ResourceNotFoundException` | Wrong name/ARN or region; secret scheduled for deletion |
| Works, but app breaks after rotation | App cached the old value forever; add refresh-on-auth-failure and cache TTL; confirm rotation Lambda succeeded (check its logs) |
| Parameter returns encrypted blob | Missing `--with-decryption` / `WithDecryption: true` |
| S3/SQS/EBS `AccessDenied` with correct IAM | KMS key policy/permissions (`kms:Decrypt`, `kms:GenerateDataKey`) for that principal or the **service** |
| ECS task fails at start with `ResourceInitializationError` | **Task execution role** can't read the secret/param or decrypt it; network path to Secrets Manager/SSM endpoints missing ([VPC](../04-networking/01-vpc.md)) |
| Throttling on `GetSecretValue` | Caching missing; too many concurrent cold starts; use the extension / longer TTL |
| Unsure who changed something | **CloudTrail** event history / Athena over the trail; **Config** timeline for the resource |

```bash
aws iam generate-credential-report && aws iam get-credential-report --query Content --output text | base64 -d
aws accessanalyzer list-findings-v2 --analyzer-arn <arn>       # external/unused access findings
```

---

## Quick Summary

- **Secrets live in one managed place** and are fetched at runtime by an authorised identity, never in code, templates, images or logs.
- **Secrets Manager** = secrets needing **rotation** (RDS has built-in support); **Parameter Store** = config and cheap/simple secrets (`SecureString`). **Cache** reads; pass **ARNs**, not values.
- **KMS** underlies encryption; use **customer managed keys** when you need control, auditability or cross-account use. Many "mystery AccessDenied" errors are **KMS**. Never delete a key casually.
- Hardening essentials: **MFA + no root use, roles/OIDC not keys, least privilege, private subnets, Block Public Access, IMDSv2, encryption, org-wide CloudTrail, GuardDuty/Security Hub/Config**.
- Have a **leaked-key runbook**, and assume exposed keys are compromised immediately.
- Add secret scanning and dependency scanning to CI.

**Next:** [Observability](./04-observability.md)
