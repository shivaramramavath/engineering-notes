# SDK v3 and Credentials

Every AWS call from Node.js goes through the **AWS SDK for JavaScript v3**. Almost every "it works on my machine" AWS bug comes down to two questions: *which region is this client talking to?* and *which identity is it signing requests as?* This note covers the SDK's shape, how it picks a region, how it finds credentials, and how to wire both into typed config so the answers are never a mystery.

> Examples are TypeScript with ESM imports. In plain JS, drop the type annotations; nothing else changes.

---

## v3 in one minute

v3 is **modular**: one npm package per service, plus shared packages.

```bash
npm install @aws-sdk/client-s3 @aws-sdk/client-sts @aws-sdk/credential-providers
```

You create a **client**, build a **command**, and `send` it. Everything is promise-based (no `.promise()` like v2).

```ts
import { S3Client, ListBucketsCommand } from "@aws-sdk/client-s3";

const s3 = new S3Client({ region: "ap-south-1" });

const res = await s3.send(new ListBucketsCommand({}));
console.log(res.Buckets?.map((b) => b.Name));
```

Why this shape matters:

- Importing only the commands you use keeps bundles small, which matters for Lambda cold starts ([Lambda handlers](../03-lambda/01-lambda-handlers.md)).
- The SDK also ships "aggregated" clients (`new S3()` with `s3.listBuckets()`). They work, but they pull in every command for that service. Prefer client + command.
- The package is `@aws-sdk/client-*`. The old monolithic `aws-sdk` package is v2. Don't mix them; most tutorials online still show v2.

Use a Node version the SDK currently supports (check the SDK's README; support windows move). Types are built in.

---

## Region

A client needs a region. If it can't find one you get `Region is missing`.

The SDK resolves it in roughly this order:

1. `region` in the client config
2. `AWS_REGION` environment variable
3. `region` in the active profile in `~/.aws/config`

```ts
// explicit, preferred in application code
const s3 = new S3Client({ region: process.env.AWS_REGION ?? "ap-south-1" });
```

Notes:

- Lambda sets `AWS_REGION` for you, and ECS tasks usually get it too, so `new S3Client({})` often works in prod and fails locally. That asymmetry is the classic cause of "Region is missing".
- `AWS_DEFAULT_REGION` is a CLI convention. Don't rely on the SDK reading it.
- Many resources are regional: a bucket, queue or table created in one region is invisible from another. `NoSuchBucket` or "table not found" with a correct name usually means wrong region.
- A few services are global-ish (IAM, CloudFront, Route 53) and use fixed regions. The service note will say so.

---

## Credentials: the default provider chain

If you don't pass `credentials`, the SDK walks a **default provider chain** and uses the first source that yields credentials. In Node.js, approximately:

```text
1. Environment variables        AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY / AWS_SESSION_TOKEN
2. SSO                          profile with sso_* settings (after `aws sso login`)
3. Shared config/credentials    ~/.aws/credentials and ~/.aws/config (profile, assume-role)
4. Credential process           credential_process in the profile
5. Web identity token           AWS_WEB_IDENTITY_TOKEN_FILE (EKS service accounts, GitHub OIDC)
6. Container / instance role    ECS task role, EC2 instance profile
```

The exact order between the middle steps is an implementation detail. What you need to remember is the practical consequence:

> **Environment variables win over profiles.** A stale `AWS_ACCESS_KEY_ID` in your shell silently overrides the SSO profile you thought you were using.

### Where each environment gets credentials

| Environment | What you should rely on | How it arrives |
|---|---|---|
| Your laptop | SSO profile | `aws sso login`, then `AWS_PROFILE=dev` |
| Lambda | Execution role | Injected as env vars by the runtime |
| ECS / Fargate | Task role | Container credentials endpoint |
| EC2 | Instance profile | Instance metadata service |
| CI (GitHub Actions) | OIDC → assumed role | Web identity token |

**Never hard-code access keys, and never commit them.** In application code you normally write no credentials at all: the chain finds them, and the same code runs locally and in prod under different identities.

---

## Local development with SSO

Long-lived IAM user keys are what you're trying to avoid. IAM Identity Center (SSO) gives you short-lived credentials.

```bash
aws configure sso          # one-time: creates the profile
aws sso login --profile dev
export AWS_PROFILE=dev     # or set it per command
```

The result in `~/.aws/config`:

```ini
[profile dev]
sso_session = my-sso
sso_account_id = 111122223333
sso_role_name = DeveloperAccess
region = ap-south-1

[sso-session my-sso]
sso_start_url = https://my-org.awsapps.com/start
sso_region = us-east-1
sso_registration_scopes = sso:account:access
```

With `AWS_PROFILE=dev` set, `new S3Client({})` just works. SSO sessions expire (hours), so when requests start failing with an expired-token message, run `aws sso login` again.

A profile can also assume a role, with no code changes:

```ini
[profile reporting]
role_arn = arn:aws:iam::111122223333:role/reporting-reader
source_profile = dev
region = ap-south-1
```

---

## Choosing credentials explicitly

Sometimes you *want* control: scripts that touch several accounts, tools with a `--profile` flag.

```ts
import { S3Client } from "@aws-sdk/client-s3";
import {
  fromIni,
  fromNodeProviderChain,
  fromTemporaryCredentials,
} from "@aws-sdk/credential-providers";

// a specific profile, ignoring AWS_PROFILE
const dev = new S3Client({
  region: "ap-south-1",
  credentials: fromIni({ profile: "dev" }),
});

// the default chain, explicitly (same as omitting `credentials`)
const auto = new S3Client({
  region: "ap-south-1",
  credentials: fromNodeProviderChain(),
});

// assume a role, refreshed automatically before it expires
const reporting = new S3Client({
  region: "ap-south-1",
  credentials: fromTemporaryCredentials({
    params: {
      RoleArn: "arn:aws:iam::111122223333:role/reporting-reader",
      RoleSessionName: "reports-service",
    },
  }),
});
```

`@aws-sdk/credential-providers` also has `fromSSO`, `fromEnv` and others. Providers cache and refresh temporary credentials for you.

---

## Always know who you are

The fastest debugging tool in AWS is asking "which identity am I?"

```ts
import { STSClient, GetCallerIdentityCommand } from "@aws-sdk/client-sts";

const sts = new STSClient({ region: "ap-south-1" });
const me = await sts.send(new GetCallerIdentityCommand({}));
console.log(me.Account, me.Arn);
```

```bash
aws sts get-caller-identity   # same thing from the CLI
```

`GetCallerIdentity` needs no permissions, so it always works if credentials are valid. Check the account ID and the role ARN before debugging anything else.

---

## Typed config

The region and endpoint are the only AWS settings your code should read from the environment. Credentials are *not* config: leave them to the chain. Validate what you do read once, at startup, so a missing value fails immediately with a clear message instead of deep inside a request.

```ts
// config.ts
import { z } from "zod";

const Env = z.object({
  AWS_REGION: z.string().min(1),
  AWS_ENDPOINT_URL: z.string().url().optional(), // set only for LocalStack
  UPLOADS_BUCKET: z.string().min(1),
});

export const config = Env.parse(process.env); // throws with the failing keys
export type Config = z.infer<typeof Env>;
```

Build every client from that one object:

```ts
// clients.ts
import { S3Client } from "@aws-sdk/client-s3";
import { SQSClient } from "@aws-sdk/client-sqs";
import { config } from "./config.js";

const base = {
  region: config.AWS_REGION,
  ...(config.AWS_ENDPOINT_URL && { endpoint: config.AWS_ENDPOINT_URL }),
};

export const s3 = new S3Client({
  ...base,
  forcePathStyle: Boolean(config.AWS_ENDPOINT_URL), // emulators need path-style S3
});
export const sqs = new SQSClient(base);
```

Why this is worth the few lines:

- Local, test and prod differ only by environment values. No `if (isLocal)` branches scattered around.
- A forgotten `AWS_REGION` fails at boot, not on the first S3 call.
- Tests can import `clients.ts` or build clients from a test config. See [local development and testing](./02-local-development-and-testing.md).

zod is just one option; any schema validator does the same job. In Lambda, the same `config.ts` reads the function's environment variables.

---

## Client lifecycle

- **Create clients once** at module scope and reuse them. Each client holds an HTTP connection pool and a credential cache; building one per request throws that away.
- In Lambda, create the client *outside* the handler so warm invocations reuse it.
- Retries are on by default (`maxAttempts` is 3 with backoff). Tune with `new S3Client({ maxAttempts: 5 })`. Timeouts, throttling and backoff are covered in [errors and retries](../04-production/01-errors-and-retries.md).

---

## Handling errors

SDK errors carry the AWS error name and request metadata.

```ts
try {
  await s3.send(new GetObjectCommand({ Bucket, Key }));
} catch (err: any) {
  console.error(err.name);                       // "NoSuchKey", "AccessDenied", ...
  console.error(err.$metadata?.httpStatusCode);  // 404, 403, ...
  console.error(err.$metadata?.requestId);       // include this in logs and support tickets
  throw err;
}
```

Branch on `err.name`, not on the message text. Names differ per service (`AccessDenied` for S3, `AccessDeniedException` for many others).

---

## Common mistakes and debugging

| Symptom | Likely cause | Check |
|---|---|---|
| `Region is missing` | No region in config, env or profile | Set `region` explicitly (typed config) |
| `Could not load credentials from any providers` (`CredentialsProviderError`) | Chain found nothing | Run `aws sso login`, check `AWS_PROFILE`, run `aws sts get-caller-identity` |
| Expired token / SSO session expired | Temporary credentials timed out | `aws sso login --profile dev` |
| Works on laptop, `AccessDenied` in Lambda | Different identity (the execution role) | Compare `GetCallerIdentity` output in both places |
| Wrong account's resources appear | Stale `AWS_*` env vars override your profile | `env \| grep AWS_`, unset them |
| `NoSuchBucket` / table not found, name is right | Wrong region | Print `await s3.config.region()` |
| Import errors, `.promise is not a function` | v2 code mixed with v3 | Use `@aws-sdk/client-*` and `await client.send(...)` |
| Slow Lambda cold start | Importing aggregated clients or the whole SDK | Import specific commands, bundle |

To see what the SDK is doing, add a logger:

```ts
const s3 = new S3Client({ region: "ap-south-1", logger: console });
```

---

## Quick summary

- v3 = client + command + `await client.send(...)`, one package per service.
- Set the region explicitly; it's the most common local-vs-prod difference.
- Don't pass keys. Let the default provider chain find credentials: SSO locally, roles in Lambda/ECS.
- Env vars beat profiles in the chain.
- When something is denied or missing, run `GetCallerIdentity` first.
- Validate region/endpoint/resource names once in a typed config; build all clients from it.
- Create clients once and reuse them.

## Next

[Local development and testing](./02-local-development-and-testing.md): running these same clients against LocalStack and mocks.