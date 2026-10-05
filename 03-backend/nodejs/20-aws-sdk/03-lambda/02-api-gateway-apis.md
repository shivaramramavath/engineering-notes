# API Gateway APIs

API Gateway is the managed front door that turns HTTP requests into Lambda invocations: routing, TLS, throttling, CORS and (importantly) authorization happen *before* your code runs. Knowing what API Gateway does for you, and the exact event/response shape it speaks, saves hours of "Internal Server Error" debugging.

Prerequisites: [Lambda handlers](./01-lambda-handlers.md). For auth you'll want the token basics from [Cognito](../02-services/06-cognito.md).

```bash
npm install -D @types/aws-lambda
npm install zod            # for request validation, used below
```

---

## Which kind of API

| | HTTP API | REST API | Function URL |
|---|---|---|---|
| What it is | Simpler, cheaper, lower-latency API Gateway | Older, fuller-featured API Gateway | A built-in HTTPS URL on one function, no gateway |
| Routes | `GET /orders/{id}`, `$default` catch-all | Resources and methods | None: one URL, your code routes |
| Auth | JWT authorizer, Lambda authorizer, IAM | Cognito authorizer, Lambda authorizer, IAM, API keys/usage plans | IAM or none |
| Extras | CORS built in, auto-deploy stages | Request validation, caching, WAF, usage plans, private APIs | Response streaming, simplest setup |
| Payload version | 2.0 (also 1.0) | 1.0 | 2.0-style |

**Default to an HTTP API.** Reach for a REST API when you need a feature only it has (usage plans and API keys for metering clients, built-in request validation, caching, private endpoints, WAF integration). Feature sets have been converging, so check the current comparison before committing.

The rest of this note uses **HTTP API with payload format 2.0**, and notes where REST differs.

---

## The event and the response

A request arrives as an `APIGatewayProxyEventV2`:

```ts
{
  routeKey: "GET /orders/{id}",
  rawPath: "/orders/01J9A",
  rawQueryString: "expand=items",
  headers: { "content-type": "application/json", authorization: "Bearer ..." }, // lowercased
  queryStringParameters: { expand: "items" },
  pathParameters: { id: "01J9A" },
  body: "{\"qty\":2}",          // a STRING, or undefined
  isBase64Encoded: false,
  cookies: ["session=abc"],
  requestContext: { requestId: "...", http: { method: "GET", path: "/orders/01J9A" }, authorizer: { /* jwt claims if used */ } },
}
```

Things that surprise people:

- **`body` is a string** (or `undefined`), and **base64-encoded if `isBase64Encoded` is true**. You must `JSON.parse` it yourself.
- **Header names are lowercased.** Read `event.headers["content-type"]`.
- Repeated query parameters and headers are **comma-joined** into one string in v2.
- Path params are strings. Validate and convert.

You return `{ statusCode, headers?, body?, cookies? }`, where **`body` must be a string**:

```ts
import type { APIGatewayProxyEventV2, APIGatewayProxyResultV2 } from "aws-lambda";

const json = (statusCode: number, data: unknown): APIGatewayProxyResultV2 => ({
  statusCode,
  headers: { "content-type": "application/json" },
  body: JSON.stringify(data),
});
```

In payload v2 a handler that returns a plain object *without* `statusCode` gets treated as a 200 JSON body. That's a convenience that hides mistakes; return an explicit response.

---

## A complete handler

```ts
import type { APIGatewayProxyEventV2, APIGatewayProxyResultV2 } from "aws-lambda";
import { z } from "zod";

const CreateOrder = z.object({ sku: z.string().min(1), qty: z.number().int().positive() });

export const handler = async (event: APIGatewayProxyEventV2): Promise<APIGatewayProxyResultV2> => {
  try {
    switch (event.routeKey) {
      case "POST /orders": {
        const raw = event.isBase64Encoded
          ? Buffer.from(event.body ?? "", "base64").toString("utf8")
          : event.body ?? "";
        const parsed = CreateOrder.safeParse(JSON.parse(raw || "null"));
        if (!parsed.success) return json(400, { error: "invalid_request", details: parsed.error.flatten() });

        const order = await createOrder(parsed.data);      // your logic
        return json(201, order);
      }
      case "GET /orders/{id}": {
        const order = await getOrder(event.pathParameters!.id!);
        return order ? json(200, order) : json(404, { error: "not_found" });
      }
      default:
        return json(404, { error: "no_route" });
    }
  } catch (err) {
    if (err instanceof SyntaxError) return json(400, { error: "invalid_json" });
    console.error({ requestId: event.requestContext.requestId, err });
    return json(500, { error: "internal_error" });            // never leak internals
  }
};
```

Notes:

- Routing on `event.routeKey` works when you deploy one function behind several routes. Alternatively map each route to its own function ("function per route"); see the trade-off below.
- Validate every input. The types on the event describe shape, not safety.
- Catch at the edge, log with `requestId`, return a generic 500. A *thrown* error in a proxy integration becomes an opaque 500/502 from API Gateway with no body, which is hard for clients and for you.

---

## Express (or any framework) on Lambda

If you already have an Express app, you can run it in Lambda with an adapter that converts API Gateway events to Node `req`/`res` and back.

Structure the app so it runs both locally and in Lambda:

```ts
// src/app.ts: no listen() here
import express from "express";
export const app = express();
app.use(express.json());
app.get("/health", (_req, res) => res.json({ ok: true }));
app.post("/orders", async (req, res) => { /* ... */ res.status(201).json({}); });
```

```ts
// src/server.ts: local dev only
import { app } from "./app.js";
app.listen(3000);
```

```ts
// src/lambda.ts: the Lambda entry point
import serverless from "serverless-http";
import { app } from "./app.js";

export const handler = serverless(app);
```

Options:

| Adapter | Idea |
|---|---|
| `serverless-http` | Small wrapper: `handler = serverless(app)` |
| `@codegenie/serverless-express` | Maintained descendant of `vendia/serverless-express`; similar API (`serverlessExpress({ app })`) |
| AWS Lambda Web Adapter | A Lambda extension that proxies events to **any** HTTP server on a port, with no code changes and no framework lock-in; also supports response streaming |

Trade-offs of "the whole app in one function":

- **Pro:** reuse your app, framework middleware and local dev flow; one deploy artifact.
- **Con:** bigger bundle and slower cold starts; one function's memory/timeout/permissions for every route (so its IAM role is the union of everything); you lose per-route scaling and metrics.
- Middle road: a few functions grouped by domain.

Limits still apply: Lambda's ~6 MB synchronous payload, and API Gateway's integration timeout of **about 30 seconds**. For big uploads, don't proxy through the API at all; return a presigned URL ([S3](../02-services/01-s3.md)). For long work, accept the request, enqueue a job ([SQS](../02-services/03-sqs.md)) and return `202`.

---

## Authorization

Authorize **at the gateway** when you can, so unauthenticated traffic never costs you an invocation.

| Mechanism | Use when |
|---|---|
| **JWT authorizer** (HTTP API) | Cognito or any OIDC provider. API Gateway verifies signature, issuer, audience and expiry, optionally checks scopes. No code |
| **Lambda authorizer** | Custom logic: API keys in your DB, multi-tenant rules, non-JWT tokens. Responses can be cached |
| **IAM auth** (SigV4) | Service-to-service calls inside AWS, signed by the caller's role |
| **Cognito authorizer** (REST API) | The REST API's equivalent of the JWT authorizer for Cognito |
| **API keys + usage plans** (REST API) | **Metering and throttling clients, not security.** Keys aren't a substitute for authentication |

### Reading the identity in your handler

With a JWT authorizer, validated claims arrive on the event:

```ts
import type { APIGatewayProxyEventV2WithJWTAuthorizer } from "aws-lambda";

export const handler = async (event: APIGatewayProxyEventV2WithJWTAuthorizer) => {
  const claims = event.requestContext.authorizer.jwt.claims;
  const userId = claims.sub as string;     // key users on `sub`
  const scopes = event.requestContext.authorizer.jwt.scopes;
  // ...
};
```

The authorizer checks the token is valid; **what the user may do** (authorization) is still your job: check `sub` ownership, `cognito:groups`, scopes.

### A Lambda authorizer (simple response format)

```ts
import type { APIGatewayRequestAuthorizerEventV2 } from "aws-lambda";
import { CognitoJwtVerifier } from "aws-jwt-verify";

const verifier = CognitoJwtVerifier.create({
  userPoolId: process.env.COGNITO_USER_POOL_ID!,
  tokenUse: "access",
  clientId: process.env.COGNITO_CLIENT_ID!,
});

export const handler = async (event: APIGatewayRequestAuthorizerEventV2) => {
  try {
    const token = event.headers?.authorization?.replace(/^Bearer /i, "") ?? "";
    const claims = await verifier.verify(token);
    return { isAuthorized: true, context: { userId: claims.sub } };   // available to the main function
  } catch {
    return { isAuthorized: false };                                   // API Gateway returns 403
  }
};
```

Configure the authorizer with **simple responses enabled** (`isAuthorized` boolean) and an identity source such as the `Authorization` header. The result is cached per identity source for a configurable TTL, so a valid token isn't re-verified on every request. (For Cognito you usually don't need this; the built-in JWT authorizer does the same job without code.)

---

## CORS

Browsers send a preflight `OPTIONS` request for cross-origin calls with custom headers or JSON bodies.

- **HTTP API:** configure CORS **on the API** (allowed origins, methods, headers). API Gateway then answers preflights and adds the headers. Configure it in one place; adding the same headers in your function as well makes behavior confusing to debug.
- **REST API:** you must handle it yourself: an `OPTIONS` method (often a mock integration) **and** CORS headers on your function's real responses.
- Don't use `*` as the allowed origin with credentials (cookies/`Authorization` in browsers); list real origins.

A "CORS error" in the browser console is frequently a masked **401/403/500**: the error response from the gateway lacked CORS headers, so the browser reports CORS instead of the real status. Check the network tab's actual status first.

---

## Throttling and limits

- API Gateway applies account-level default throttling and lets you set limits per stage/route; clients get **429** when exceeded. Check current default quotas.
- Lambda concurrency limits (and reserved concurrency) can also throttle: those surface as 429/500 depending on integration.
- Payloads: about 10 MB at the API Gateway layer but only ~6 MB through Lambda's synchronous limit, so treat 6 MB as your ceiling.

---

## Debugging

| Symptom | Likely cause | Check |
|---|---|---|
| `{"message":"Internal Server Error"}` (500) | Lambda threw, or returned a malformed response (non-string `body`, wrong shape, REST API proxy) | Function logs; return `statusCode` + string `body` |
| `{"message":"Service Unavailable"}` / 503, or "Execution failed due to configuration error: Invalid permissions on Lambda function" | API Gateway isn't allowed to invoke the function | Resource policy allowing `apigateway.amazonaws.com` to `lambda:InvokeFunction` (IaC normally adds it) |
| 504 / "Endpoint request timed out" | Function exceeded the integration timeout (~30 s) or its own timeout | Make it faster, or go async (202 + queue) |
| 403 `{"message":"Forbidden"}` | Authorizer denied, or no route matched, or missing auth | Check route key, authorizer config, token |
| 404 `{"message":"Not Found"}` from API Gateway itself | Route/method doesn't exist | Compare `routeKey` with deployed routes; HTTP API stage and base path |
| `body` parsing fails | `isBase64Encoded: true`, or empty body | Decode and guard |
| Binary response corrupted | Not base64-encoded (and, on REST APIs, binary media types not configured) | Return `isBase64Encoded: true` with base64 body |
| CORS error in browser | Preflight not handled, or a real error response without CORS headers | Configure API-level CORS; look at the actual status |
| Works in test event, not via URL | Test event shape isn't what API Gateway really sends | Log the real event once (`console.log(JSON.stringify(event))`) |

Turn on **access logging** for the stage (to CloudWatch) so you can see gateway-level rejections that never reach your function.

---

## REST API (v1) differences in one glance

- Event type `APIGatewayProxyEvent` (payload 1.0): `httpMethod`, `path`, `resource`, `multiValueHeaders`/`multiValueQueryStringParameters`, and claims at `requestContext.authorizer.claims` for the Cognito authorizer.
- Responses must be `{ statusCode, headers, body }` with a string `body` or you get a 502.
- You wire CORS and binary media types yourself.

---

## Quick summary

- Default to an **HTTP API** (payload v2); use a REST API only for features it uniquely has; Function URLs are the minimal option.
- The event's `body` is a string (maybe base64), headers are lowercased; respond with a `statusCode` and a **string** `body`.
- Validate input, catch at the edge, return generic 500s, and log the request ID.
- Express on Lambda via `serverless-http`/`serverless-express`/Web Adapter works, with cold-start and permission trade-offs; mind the 6 MB / ~30 s limits.
- Authenticate at the gateway (JWT authorizer) and authorize in code using `sub`, groups and scopes.
- Configure CORS in one place; a browser "CORS error" often hides a real 4xx/5xx.

## Next

[Event-driven Lambda](./03-event-driven-lambda.md): functions triggered by S3, queues and streams, where retries and duplicates become your problem.