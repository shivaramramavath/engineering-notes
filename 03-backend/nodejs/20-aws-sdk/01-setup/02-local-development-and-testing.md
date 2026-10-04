# Local Development and Testing

You don't want every test hitting a real AWS account: it's slow, costs money, needs credentials in CI, and leaves debris behind. But you also can't trust code that has only ever talked to a mock. The usual answer is layers: **mock the SDK for unit tests, run against LocalStack for integration tests, and keep a thin real-account smoke test for what emulators can't prove.**

Prerequisite: [SDK v3 and credentials](./01-sdk-v3-and-credentials.md), especially the typed config and "create clients once" parts. Testing is easy when clients are built from config, and painful when they're hard-coded.

---

## Which layer for what

```text
            fast, narrow                                slow, realistic
 ┌──────────────────┐   ┌────────────────────┐   ┌─────────────────────┐
 │ Unit: mock SDK   │ → │ Integration:       │ → │ Smoke: real dev     │
 │ (ms)             │   │ LocalStack (secs)  │   │ account (mins)      │
 └──────────────────┘   └────────────────────┘   └─────────────────────┘
 your logic, branches    request shapes, wiring,   IAM, quotas, exact
 and error handling      real serialization        service behavior
```

| Question you're asking | Use |
|---|---|
| Does my code handle `NoSuchKey` correctly? | Mock |
| Does my handler parse an SQS event and call DynamoDB with the right input? | Mock |
| Does my S3 → process → DynamoDB flow actually work end to end? | LocalStack |
| Did I get the multipart upload sequence right? | LocalStack |
| Does my IAM policy allow this? | Real account (emulators usually don't enforce IAM) |
| Do I hit throttling or a quota under load? | Real account |

---

## Make code testable first

Two habits do most of the work:

1. **Clients come from config** (or are passed in), never constructed inside business logic.
2. **Business logic is separate from the Lambda/Express wrapper.** The handler parses the event and calls a plain function.

```ts
// uploads.ts: logic takes its dependencies as arguments
import { S3Client, GetObjectCommand } from "@aws-sdk/client-s3";

export async function readJson<T>(s3: S3Client, bucket: string, key: string): Promise<T> {
  const res = await s3.send(new GetObjectCommand({ Bucket: bucket, Key: key }));
  return JSON.parse(await res.Body!.transformToString()) as T;
}
```

Passing the client in means a test can hand over a mocked or LocalStack-pointed client with no module patching.

---

## Unit tests: mocking the SDK

`aws-sdk-client-mock` stubs `send` on a client class, so you can script responses per command.

```bash
npm install -D aws-sdk-client-mock @smithy/util-stream vitest
```

```ts
// uploads.test.ts
import { describe, it, expect, beforeEach } from "vitest";
import { mockClient } from "aws-sdk-client-mock";
import { S3Client, GetObjectCommand, PutObjectCommand } from "@aws-sdk/client-s3";
import { sdkStreamMixin } from "@smithy/util-stream";
import { Readable } from "node:stream";
import { readJson } from "./uploads.js";

const s3Mock = mockClient(S3Client);
beforeEach(() => s3Mock.reset());

describe("readJson", () => {
  it("parses the object body", async () => {
    // GetObject's Body is a special stream type; sdkStreamMixin adds transformToString()
    const body = sdkStreamMixin(Readable.from([JSON.stringify({ id: 1 })]));
    s3Mock.on(GetObjectCommand, { Bucket: "b", Key: "k" }).resolves({ Body: body as any });

    const out = await readJson<{ id: number }>(new S3Client({ region: "ap-south-1" }), "b", "k");

    expect(out).toEqual({ id: 1 });
  });

  it("surfaces NoSuchKey", async () => {
    s3Mock
      .on(GetObjectCommand)
      .rejects(Object.assign(new Error("missing"), { name: "NoSuchKey" }));

    await expect(
      readJson(new S3Client({ region: "ap-south-1" }), "b", "nope")
    ).rejects.toMatchObject({ name: "NoSuchKey" });
  });
});
```

Asserting on what was *sent*:

```ts
const calls = s3Mock.commandCalls(PutObjectCommand);
expect(calls).toHaveLength(1);
expect(calls[0].args[0].input).toMatchObject({ Bucket: "b", Key: "reports/1.json" });
```

Things to know:

- `mockClient(S3Client)` patches the **class**, so it affects every instance. Call `s3Mock.reset()` in `beforeEach` or tests leak into each other.
- Match on command *and* input (`.on(Cmd, { Bucket: "b" })`) so a wrong bucket makes the test fail rather than silently getting your canned response.
- Unmocked commands resolve to `undefined`. If your code reads `res.Items` and gets a TypeError, you forgot a `.resolves(...)`.
- `.resolvesOnce(...)` / `.rejectsOnce(...)` chain for sequences, e.g. fail once then succeed to test retry logic.
- The mock never validates your input against the real service. A mocked test passing says nothing about whether AWS would accept the request. That's what the next layer is for.

### Testing handlers

Call the handler directly with a fixture event. No emulator needed for the parsing and branching logic.

```ts
import { handler } from "./handler.js";
import sqsEvent from "./fixtures/sqs-two-messages.json" assert { type: "json" };

it("reports only the failed message for partial batch failure", async () => {
  ddbMock.on(PutCommand).resolvesOnce({}).rejectsOnce(new Error("boom"));
  const res = await handler(sqsEvent as any);
  expect(res.batchItemFailures).toEqual([{ itemIdentifier: "msg-2" }]);
});
```

Event shapes and the partial-failure contract are in [event-driven Lambda](../03-lambda/03-event-driven-lambda.md). Keep real captured events as JSON fixtures.

---

## Integration tests: LocalStack

[LocalStack](https://www.localstack.cloud) emulates many AWS services behind one endpoint, `http://localhost:4566`. Your code, using the real SDK, serializes real requests; the emulator answers.

> Check LocalStack's current docs for the image name, licensing and auth requirements, and which services are included in the free tier. Those have changed over time and I won't pin them here. Coverage and fidelity vary by service, so verify the features you rely on.

### Start it

```yaml
# docker-compose.yml
services:
  localstack:
    image: localstack/localstack   # confirm the current image/tag in LocalStack's docs
    ports:
      - "4566:4566"
    environment:
      SERVICES: s3,sqs,dynamodb    # only start what you need
```

```bash
docker compose up -d
curl -s http://localhost:4566/_localstack/health   # wait until services show as available
```

### Point the SDK at it

With typed config this is just environment values:

```bash
AWS_REGION=us-east-1
AWS_ENDPOINT_URL=http://localhost:4566
AWS_ACCESS_KEY_ID=test
AWS_SECRET_ACCESS_KEY=test
UPLOADS_BUCKET=test-uploads
```

The dummy `test`/`test` credentials are for the emulator only. Real account keys should never be in a test environment. Recent SDK v3 versions also read `AWS_ENDPOINT_URL` themselves, but passing `endpoint` explicitly (as in the typed config) works regardless of version.

S3 needs `forcePathStyle: true` locally, because the default virtual-hosted style (`bucket.localhost`) won't resolve. The typed config from the previous note already sets this whenever an endpoint is set.

### A test that exercises real SDK calls

```ts
// uploads.int.test.ts
import { beforeAll, afterAll, it, expect } from "vitest";
import { randomUUID } from "node:crypto";
import {
  CreateBucketCommand, DeleteBucketCommand, PutObjectCommand,
  DeleteObjectCommand, S3Client,
} from "@aws-sdk/client-s3";
import { readJson } from "./uploads.js";

const s3 = new S3Client({
  region: "us-east-1",
  endpoint: "http://localhost:4566",
  forcePathStyle: true,
  credentials: { accessKeyId: "test", secretAccessKey: "test" },
});
const Bucket = `test-${randomUUID()}`;

beforeAll(() => s3.send(new CreateBucketCommand({ Bucket })));
afterAll(async () => {
  await s3.send(new DeleteObjectCommand({ Bucket, Key: "a.json" }));
  await s3.send(new DeleteBucketCommand({ Bucket }));
});

it("round-trips JSON through S3", async () => {
  await s3.send(new PutObjectCommand({ Bucket, Key: "a.json", Body: JSON.stringify({ ok: true }) }));
  expect(await readJson(s3, Bucket, "a.json")).toEqual({ ok: true });
});
```

Habits that keep these tests reliable:

- **Unique resource names per test run** (`randomUUID()`), so parallel runs and leftovers never collide.
- **Clean up** what you create, or reset the container between runs in CI.
- **Wait for readiness** before the suite starts: poll the health endpoint in a vitest `globalSetup`. Testcontainers also has a LocalStack module if you'd rather have tests start and stop the container themselves.
- Keep integration tests in a separate script (`npm run test:int`) so the fast unit suite stays instant.

### Other emulators

For DynamoDB alone, AWS's `amazon/dynamodb-local` image is a smaller option. The same endpoint-override pattern applies.

### The `awslocal` helper

The `awslocal` command (from `awscli-local`) is the AWS CLI preconfigured for LocalStack. Handy for poking at state:

```bash
awslocal s3 ls
awslocal sqs list-queues
```

---

## What emulators don't tell you

LocalStack and mocks answer "does my logic and request shape work?". They are weak at:

- **IAM.** Emulators generally don't enforce your policies by default, so a missing permission passes locally and fails in AWS. Cover this with a real-account smoke test under the same role your app uses, e.g. a profile with `role_arn` as shown in the [credentials note](./01-sdk-v3-and-credentials.md).
- **Quotas, throttling and latency.**
- **Exact edge-case behavior and newer features.** An emulator can lag the real service.
- **Cross-service event wiring** that depends on real S3 notifications, EventBridge rules and similar. Test it, but trust it only after a real deployment confirms it.

A reasonable shape: unit + LocalStack on every push; a small smoke suite against a dedicated dev/sandbox account on merge, using unique prefixes and automatic cleanup. Never run tests against production resources.

---

## Common mistakes and debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `ECONNREFUSED 127.0.0.1:4566` | Container not up yet, or wrong port | `docker compose ps`, hit the health endpoint, wait in `globalSetup` |
| S3 `ENOTFOUND bucket.localhost` | Virtual-hosted addressing | `forcePathStyle: true` |
| `Region is missing` in tests | Test env lacks `AWS_REGION` | Set it in the test setup, or pass `region` explicitly |
| Tests pass locally, fail in CI with credential errors | CI has no AWS creds and the code hit real AWS | Ensure the endpoint override is set; use dummy creds for the emulator |
| Mocked test passes, real call fails | The mock doesn't validate input | Add an integration test for that call |
| `res.Body.transformToString is not a function` in a mock test | Plain stream used as Body | Wrap with `sdkStreamMixin` |
| Test results depend on order | `mockClient` not reset, or shared bucket/table names | `reset()` in `beforeEach`; unique names per run |
| Passed in LocalStack, `AccessDenied` in AWS | Emulator doesn't enforce IAM | Run a real-account smoke test as the deployed role |

---

## Quick summary

- Layer your tests: **mock** for logic, **LocalStack** for integration, **real dev account** for IAM and true behavior.
- Build clients from typed config or inject them; keep business logic out of handlers.
- `aws-sdk-client-mock`: `mockClient(Client)`, `.on(Command, input).resolves/rejects`, `commandCalls()`, and `reset()` every test. Wrap S3 bodies with `sdkStreamMixin`.
- LocalStack: endpoint `http://localhost:4566`, dummy creds, `forcePathStyle` for S3, wait for health, unique names, clean up.
- Mocks and emulators prove shape and logic, not permissions or quotas.

## Next

[S3](../02-services/01-s3.md): the first service where you'll put these testing patterns to work.