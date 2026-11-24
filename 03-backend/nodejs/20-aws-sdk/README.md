# AWS SDK for JavaScript v3 with Node.js

> Comprehensive, production-grade guide to architecting, deploying, and maintaining cloud-native Node.js applications on Amazon Web Services using the AWS SDK for JavaScript v3 (`@aws-sdk/*`).

---

## Curriculum Overview

The AWS SDK v3 is a modular, TypeScript-first SDK built for Node.js runtimes. Unlike the monolithic AWS SDK v2 (`aws-sdk`), v3 publishes individual, tree-shakeable packages per AWS service (such as `@aws-sdk/client-s3` and `@aws-sdk/client-dynamodb`), introduces a composable middleware stack for request/response lifecycle interception, and natively supports modern Node.js streams and ESM.

This module provides exhaustive coverage of modern AWS development patterns in Node.js 22 LTS and Node.js 24.

---

## Module Index

### 01. Setup & Environment
- **[01. SDK v3 Architecture & Credentials](01-setup/01-sdk-v3-and-credentials.md)**: Modular client architecture, credential provider chain (`fromNodeProviderChain`, IAM Roles for Service Accounts / IRSA, ECS Task execution roles), middleware customization, client configuration best practices, and HTTP connection pooling via `@smithy/node-http-handler`.
- **[02. Local Development & Testing](01-setup/02-local-development-and-testing.md)**: Emulating AWS cloud primitives locally using LocalStack, `aws-sdk-client-mock` unit testing, Docker Compose local infrastructure, and integration test patterns.

### 02. Core AWS Cloud Services
- **[01. Amazon S3 (Simple Storage Service)](02-services/01-s3.md)**: PutObject, GetObject, multipart uploads with `@aws-sdk/lib-storage`, presigned URLs with `@aws-sdk/s3-request-presigner`, SHA256 checksums, and streaming uploads from Node.js `Readable` streams.
- **[02. Amazon DynamoDB](02-services/02-dynamodb.md)**: Low-level client vs `@aws-sdk/lib-dynamodb` DocumentClient, Single-Table Design modeling, Query vs Scan, optimistic locking with `ConditionExpression`, transactions (`TransactWriteCommand`), and pagination.
- **[03. Amazon SQS (Simple Queue Service)](02-services/03-sqs.md)**: Standard vs FIFO message queues, batch sending and deletion, dead-letter queues (DLQs), visibility timeout management, long polling with `WaitTimeSeconds`, and graceful worker consumer loops.
- **[04. Amazon SNS & EventBridge](02-services/04-sns-and-eventbridge.md)**: Fan-out pub/sub messaging with SNS, decoupled event-driven architectures with EventBridge, custom schema validation, content-based filtering, and dead-letter routing.
- **[05. Amazon SES (Simple Email Service)](02-services/05-ses-email.md)**: Transactional HTML email dispatch, SendRawEmail for attachments, DKIM/SPF domain verification, handling bounce/complaint webhooks, and rate-limiting outgoing messages.
- **[06. Amazon Cognito](02-services/06-cognito.md)**: User Pools and Identity Pools, SRP (Secure Remote Password) authentication, JWT verification via `aws-jwt-verify`, refresh token rotation, and custom Lambda auth triggers.
- **[07. Amazon Bedrock & Generative AI](02-services/07-bedrock.md)**: Foundation model invocation (`@aws-sdk/client-bedrock-runtime`), Anthropic Claude and Amazon Titan streaming completions, embeddings generation, and prompt orchestration.
- **[08. Secrets Manager & Parameter Store](02-services/08-secrets-and-parameters.md)**: Dynamic configuration loading at startup, automatic secret rotation, in-memory caching with TTL to minimize API costs, and cross-account KMS decryption.

### 03. AWS Lambda & Serverless Runtimes
- **[01. Lambda Handlers & Runtime Architecture](03-lambda/01-lambda-handlers.md)**: Event loop execution model in AWS Lambda, initialization vs invocation phase, cold start optimization via ESBuild tree-shaking, top-level client reuse, and response streaming.
- **[02. API Gateway & HTTP APIs](03-lambda/02-api-gateway-apis.md)**: REST APIs vs HTTP APIs (v2 payload format), path parameters, CORS preflight handling, stage variables, custom authorizers, and payload compression.
- **[03. Event-Driven Lambda](03-lambda/03-event-driven-lambda.md)**: Processing SQS queue batches with partial batch failure reporting (`ReportBatchItemFailures`), DynamoDB Streams with CDC (Change Data Capture), S3 event notifications, and EventBridge pipe consumers.

### 04. Production Operations, Observability & Resilience
- **[01. Error Handling, Retries & Resilience](04-production/01-errors-and-retries.md)**: SDK v3 retry strategies (exponential backoff with full jitter), adaptive retry mode, circuit breakers, idempotent requests using client tokens, and AWS service exception discrimination.
- **[02. Logging, Tracing & AWS X-Ray](04-production/02-logging-and-tracing.md)**: Structured JSON logging with Pino/PowerTools, distributed tracing with AWS X-Ray SDK and OpenTelemetry, correlation ID propagation via headers, and CloudWatch embedded metrics format (EMF).

### 05. Reference Projects
- **[05-projects/](05-projects/README.md)**: Complete production-ready reference systems with full infrastructure, application code, test suites, and container configurations:
  1. `01-s3-upload-api`: High-performance S3 multipart upload API with presigned URLs, streaming, checksums, and LocalStack integration.
  2. `02-serverless-api`: Production Serverless REST API with AWS Lambda, API Gateway HTTP API, DynamoDB DocumentClient, and SAM/CDK.
  3. `03-node-app-on-ecs`: Enterprise Node.js microservice running on AWS ECS Fargate with ALB healthchecks, multi-stage Docker build, Secrets Manager, and Terraform.

### Reference & Quick Reference
- **[reference/cheatsheet.md](reference/cheatsheet.md)**: High-density quick-reference syntax card for AWS SDK v3 client initialization, common commands, error parsing, and stream handling.

---

## Architectural Principles for AWS on Node.js

1. **Modular Imports**: Always import individual command classes and clients (e.g. `import { S3Client, PutObjectCommand } from '@aws-sdk/client-s3';`). Never import monolithic barrels.
2. **Reuse Clients Outside Handlers**: Initialize clients once outside the request handler or Lambda handler to preserve TCP/TLS sockets and HTTP connection pools across invocations.
3. **Configure Custom HTTP Agents**: Set `keepAlive: true`, `maxSockets`, and connection timeouts on the client's `requestHandler` to prevent socket exhaustion during traffic spikes.
4. **Use lib-storage for Uploads**: Always use `@aws-sdk/lib-storage` `Upload` for files or streams larger than 5MB to handle automatic parallel chunking, retries, and memory throttling.
5. **Enforce Type Safety**: Utilize AWS SDK v3 output interfaces with TypeScript 5+ and validate inbound user payloads using Zod schemas before calling AWS services.
