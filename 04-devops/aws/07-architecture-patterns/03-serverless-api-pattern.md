---
topic: "Serverless API Architecture Pattern"
module: "07-architecture-patterns"
track: "aws"
last-tested: "2026-11"
prerequisites:
  - "02-compute/02-lambda.md"
  - "04-networking/08-api-gateway.md"
  - "03-storage-and-databases/02-dynamodb.md"
complexity: "advanced"
tags:
  - "serverless"
  - "api-gateway"
  - "lambda"
  - "dynamodb"
  - "cognito"
  - "architecture"
---

# Serverless API Architecture Pattern

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

The Serverless API architecture pattern is the industry standard for cloud-native web services and microservices that require zero server management, sub-second auto-scaling from zero to tens of thousands of requests per second, and a pay-per-execution pricing model. By composing **Amazon API Gateway** (REST or HTTP APIs), **AWS Lambda** compute, and **Amazon DynamoDB** (managed NoSQL), teams can build resilient, horizontally scalable REST and GraphQL endpoints without maintaining operating systems, runtime patching, capacity planning, or idle compute instances.

### Why Serverless APIs?
- **Zero Idle Capacity Cost**: When incoming traffic drops to zero, compute billing is zero dollars. Organizations avoid paying for over-provisioned EC2 fleets during off-peak hours.
- **Instant Elastic Scalability**: Lambda instances scale out concurrently within milliseconds to match traffic spikes, with automated regional redundancy across three Availability Zones.
- **Built-in Resiliency & High Availability**: The triad of Route 53, CloudFront/API Gateway, Lambda, and DynamoDB inherits multi-AZ fault tolerance with native 99.99% to 99.999% SLA availability.
- **Developer Velocity**: Engineers write business logic handlers without building custom load balancing, SSL termination, or process supervisor daemons.

---

## 2. Core Concepts

### Core Components of the Pattern
1. **Client & Edge Routing**: Clients connect over TLS 1.3 to Amazon CloudFront or API Gateway regional/edge endpoints with Route 53 DNS routing.
2. **API Management Layer**:
   - **HTTP APIs**: Ultra-low latency (<10 ms overhead), cost-effective ($1.00/million requests), native JWT validation for Cognito/OIDC.
   - **REST APIs**: Full feature set including request/response validation, API keys, usage plans, WAF integration, and request transformation templates.
3. **Authentication & Authorization**:
   - **Amazon Cognito User Pools**: Issues RS256-signed JWT ID and Access Tokens verified at the API Gateway edge before invoking compute.
   - **Lambda Authorizers**: Custom token or request parameter validation for external identity providers or fine-grained RBAC.
4. **Execution Compute (AWS Lambda)**:
   - **Single-Purpose Handlers (Micro-functions)**: One Lambda function per route/method (`getUser`, `createOrder`). Provides least-privilege IAM isolation and independent scaling.
   - **Monolithic Lambda (Fat Lambda / Web Adapter)**: Single container or Express/FastAPI handler behind a proxy route (`/{proxy+}`). Simplifies deployment and local testing at the expense of bundle size and broader IAM blast radius.
5. **Data Persistence**:
   - **Amazon DynamoDB**: Single-table design with sub-10ms read/write latencies, on-demand capacity, and Global Secondary Indexes (GSIs).
   - **Amazon ElastiCache / MemoryDB**: Redis caching layer for read-heavy payloads with high cache hit ratios.
6. **Observability & Tracing**:
   - **AWS X-Ray**: Distributed request tracing across API Gateway, Lambda execution, and downstream DynamoDB calls.
   - **Amazon CloudWatch Logs & Embedded Metric Format (EMF)**: Structured JSON logging and high-resolution operational metrics.

---

## 3. How It Works Internally

```
+----------------------------------------------------------------------------------------------------+
|                               SERVERLESS API ARCHITECTURE OVERVIEW                                 |
+----------------------------------------------------------------------------------------------------+

  [ Mobile / Web Client ]
            |
            | HTTPS / TLS 1.3 (Route 53 DNS)
            v
  +--------------------------------------------------------------------------------------------------+
  | Amazon CloudFront Edge & AWS WAF (Rate Limiting, Bot Control, OWASP Top 10)                      |
  +--------------------------------------------------------------------------------------------------+
            |
            v
  +--------------------------------------------------------------------------------------------------+
  | Amazon API Gateway (HTTP API / REST API)                                                         |
  |  - TLS Termination & Request Validation                                                          |
  |  - Built-in JWT Authorizer (Cognito User Pools / OAuth2 OIDC)                                    |
  |  - Throttling (Token Bucket: 10,000 req/sec limit, 5,000 burst)                                  |
  +--------------------------------------------------------------------------------------------------+
            |                                         |
     (Direct Mock/Integration)                 (IAM / AssumeRole)
            |                                         v
            |                      +---------------------------------------+
            |                      | AWS Lambda Authorizer (Custom RBAC)   |
            |                      +---------------------------------------+
            v
  +--------------------------------------------------------------------------------------------------+
  | AWS Lambda Execution Fleet (VPC-enabled or Non-VPC)                                              |
  |                                                                                                  |
  |   +-----------------------+   +-----------------------+   +-----------------------+              |
  |   | GET /orders           |   | POST /orders          |   | DELETE /orders/{id}   |              |
  |   | Lambda Handler        |   | Lambda Handler        |   | Lambda Handler        |              |
  |   | Provisioned Concurrency|  | Cold Start Optimized  |   | Least Privilege IAM   |              |
  |   +-----------------------+   +-----------------------+   +-----------------------+              |
  +--------------------------------------------------------------------------------------------------+
            |                                         |
            | AWS SDK v3 Client (HTTP Keep-Alive)     | Direct Batch / Streams
            v                                         v
  +---------------------------------------+  +-------------------------------------------------------+
  | Amazon DynamoDB (Single-Table Design) |  | DynamoDB Streams -> Async Worker Lambda (EventBridge) |
  |   PK: USER#<id> | SK: ORDER#<id>      |  +-------------------------------------------------------+
  |   GSI1PK: STATUS | GSI1SK: TIMESTAMP  |
  +---------------------------------------+
```

### Execution Lifecycle of a Request
1. **Network Ingress & WAF Inspection**: The client initiates an HTTPS connection. CloudFront and AWS WAF inspect IP reputation, SQL injection, and rate limits.
2. **Authentication Gate**: API Gateway validates the incoming Bearer token signature against Cognito's JWKS public keys. If invalid or expired, API Gateway immediately terminates the request with a `401 Unauthorized` without invoking Lambda.
3. **Lambda Worker Allocation**:
   - If a warm worker is available, the runtime delivers the JSON event directly to the function handler.
   - If no warm worker exists, Lambda allocates an execution sandbox (MicroVM via Firecracker), downloads the deployment zip or container layer, initializes the runtime, and executes top-level initialization code (cold start phase).
4. **Data Access**: The handler processes the request, performs business validations, and reads/writes to DynamoDB using persistent HTTP keep-alive connections.
5. **Response Delivery**: Lambda returns a JSON payload (`statusCode`, `headers`, `body`) to API Gateway, which serializes the HTTP response back to the client.

---

## 4. Syntax & Usage

### 4.1 Terraform Implementation: HTTP API + Lambda + DynamoDB

```hcl
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.40"
    }
  }
}

provider "aws" {
  region = "us-east-1"
}

# DynamoDB Single Table
resource "aws_dynamodb_table" "orders" {
  name         = "production-orders-table"
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "PK"
  range_key    = "SK"

  attribute {
    name = "PK"
    type = "S"
  }

  attribute {
    name = "SK"
    type = "S"
  }

  point_in_time_recovery {
    enabled = true
  }

  server_side_encryption {
    enabled = true
  }
}

# IAM Execution Role for Lambda
resource "aws_iam_role" "lambda_exec" {
  name = "serverless-order-api-lambda-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "lambda.amazonaws.com" }
    }]
  })
}

resource "aws_iam_policy" "lambda_dynamodb" {
  name = "serverless-order-api-dynamodb-policy"

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "dynamodb:GetItem",
          "dynamodb:PutItem",
          "dynamodb:UpdateItem",
          "dynamodb:DeleteItem",
          "dynamodb:Query"
        ]
        Resource = [
          aws_dynamodb_table.orders.arn,
          "${aws_dynamodb_table.orders.arn}/index/*"
        ]
      },
      {
        Effect = "Allow"
        Action = [
          "logs:CreateLogGroup",
          "logs:CreateLogStream",
          "logs:PutLogEvents"
        ]
        Resource = "arn:aws:logs:*:*:*"
      }
    ]
  })
}

resource "aws_iam_role_policy_attachment" "lambda_attach" {
  role       = aws_iam_role.lambda_exec.name
  policy_arn = aws_iam_policy.lambda_dynamodb.arn
}

# Lambda Function
resource "aws_lambda_function" "api_handler" {
  filename         = "handler.zip"
  function_name    = "orders-api-handler"
  role             = aws_iam_role.lambda_exec.arn
  handler          = "index.handler"
  runtime          = "nodejs20.x"
  architectures    = ["arm64"]
  memory_size      = 1024
  timeout          = 10

  environment {
    variables = {
      TABLE_NAME = aws_dynamodb_table.orders.name
      NODE_OPTIONS = "--enable-source-maps"
    }
  }

  tracing_config {
    mode = "Active"
  }
}

# API Gateway HTTP API
resource "aws_apigatewayv2_api" "http_api" {
  name          = "orders-http-api"
  protocol_type = "HTTP"

  cors_configuration {
    allow_origins = ["https://app.example.com"]
    allow_methods = ["GET", "POST", "PUT", "DELETE", "OPTIONS"]
    allow_headers = ["Authorization", "Content-Type", "X-Api-Key"]
    max_age       = 3600
  }
}

resource "aws_apigatewayv2_stage" "prod" {
  api_id      = aws_apigatewayv2_api.http_api.id
  name        = "$default"
  auto_deploy = true

  access_log_settings {
    destination_arn = aws_cloudwatch_log_group.api_gw_logs.arn
    format = jsonencode({
      requestId   = "$context.requestId"
      ip          = "$context.identity.sourceIp"
      requestTime = "$context.requestTime"
      httpMethod  = "$context.httpMethod"
      routeKey    = "$context.routeKey"
      status      = "$context.status"
      protocol    = "$context.protocol"
      latency     = "$context.responseLatency"
    })
  }
}

resource "aws_cloudwatch_log_group" "api_gw_logs" {
  name              = "/aws/apigateway/orders-http-api"
  retention_in_days = 30
}

# Integration & Route
resource "aws_apigatewayv2_integration" "lambda_integration" {
  api_id                 = aws_apigatewayv2_api.http_api.id
  integration_type       = "AWS_PROXY"
  integration_uri        = aws_lambda_function.api_handler.invoke_arn
  payload_format_version = "2.0"
}

resource "aws_apigatewayv2_route" "orders_route" {
  api_id    = aws_apigatewayv2_api.http_api.id
  route_key = "ANY /orders/{proxy+}"
  target    = "integrations/${aws_apigatewayv2_integration.lambda_integration.id}"
}

resource "aws_lambda_permission" "apigw_invoke" {
  statement_id  = "AllowAPIGatewayInvoke"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.api_handler.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.http_api.execution_arn}/*/*"
}
```

---

## 5. Practical Examples

### Example 1: Node.js 20 Lambda Handler with AWS SDK v3 Keep-Alive & DynamoDB

```javascript
// index.mjs
import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import { DynamoDBDocumentClient, GetCommand, PutCommand } from "@aws-sdk/lib-dynamodb";

// Reusable client across invocations (outside handler to leverage connection reuse)
const ddbRawClient = new DynamoDBClient({
  region: process.env.AWS_REGION || "us-east-1",
  maxAttempts: 3,
});

const ddb = DynamoDBDocumentClient.from(ddbRawClient, {
  marshallOptions: { removeUndefinedValues: true },
});

const TABLE_NAME = process.env.TABLE_NAME;

export const handler = async (event) => {
  const { httpMethod, path, body, pathParameters } = event.requestContext?.http || event;

  try {
    if (httpMethod === "POST" && event.rawPath === "/orders") {
      const payload = JSON.parse(event.body || "{}");
      if (!payload.orderId || !payload.amount) {
        return buildResponse(400, { error: "Missing required fields: orderId, amount" });
      }

      const item = {
        PK: `ORDER#${payload.orderId}`,
        SK: `METADATA#${payload.orderId}`,
        amount: payload.amount,
        customerId: payload.customerId,
        createdAt: new Date().toISOString(),
      };

      await ddb.send(new PutCommand({ TableName: TABLE_NAME, Item: item }));
      return buildResponse(201, { message: "Order created successfully", order: item });
    }

    if (httpMethod === "GET" && event.rawPath.startsWith("/orders/")) {
      const orderId = event.pathParameters?.proxy || event.rawPath.split("/")[2];
      const result = await ddb.send(new GetCommand({
        TableName: TABLE_NAME,
        Key: { PK: `ORDER#${orderId}`, SK: `METADATA#${orderId}` },
      }));

      if (!result.Item) {
        return buildResponse(404, { error: "Order not found" });
      }

      return buildResponse(200, result.Item);
    }

    return buildResponse(404, { error: `Route not supported: ${httpMethod} ${event.rawPath}` });
  } catch (error) {
    console.error("Handler error:", error);
    return buildResponse(500, { error: "Internal Server Error", requestId: event.requestContext?.requestId });
  }
};

function buildResponse(statusCode, data) {
  return {
    statusCode,
    headers: {
      "Content-Type": "application/json",
      "Access-Control-Allow-Origin": "*",
    },
    body: JSON.stringify(data),
  };
}
```

### Example 2: Python (FastAPI + Mangum Adapter) for Fat-Lambda Architectures

```python
# app.py
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from mangum import Mangum
import boto3
import os

app = FastAPI(title="Serverless FastAPI Order Service")
dynamodb = boto3.resource("dynamodb")
table = dynamodb.Table(os.environ.get("TABLE_NAME", "production-orders-table"))

class OrderCreate(BaseModel):
    order_id: str
    customer_id: str
    amount: float

@app.post("/orders", status_code=201)
def create_order(order: OrderCreate):
    item = {
        "PK": f"ORDER#{order.order_id}",
        "SK": f"METADATA#{order.order_id}",
        "customer_id": order.customer_id,
        "amount": str(order.amount),
    }
    table.put_item(Item=item)
    return {"message": "Created", "order": item}

@app.get("/orders/{order_id}")
def get_order(order_id: str):
    response = table.get_item(Key={"PK": f"ORDER#{order_id}", "SK": f"METADATA#{order_id}"})
    if "Item" not in response:
        raise HTTPException(status_code=404, detail="Order not found")
    return response["Item"]

# Mangum wraps ASGI application for API Gateway Lambda Proxy integration
handler = Mangum(app, lifespan="off")
```

### Example 3: Configuring Cognito JWT Authorizer on API Gateway HTTP API via AWS CLI

```bash
# 1. Create the JWT Authorizer targeting an existing Cognito User Pool
aws apigatewayv2 create-authorizer \
    --api-id "api-12345abcde" \
    --authorizer-type JWT \
    --identity-source '$request.header.Authorization' \
    --name "CognitoUserPoolAuthorizer" \
    --jwt-configuration \
      Issuer="https://cognito-idp.us-east-1.amazonaws.com/us-east-1_ExamplePoolId",\
      Audience="1234567890abcdefghijklmnop"

# 2. Attach Authorizer to the secured route
aws apigatewayv2 update-route \
    --api-id "api-12345abcde" \
    --route-id "route-98765" \
    --authorization-type JWT \
    --authorizer-id "auth-554433"
```

### Example 4: Provisioned Concurrency with Auto Scaling to Eliminate Cold Starts

```hcl
# Lambda Provisioned Concurrency Configuration
resource "aws_lambda_provisioned_concurrency_config" "api_concurrency" {
  function_name                     = aws_lambda_function.api_handler.function_name
  provisioned_concurrent_executions = 20
  qualifier                         = aws_lambda_alias.live.name
}

resource "aws_lambda_alias" "live" {
  name             = "live"
  description      = "Production alias with provisioned concurrency"
  function_name    = aws_lambda_function.api_handler.function_name
  function_version = aws_lambda_function.api_handler.version
}

# Auto Scaling Target for Provisioned Concurrency
resource "aws_appautoscaling_target" "lambda_target" {
  max_capacity       = 100
  min_capacity       = 10
  resource_id        = "function:${aws_lambda_function.api_handler.function_name}:${aws_lambda_alias.live.name}"
  scalable_dimension = "lambda:function:ProvisionedConcurrency"
  service_namespace  = "lambda"
}

resource "aws_appautoscaling_policy" "lambda_policy" {
  name               = "LambdaProvisionedConcurrencyUtilization"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.lambda_target.resource_id
  scalable_dimension = aws_appautoscaling_target.lambda_target.scalable_dimension
  service_namespace  = aws_appautoscaling_target.lambda_target.service_namespace

  target_tracking_scaling_policy_configuration {
    target_value       = 0.75 # Scale out when concurrency utilization hits 75%
    predefined_metric_specification {
      predefined_metric_type = "LambdaProvisionedConcurrencyUtilization"
    }
  }
}
```

### Example 5: Amazon CloudWatch Embedded Metric Format (EMF) for High-Speed Metrics

```javascript
// High-performance operational logging without blocking Lambda execution latency
import { createMetricsLogger, Unit } from "aws-embedded-metrics";

export const handler = async (event) => {
  const metrics = createMetricsLogger();
  metrics.setNamespace("ECommerce/ServerlessAPI");
  metrics.setDimensions({ Service: "OrderProcessing", Stage: "Production" });

  const startTime = Date.now();
  try {
    // Process business transaction
    metrics.putMetric("OrdersReceived", 1, Unit.Count);
    
    // Perform processing...
    const executionDuration = Date.now() - startTime;
    metrics.putMetric("OrderLatency", executionDuration, Unit.Milliseconds);

    await metrics.flush(); // Automatically outputs JSON adhering to EMF schema
    return { statusCode: 200, body: JSON.stringify({ status: "success" }) };
  } catch (err) {
    metrics.putMetric("OrderFailures", 1, Unit.Count);
    await metrics.flush();
    throw err;
  }
};
```

---

## 6. Best Practices

1. **Use HTTP APIs Over REST APIs When Advanced Features Are Not Required**: HTTP APIs offer 60% lower cost ($1.00/M vs $3.50/M) and roughly 10-15 ms lower latency overhead.
2. **Initialize AWS SDK and Database Clients Outside Handler Scope**: Open database connection pools and SDK clients globally so they persist across warm container invocations.
3. **Adopt ARM64 (Graviton2/Graviton3) Execution Runtimes**: ARM64 provides up to 34% better price-performance compared to x86_64 for identical compute workloads.
4. **Tune Lambda Memory Allocation to Optimize vCPU and Execution Time**: Memory directly scales allocated CPU. Increasing memory from 512 MB to 1024 MB may cut runtime in half, resulting in identical or lower billing costs.
5. **Implement Strict Request Validation at API Gateway**: Reject malformed payloads and missing headers at the API Gateway level to avoid paying for downstream Lambda execution invocations.
6. **Enable DynamoDB On-Demand Billing and Point-in-Time Recovery (PITR)**: Accommodate unpredictable traffic spikes without capacity throttling, while preserving continuous backups.

---

## 7. Common Mistakes

| Mistake | Why It Hurts | Correct Approach |
|:---|:---|:---|
| **Opening DB connection inside handler** | Re-authenticates and creates new TCP sockets on every single request, causing latency spikes and DB exhaustion. | Initialize database connections globally outside `exports.handler`. |
| **Using Relational DB (RDS) without RDS Proxy** | Rapid Lambda horizontal auto-scaling opens thousands of connections, crashing PostgreSQL/MySQL connection pools. | Deploy AWS RDS Proxy between Lambda and RDS, or use Amazon DynamoDB. |
| **Placing Lambda inside VPC unnecessarily** | Adds unnecessary ENI management and complexity when talking only to AWS native public services (DynamoDB, SQS). | Keep Lambda outside VPC unless accessing private VPC resources (RDS, Redis, internal ALBs). |
| **Single Massive "Fat" Lambda with all dependencies** | Large deployment package size (>50MB) drastically increases cold start download times. | Tree-shake bundles using esbuild/webpack; keep package size under 10MB or use container images with base layer caching. |
| **Granting `*` permissions on DynamoDB** | Breaches least-privilege security; a vulnerable route can dump or delete the entire enterprise database table. | Restrict IAM policy actions to specific commands (`GetItem`, `PutItem`) and specific resource ARNs. |
| **Ignoring API Gateway Throttling / Rate Limits** | Sudden denial-of-service or bug in client app can exhaust account-level regional Lambda concurrency limits. | Configure route-level throttling and token bucket limits in API Gateway stages. |

---

## 8. Performance, Security & Cost Notes

### Performance Optimization
- **Cold Start Mitigation**: Use lightweight runtimes (Node.js, Python, Rust, Go). Avoid heavy enterprise Java or .NET frameworks unless using Provisioned Concurrency or SnapStart.
- **Payload Compression**: Enable gzip/brotli compression on API Gateway responses exceeding 1 KB.

### Security Controls
- **Zero Static Credentials**: Never store access keys or database passwords in environment variables. Use IAM Execution Roles and AWS Secrets Manager with client-side caching.
- **OWASP Protection**: Attach AWS WAF to API Gateway REST APIs or CloudFront distributions in front of HTTP APIs.

### Cost Analysis
- **API Gateway HTTP API**: \$1.00 per 1 million requests.
- **AWS Lambda (ARM64)**: \$0.0000133334 per GB-second + \$0.20 per 1M requests.
- **DynamoDB (On-Demand)**: \$0.25 per 1M write units, \$0.05 per 1M read units.
- **10 Million Requests/Month Baseline Cost**: ~\$10 (APIGW) + ~\$3 (Lambda 512MB 100ms) + ~\$2.50 (DynamoDB) = **~\$15.50/month total**.

---

## 9. Comparison Table

| Architecture Pattern | Scalability | Latency (P99) | Maintenance Overhead | Cost at Zero Traffic | Best For |
|:---|:---|:---|:---|:---|:---|
| **Serverless API (APIGW + Lambda + DDB)** | Instant Elastic (0 to 10k+ req/s) | 15ms - 150ms (cold starts up to 1s) | Minimal (No OS or cluster management) | **\$0.00** | Microservices, SaaS APIs, dynamic workloads, rapid MVPs |
| **Containerized API (ECS Fargate + ALB)** | Moderate (Minutes to scale tasks) | 5ms - 25ms (Consistent, no cold starts) | Low (Docker containers, ALB configuration) | Baseline task costs (~$30/mo min) | Predictable high-traffic APIs, long-lived WebSockets, legacy apps |
| **Kubernetes (Amazon EKS)** | High (Node & Pod auto-scalers) | 2ms - 15ms (Ultra-consistent) | High (Control plane, ingress, operators) | High (~$73/mo control plane + nodes) | Complex enterprise multi-team platforms, hybrid cloud workloads |

---

## 10. Interview Questions

<details>
<summary><strong>1. How do you mitigate and diagnose cold starts in an AWS Lambda-based serverless API?</strong></summary>

Cold starts occur when Lambda initializes a new execution environment (downloading code, launching Firecracker MicroVM, running runtime/handler init code). To diagnose, examine AWS X-Ray traces or CloudWatch Logs `REPORT` lines, where `Init Duration` represents cold start latency. Mitigation techniques include:
1. **Provisioned Concurrency**: Keeps pre-warmed execution environments ready to respond instantly with zero init latency.
2. **Lambda SnapStart**: (For Java runtimes) takes a Firecracker snapshot of initialized state and restores it in under 200 ms.
3. **Minimize Package Size**: Use bundlers (esbuild, rollup) to tree-shake unused modules and dependencies.
4. **Choose Fast Runtimes**: Node.js, Python, Go, and Rust initialize significantly faster than JVM or CLR runtimes.
</details>

<details>
<summary><strong>2. When should you choose API Gateway HTTP APIs over REST APIs?</strong></summary>

Choose **HTTP APIs** when building low-latency, cost-sensitive services that require native OIDC/OAuth2/JWT authorizers, CORS support, and basic path routing. HTTP APIs offer up to 60% cost savings and lower latency overhead. Choose **REST APIs** when you need advanced capabilities such as request/response payload validation via JSON Schema, API Keys with usage plans, request transformation via VTL, AWS WAF integration directly on the API Gateway resource, or private VPC endpoint publishing.
</details>

<details>
<summary><strong>3. How does Lambda handle connection pooling when connecting to relational databases like Amazon RDS?</strong></summary>

Unlike long-running containers that hold persistent pools of 10-50 connections, each Lambda execution instance scales horizontally independently. A sudden spike of 1,000 concurrent requests creates 1,000 Lambda containers, opening 1,000 simultaneous connections and potentially crashing PostgreSQL or MySQL. The solution is **AWS RDS Proxy**: a managed, highly available database proxy that pools and multiplexes connections from Lambda, pinning connections only during active transaction execution and reducing database connection overhead by over 90%.
</details>

<details>
<summary><strong>4. What is the difference between a Single-Purpose Lambda architecture and a Fat/Monolithic Lambda architecture?</strong></summary>

- **Single-Purpose Lambda**: Every route/action (`GET /orders`, `POST /orders`) maps to an individual Lambda function. Benefits include strict least-privilege IAM roles per endpoint, independent scaling/timeout configurations, smaller bundle sizes, and isolated failure domains. Downsides include managing hundreds of Lambda resources and deployment overhead.
- **Fat/Monolithic Lambda**: An entire web framework (Express, FastAPI) runs inside a single Lambda function handling `/{proxy+}`. Benefits include fast local development and simple routing. Downsides include broader IAM permissions, larger deployment artifacts leading to longer cold starts, and inability to tune memory/timeouts per individual route.
</details>

<details>
<summary><strong>5. How do you implement idempotent request handling in a serverless POST /checkout API?</strong></summary>

Implement an idempotency layer using an **Idempotency Key** sent by the client in an HTTP header (e.g., `X-Idempotency-Key: uuid`). The Lambda handler attempts to write this key into a DynamoDB table with a conditional expression `attribute_not_exists(IdempotencyKey)` and a TTL of 24 hours:
- If the write succeeds, the handler processes the payment and updates the record with the response payload.
- If the write fails with `ConditionalCheckFailedException`, another request with the same key is in-flight or completed. The handler reads the cached response from DynamoDB and returns it immediately without re-executing payment processing.
</details>

<details>
<summary><strong>6. How can an API Gateway prevent downstream service exhaustion during unexpected traffic spikes?</strong></summary>

1. **Throttling Limits**: Configure token-bucket rate limits (steady-state requests per second) and burst limits at the stage, route, or client API key level.
2. **Usage Plans**: Enforce daily or monthly quotas on specific consumer tiers.
3. **Lambda Reserved Concurrency**: Cap the maximum number of concurrent executions allocated to the backing function to prevent overwhelming backend databases or third-party APIs. Excess requests return HTTP 429 Too Many Requests.
</details>

<details>
<summary><strong>7. How do you secure a serverless API without putting Lambda inside a VPC?</strong></summary>

AWS Lambda functions communicate with AWS native services (DynamoDB, S3, Secrets Manager) over secure AWS backbone networks via TLS 1.3 endpoints authenticated by IAM SigV4. Placing Lambda outside a VPC is secure because:
- Incoming invocations require IAM authentication from API Gateway.
- Outgoing AWS SDK calls are signed using ephemeral AWS STS tokens from the Lambda execution role.
- All network transit is encrypted. VPC placement is only required when Lambda needs to reach private IP resources (such as RDS or ElastiCache inside private subnets).
</details>

<details>
<summary><strong>8. What is the impact of Lambda memory configuration on CPU performance and execution cost?</strong></summary>

Lambda allocates CPU proportionally to configured memory. At 1,769 MB of RAM, Lambda provisions exactly 1 full vCPU; at 3,538 MB, it provisions 2 vCPUs. Increasing memory for compute-intensive tasks (e.g., image processing or cryptographic hashing) often cuts execution time proportionally. Since billing is calculated as `Memory (GB) * Duration (Seconds)`, a function running twice as fast at double the memory costs the exact same amount while delivering a 50% lower latency to the end user.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Challenge 1: Build a CRUD API using Terraform and HTTP API</strong></summary>

**Goal**: Provision an HTTP API Gateway with two routes (`GET /items` and `POST /items`) backed by a single Lambda function and a DynamoDB table using on-demand billing.
**Verification**: Use `curl -X POST https://<api-id>.execute-api.us-east-1.amazonaws.com/items` with a JSON payload, followed by a `GET` request to verify persistence.
</details>

<details>
<summary><strong>Challenge 2: Implement JWT Token Validation with Cognito</strong></summary>

**Goal**: Create an Amazon Cognito User Pool, register an app client, and attach a native JWT authorizer to an API Gateway route.
**Verification**: Verify that requests lacking a valid `Authorization: Bearer <jwt>` header are rejected by API Gateway with a `401 Unauthorized` without invoking the Lambda handler.
</details>

<details>
<summary><strong>Challenge 3: Benchmark Cold Starts vs Warm Invocations</strong></summary>

**Goal**: Deploy a Python or Node.js Lambda function with 256 MB and 1024 MB memory settings. Use Apache Bench (`ab`) or Artillery to execute 1,000 requests with varying concurrency.
**Verification**: Inspect CloudWatch Logs to quantify `Init Duration` versus standard `Duration` across memory tiers.
</details>

<details>
<summary><strong>Challenge 4: Implement Idempotency Guard with DynamoDB Conditional Writes</strong></summary>

**Goal**: Write a Node.js Lambda handler that checks an `X-Idempotency-Key` header and performs a conditional `PutItem` to ensure payments cannot be charged twice.
**Verification**: Submit identical concurrent requests with the same key; confirm exactly one executes and the other returns the cached result.
</details>

<details>
<summary><strong>Challenge 5: Configure Rate Limiting and WAF Bot Control</strong></summary>

**Goal**: Deploy an AWS WAF WebACL with a rate-based rule limiting requests to 100 per 5 minutes per IP. Associate the WebACL with API Gateway.
**Verification**: Run a load test exceeding the threshold and confirm HTTP 403 Forbidden responses are generated by the WAF.
</details>

<details>
<summary><strong>Challenge 6: Set Up Distributed Tracing with AWS X-Ray</strong></summary>

**Goal**: Enable active tracing on API Gateway and Lambda. Instrument the AWS SDK client with `@aws-sdk/xray-sdk`.
**Verification**: Open the AWS X-Ray Service Map in the CloudWatch console to inspect the end-to-end trace timeline from client ingress to DynamoDB execution.
</details>

---

## 12. Summary / Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
|                               SERVERLESS API DESIGN CHEAT SHEET                                    |
+----------------------------------------------------------------------------------------------------+
| Decision Dimension      | Recommendation                                                           |
+-------------------------+--------------------------------------------------------------------------+
| API Protocol            | HTTP APIs for 90% of microservices; REST APIs for WAF, JSON Schema, Keys|
| Compute Runtime         | Node.js / Python on ARM64 architecture (Graviton)                        |
| Database Layer          | Amazon DynamoDB (On-Demand, Single-Table Design)                         |
| Relational DB Access    | AWS RDS Proxy mandatory to prevent connection pool exhaustion            |
| Auth Validation         | Built-in HTTP API JWT Authorizer (Cognito User Pools / Auth0)            |
| Observability           | AWS X-Ray tracing enabled + CloudWatch Embedded Metric Format (EMF)      |
| Cold Start Defense      | Minimized package size, tree-shaking, Provisioned Concurrency for P99 SLA|
+-------------------------+--------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Serverless Applications Lens - AWS Well-Architected Framework](https://docs.aws.amazon.com/wellarchitected/latest/serverless-applications-lens/)
- [Amazon API Gateway HTTP APIs Documentation](https://docs.aws.amazon.com/apigateway/latest/developerguide/http-api.html)
- [AWS Lambda Execution Environment Lifecycle & Cold Starts](https://docs.aws.amazon.com/lambda/latest/dg/runtimes-context.html)
- [Single-Table Design with Amazon DynamoDB by Alex DeBrie](https://www.alexdebrie.com/posts/dynamodb-single-table-design/)
- [AWS RDS Proxy Best Practices with AWS Lambda](https://aws.amazon.com/blogs/compute/using-amazon-rds-proxy-with-aws-lambda/)
