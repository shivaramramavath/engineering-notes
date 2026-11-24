# Module 05: Application Integration & Messaging

Welcome to the **AWS Application Integration & Messaging** module. This curriculum provides comprehensive, production-grade coverage of asynchronous decoupling, publish/subscribe messaging, real-time event streaming, serverless workflow orchestration, transactional email delivery, customer identity management, managed GraphQL APIs, enterprise generative AI platforms, and distributed event-driven architecture patterns.

---

## Module Overview

Modern cloud applications rely on decoupled, event-driven architectures to achieve infinite scale, high availability, and operational resilience. This module equips distributed systems engineers, backend architects, and cloud developers with the architectural knowledge required to eliminate point-to-point coupling, avoid cascading outages, orchestrate distributed sagas, and safely ingest, stream, and process enterprise data.

---

## Detailed Curriculum Roadmap

| # | Note | Description | Key Focus Areas |
|:---|:---|:---|:---|
| **01** | [Amazon SQS & Distributed Messaging](01-sqs.md) | Asynchronous message queues, buffering, consumer scaling, and error handling | Standard vs FIFO, Visibility Timeout, Long Polling, Dead-Letter Queues, Consumer Idempotency |
| **02** | [Amazon SNS & Amazon EventBridge](02-sns-and-eventbridge.md) | High-throughput pub/sub broadcast and content-based enterprise event buses | Fan-out pattern, Subscription filter policies, Custom event buses, EventBridge Pipes & API Destinations |
| **03** | [Real-Time Streaming: Kinesis & MSK](03-kinesis-and-msk-overview.md) | Continuous streaming ingestion, replay logs, and managed Apache Kafka | Kinesis shards & partition keys, Enhanced Fan-Out, Firehose S3 Parquet ETL, Amazon MSK Serverless |
| **04** | [AWS Step Functions Orchestration](04-step-functions.md) | Serverless finite state machines, distributed transaction sagas, and big data batching | Standard vs Express workflows, ASL syntax, Task token callbacks, Distributed Map, Compensating sagas |
| **05** | [Amazon SES & Email Deliverability](05-ses-email.md) | High-volume transactional email, authentication standards, and sender reputation | SPF, Easy DKIM, DMARC alignment, Dedicated IP pools, Bounce & Complaint suppression loops |
| **06** | [Amazon Cognito: User & Identity Pools](06-cognito.md) | Customer identity (CIAM), OAuth 2.0 / OIDC, JWT verification, and AWS authorization | User Pools vs Identity Pools, ID/Access/Refresh tokens, Hosted UI, PKCE flows, Scoped S3 IAM policies |
| **07** | [AWS AppSync Managed GraphQL](07-appsync-overview.md) | Serverless GraphQL APIs, multi-source data aggregation, and real-time WebSockets | GraphQL SDL, APPSYNC_JS JavaScript resolvers, Pipeline resolvers, Direct DynamoDB data sources, Subscriptions |
| **08** | [Amazon Bedrock Generative AI Platform](08-bedrock.md) | Serverless foundation models, enterprise RAG, autonomous agents, and AI safety | Claude 3.5 Sonnet, Titan, Llama 3, Converse API, Knowledge Bases (RAG), Bedrock Agents, Guardrails |
| **09** | [Enterprise Event-Driven Architecture (EDA) Patterns](09-event-driven-patterns-on-aws.md) | Distributed system patterns, consistency models, and reliability guarantees | Choreography vs Orchestration, Transactional Outbox (CDC), Idempotent Consumer, Claim Check, Circuit Breaker |

---

## Architectural Principles Enforced

1. **Decouple via Asynchronous Buffering**:
   - Prevent cascading failures by placing durable queues (Amazon SQS) and event buses (Amazon EventBridge) between microservices, allowing consumers to process traffic spikes at sustainable rates.
2. **Eliminate Dual-Writes**:
   - Never write to a database and emit an event in separate, non-atomic application steps. Implement the **Transactional Outbox Pattern** with Change Data Capture (DynamoDB Streams / EventBridge Pipes) to guarantee at-least-once event publication.
3. **Mandatory Idempotency**:
   - Design all event consumers to be idempotent. In distributed clouds where at-least-once delivery is the standard, duplicate messages are inevitable. Use unique transaction tokens in DynamoDB with conditional writes to guarantee safe retries.
4. **Zero-Compute Orchestration & Callbacks**:
   - Eliminate costly idle Lambda executions by using **AWS Step Functions** with `.waitForTaskToken` for long-running workflows and human approval tasks.

---

## Recommended Learning Flow

```
[01. SQS Messaging] --------------> [02. SNS & EventBridge Pub/Sub]
        |                                       |
        v                                       v
[03. Kinesis & MSK Streaming] -----> [04. Step Functions Orchestration]
        |                                       |
        +---------------------------------------+
        |
        v
[05. SES Email] -------------------> [06. Cognito Identity & CIAM]
        |                                       |
        v                                       v
[07. AppSync Managed GraphQL] -----> [08. Bedrock Generative AI]
        |                                       |
        +---------------------------------------+
        |
        v
[09. Enterprise Event-Driven Architecture (EDA) Patterns]
```

---

## Module Pre-requisites & Cross-References

- **Compute & Containers**: See [02-compute](../02-compute/) for Lambda event source mappings and containerized worker tasks.
- **Storage & Databases**: See [03-storage-and-databases](../03-storage-and-databases/) for DynamoDB Streams and OpenSearch vector indexing.
- **Networking & Delivery**: See [04-networking](../04-networking/) for API Gateway integrations, VPC Links, and PrivateLink endpoints.
