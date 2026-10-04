# Load Balancing and API Gateway

Two ways to put a stable, scalable front end in front of your compute:

- **Elastic Load Balancing (ELB)** spreads traffic across targets (EC2 instances, containers, IPs, Lambda functions), checks their health, and terminates TLS. You run services behind it.
- **API Gateway** is a managed API front door: routing, auth, throttling, request validation and (for REST APIs) caching, with no servers to run, typically in front of [Lambda](../02-compute/02-lambda.md).

They overlap for HTTP APIs, which is why choosing between them is a common design question. This note covers both and finishes with how to choose.

Prerequisites: [VPC](./01-vpc.md) (subnets, security groups), [IAM](../01-foundations/03-iam.md).

---

# Part 1: Elastic Load Balancing

## The load balancer family

| Type | Layer | Best for | Notable traits |
|---|---|---|---|
| **Application Load Balancer (ALB)** | 7 (HTTP/HTTPS, gRPC, WebSocket) | Web apps and APIs, microservices | Routing by host/path/header/query, redirects, fixed responses, OIDC/Cognito auth, Lambda targets |
| **Network Load Balancer (NLB)** | 4 (TCP/UDP/TLS) | Extreme throughput/low latency, non-HTTP protocols, static IPs | Static/Elastic IP per AZ, preserves client source IP, PrivateLink-compatible |
| **Gateway Load Balancer (GWLB)** | 3 | Inline virtual appliances (firewalls, inspection) | Specialised |
| **Classic Load Balancer** | 4/7 | Legacy | Don't use for new work |

Default choice for web traffic: **ALB**. Reach for **NLB** when you need non-HTTP protocols, fixed IPs, or extreme performance.

## ALB building blocks

```
Client ─► ALB (in public subnets, 2+ AZs)
            │  Listener :443 (HTTPS, ACM certificate)
            │    ├─ Rule: host = api.example.com  AND path /v1/* ─► Target group "api"   ─► ECS tasks / EC2 / IPs
            │    ├─ Rule: path /static/*                          ─► Target group "web"
            │    └─ Default                                       ─► Target group "web"
            └─ Listener :80 ─► redirect to HTTPS
```

| Piece | Meaning |
|---|---|
| **Listener** | Protocol + port the LB accepts (e.g. HTTPS:443) and the default action |
| **Rule** | Condition(s) (host, path, header, method, source IP…) → action (forward, redirect, fixed response, authenticate) |
| **Target group** | A set of targets plus the health check settings and protocol/port to reach them |
| **Target** | **Instance**, **IP address** (required for Fargate/`awsvpc` tasks), or a **Lambda function** |

**Health checks** decide which targets receive traffic: path, expected status codes, interval, and thresholds. A target that fails is taken out of rotation until it passes again. This is why a wrong health check path makes a healthy app look broken.

### HTTPS

Terminate TLS at the ALB with a certificate from **ACM** (free for public certs, auto-renewed, DNS-validated), choose a modern security policy, and redirect HTTP→HTTPS. Whether the ALB-to-target hop is HTTP or HTTPS is your call. Plain HTTP inside the VPC is common, but end-to-end encryption is available if compliance requires it. Point your domain at the ALB with a Route 53 **alias** record ([Route 53 and CloudFront](./02-route53-and-cloudfront.md)).

### Network and security group wiring

```
alb-sg  : inbound 443 (and 80) from 0.0.0.0/0
app-sg  : inbound <app port> from alb-sg only
```

- Place the ALB in **public subnets** across at least two AZs; place targets in **private subnets**.
- The ALB talks to targets using its own private IPs, so allow the **ALB's security group** into the targets.

### Behaviours worth knowing

- **Cross-zone load balancing** is on by default for ALB (traffic is spread across targets in all AZs). For NLB it is configurable and may incur cross-AZ data charges.
- **Idle timeout** (default 60 s): the ALB closes connections idle longer than that. Long-polling/streaming requests need a larger timeout or keep-alives, and the **app's keep-alive timeout should exceed the ALB's**, or you get sporadic 502s.
- **Deregistration delay** (default 300 s): how long in-flight requests get to finish when a target is removed, for example during deployments. Lower it for fast-draining apps; and handle `SIGTERM` gracefully.
- **Client IP** arrives in the `X-Forwarded-For` header (ALB), not as the TCP source address. NLB can preserve the original source IP.
- **Sticky sessions** (cookie-based) exist but are a crutch. Prefer stateless apps with sessions in a shared store.
- **Slow start** and **least-outstanding-requests** routing help when targets warm up or have uneven load.
- Pricing: hourly charge plus **LCU** (capacity-unit) usage. A load balancer left running idle still bills hourly, and each uses public IPv4 addresses (also billed).

### Connecting compute

```bash
# Typical ECS service wiring (see ECS note)
aws ecs create-service ... \
  --load-balancers 'targetGroupArn=arn:aws:elasticloadbalancing:...,containerName=app,containerPort=8080'
```

For [ECS/Fargate](../02-compute/03-ecs-fargate.md) the target type is **IP**; for [EC2](../02-compute/01-ec2.md) Auto Scaling groups it's **instance**. A Lambda target lets an ALB invoke a function per request, which is handy when you already have an ALB.

---

# Part 2: API Gateway

## What it gives you

A fully managed service that exposes HTTP/WebSocket APIs and routes them to backends (usually Lambda, but also HTTP endpoints, AWS services, or private VPC resources). It adds, without you running anything: custom domains, authorizers, throttling, CORS handling, request/response mapping, logging, and scaling.

## The three API types

| | **HTTP API** | **REST API** | **WebSocket API** |
|---|---|---|---|
| Positioning | Simpler, **cheaper, lower latency** | Feature-rich, older | Two-way persistent connections |
| Auth | JWT authorizers (Cognito/OIDC), Lambda authorizers, IAM | IAM, Cognito user pools, Lambda authorizers | Lambda/IAM |
| Extras | Auto-deploy stages, simple CORS | **API keys + usage plans, request validation, response caching, WAF integration, private APIs, request/response transformation (VTL)**, canary releases | Route by message content |
| Choose when | Straightforward Lambda/HTTP backends | You need those REST-only features | Chat, live dashboards, push |

**Default to HTTP API** unless you specifically need a REST-only feature. (Check the current feature comparison in the docs. The gap changes over time.)

## A minimal HTTP API in front of Lambda (CDK)

```ts
import * as apigwv2 from "aws-cdk-lib/aws-apigatewayv2";
import { HttpLambdaIntegration } from "aws-cdk-lib/aws-apigatewayv2-integrations";

const api = new apigwv2.HttpApi(this, "Api", {
  corsPreflight: {
    allowOrigins: ["https://app.example.com"],
    allowMethods: [apigwv2.CorsHttpMethod.GET, apigwv2.CorsHttpMethod.POST],
    allowHeaders: ["content-type", "authorization"],
  },
});

api.addRoutes({
  path: "/orders/{id}",
  methods: [apigwv2.HttpMethod.GET],
  integration: new HttpLambdaIntegration("GetOrder", getOrderFn),
});
```

The Lambda handler receives the request as an event and returns the response shape API Gateway expects (this is the **proxy integration**):

```ts
export const handler = async (event) => {
  const id = event.pathParameters?.id;
  return {
    statusCode: 200,
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ id }),
  };
};
```

The payload format differs between HTTP API (v2.0 by default) and REST API (v1.0). Event field names aren't identical, so check which one your function targets. A response that isn't valid JSON in the expected shape produces a `502`/`Internal server error` from API Gateway.

## Stages, deployments and the REST API gotcha

- A **stage** (`dev`, `prod`, or `$default`) is a named, deployed snapshot reachable at its own URL, with its own settings (throttling, logging, stage variables).
- **HTTP APIs can auto-deploy** changes to a stage. **REST APIs do not.** After changing routes or integrations you must **create a new deployment** and attach it to a stage. Forgetting this is the classic "I changed it but nothing happened".
- Infrastructure-as-code handles deployments for you (CDK/SAM create them), but be aware of it when using the console/CLI.

## Authorization options

| Mechanism | How it works | Typical use |
|---|---|---|
| **JWT authorizer** (HTTP API) | Validates a JWT issued by Cognito or any OIDC provider (signature, issuer, audience, scopes) | User-facing apps, see [Cognito](../05-app-services/04-cognito.md) |
| **Cognito user pool authorizer** (REST) | Same idea, REST flavour | |
| **Lambda authorizer** | Your function decides allow/deny (custom tokens, API keys, legacy auth); results can be cached | Custom logic |
| **IAM authorization** | Caller signs requests with SigV4; needs `execute-api:Invoke` | Service-to-service, internal tools |
| **API keys + usage plans** (REST) | Per-client throttle/quota | Metering partners. API keys are **not a security mechanism** by themselves. |
| **Mutual TLS**, **WAF** | Client certs; rule-based filtering (REST/regional) | Hardening |

## Limits and behaviours to design around

- **Integration timeout**: roughly **29 seconds by default** for synchronous integrations (some API types can be raised on request). If the work takes longer, return `202 Accepted` and process asynchronously (SQS → worker, Step Functions), then let the client poll or receive a callback. See [SQS](../05-app-services/01-sqs.md).
- **Payload size** is limited (on the order of 10 MB). For uploads, hand out **S3 presigned URLs** instead of proxying files through API Gateway/Lambda ([S3](../03-storage-and-databases/01-s3.md)).
- **Throttling** exists at account, stage and route level (steady-state rate plus burst). Clients receive `429` when exceeded. Check the current quotas in Service Quotas.
- **CORS** has to be handled explicitly (below).
- **Edge-optimised vs regional endpoints** (REST): edge-optimised routes via CloudFront-managed edges; regional is the simple default, and you can put your own CloudFront in front.
- Pricing is **per request** (plus data transfer, caching if used), so it's very cheap at low volume but can exceed an ALB at sustained high throughput. Compare for your traffic.

### CORS in a sentence

Browsers send a **preflight `OPTIONS`** request and require `Access-Control-Allow-Origin` (and related headers) on responses. On HTTP API, configure `corsPreflight` and the gateway answers preflight for you. On REST or with Lambda proxy, your function (and an `OPTIONS` route) must return the CORS headers on **every** response, including errors. Wildcard `*` origin and credentials (cookies/Authorization) can't be combined.

---

# Choosing: ALB vs API Gateway vs Lambda Function URL vs NLB

| Need | Pick |
|---|---|
| Containers/EC2 serving steady HTTP traffic | **ALB** |
| Serverless API on Lambda, per-request cost, built-in auth/throttling | **API Gateway (HTTP API)** |
| API keys/usage plans, request validation, response caching, private API | **API Gateway (REST API)** |
| Real-time bidirectional | **API Gateway WebSocket** (or ALB WebSocket for your own servers) |
| One Lambda, public HTTPS, no extras | **Lambda Function URL** |
| Non-HTTP (TCP/UDP), static IPs, extreme performance | **NLB** |
| Cache static/dynamic responses globally, WAF at the edge | **CloudFront** in front of any of the above |

Rule of thumb: **ALB for services you run, API Gateway for APIs you expose.** They combine well, e.g. API Gateway → VPC link → internal ALB → ECS, though add that complexity only when you need API Gateway's features.

---

## Common mistakes

- Health check **path/port wrong**, so targets are marked unhealthy and killed in a loop.
- **Target security group** doesn't allow the ALB's security group.
- ALB in **one AZ** only, or targets in AZs the LB isn't enabled for.
- Using **instance** target type for Fargate (it needs **IP**).
- App keep-alive timeout **shorter** than the ALB idle timeout, causing intermittent 502s.
- Leaving **HTTP:80** open without redirecting to HTTPS.
- Forgetting that REST APIs need a **redeployment**.
- Returning the wrong **response shape** from a proxy Lambda (502).
- Proxying large **file uploads** through API Gateway/Lambda instead of presigned S3 URLs.
- Expecting API Gateway to wait **more than ~29 s** for a long job.
- Treating **API keys** as authentication.
- Forgetting CORS headers on **error** responses (browser reports a CORS error that is really a 4xx/5xx).
- Paying for API Gateway at very high sustained volume without comparing against an ALB.

---

## Debugging

**Load balancer errors (ALB):**

| Code | Meaning | Check |
|---|---|---|
| **502 Bad Gateway** | Target sent an invalid response or closed the connection | App crashed, keep-alive mismatch, TLS mismatch to target, Lambda returned a malformed response |
| **503 Service Unavailable** | No healthy targets registered | Health check failing, target group empty, scaling at zero |
| **504 Gateway Timeout** | Target didn't respond within the idle timeout | Slow app, blocked security group/NACL path, timeout too low |
| **400/460/463** etc. | Client or header problems | Header size, malformed request, X-Forwarded-For limits |

Start with the target group's **Targets tab** (health state and reason code), then ALB **access logs** (to S3) and CloudWatch metrics (`HTTPCode_Target_5XX_Count`, `HTTPCode_ELB_5XX_Count`, `TargetResponseTime`, `UnHealthyHostCount`). `ELB_5XX` = the load balancer itself; `Target_5XX` = your app.

**API Gateway:**

| Symptom | Likely cause |
|---|---|
| `{"message":"Not Found"}` / `Missing Authentication Token` | Wrong path, method or **stage** in the URL; route doesn't exist; REST API not redeployed |
| `{"message":"Forbidden"}` / `Unauthorized` | Authorizer rejected the token; IAM/SigV4 problem; WAF or resource policy block |
| `{"message":"Internal server error"}` (502) | Lambda threw an error or returned a malformed proxy response. Check the **Lambda logs** |
| `504` / `Endpoint request timed out` | Integration took longer than the timeout |
| `429 Too Many Requests` | Throttling at the gateway or the account limit |
| Browser "CORS error" | Missing CORS config or headers on the actual (including error) response; preflight not handled |

Turn on **access logging** and (for REST) **execution logging** to CloudWatch to see what the gateway did, and test routes with `curl -i` to inspect headers. Distributed tracing with X-Ray is covered in [Observability](../06-operations/04-observability.md).

---

## Quick Summary

- **ALB**: HTTP(S) load balancer, routing by host/path, health-checked target groups (instance / **IP** / Lambda); terminate TLS with ACM; put targets in private subnets and allow only the ALB's security group.
- **NLB**: L4, static IPs, TCP/UDP, extreme performance; **GWLB** for appliances.
- Most ALB problems are **health checks, security groups, or timeouts** (`502` = bad response, `503` = no healthy targets, `504` = timeout).
- **API Gateway**: managed API front door. Default to **HTTP API**; use **REST API** for keys/usage plans, validation, caching, private APIs.
- Remember the **~29 s** integration timeout, payload limits, REST **redeployment**, and CORS on every response.
- Rule of thumb: **ALB for services you run, API Gateway for APIs you expose**; CloudFront in front of either when you want caching, WAF and edge TLS.

**Next:** [SQS](../05-app-services/01-sqs.md)