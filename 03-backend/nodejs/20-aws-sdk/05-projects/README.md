# AWS SDK v3 Production Reference Projects

> Production-grade, fully specified architectural reference projects implementing cloud-native Node.js services using AWS SDK v3, modern server frameworks, infrastructure-as-code, and comprehensive automated test suites.

---

## Projects Catalog

| Project | Domain / Type | Key AWS Services | Architecture Highlights |
| :--- | :--- | :--- | :--- |
| **[01. S3 Upload & Streaming API](01-s3-upload-api/README.md)** | Object Storage & Streaming | Amazon S3, LocalStack | Express 5, `@aws-sdk/lib-storage` multipart upload, `@aws-sdk/s3-request-presigner` presigned URLs, SHA256 checksums, Zod schema validation, Vitest + `aws-sdk-client-mock`. |
| **[02. Serverless REST API](02-serverless-api/README.md)** | Serverless Microservice | AWS Lambda, API Gateway v2, DynamoDB | HTTP API (Payload v2), `@aws-sdk/lib-dynamodb` Single-Table Design, JWT authorizer, AWS Lambda PowerTools for logging & metrics, SAM / CDK IaC template. |
| **[03. Containerized App on ECS Fargate](03-node-app-on-ecs/README.md)** | Microservice & Containers | AWS ECS Fargate, ALB, Secrets Manager, CloudWatch | Multi-stage distroless Docker container, graceful SIGTERM shutdown with connection draining, Terraform IaC, dynamic secret loading with caching. |

---

## Architecture Standards

All projects adhere strictly to the following standards:
1. **Node.js Baseline**: Node.js 22 LTS / 24+ using native ECMAScript Modules (`"type": "module"`).
2. **AWS SDK v3**: Modular `@aws-sdk/client-*` and `@aws-sdk/lib-*` packages with tree-shaking and connection pooling.
3. **Reproducibility**: Complete project manifests (`package.json`, `tsconfig.json`), source implementations, automated test suites, and container/infrastructure configurations specified directly within the Markdown document.
4. **Validation & Resilience**: Input payloads validated via Zod schemas; AWS API calls wrapped with exponential backoff and structured error discriminators.
