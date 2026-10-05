# AWS SDK v3 Cheatsheet

Snippets by service for quick lookup. Explanations, rules and gotchas live in the linked notes. All examples are TypeScript (ESM); `Bucket`, `Key`, `QueueUrl` and similar are assumed to be defined.

---

## Setup and identity

[Note](../01-setup/01-sdk-v3-and-credentials.md)

```bash
npm i @aws-sdk/client-s3 @aws-sdk/client-sts @aws-sdk/credential-providers
aws configure sso && aws sso login --profile dev
export AWS_PROFILE=dev AWS_REGION=ap-south-1
aws sts get-caller-identity          # who am I? (check this FIRST when debugging)
env | grep AWS_                      # env vars beat profiles in the credential chain
```

```ts
const s3 = new S3Client({ region: process.env.AWS_REGION });        // create once, reuse
await s3.send(new ListBucketsCommand({}));                           // client.send(Command)

import { fromIni, fromTemporaryCredentials } from "@aws-sdk/credential-providers";
new S3Client({ credentials: fromIni({ profile: "dev" }) });
new S3Client({ credentials: fromTemporaryCredentials({ params: { RoleArn, RoleSessionName: "app" } }) });

// LocalStack / emulator
new S3Client({ region: "us-east-1", endpoint: "http://localhost:4566", forcePathStyle: true,
               credentials: { accessKeyId: "test", secretAccessKey: "test" } });

// error fields
err.name; err.$fault; err.$metadata?.httpStatusCode; err.$metadata?.requestId; err.$metadata?.attempts;
```

---

## S3

[Note](../02-services/01-s3.md) · `@aws-sdk/client-s3`, `lib-storage`, `s3-request-presigner`, `s3-presigned-post`

```ts
await s3.send(new PutObjectCommand({ Bucket, Key, Body, ContentType: "application/json" }));

const r = await s3.send(new GetObjectCommand({ Bucket, Key }));
const text = await r.Body!.transformToString();                      // small objects only
await pipeline(r.Body as Readable, createWriteStream(path));          // large: stream

await s3.send(new HeadObjectCommand({ Bucket, Key }));                // exists? (throws NotFound)
await s3.send(new DeleteObjectCommand({ Bucket, Key }));              // succeeds on missing keys
await s3.send(new CopyObjectCommand({ Bucket, Key: dst, CopySource: `${Bucket}/${encodeURIComponent(src)}` }));

// list all (1000 per page)
for await (const p of paginateListObjectsV2({ client: s3 }, { Bucket, Prefix: "a/" }))
  p.Contents?.forEach((o) => o.Key);

// multipart / streamed upload
await new Upload({ client: s3, params: { Bucket, Key, Body: stream }, partSize: 8 * 1024 * 1024, queueSize: 4 }).done();

// presigned URLs (signing is local; sign only headers the client will send)
const putUrl = await getSignedUrl(s3, new PutObjectCommand({ Bucket, Key, ContentType }), { expiresIn: 300 });
const getUrl = await getSignedUrl(s3, new GetObjectCommand({ Bucket, Key }), { expiresIn: 300 });

// size/type limits → presigned POST
const { url, fields } = await createPresignedPost(s3, { Bucket, Key, Expires: 300,
  Conditions: [["content-length-range", 1, 10_485_760]] });
```

IAM: `s3:ListBucket` → `arn:aws:s3:::bucket`; `s3:GetObject`/`s3:PutObject` → `arn:aws:s3:::bucket/*`.
Event keys are URL-encoded: `decodeURIComponent(key.replace(/\+/g, " "))`.

---

## DynamoDB

[Note](../02-services/02-dynamodb.md) · `client-dynamodb` + `lib-dynamodb` (import commands from **lib-dynamodb**)

```ts
const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}), { marshallOptions: { removeUndefinedValues: true } });

await ddb.send(new PutCommand({ TableName, Item }));                  // replaces the whole item
const { Item } = await ddb.send(new GetCommand({ TableName, Key, ConsistentRead: true }));

await ddb.send(new UpdateCommand({
  TableName, Key,
  UpdateExpression: "SET #s = :s ADD views :one",                     // SET / ADD / REMOVE / DELETE
  ExpressionAttributeNames: { "#s": "status" },                       // reserved words need #names
  ExpressionAttributeValues: { ":s": "PAID", ":one": 1 },
  ConditionExpression: "attribute_exists(pk)",
  ReturnValues: "ALL_NEW",
}));

await ddb.send(new DeleteCommand({ TableName, Key }));

// insert-if-new / idempotency
await ddb.send(new PutCommand({ TableName, Item, ConditionExpression: "attribute_not_exists(pk)" }));
// → catch err.name === "ConditionalCheckFailedException"

// query (PK equality + optional SK condition); GSI via IndexName
await ddb.send(new QueryCommand({
  TableName, IndexName,                                               // IndexName optional
  KeyConditionExpression: "pk = :p AND sk BETWEEN :a AND :b",         // also begins_with(sk, :x)
  ExpressionAttributeValues: { ":p": "u#1", ":a": "a", ":b": "z" },
  ScanIndexForward: false, Limit: 20,
}));

// all pages
for await (const p of paginateQuery({ client: ddb }, queryInput)) p.Items;
// manual: pass LastEvaluatedKey back as ExclusiveStartKey; done only when it's absent

// batches / transactions
await ddb.send(new BatchWriteCommand({ RequestItems: { [TableName]: [{ PutRequest: { Item } }] } })); // ≤25, retry UnprocessedItems
await ddb.send(new TransactWriteCommand({ TransactItems: [{ Put: { TableName, Item } }, { Update: { /* … */ } }] })); // ≤100, atomic
```

IAM: `dynamodb:GetItem|PutItem|UpdateItem|DeleteItem|Query` on the table ARN **and** `table/NAME/index/*`.
TTL attribute = epoch **seconds**. `FilterExpression` doesn't reduce cost.

---

## SQS

[Note](../02-services/03-sqs.md) · `client-sqs` · the SDK wants the queue **URL**

```ts
await sqs.send(new SendMessageCommand({ QueueUrl, MessageBody: JSON.stringify(msg),
  MessageAttributes: { type: { DataType: "String", StringValue: "order.paid" } }, DelaySeconds: 0 }));

const r = await sqs.send(new SendMessageBatchCommand({ QueueUrl,
  Entries: msgs.map((m, i) => ({ Id: String(i), MessageBody: JSON.stringify(m) })) }));   // ≤10; check r.Failed

// worker loop: long poll → process → delete only on success
const { Messages = [] } = await sqs.send(new ReceiveMessageCommand({
  QueueUrl, MaxNumberOfMessages: 10, WaitTimeSeconds: 20, VisibilityTimeout: 60,
  MessageSystemAttributeNames: ["ApproximateReceiveCount"], MessageAttributeNames: ["All"] }));
await sqs.send(new DeleteMessageCommand({ QueueUrl, ReceiptHandle }));
await sqs.send(new ChangeMessageVisibilityCommand({ QueueUrl, ReceiptHandle, VisibilityTimeout: 300 }));
```

Redrive policy: `{"deadLetterTargetArn":"arn:…:dlq","maxReceiveCount":5}`; DLQ same type, longer retention, **alarm on depth**.
FIFO: `MessageGroupId` + dedup id. Standard = at-least-once → idempotent consumers.

---

## SNS and EventBridge

[Note](../02-services/04-sns-and-eventbridge.md) · `client-sns`, `client-eventbridge`

```ts
await sns.send(new PublishCommand({ TopicArn, Message: JSON.stringify(evt),
  MessageAttributes: { type: { DataType: "String", StringValue: "order.paid" } } }));
// subscriber gets an envelope unless RawMessageDelivery: JSON.parse(JSON.parse(body).Message)

const r = await eb.send(new PutEventsCommand({ Entries: [{
  EventBusName: "app-bus", Source: "app.orders", DetailType: "OrderPaid",
  Detail: JSON.stringify({ orderId }) }] }));                         // Detail must be a JSON string
if (r.FailedEntryCount) { /* retry r.Entries.filter(e => e.ErrorCode) */ }
```

Rule pattern: `{"source":["app.orders"],"detail-type":["OrderPaid"],"detail":{"total":[{"numeric":[">",1000]}]}}`.
Queue policy for SNS/EventBridge: principal = service, `Condition: aws:SourceArn`.

---

## SES (v2)

[Note](../02-services/05-ses-email.md) · `client-sesv2`

```ts
await ses.send(new SendEmailCommand({
  FromEmailAddress: "App <no-reply@example.com>",
  Destination: { ToAddresses: [to] },
  Content: { Simple: { Subject: { Data: "Hi", Charset: "UTF-8" },
    Body: { Html: { Data: html, Charset: "UTF-8" }, Text: { Data: text, Charset: "UTF-8" } } } },
  ConfigurationSetName: "app-default",
}));

// stored template
Content: { Template: { TemplateName: "welcome", TemplateData: JSON.stringify({ name }) } }
```

Verify the domain (DKIM, SPF/MAIL FROM, DMARC) **in the sending region**; leave the sandbox; track bounces/complaints through a configuration set and suppress them. Attachments → raw MIME (nodemailer SES transport).

---

## Cognito

[Note](../02-services/06-cognito.md) · `client-cognito-identity-provider`, `aws-jwt-verify`

```ts
await idp.send(new SignUpCommand({ ClientId, Username, Password, UserAttributes: [{ Name: "email", Value: email }] }));
await idp.send(new ConfirmSignUpCommand({ ClientId, Username, ConfirmationCode }));
const out = await idp.send(new InitiateAuthCommand({ ClientId, AuthFlow: "USER_PASSWORD_AUTH",
  AuthParameters: { USERNAME, PASSWORD } }));                         // + SECRET_HASH if the client has a secret
// out.AuthenticationResult → { AccessToken, IdToken, RefreshToken }  |  out.ChallengeName + out.Session
await idp.send(new InitiateAuthCommand({ ClientId, AuthFlow: "REFRESH_TOKEN_AUTH", AuthParameters: { REFRESH_TOKEN } }));
await idp.send(new GlobalSignOutCommand({ AccessToken }));

// verify (never just decode)
const verifier = CognitoJwtVerifier.create({ userPoolId, tokenUse: "access", clientId });
const claims = await verifier.verify(token);                          // claims.sub, claims["cognito:groups"]
```

Access token → API auth; ID token → profile; key users on `sub`. `SECRET_HASH` = base64(HMAC-SHA256(username + clientId, clientSecret)).

---

## Bedrock

[Note](../02-services/07-bedrock.md) · `client-bedrock-runtime` (call) · `client-bedrock` (list/manage)

```ts
const r = await bedrock.send(new ConverseCommand({
  modelId: process.env.BEDROCK_MODEL_ID!,                             // model ID or inference profile ID
  system: [{ text: "Be concise." }],
  messages: [{ role: "user", content: [{ text: prompt }] }],
  inferenceConfig: { maxTokens: 300, temperature: 0.2 },
}));
r.output?.message?.content;  r.stopReason;  r.usage;                  // content = blocks; check stopReason

const s = await bedrock.send(new ConverseStreamCommand({ modelId, messages }));
for await (const ev of s.stream!) ev.contentBlockDelta?.delta?.text;

// LangChain.js
const model = new ChatBedrockConverse({ model: modelId, region, temperature: 0 });
await model.invoke([["system", "…"], ["human", "…"]]);
```

IAM: `bedrock:InvokeModel` + `InvokeModelWithResponseStream` (Converse uses these). Inference profiles: allow the profile **and** the underlying foundation-model ARNs.

---

## Secrets Manager and Parameter Store

[Note](../02-services/08-secrets-and-parameters.md)

```ts
const sec = await sm.send(new GetSecretValueCommand({ SecretId: "prod/orders/db" }));
const creds = JSON.parse(sec.SecretString!);

const { Parameter } = await ssm.send(new GetParameterCommand({ Name: "/orders/prod/key", WithDecryption: true }));
for await (const p of paginateGetParametersByPath({ client: ssm }, { Path: "/orders/prod/", Recursive: true, WithDecryption: true }))
  p.Parameters;
```

Cache with a TTL (cache the promise, don't cache failures). Pass the secret's **name** in env vars, never the value.
IAM: secret ARN needs a trailing `-*`; SSM ARN is `…:parameter/path/*`; customer-managed KMS key → `kms:Decrypt`.

---

## Lambda skeletons

[Handlers](../03-lambda/01-lambda-handlers.md) · [API Gateway](../03-lambda/02-api-gateway-apis.md) · [Event-driven](../03-lambda/03-event-driven-lambda.md)

```ts
// init (once per environment) …  clients, config
// handler (every invocation) …   await everything, per-request state in locals

// API Gateway HTTP API (payload v2): body is a STRING; respond with string body
export const api = async (e: APIGatewayProxyEventV2): Promise<APIGatewayProxyResultV2> => ({
  statusCode: 200, headers: { "content-type": "application/json" }, body: JSON.stringify({ ok: true }) });
const claims = (e as APIGatewayProxyEventV2WithJWTAuthorizer).requestContext.authorizer.jwt.claims;

// SQS with partial batch failure (enable ReportBatchItemFailures on the mapping)
export const worker = async (e: SQSEvent): Promise<SQSBatchResponse> => {
  const res = await Promise.allSettled(e.Records.map((r) => handle(JSON.parse(r.body))));
  return { batchItemFailures: res.flatMap((x, i) => x.status === "rejected" ? [{ itemIdentifier: e.Records[i].messageId }] : []) };
};

// S3 trigger: decode key, never write back to the trigger prefix
const key = decodeURIComponent(rec.s3.object.key.replace(/\+/g, " "));

// DynamoDB stream: images are typed JSON
const item = unmarshall(rec.dynamodb!.NewImage as any);

// streaming response (function URL, invoke mode RESPONSE_STREAM)
export const h = awslambda.streamifyResponse(async (_e, res) => { await pipeline(readable, res); });
```

esbuild: `esbuild src/h.ts --bundle --platform=node --target=node22 --format=esm --outfile=dist/index.mjs --minify --sourcemap --external:@aws-sdk/*` + env `NODE_OPTIONS=--enable-source-maps`.

---

## Retries, timeouts, partial failures

[Note](../04-production/01-errors-and-retries.md)

```ts
new S3Client({ maxAttempts: 5 });                                     // default: 3 attempts, standard mode
new DynamoDBClient({ requestHandler: new NodeHttpHandler({ connectionTimeout: 2000, requestTimeout: 5000 }) });
await client.send(cmd, { abortSignal: AbortSignal.timeout(3000) });   // per-call deadline

// full-jitter backoff
await sleep(Math.random() * Math.min(capMs, baseMs * 2 ** attempt));
```

Batch APIs return 200 with failures inside; the SDK won't retry them:

| API | Check |
|---|---|
| DynamoDB `BatchWrite` / `BatchGet` | `UnprocessedItems` / `UnprocessedKeys` |
| SQS / SNS batch | `Failed` |
| EventBridge `PutEvents` | `FailedEntryCount` |
| S3 `DeleteObjects` | `Errors` |

---

## Logging, metrics, tracing

[Note](../04-production/02-logging-and-tracing.md)

```ts
logger.addContext(context); logger.appendKeys({ orderId }); logger.info("msg"); logger.error("msg", err as Error);  // Powertools
metrics.addMetric("OrderPlaced", MetricUnit.Count, 1); metrics.publishStoredMetrics();                              // EMF
const s3 = tracer.captureAWSv3Client(new S3Client({}));  tracer.putAnnotation("orderId", id);                       // X-Ray
```

```text
fields @timestamp, level, message | filter level = "ERROR" | sort @timestamp desc | limit 50
filter @type = "REPORT" | stats avg(@duration), pct(@duration, 99), max(@maxMemoryUsed/1024/1024) by bin(5m)
filter @type = "REPORT" and ispresent(@initDuration) | stats count() by bin(1h)      # cold starts
```

Alarm on: **DLQ depth > 0**, Lambda errors/throttles, queue age, stream iterator age, API 5xx, p99 latency.

---

## Testing

[Note](../01-setup/02-local-development-and-testing.md)

```ts
const s3Mock = mockClient(S3Client);                                  // aws-sdk-client-mock; reset() in beforeEach
s3Mock.on(GetObjectCommand, { Bucket: "b" }).resolves({ Body: sdkStreamMixin(Readable.from(["x"])) as any });
s3Mock.on(GetObjectCommand).rejects(Object.assign(new Error("x"), { name: "NoSuchKey" }));
s3Mock.commandCalls(PutObjectCommand)[0].args[0].input;
```

```bash
docker compose up -d && curl -s localhost:4566/_localstack/health
awslocal s3 ls            # LocalStack: forcePathStyle for S3, dummy creds, unique names per run
```

---

## Error names you'll meet

| Name | Usually means | First check |
|---|---|---|
| `CredentialsProviderError` | No credentials found | `aws sso login`, `AWS_PROFILE`, `get-caller-identity` |
| `Region is missing` | No region configured | Set `region` explicitly |
| `AccessDenied` / `AccessDeniedException` | IAM, resource policy, SCP, or KMS | Caller identity, ARN (`bucket/*`, `-*`), explicit Deny |
| `NoSuchKey` / `NotFound` (HEAD) | Missing object | Key, region; `s3:ListBucket` gives 404 instead of 403 |
| `NoSuchBucket`, "table not found" | Wrong region/name | Client region |
| `SignatureDoesNotMatch` (presigned) | Signed header not sent, or clock skew | Send exact `Content-Type` |
| `ConditionalCheckFailedException` | Your condition was false | Expected control flow |
| `ProvisionedThroughputExceededException` / `ThrottlingException` / `SlowDown` / `TooManyRequestsException` | Throttled | Reduce/smooth load; the SDK retries |
| `ValidationException` | Bad input (or Bedrock needs an inference profile) | Request shape, model ID |
| `MessageRejected` (SES) | Sandbox or unverified identity | Verify, leave sandbox, right region |
| `NotAuthorizedException` / `UserNotConfirmedException` (Cognito) | Flow not enabled / unconfirmed user | App client auth flows; confirm |
| `ResourceNotFoundException` / `ParameterNotFound` | Wrong name or region | Name, leading `/`, region |

---

## Limits and numbers worth remembering

Check the current AWS docs for exact, current values.

| Thing | Value |
|---|---|
| Lambda timeout / memory | up to 15 min / 128–10,240 MB |
| Lambda sync payload | ~6 MB |
| Lambda package | 50 MB zipped, 250 MB unzipped (with layers) |
| API Gateway integration timeout | ~30 s |
| S3 single `PutObject` / object max | 5 GB / 5 TiB; multipart parts 5 MiB–5 GiB, ≤10,000 |
| S3 `ListObjectsV2` page | 1,000 keys |
| DynamoDB item / query page | 400 KB / 1 MB |
| DynamoDB batch write / transact | 25 / 100 items |
| SQS batch | 10 messages |
| SQS long poll | `WaitTimeSeconds` ≤ 20 |
| SDK retry default | 3 attempts (standard mode) |
| Visibility timeout (SQS→Lambda) | ≥ ~6× function timeout |
| Secrets Manager / SSM standard parameter | 64 KB / 4 KB |