---
topic: "AWS Lambda & API Gateway Production Cheatsheet"
module: "10-cheatsheets"
track: "aws"
difficulty: "reference"
last-tested: "2026-11"
tags:
  - "cheatsheet"
  - "lambda"
  - "api-gateway"
  - "serverless"
  - "performance"
---

# AWS Lambda & API Gateway Production Cheatsheet

A high-density reference card for AWS Lambda runtime configurations, memory-to-CPU scaling, and Amazon API Gateway HTTP/REST patterns.

---

## 1. AWS Lambda Memory-to-vCPU Scaling Table

CPU allocation scales linearly with configured memory.

| Configured Memory (MB) | vCPU Power Equivalent | Primary Advantage | Typical Workloads |
|:---|:---|:---|:---|
| **128 MB** | ~0.07 vCPU | Lowest cost per ms | Simple SQS/SNS consumers, text logging |
| **512 MB** | ~0.29 vCPU | Balanced basic compute | Microservice CRUD APIs, lightweight Node.js/Python |
| **1,024 MB** | ~0.58 vCPU | Fast cold starts | High-traffic REST APIs, database queries |
| **1,769 MB** | **Exactly 1.0 Full vCPU** | First full single-thread CPU | Heavy JSON processing, file compression |
| **3,538 MB** | **Exactly 2.0 Full vCPUs**| Multi-threaded processing | Image processing, cryptographic hashing |
| **10,240 MB** (Max) | **6.0 Full vCPUs** | Maximum compute throughput | Video transcoding, large dataset ML inference |

*Key Optimization Takeaway*: Increasing memory from 512 MB to 1024 MB doubles CPU speed. If an invocation runs in half the time, **total billed cost is identical**, but user latency improves by 50%.

---

## 2. Parsing the CloudWatch Lambda `REPORT` Line

Every completed Lambda invocation writes a final structured `REPORT` line to CloudWatch Logs:

```text
REPORT RequestId: 7b819f3a-1234-5678-90ab-cdef12345678
Duration: 42.15 ms
Billed Duration: 43 ms
Memory Size: 1024 MB
Max Memory Used: 78 MB
Init Duration: 184.22 ms
```

- **`Duration`**: Actual execution runtime inside `exports.handler`.
- **`Billed Duration`**: Rounded up to nearest millisecond.
- **`Max Memory Used`**: Peak RAM consumed. (If close to `Memory Size`, increase allocation to prevent OOM crash).
- **`Init Duration`**: Present **only during Cold Starts**. Represents download, container launch, and runtime initialization time outside the handler.

---

## 3. Node.js 20 ESM Production Handler Template

```javascript
// index.mjs
import { DynamoDBClient } from "@aws-sdk/client-dynamodb";

// 1. REUSABLE INITIALIZATION (Outside handler: persists across warm containers)
const ddb = new DynamoDBClient({ region: process.env.AWS_REGION });

export const handler = async (event, context) => {
  // context properties
  const remainingMs = context.getRemainingTimeInMillis();
  const requestId = context.awsRequestId;

  try {
    const body = event.body ? JSON.parse(event.body) : {};

    return {
      statusCode: 200,
      headers: {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "https://app.enterprise.com",
        "Access-Control-Allow-Methods": "GET,POST,OPTIONS"
      },
      body: JSON.stringify({ status: "success", remainingMs, requestId })
    };
  } catch (err) {
    console.error("Execution error:", err);
    return {
      statusCode: 500,
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ error: "Internal Server Error" })
    };
  }
};
```

---

## 4. API Gateway HTTP APIs (v2) Payload Format 2.0 Reference

```json
{
  "version": "2.0",
  "routeKey": "POST /orders/{id}",
  "rawPath": "/orders/ord-9921",
  "rawQueryString": "priority=high",
  "headers": {
    "authorization": "Bearer eyJhbGciOi...",
    "content-type": "application/json",
    "x-forwarded-for": "203.0.113.195"
  },
  "requestContext": {
    "accountId": "123456789012",
    "apiId": "r3pm15abcde",
    "http": {
      "method": "POST",
      "path": "/orders/ord-9921",
      "protocol": "HTTP/1.1",
      "sourceIp": "203.0.113.195"
    },
    "authorizer": {
      "jwt": {
        "claims": { "sub": "user_12345", "email": "dev@enterprise.com" }
      }
    }
  },
  "pathParameters": { "id": "ord-9921" },
  "body": "{\"quantity\": 2}",
  "isBase64Encoded": false
}
```

---

## 5. API Gateway Common Error Codes & Diagnostics

| Status Code | Error Message / Cause | How to Fix |
|:---|:---|:---|
| **401 Unauthorized** | Missing or invalid Bearer JWT token | Verify Cognito User Pool ID / client ID and token expiration. |
| **403 Forbidden** | `Missing Authentication Token` | **Wrong URL path or HTTP method**! (Does not match route key). |
| **429 Too Many Requests** | Concurrency / Rate Limit Exceeded | Account Lambda concurrency cap or API Gateway stage rate limit hit. |
| **502 Bad Gateway** | Malformed Lambda proxy response | Lambda returned invalid JSON structure (must include `statusCode` and `body`). |
| **504 Gateway Timeout** | Backend integration timeout | Lambda execution exceeded **29 seconds** (Hard API Gateway timeout cap). |
