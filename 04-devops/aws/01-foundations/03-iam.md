# IAM: Identity and Access Management

IAM decides **who can do what to which resource** in AWS. Every API call, whether from the console, the CLI, your Lambda function or an EC2 instance, is authenticated and then authorised by IAM. Almost every `AccessDenied` you will ever debug is an IAM problem, so this is the most valuable foundation topic to understand properly.

IAM itself is free and **global** (not tied to one region).

---

## The pieces

```
Principal (who)  ──► Action (what)  ──► Resource (on which)  ──► Condition (when/how)
IAM user / role      s3:GetObject       arn:aws:s3:::my-bucket/*   MFA present? From this IP?
```

| Concept | What it is |
|---|---|
| **Root user** | The account owner identity. All-powerful. Lock it away (see [Accounts](./02-accounts-regions-and-billing.md)). |
| **IAM user** | A long-lived identity with a password and/or access keys. Legacy-ish for humans today. |
| **Group** | A set of IAM users that share policies. Groups cannot be principals in a policy and cannot be assumed. |
| **Role** | An identity with **no permanent credentials** that something *assumes* and receives **temporary credentials**. The workhorse of AWS. |
| **Policy** | A JSON document that allows or denies actions. Attached to identities (or resources). |
| **ARN** | The unique name of a resource: `arn:aws:service:region:account-id:resource`. |

**Mental shift:** humans and machines should use **roles / temporary credentials**, not long-lived access keys. Keys leak (git commits, logs, laptops); temporary credentials expire.

---

## Reading a policy

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "ReadUploads",
      "Effect": "Allow",
      "Action": ["s3:GetObject", "s3:ListBucket"],
      "Resource": [
        "arn:aws:s3:::my-app-uploads",
        "arn:aws:s3:::my-app-uploads/*"
      ]
    }
  ]
}
```

- `Effect`: `Allow` or `Deny`.
- `Action`: service-prefixed API operations. Wildcards work (`s3:Get*`).
- `Resource`: ARNs the statement applies to.
- `Condition` (optional): extra requirements.
- `Version` is a fixed policy-language version string; use `2012-10-17`.

Note the two resource ARNs: `s3:ListBucket` applies to the **bucket** ARN, `s3:GetObject` to the **objects** (`/*`). Getting this wrong is a classic cause of "I allowed it but it still fails".

### A condition example

```json
{
  "Effect": "Deny",
  "Action": "*",
  "Resource": "*",
  "Condition": {
    "StringNotEquals": { "aws:RequestedRegion": ["ap-south-1", "us-east-1"] }
  }
}
```

(Denies actions outside two regions. In practice, global services need care with blanket denies like this. Test before applying widely.)

---

## How a request is decided

Simplified evaluation logic:

```
1. Is there an explicit DENY anywhere that applies?   → DENIED
2. Is there an ALLOW (and no guardrail blocks it)?    → ALLOWED
3. Otherwise                                          → DENIED (implicit deny)
```

Key rules:

- **Everything is denied by default.**
- **An explicit Deny always wins** over any Allow.
- Permissions are **the union** of what your identity policies allow, *limited* by any guardrails: **permission boundaries**, **Service Control Policies (SCPs)** from AWS Organizations, and **session policies**. Guardrails never grant permission, they only cap it.
- **Identity-based policies** attach to a user/role. **Resource-based policies** attach to the resource (S3 bucket policy, SQS queue policy, Lambda resource policy, KMS key policy, role trust policy).
- **Same account:** an Allow in *either* the identity policy or the resource policy is generally enough (KMS key policies are a notable exception, as they have their own rules).
- **Cross-account:** both sides must allow: the resource owner's policy must trust the caller, *and* the caller's identity must be allowed.

---

## Roles and assume-role

A role has two separate policies:

1. **Trust policy**: *who may assume this role* (a resource-based policy on the role).
2. **Permissions policy**: *what the role can do once assumed*.

```json
// Trust policy: let the Lambda service assume this role
{
  "Version": "2012-10-17",
  "Statement": [{
    "Effect": "Allow",
    "Principal": { "Service": "lambda.amazonaws.com" },
    "Action": "sts:AssumeRole"
  }]
}
```

```
 Caller (user/service)
        │  sts:AssumeRole (trust policy must allow caller)
        ▼
   AWS STS ──► temporary credentials (access key + secret + session token, expire)
        │
        ▼
 Calls made with those credentials are limited to the role's permissions
```

### Where roles show up

| Use | Role type |
|---|---|
| Lambda function accessing DynamoDB | **Execution role** |
| EC2 app accessing S3 | **Instance profile** (wrapper for a role on EC2) |
| ECS task accessing SQS | **Task role** (separate from the *task execution role*, which pulls images/writes logs) |
| Humans / CI in another account | Cross-account role |
| GitHub Actions deploying | Role assumed via OIDC, see [CI/CD to AWS](../06-operations/02-ci-cd-to-aws.md) |

### Assuming a role by hand

```bash
aws sts assume-role \
  --role-arn arn:aws:iam::111122223333:role/DeployRole \
  --role-session-name my-session
# returns AccessKeyId, SecretAccessKey, SessionToken, Expiration
```

In day-to-day use you rarely paste these. You configure a CLI profile with `role_arn` and the CLI assumes it for you. See [AWS CLI](./04-aws-cli.md).

---

## Humans: use IAM Identity Center

For people, the recommended approach is **IAM Identity Center** (formerly AWS SSO): one login, access to one or more accounts through **permission sets**, which hand you short-lived role credentials. Compared to IAM users with access keys: no long-lived secrets, central offboarding, MFA enforced in one place, and it works well with Organizations. The CLI side is in [AWS CLI](./04-aws-cli.md).

IAM users still exist and are fine for learning, but treat each access key as a liability.

---

## Least privilege, practically

Least privilege = grant only the actions on the resources that are actually needed. Perfect least privilege up front is unrealistic. A workable loop:

1. Start with a **narrow** policy for the known actions and specific resource ARNs.
2. Run the workload. When it hits `AccessDenied`, add the *specific* missing action.
3. Tighten later using tools: **IAM Access Analyzer** (policy generation from CloudTrail activity, finding external access), and **last accessed** data to remove unused permissions.
4. Prefer **AWS managed policies** for learning and read-only access; prefer **customer-managed policies** you write for production roles. Managed policies like `AdministratorAccess` and `PowerUserAccess` are convenient but broad.

Habits that matter:

- Scope `Resource` to ARNs, not `"*"`, wherever the service supports it.
- Use `Condition` for MFA, source IP or region constraints when appropriate.
- Separate roles per workload (one role per Lambda/service), not one shared mega-role.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `"Action": "*", "Resource": "*"` on a workload role | Name the actions and ARNs it needs |
| Access keys in code, `.env` files in git, or CI secrets | Use roles; for CI use OIDC; rotate/delete leaked keys immediately |
| Trust policy with `"Principal": {"AWS": "*"}` | Name the exact account/role; use conditions like `sts:ExternalId` for third parties |
| Forgetting `/*` vs bucket ARN for S3 | Bucket actions → bucket ARN; object actions → `bucket/*` |
| Cross-account access set up on one side only | Both the resource policy and the caller's identity policy must allow |
| Confusing Lambda **execution role** with who may *invoke* the function | Execution role = what the function can do. Invocation permission = resource-based policy on the function |
| Mixing up ECS **task role** and **task execution role** | Task role is for your app code; execution role is for ECS to pull images and ship logs |
| Using root or an admin user for the CLI | Use Identity Center / an assumed role |

---

## Debugging `AccessDenied`

1. **Read the error.** It usually names the action, the resource ARN and the principal. Confirm the principal is who you think it is:
   ```bash
   aws sts get-caller-identity
   ```
   A surprising number of "permission bugs" are the wrong profile or wrong account.
2. **Check for an explicit deny**: SCP, permission boundary, or a Deny statement. The error message may say "explicit deny" or "no identity-based policy allows".
3. **Decode the message** if it is an encoded authorization failure (some services return one):
   ```bash
   aws sts decode-authorization-message --encoded-message <blob>
   ```
   (Needs `sts:DecodeAuthorizationMessage` permission.)
4. **Use the IAM Policy Simulator** to test an identity against an action and resource.
5. **Check CloudTrail** for the denied event: it shows who called what and the error.
6. **Cross-account or resource policies?** Check the resource's own policy (bucket policy, KMS key policy, queue policy), not just the identity policy.
7. **Just created or changed it?** IAM changes are usually effective quickly, but allow a short propagation delay before concluding something is broken.

---

## Quick Summary

- IAM answers *who can do what on which resource, under what conditions*; it is global and free.
- **Default deny; explicit deny always wins.** Guardrails (SCPs, boundaries) only limit, never grant.
- **Roles + temporary credentials** beat long-lived keys, for services, CI and humans (via IAM Identity Center).
- A role has a **trust policy** (who can assume it) and **permissions policies** (what it can do).
- Cross-account = both sides must allow.
- Start narrow, loosen only as needed, and tighten with Access Analyzer. Debug with `get-caller-identity`, the error text, the simulator and CloudTrail.

**Next:** [AWS CLI](./04-aws-cli.md)