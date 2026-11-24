# Amazon API Gateway & Serverless API Architecture

> **Module**: `04-networking`  
> **Topic**: REST APIs, HTTP APIs, WebSocket APIs, Authentication, Throttling & Private Integrations  
> **Target Audience**: Cloud Engineers, Backend Architects, Serverless Developers  
> **Last Updated**: November 2026

---

## Navigation & Contents

- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Building and managing enterprise-grade APIs requires much more than simply running an Express, FastAPI, or Spring Boot application on a server. Production APIs require robust rate limiting and throttling to prevent denial-of-service, request validation to reject malformed payloads, API key metering for monetization, CORS handling, client SDK generation, and standardized authentication mechanisms (OAuth 2.0, JWT, IAM).

**Amazon API Gateway** is a fully managed service that makes it easy for developers to create, publish, maintain, monitor, and secure APIs at any scale. Acting as the "front door" for backend applications, API Gateway:
1. **Handles Serverless Ingestion**: Seamlessly invokes AWS Lambda functions, AWS services (DynamoDB, SQS, Step Functions) directly without intermediate compute, or HTTP endpoints in private VPCs.
2. **Enforces Edge Security & Governance**: Manages authentication via Amazon Cognito, Lambda authorizers, or AWS IAM, paired with fine-grained throttling limits.
3. **Supports Diverse Protocols**: Offers high-performance **HTTP APIs** for low-latency RESTful services, full-featured **REST APIs** for enterprise monetization and transformation, and **WebSocket APIs** for real-time bidirectional messaging.

---

## 2. Core Concepts

### 2.1 API Gateway Flavors
1. **HTTP APIs (v2)**:
   - Designed for low-latency, cost-effective RESTful APIs.
   - Up to 70% cheaper and significantly faster than legacy REST APIs.
   - Native support for OIDC/OAuth 2.0 JWT authorizers, VPC links for private ALB/NLB integration, and automatic CORS configuration.
2. **REST APIs (v1)**:
   - Feature-rich enterprise API management.
   - Supports request/response Velocity Template Language (VTL) data transformations, API keys, usage plans, request validation, client SDK generation, response caching, and AWS WAF integration.
   - Endpoint types: **Edge-Optimized** (routed via CloudFront), **Regional** (resides in the local region), or **Private** (accessible only within VPCs via VPC Endpoints).
3. **WebSocket APIs**:
   - Maintains persistent, stateful bidirectional TCP connections between clients and API Gateway.
   - Uses route selection expressions (`$connect`, `$disconnect`, `$default`, and custom message routes) to invoke backend Lambda functions or services.

### 2.2 Stages, Deployments & Stage Variables
- **Deployments**: APIs are immutable snapshots of the resource tree. To make changes live, you must deploy the API to a Stage.
- **Stages**: Environments such as `dev`, `staging`, `prod`. Each stage has its own caching, throttling, logging, and metrics configuration.
- **Stage Variables**: Key-value pairs acting as environment variables inside API Gateway configurations (e.g., dynamically directing traffic to different Lambda aliases or backend URLs based on the stage).
- **Canary Deployments**: Route a percentage of stage traffic to a newly deployed revision to verify stability before 100% rollout.

### 2.3 Authentication & Authorization
- **AWS IAM Authorization**: Requests are signed using AWS Signature Version 4 (SigV4). Ideal for server-to-server or internal cross-account communication.
- **Amazon Cognito User Pools**: Validates OpenID Connect / OAuth 2.0 ID and Access tokens natively without invoking custom code.
- **Lambda Authorizers (Custom Authorizers)**:
  - *Token-based*: Validates a bearer token string.
  - *Request-parameter-based*: Evaluates headers, query strings, and request context, returning an IAM policy allowing or denying execution.

### 2.4 Throttling, Quotas & Usage Plans
- **Token Bucket Algorithm**: Controls request ingestion using steady-state rates (requests per second) and burst capacities.
- **Usage Plans**: Associates API keys with specific customer tiers (e.g., Basic: 100 req/sec and 10,000 req/month; Premium: 1,000 req/sec and 1,000,000 req/month).

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                            AMAZON API GATEWAY REQUEST PROCESSING PIPELINE                         |
|                                                                                                   |
|  [Client Browser / Mobile App]                                                                    |
|         |                                                                                         |
|         v (HTTPS Request: GET https://api.corp.com/v1/orders?id=123)                              |
|  +---------------------------------------------------------------------------------------------+  |
|  | API GATEWAY FRONT DOOR (Regional / Edge PoP)                                                |  |
|  | 1. SSL/TLS Termination & Custom Domain SNI Validation                                       |  |
|  | 2. AWS WAF Inspection (SQLi, XSS, Rate-Limiting rules)                                     |  |
|  | 3. Stage Throttling Check (Token Bucket: Is rate < steady-state and within burst?)          |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Passes WAF & Rate Limit)                                                               |
|  +---------------------------------------------------------------------------------------------+  |
|  | METHOD REQUEST EVALUATION                                                                   |  |
|  | 4. Authentication / Authorization:                                                          |  |
|  |    - Cognito JWT validation OR Lambda Authorizer policy lookup                              |  |
|  | 5. Request Validation: Schema validation (JSON Schema Draft 4)                              |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Authorized & Validated)                                                                |
|  +---------------------------------------------------------------------------------------------+  |
|  | INTEGRATION REQUEST                                                                         |  |
|  | 6. VTL Data Mapping: Transform incoming payload/headers (REST API only)                     |  |
|  | 7. Dispatch to Backend:                                                                     |  |
|  |    +-----------------------------+-----------------------------+                           |  |
|  |    | Lambda Proxy Integration    | Direct AWS Service (Dynamo) | VPC Link (Private ALB)    |  |
|  +----+-----------------------------+-----------------------------+---------------------------+  |
|         |                                                                                         |
|         v (Backend Execution Response)                                                            |
|  +---------------------------------------------------------------------------------------------+  |
|  | INTEGRATION RESPONSE -> METHOD RESPONSE                                                     |  |
|  | 8. Status Code Mapping & VTL Response Transformation                                        |  |
|  | 9. CORS Header Injection (`Access-Control-Allow-Origin: *`)                                 |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (HTTPS 200 OK + JSON Payload)                                                           |
|  [Client Receives Response]                                                                       |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating an HTTP API with Lambda Integration via AWS CLI

```bash
# 1. Create the HTTP API
aws apigatewayv2 create-api \
    --name "quick-service-api" \
    --protocol-type "HTTP" \
    --target "arn:aws:lambda:us-east-1:123456789012:function:orders-handler" \
    --route-key "GET /orders"

# 2. Grant API Gateway permission to invoke the Lambda function
aws lambda add-permission \
    --function-name "orders-handler" \
    --statement-id "apigateway-invoke-permission" \
    --action "lambda:InvokeFunction" \
    --principal "apigateway.amazonaws.com" \
    --source-arn "arn:aws:execute-api:us-east-1:123456789012:*/*"
```

### 4.2 Deploying a REST API Stage

```bash
# Deploy REST API to 'production' stage
aws apigateway create-deployment \
    --rest-api-id "a1b2c3d4e5" \
    --stage-name "production" \
    --stage-description "Production deployment v1.2" \
    --description "Deployed via CI/CD pipeline"
```

---

## 5. Practical Examples

### 5.1 High-Performance HTTP API with JWT Authorizer (Terraform)

```hcl
# HTTP API Gateway (v2)
resource "aws_apigatewayv2_api" "http_api" {
  name          = "enterprise-http-api"
  protocol_type = "HTTP"

  cors_configuration {
    allow_origins = ["https://app.enterprise.com"]
    allow_methods = ["GET", "POST", "OPTIONS"]
    allow_headers = ["Authorization", "Content-Type"]
    max_age       = 3600
  }
}

# Native JWT Authorizer (Cognito / Auth0 / Okta)
resource "aws_apigatewayv2_authorizer" "jwt_auth" {
  api_id           = aws_apigatewayv2_api.http_api.id
  authorizer_type  = "JWT"
  identity_sources = ["$request.header.Authorization"]
  name             = "cognito-jwt-authorizer"

  jwt_configuration {
    audience = [var.cognito_client_id]
    issuer   = "https://cognito-idp.${var.aws_region}.amazonaws.com/${var.user_pool_id}"
  }
}

# Integration with Lambda Function
resource "aws_apigatewayv2_integration" "lambda_integration" {
  api_id                 = aws_apigatewayv2_api.http_api.id
  integration_type       = "AWS_PROXY"
  integration_uri        = var.lambda_function_arn
  payload_format_version = "2.0"
}

# Protected Route: GET /profile
resource "aws_apigatewayv2_route" "profile_route" {
  api_id             = aws_apigatewayv2_api.http_api.id
  route_key          = "GET /profile"
  target             = "integrations/${aws_apigatewayv2_integration.lambda_integration.id}"
  authorization_type = "JWT"
  authorizer_id      = aws_apigatewayv2_authorizer.jwt_auth.id
}

# Auto-deploying Stage
resource "aws_apigatewayv2_stage" "prod" {
  api_id      = aws_apigatewayv2_api.http_api.id
  name        = "$default"
  auto_deploy = true

  default_route_settings {
    throttling_burst_limit = 500
    throttling_rate_limit  = 1000
  }
}
```

### 5.2 Private API Gateway Accessible Only via VPC Interface Endpoint

```hcl
# 1. VPC Interface Endpoint for API Gateway
resource "aws_vpc_endpoint" "execute_api" {
  vpc_id              = var.vpc_id
  service_name        = "com.amazonaws.${var.aws_region}.execute-api"
  vpc_endpoint_type   = "Interface"
  subnet_ids          = var.private_subnet_ids
  security_group_ids  = [aws_security_group.vpce_sg.id]
  private_dns_enabled = true
}

# 2. Private REST API
resource "aws_api_gateway_rest_api" "private_api" {
  name = "internal-banking-api"

  endpoint_configuration {
    types            = ["PRIVATE"]
    vpc_endpoint_ids = [aws_vpc_endpoint.execute_api.id]
  }

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect    = "Deny"
        Principal = "*"
        Action    = "execute-api:Invoke"
        Resource  = "execute-api:/*"
        Condition = {
          StringNotEquals = {
            "aws:sourceVpce" = aws_vpc_endpoint.execute_api.id
          }
        }
      },
      {
        Effect    = "Allow"
        Principal = "*"
        Action    = "execute-api:Invoke"
        Resource  = "execute-api:/*"
      }
    ]
  })
}
```

### 5.3 Lambda Custom Request Authorizer (Python)

```python
"""
Validates an HMAC API token and returns an IAM execution policy.
"""
import os
import hmac
import hashlib

EXPECTED_SECRET = os.environ.get("AUTH_SECRET", "super-secure-token-2026")

def lambda_handler(event, context):
    token = event.get("headers", {}).get("x-api-auth")
    method_arn = event.get("routeArn", event.get("methodArn"))

    if token and hmac.compare_digest(token, EXPECTED_SECRET):
        return generate_policy("user-12345", "Allow", method_arn)
    else:
        return generate_policy("anonymous", "Deny", method_arn)

def generate_policy(principal_id: str, effect: str, resource: str) -> dict:
    return {
        "principalId": principal_id,
        "policyDocument": {
            "Version": "2012-10-17",
            "Statement": [
                {
                    "Action": "execute-api:Invoke",
                    "Effect": effect,
                    "Resource": resource
                }
            ]
        },
        "context": {
            "org_id": "org_9876",
            "tier": "enterprise"
        }
    }
```

### 5.4 VPC Link to Private Application Load Balancer

```hcl
# VPC Link connecting HTTP API to private VPC resources
resource "aws_apigatewayv2_vpc_link" "alb_link" {
  name               = "http-api-to-private-alb"
  security_group_ids = [aws_security_group.vpc_link_sg.id]
  subnet_ids         = var.private_subnet_ids
}

# Integration forwarding requests through VPC Link
resource "aws_apigatewayv2_integration" "private_alb_integration" {
  api_id           = aws_apigatewayv2_api.http_api.id
  integration_type = "HTTP_PROXY"
  integration_uri  = aws_lb_listener.private_alb_listener.arn

  integration_method = "ANY"
  connection_type    = "VPC_LINK"
  connection_id      = aws_apigatewayv2_vpc_link.alb_link.id
}
```

### 5.5 Usage Plan and Metered API Key Setup (REST API)

```hcl
# API Key
resource "aws_api_gateway_api_key" "enterprise_client" {
  name = "client-acme-corp"
}

# Usage Plan: 500 req/sec rate, 1,000 burst, 500,000 requests/month quota
resource "aws_api_gateway_usage_plan" "tier_silver" {
  name = "silver-tier-plan"

  api_stages {
    api_id = aws_api_gateway_rest_api.private_api.id
    stage  = "production"
  }

  throttle_settings {
    burst_limit = 1000
    rate_limit  = 500
  }

  quota_settings {
    limit  = 500000
    period = "MONTH"
  }
}

resource "aws_api_gateway_usage_plan_key" "assign_key" {
  key_id        = aws_api_gateway_api_key.enterprise_client.id
  key_type      = "API_KEY"
  usage_plan_id = aws_api_gateway_usage_plan.tier_silver.id
}
```

---

## 6. Best Practices

1. **Default to HTTP APIs Unless REST Features Are Required**: Use HTTP APIs for lower latency (~10ms faster) and ~70% cost reduction unless you explicitly require Usage Plans, Request Validation, or VTL payload transformations.
2. **Always Use Lambda Proxy Integration**: Avoid manual parameter and body mapping in VTL. Proxy integration passes the raw JSON request to Lambda and forwards the standardized response back to the client cleanly.
3. **Protect Public APIs with AWS WAF**: Associate an AWS WAF Web ACL to protect REST and HTTP APIs from credential stuffing, bot scrapers, and Layer 7 denial-of-service.
4. **Implement Request Validation at Gateway Level**: Define JSON schemas in REST APIs. Invalid requests are rejected with `400 Bad Request` directly at the edge, saving Lambda invocation costs.
5. **Cache Authorizer Responses**: When using Lambda Custom Authorizers, set the authorizer TTL (default 300s) to avoid invoking the authentication Lambda function on every individual HTTP request.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Forgetting to Deploy REST API | Changes saved in console or code but deployment not triggered. | Stale API version served to clients. | Always trigger `create-deployment` or enable auto-deploy on stages. |
| Over-Broad Lambda Authorizer Cache Key | Authorizer caches policy based only on Authorization header. | User A accesses User B's resources. | Set cache key to include user identity or set Authorizer TTL to 0 for dynamic RBAC. |
| Missing Lambda Resource-Based Policy | API Gateway lacks `lambda:InvokeFunction` permission. | API returns `500 Internal Server Error`. | Add `aws_lambda_permission` allowing principal `apigateway.amazonaws.com`. |
| CORS Preflight Failures | Missing `OPTIONS` mock integration or missing CORS headers. | Browser blocks API calls with CORS errors. | Configure CORS in HTTP API console or add `Access-Control-Allow-Origin` in Lambda response. |
| Using API Keys for Security Authentication | Using API keys as passwords instead of usage tracking. | Vulnerable to key leakage; keys lack identity validation. | Use Cognito JWT or Lambda Authorizers for security; use API keys only for billing/quotas. |
| Hitting Regional API Gateway Throttling | Account-level default rate limit is 10,000 req/sec. | API returns `429 Too Many Requests`. | Request a Service Quota increase for API Gateway steady-state throughput. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Latency
- **Endpoint Types**:
  - *Edge-Optimized*: Global CloudFront distribution fronting API Gateway. Best for globally distributed public consumers.
  - *Regional*: Deployed in the same AWS region. Best when callers are in the same region or when you manage your own custom CloudFront distribution in front of API Gateway.
- **API Caching**: REST APIs support dedicated in-memory caches (0.5 GB to 237 GB) to serve identical GET requests without hitting backends.

### 8.2 Security
- **Private APIs**: Completely invisible to the public Internet; reachable only through VPC Interface Endpoints with strict VPC Endpoint Resource Policies.
- **Mutual TLS (mTLS)**: Supports two-way TLS authentication on custom domain names using X.509 client certificates.

### 8.3 Cost Breakdown
- **HTTP APIs**: \$1.00 per million requests (first 300 million).
- **REST APIs**: \$3.50 per million requests (first 333 million) + optional cache fees (\$0.020/hr for 0.5 GB).
- **WebSocket APIs**: \$1.00 per million messages + \$0.25 per million connection minutes.

---

## 9. Comparison Table

| Dimension | HTTP APIs (v2) | REST APIs (v1) | WebSocket APIs |
|---|---|---|---|
| **Protocol** | HTTP / HTTPS | HTTP / HTTPS | WebSocket (ws:// / wss://) |
| **Primary Focus** | High speed, low cost | Enterprise features | Bidirectional real-time |
| **Cost per 1M req** | \$1.00 | \$3.50 | \$1.00 + connection time |
| **Built-in JWT Auth** | Yes (Native OIDC) | No (Requires Lambda Auth)| No |
| **VTL Transformations**| No | Yes (Request / Response) | No |
| **Usage Plans & Keys** | No | Yes | No |
| **WAF Support** | Yes | Yes | Yes (via Connect route) |
| **VPC Integration** | VPC Link to ALB/NLB | VPC Link to NLB only | VPC Link to NLB only |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the difference between Lambda Proxy Integration and Lambda Non-Proxy (Custom) Integration?</strong></summary>

- **Lambda Proxy Integration**: API Gateway passes the entire HTTP request (headers, query strings, path parameters, body, client identity) as a single JSON `event` object directly to the Lambda function. The Lambda function must return a specific JSON response containing `statusCode`, `headers`, and `body`. No data transformation mapping is performed at the gateway level.
- **Non-Proxy Integration**: The developer defines explicit Velocity Template Language (VTL) mapping templates in API Gateway to transform incoming HTTP parameters into a custom payload before invoking Lambda, and transforms Lambda's output back into HTTP status codes and headers.
</details>

<details>
<summary><strong>Q2: How does API Gateway handle rate limiting and throttling using the Token Bucket algorithm?</strong></summary>

API Gateway employs a Token Bucket algorithm where the bucket holds a maximum number of tokens representing the **Burst Limit** (e.g., 5,000 requests), and new tokens are continuously added at a steady **Rate Limit** (e.g., 2,500 requests per second). When a request arrives, a token is subtracted. If tokens are available, the request proceeds; if the bucket is empty, API Gateway immediately rejects the request with an `HTTP 429 Too Many Requests` error before invoking any backend resources.
</details>

<details>
<summary><strong>Q3: When should you choose an HTTP API over a REST API?</strong></summary>

Choose an **HTTP API** for modern serverless microservices where you require:
1. Significant cost savings (70% cheaper at \$1.00/million vs \$3.50/million).
2. Lower latency (~10ms improvement).
3. Native OIDC/OAuth 2.0 JWT authorization without coding custom Lambda authorizers.
4. Direct routing to private VPC resources (ALB, NLB, Cloud Map) via VPC Links.
Choose a **REST API** only when you need API keys with Usage Plans, request/response payload transformations with VTL, request schema validation, edge caching, or client SDK generation.
</details>

<details>
<summary><strong>Q4: How do WebSocket APIs maintain state and push messages to connected clients?</strong></summary>

API Gateway maintains the persistent TCP connection at its managed edge layer. When a client connects, API Gateway executes the `$connect` route, passing a unique `connectionId` to a Lambda backend, which stores the ID in DynamoDB. To push data to a connected client asynchronously, backend systems (e.g., Lambda, EC2) call the API Gateway Management API endpoint (`POST /@connections/{connectionId}`) with the payload. API Gateway locates the active socket and pushes the frame to the client.
</details>

<details>
<summary><strong>Q5: What is the function of a VPC Link in API Gateway?</strong></summary>

A VPC Link bridges API Gateway to backend resources running inside private VPC subnets without exposing those resources to the public Internet:
- In **HTTP APIs**, VPC Links utilize private Elastic Network Interfaces (ENIs) to connect directly to private ALBs, NLBs, or AWS Cloud Map service discovery endpoints.
- In **REST APIs**, VPC Links connect exclusively to private Network Load Balancers (NLBs).
</details>

<details>
<summary><strong>Q6: How do you resolve CORS errors when using Lambda Proxy Integration?</strong></summary>

With Lambda Proxy Integration, API Gateway does NOT inject CORS headers into responses returned from backend Lambda functions. The Lambda function itself is strictly responsible for returning the required CORS headers inside its response dictionary:
```json
{
  "statusCode": 200,
  "headers": {
    "Access-Control-Allow-Origin": "https://example.com",
    "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type,Authorization"
  },
  "body": "{\"status\":\"ok\"}"
}
```
In addition, the Gateway must have an `OPTIONS` mock or preflight route configured that responds with HTTP 200 and the matching allowed headers.
</details>

<details>
<summary><strong>Q7: What are Stage Variables and how are they used in continuous deployment?</strong></summary>

Stage Variables act like environment variables scoped to a specific API Gateway stage (e.g., `lambdaAlias = "prod"` in production stage, and `lambdaAlias = "dev"` in development stage). In the API integration URI, you configure `arn:aws:lambda:...:function:my-func:${stageVariables.lambdaAlias}`. This allows you to promote code through stages without duplicating or modifying the API Gateway definition.
</details>

<details>
<summary><strong>Q8: How does a Private API Gateway differ from a Regional API Gateway?</strong></summary>

- **Regional API Gateway**: Has a publicly resolvable DNS name and is accessible over the public Internet from any authorized client.
- **Private API Gateway**: Can only be accessed from within an Amazon VPC via an **Interface VPC Endpoint** (`com.amazonaws.<region>.execute-api`). Traffic never traverses the public Internet. Its resource policy explicitly restricts invocation to the VPC Endpoint ID (`aws:sourceVpce`).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Build a Serverless HTTP API with Python Lambda</strong></summary>

**Goal**: Deploy a working CRUD endpoint returning JSON.
1. Create a Python Lambda function that handles `event["queryStringParameters"]`.
2. Create an HTTP API with a route `GET /items`.
3. Configure the integration using Lambda Proxy Integration (Payload version 2.0).
4. Invoke the generated API endpoint URL with `curl` and verify the JSON payload.
</details>

<details>
<summary><strong>Exercise 2: Secure Routes with Cognito User Pool JWT Authorizer</strong></summary>

**Goal**: Enforce OAuth 2.0 token validation.
1. Create a Cognito User Pool and an App Client.
2. In API Gateway, create a JWT Authorizer pointing to the User Pool Issuer URL.
3. Attach the authorizer to route `POST /orders`.
4. Attempt to curl `/orders` without a token (verify `401 Unauthorized`).
5. Obtain a valid ID token from Cognito and curl with `Authorization: Bearer <token>` (verify `200 OK`).
</details>

<details>
<summary><strong>Exercise 3: Configure Token Bucket Rate Throttling</strong></summary>

**Goal**: Observe HTTP 429 Too Many Requests in action.
1. Set the API stage default throttling rate limit to 2 req/sec and burst limit to 2.
2. Use a load-testing tool (e.g., `hey`, `ab`, or a bash loop) to fire 10 concurrent requests.
3. Verify that requests exceeding the burst limit receive HTTP 429 responses.
</details>

<details>
<summary><strong>Exercise 4: Implement a Lambda Request Authorizer with Policy Caching</strong></summary>

**Goal**: Build an API token authorizer with a 300-second cache TTL.
1. Write a Lambda authorizer inspecting header `X-API-Key`.
2. Generate an IAM Allow policy if key equals `secret-pass-2026`.
3. Test requests and review CloudWatch logs for the authorizer to confirm it executes only once every 5 minutes per unique token.
</details>

<details>
<summary><strong>Exercise 5: Build a WebSocket Echo Server</strong></summary>

**Goal**: Establish a bidirectional WebSocket connection.
1. Create a WebSocket API in API Gateway with route selection `$request.body.action`.
2. Connect a Lambda function to the `$default` route that echoes back received messages using the `apigatewaymanagementapi` SDK.
3. Use a tool like `wscat` (`wscat -c wss://<api-id>.execute-api.us-east-1.amazonaws.com/prod`) to send and receive frames.
</details>

<details>
<summary><strong>Exercise 6: Restrict Access to a Private API via Interface VPC Endpoint</strong></summary>

**Goal**: Prevent public access and permit internal VPC traffic only.
1. Create an Interface VPC Endpoint for `execute-api`.
2. Create a Private REST API with a resource policy requiring `aws:sourceVpce`.
3. Try to access the private URL from an external network (verify DNS fails or access is denied).
4. Curl the endpoint from an EC2 instance inside the VPC and verify successful response.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| API GATEWAY CAPABILITIES CHEAT SHEET                                                              |
+--------------------------+-----------------------+-----------------------+------------------------+
| Feature                  | HTTP API              | REST API              | WebSocket API          |
+--------------------------+-----------------------+-----------------------+------------------------+
| Communication Model      | Request-Response      | Request-Response      | Bidirectional Stream   |
| Latency Overhead         | Lowest (~10ms)        | Moderate (~20-30ms)   | Sub-millisecond frames |
| Pricing (per 1M)         | $1.00                 | $3.50                 | $1.00 + minute fees    |
| JWT / OIDC Auth          | Native (No Lambda)    | Lambda Authorizer     | Lambda Authorizer      |
| Direct AWS Service Proxy | Limited               | Yes (VTL Mapping)     | Limited                |
| Custom Domain Names      | Yes                   | Yes                   | Yes                    |
| VPC Access               | VPC Link (ALB/NLB)    | VPC Link (NLB only)   | VPC Link (NLB only)    |
+--------------------------+-----------------------+-----------------------+------------------------+
```

- **HTTP APIs**: Preferred default for modern REST services and microservices.
- **REST APIs**: Choose when enterprise monetization (API Keys/Usage Plans) or VTL transformations are required.
- **WebSocket APIs**: Ideal for chat, collaborative editing, gaming, and real-time streaming notifications.

---

## 13. Further Reading

- [Amazon API Gateway Developer Guide](https://docs.aws.amazon.com/apigateway/latest/developerguide/)
- [Choosing Between HTTP APIs and REST APIs](https://docs.aws.amazon.com/apigateway/latest/developerguide/http-api-vs-rest.html)
- [Building WebSocket APIs with Amazon API Gateway](https://docs.aws.amazon.com/apigateway/latest/developerguide/apigateway-websocket-api.html)
- [Working with VPC Links for Private Integrations](https://docs.aws.amazon.com/apigateway/latest/developerguide/http-api-vpc-links.html)
- [04-networking/07-load-balancing.md](file:///04-devops/aws/04-networking/07-load-balancing.md)
