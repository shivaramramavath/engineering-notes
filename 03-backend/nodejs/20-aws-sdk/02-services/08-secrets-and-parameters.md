# Secrets and Parameters

Your app needs values it must not hard-code: database passwords, third-party API keys, feature flags, endpoints that differ per environment. AWS gives you two managed stores for this, both read with a couple of SDK calls:

- **Secrets Manager**: built for secrets. Encrypted, versioned, can **rotate** automatically.
- **Systems Manager Parameter Store**: hierarchical config values, with an encrypted `SecureString` type for secrets. Cheaper, simpler, no built-in rotation.

This note is about *reading* them from Node, and doing it without slowing down or breaking your app. How the values stay out of logs and repos more broadly belongs to the production notes.

Prerequisites: [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md), plus basic IAM policy reading for the permission section.

```bash
npm install @aws-sdk/client-secrets-manager @aws-sdk/client-ssm
```

---

## Which one?

| | Secrets Manager | Parameter Store |
|---|---|---|
| Best for | Credentials, API keys, anything that should rotate | App config, flags, plus simple secrets |
| Rotation | Built in (Lambda-based; managed for RDS and some others) | None (do it yourself) |
| Cost | Per secret per month plus per API call | Standard parameters have no per-parameter charge |
| Size | Up to 64 KB | 4 KB standard, 8 KB advanced |
| Structure | Flat name, usually a JSON blob | Hierarchy: `/app/prod/db/host`, fetch by path |
| Versioning | Staging labels (`AWSCURRENT`, `AWSPREVIOUS`) | Numbered versions |
| Cross-account sharing, replication | Yes | Limited |

Rule of thumb: **Secrets Manager for secrets that rotate or that you share across accounts; Parameter Store for configuration and cheap static secrets.** Many apps use both. Check current pricing; it changes.

---

## Secrets Manager

```ts
import { SecretsManagerClient, GetSecretValueCommand } from "@aws-sdk/client-secrets-manager";

const sm = new SecretsManagerClient({ region: process.env.AWS_REGION });

const res = await sm.send(new GetSecretValueCommand({ SecretId: "prod/orders/db" }));
const secret = JSON.parse(res.SecretString!) as {
  username: string; password: string; host: string; port: number; dbname: string;
};
```

- `SecretId` is the name or the ARN.
- Secrets are usually stored as a **JSON string** (RDS-managed secrets have `username`, `password`, `host`, `port`, `dbname`), so parse `SecretString`. Binary secrets come back in `SecretBinary` instead.
- By default you get the version labeled `AWSCURRENT`. During rotation, `AWSPREVIOUS` holds the old one; request it with `VersionStage` if needed.
- Errors: `ResourceNotFoundException` (wrong name or region), `AccessDeniedException` (IAM, or the KMS key), `DecryptionFailure`.

---

## Parameter Store

```ts
import { SSMClient, GetParameterCommand, paginateGetParametersByPath } from "@aws-sdk/client-ssm";

const ssm = new SSMClient({ region: process.env.AWS_REGION });

// one value
const { Parameter } = await ssm.send(new GetParameterCommand({
  Name: "/orders/prod/stripe-key",
  WithDecryption: true,           // REQUIRED to get SecureString plaintext
}));
const key = Parameter?.Value;

// everything under a path, paginated
const config: Record<string, string> = {};
for await (const page of paginateGetParametersByPath({ client: ssm }, {
  Path: "/orders/prod/",
  Recursive: true,
  WithDecryption: true,
})) {
  for (const p of page.Parameters ?? []) config[p.Name!.replace("/orders/prod/", "")] = p.Value!;
}
```

Parameter types: `String`, `StringList` (comma-separated) and `SecureString` (encrypted with KMS).

Details that bite:

- **Without `WithDecryption: true`, a `SecureString` returns the encrypted ciphertext**, not an error. Your app then "works" with a garbage password.
- Names are paths: `/app/env/name`. A hierarchy per app and environment (`/orders/prod/...`) lets you fetch a whole config set and scope IAM by path.
- `GetParametersByPath` returns limited results per page; use the paginator or you'll silently get only the first batch.
- `GetParameters` (plural) fetches up to 10 specific names per call and returns the missing ones in `InvalidParameters` rather than throwing.
- Errors: `ParameterNotFound`, `AccessDeniedException`.

---

## Reading them without hurting your app

Calling AWS on every request is slow, costs money (Secrets Manager bills per API call) and can get throttled. Fetch once, cache, refresh occasionally.

```ts
// secrets.ts
import { SecretsManagerClient, GetSecretValueCommand } from "@aws-sdk/client-secrets-manager";

const sm = new SecretsManagerClient({ region: process.env.AWS_REGION });
const cache = new Map<string, { expires: number; value: Promise<string> }>();

export function getSecret(id: string, ttlMs = 5 * 60_000): Promise<string> {
  const hit = cache.get(id);
  if (hit && hit.expires > Date.now()) return hit.value;

  // cache the PROMISE so concurrent callers share one request
  const value = sm
    .send(new GetSecretValueCommand({ SecretId: id }))
    .then((r) => r.SecretString!)
    .catch((err) => { cache.delete(id); throw err; }); // never cache failures

  cache.set(id, { expires: Date.now() + ttlMs, value });
  return value;
}
```

Why these choices:

- **Module-scope cache with a TTL.** In Lambda it survives across warm invocations; in a long-running service the TTL lets you pick up **rotated** secrets without a restart.
- **Cache the promise**, so a burst of requests at startup makes one call, not hundreds.
- **Don't cache errors.**
- If a rotated password can invalidate a live connection, refetch on an authentication failure and retry once, rather than waiting for the TTL.

### Where to load at startup

- **Containers/servers**: load config and secrets at boot (top-level `await` in ESM, or in your bootstrap function), fail fast if anything is missing, then pass values into your clients. Typed-config validation from [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md#typed-config) fits here.
- **Lambda**: fetch in init code *outside* the handler (or lazily with the cache above) so cold starts pay once. Each fetch adds latency to the cold start, so fetch only what you need.

### Don't write the secret to env vars yourself

It's tempting to resolve secrets at deploy time and inject them into environment variables. That puts plaintext in function configuration, in process dumps and often in logs. Prefer passing the secret's **name** (or ARN) as the environment variable and reading the value at runtime.

```ts
const dbSecretId = process.env.DB_SECRET_ID!; // "prod/orders/db": not sensitive
const db = JSON.parse(await getSecret(dbSecretId));
```

### Tools that do the caching for you

- **Powertools for AWS Lambda (TypeScript)**: the `@aws-lambda-powertools/parameters` package has `getSecret` and `getParameter` helpers with built-in caching (`maxAge`) and transforms such as JSON parsing. See [logging and tracing](../04-production/02-logging-and-tracing.md), where Powertools comes up again.
- **AWS Parameters and Secrets Lambda Extension**: a Lambda layer that caches values and serves them over a local HTTP endpoint, authenticated with the function's session token. Useful if you'd rather not bundle SDK clients; see the AWS docs for the layer ARN and usage for your region.

---

## Permissions

Secrets Manager: scope to the secret. Secret ARNs end in a random 6-character suffix, so use a trailing wildcard to match by name.

```json
{
  "Effect": "Allow",
  "Action": "secretsmanager:GetSecretValue",
  "Resource": "arn:aws:secretsmanager:ap-south-1:111122223333:secret:prod/orders/db-*"
}
```

Parameter Store: scope by path. Note the ARN format has `parameter` followed by the name (which begins with `/`):

```json
{
  "Effect": "Allow",
  "Action": ["ssm:GetParameter", "ssm:GetParameters", "ssm:GetParametersByPath"],
  "Resource": "arn:aws:ssm:ap-south-1:111122223333:parameter/orders/prod/*"
}
```

If the secret or parameter is encrypted with a **customer-managed KMS key** (rather than the AWS-managed default), the role also needs `kms:Decrypt` on that key. A confusing `AccessDeniedException` on a correct Secrets/SSM policy is often exactly this.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| `SecureString` returned as gibberish | `WithDecryption: true` |
| Calling Secrets Manager on every request | Cache with a TTL, promise-cached |
| Only the first page of `GetParametersByPath` | Use the paginator |
| Secret value in plaintext env var, logs or Git | Pass the *name*, read at runtime; never log values |
| Cached forever, then rotation breaks the app | TTL, and refetch on auth failure |
| `ResourceNotFoundException` / `ParameterNotFound` with the right name | Wrong region or missing leading `/` on a parameter name |
| `AccessDenied` despite the right IAM statement | Customer-managed KMS key needs `kms:Decrypt`; or secret ARN missing the `-*` suffix match |
| Storing a >4 KB value as a standard parameter | Advanced parameter, or Secrets Manager (64 KB) |
| One shared secret for all environments | Separate secrets/paths per environment, with scoped roles |
| Swallowing a failed fetch and starting anyway | Fail fast at startup |

---

## Quick summary

- **Secrets Manager** for rotating or shared secrets (usually JSON); **Parameter Store** for hierarchical config and cheap `SecureString` values.
- Read with `GetSecretValueCommand` (parse `SecretString`) and `GetParameterCommand` / `GetParametersByPath` (`WithDecryption: true`, paginate).
- Cache in module scope with a TTL and share in-flight promises; refetch on auth failure to survive rotation.
- Put the *name* in env vars, not the value; never log secrets.
- IAM: scope to the secret ARN (with `-*`) or parameter path; add `kms:Decrypt` for customer-managed keys.

## Next

[Lambda handlers](../03-lambda/01-lambda-handlers.md): running this code in functions, where init-time fetching and caching matter most.
