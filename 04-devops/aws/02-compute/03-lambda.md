# AWS Lambda Serverless Computing Fundamentals

> **Module**: `02-compute`  
> **Topic**: Event-Driven Serverless Compute Architecture  
> **Target Audience**: Cloud Engineers, Backend Developers, Serverless Architects  
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

AWS Lambda is a serverless, event-driven compute service that lets you run code without provisioning, managing, or patching virtual servers or container clusters. You upload your application logic, define an event trigger, and Lambda automatically scales execution from zero to tens of thousands of concurrent requests within milliseconds.

Unlike long-running IaaS instances (EC2) or container tasks (ECS/EKS) that incur continuous idle hourly costs, AWS Lambda operates under a strict **pay-per-invocation** model. You are billed exclusively for the compute time consumed in 1-millisecond increments.

Key architectural advantages include:
- **Zero Server Management**: AWS manages the OS, hypervisor, runtime patches, and physical hardware.
- **Event-Driven Integration**: Natively connects with over 200 AWS services (S3, DynamoDB, SQS, Kinesis, EventBridge, API Gateway).
- **Automated High Availability**: Functions are automatically distributed across multiple Availability Zones within the target region.
- **MicroVM Isolation**: Powered by the **Firecracker** open-source virtualization technology, guaranteeing hardware-level tenant isolation with container-like millisecond startup times.

---

## 2. Core Concepts

### 2.1 The Lambda Lifecycle Phases
Lambda execution is divided into three distinct phases:
1. **Init Phase**:
   - **Extension Init**: Starts internal/external extensions (telemetry, security agents).
   - **Runtime Init**: Initializes the language runtime (Node.js, Python, Java, Go, Rust).
   - **Function Init**: Executes static initialization code outside the handler (database connections, SDK client initialization, loading models/secrets).
2. **Invoke Phase**:
   - Executes the function handler with the event payload and context object.
   - Bounded by the configured timeout (minimum 1 second, maximum 15 minutes / 900 seconds).
3. **Shutdown Phase**:
   - Triggered when no requests arrive for a period of time, or before an environment is terminated.
   - Runs runtime cleanup and signals extensions; capped at 2 seconds.

### 2.2 Cold Starts vs. Warm Starts
- **Cold Start**: Occurs when a new execution environment must be spun up (downloading the deployment package, initializing the Firecracker MicroVM, running Init phase code). Typically adds 100–1,000+ ms depending on runtime and package size.
- **Warm Start**: Occurs when a subsequent request arrives while an existing execution environment is idle. Reuses existing global state, memory, and database connections. Execution begins immediately at the handler function (sub-millisecond overhead).

### 2.3 Compute & Resource Sizing
- **Memory Allocation**: Configurable from **128 MB to 10,240 MB (10 GB)** in 1 MB increments.
- **CPU Scaling**: CPU power is **linearly proportional to memory**. You cannot configure vCPUs directly.
  - At **1,769 MB**, Lambda allocates exactly **1 full vCPU core**.
  - Above 1,769 MB, functions gain access to multi-threaded execution (up to 6 vCPUs at 10 GB RAM).
- **Ephemeral Storage (`/tmp`)**: Dedicated disk space configurable from **512 MB to 10,240 MB (10 GB)**, preserving scratch data across warm invocations.

### 2.4 Invocation Models
1. **Synchronous Invocation (`RequestResponse`)**:
   - Caller waits for the function to execute and return a payload.
   - Handled by: Application Load Balancer (ALB), Amazon API Gateway, AWS SDK direct calls.
   - Errors are returned immediately to the client.
2. **Asynchronous Invocation (`Event`)**:
   - Lambda places the event into an internal managed queue and returns an HTTP `202 Accepted` immediately.
   - Handled by: Amazon S3, Amazon SNS, Amazon EventBridge.
   - Built-in automatic retry: retries twice by default with exponential backoff before sending failures to a Dead-Letter Queue (DLQ) or On-Failure Destination.
3. **Poll-Based / Event Source Mapping (ESM)**:
   - Lambda internal pollers read records from stream or queue sources and pass batches to the function.
   - Handled by: Amazon SQS, Amazon Kinesis Data Streams, Amazon DynamoDB Streams, Amazon MSK (Kafka).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        AWS Lambda Firecracker MicroVM Architecture                |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ AWS Event Source ] (API Gateway / S3 / SQS / SDK)                              |
|           |                                                                       |
|           v                                                                       |
|  +-------------------------------------------------------------+                  |
|  |             AWS Lambda Frontend / Placement Worker          |                  |
|  | - Authenticates caller & validates concurrency limits       |                  |
|  | - Locates idle Warm Worker or allocates new MicroVM Host    |                  |
|  +-------------------------------------------------------------+                  |
|           |                                                                       |
|           v                                                                       |
|  +-----------------------------------------------------------------------------+  |
|  |                         Bare Metal Worker Host (Nitro)                      |  |
|  |                                                                             |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |  |                     Firecracker MicroVM (Isolated Sandbox)            |  |  |
|  |  |                                                                       |  |  |
|  |  |  +---------------------+   +---------------------------------------+  |  |  |
|  |  |  | Dedicated Guest OS  |   | Dedicated Linux cgroups & Namespaces  |  |  |  |
|  |  |  | Minimal Kernel      |   | CPU & RAM pinned to allocation        |  |  |  |
|  |  |  +---------------------+   +---------------------------------------+  |  |  |
|  |  |                                 |                                     |  |  |
|  |  |  +-----------------------------------------------------------------+  |  |  |
|  |  |  |                    Language Runtime Environment                 |  |  |  |
|  |  |  |  [ Init Code outside Handler: DB Pool / SDK Clients / Secrets ] |  |  |  |
|  |  |  |  -------------------------------------------------------------  |  |  |  |
|  |  |  |  [ Function Handler: def handler(event, context): ...        ] |  |  |  |
|  |  |  +-----------------------------------------------------------------+  |  |  |
|  |  |                                 |                                     |  |  |
|  |  |  +-----------------------------------------------------------------+  |  |  |
|  |  |  | Ephemeral Storage: /tmp (512 MB - 10 GB Local Loopback Mount)   |  |  |  |
|  |  |  +-----------------------------------------------------------------+  |  |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

### Execution Environment Lifecycle
```
[Start]
   |
   v
[Init Phase] ----> (1. Extension Init) ----> (2. Runtime Init) ----> (3. Function Init)
   |
   v
[Invoke Phase] --> (Runs handler with Event payload)
   |
   +-----------------------+ (Subsequent requests reuse environment)
   |                       |
   v                       v
[Freeze / Idle] ------> [Invoke Phase (Warm)]
   |
   v (Timeout / Capacity reclamation)
[Shutdown Phase] -> (Extensions cleanup, 2s max) ----> [Destroy MicroVM]
```

---

## 4. Syntax & Usage

### 4.1 Production Python Handler Example
```python
import os
import json
import logging
import boto3

# 1. INIT PHASE: Runs once during cold start
logger = logging.getLogger()
logger.setLevel(os.environ.get("LOG_LEVEL", "INFO"))

# Initialize clients globally to reuse TCP connection pools across warm invokes
s3_client = boto3.client("s3")
TABLE_NAME = os.environ.get("TABLE_NAME")

def lambda_handler(event, context):
    """
    2. INVOKE PHASE: Executes on every incoming request
    """
    request_id = context.aws_request_id
    remaining_ms = context.get_remaining_time_in_millis()
    logger.info(f"Processing request {request_id}. Remaining budget: {remaining_ms}ms")
    
    try:
        # Parse payload depending on source (API Gateway proxy format)
        body = json.loads(event.get("body", "{}")) if isinstance(event.get("body"), str) else event
        
        item_id = body.get("itemId", "unknown")
        
        # Business logic execution
        result = {
            "status": "PROCESSED",
            "itemId": item_id,
            "requestId": request_id
        }
        
        return {
            "statusCode": 200,
            "headers": {
                "Content-Type": "application/json",
                "X-Custom-Header": "Lambda-Verified"
            },
            "body": json.dumps(result)
        }
        
    except Exception as e:
        logger.error(f"Execution failed: {str(e)}", exc_info=True)
        return {
            "statusCode": 500,
            "body": json.dumps({"error": "Internal Processing Error", "requestId": request_id})
        }
```

### 4.2 Packaging and Deploying via AWS CLI
```bash
# Package function code
zip -r function.zip index.py

# Create IAM execution role
aws iam create-role \
  --role-name LambdaBasicExecutionRole \
  --assume-role-policy-document '{
    "Version": "2012-10-17",
    "Statement": [{
      "Effect": "Allow",
      "Principal": {"Service": "lambda.amazonaws.com"},
      "Action": "sts:AssumeRole"
    }]
  }'

# Attach CloudWatch logs policy
aws iam attach-role-policy \
  --role-name LambdaBasicExecutionRole \
  --policy-arn arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole

# Create Lambda function
aws lambda create-function \
  --function-name order-processor \
  --runtime python3.12 \
  --role arn:aws:iam::111122223333:role/LambdaBasicExecutionRole \
  --handler index.lambda_handler \
  --zip-file fileb://function.zip \
  --memory-size 1769 \
  --timeout 30 \
  --ephemeral-storage Size=1024 \
  --environment "Variables={LOG_LEVEL=INFO,TABLE_NAME=Orders}"
```

---

## 5. Practical Examples

### Example 1: Synchronous vs. Asynchronous CLI Invocation
```bash
# 1. Synchronous Invocation (Waits for response payload)
aws lambda invoke \
  --function-name order-processor \
  --invocation-type RequestResponse \
  --cli-binary-format raw-in-base64-out \
  --payload '{"itemId": "SKU-9941"}' \
  response.json

cat response.json

# 2. Asynchronous Invocation (Returns immediately with HTTP 202)
aws lambda invoke \
  --function-name order-processor \
  --invocation-type Event \
  --cli-binary-format raw-in-base64-out \
  --payload '{"itemId": "SKU-9942"}' \
  async_output.json
# Output shows StatusCode: 202
```

### Example 2: Terraform Infrastructure for S3-Triggered Lambda Function
Declarative provisioning of an S3 bucket event trigger that executes Lambda upon object creation.

```hcl
resource "aws_iam_role" "lambda_exec" {
  name = "s3-event-processor-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "lambda.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy_attachment" "lambda_logs" {
  role       = aws_iam_role.lambda_exec.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole"
}

resource "aws_lambda_function" "s3_processor" {
  function_name = "s3-image-resizer"
  role          = aws_iam_role.lambda_exec.arn
  handler       = "index.handler"
  runtime       = "python3.12"
  filename      = "lambda_payload.zip"
  memory_size   = 2048
  timeout       = 60
}

resource "aws_s3_bucket" "uploads" {
  bucket = "prod-user-uploads-bucket-12345"
}

# Grant S3 permission to invoke the Lambda function
resource "aws_lambda_permission" "allow_s3" {
  statement_id  = "AllowExecutionFromS3"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.s3_processor.function_name
  principal     = "s3.amazonaws.com"
  source_arn    = aws_s3_bucket.uploads.arn
}

resource "aws_s3_bucket_notification" "bucket_notification" {
  bucket = aws_s3_bucket.uploads.id

  lambda_function {
    lambda_function_arn = aws_lambda_function.s3_processor.arn
    events              = ["s3:ObjectCreated:*"]
    filter_suffix       = ".png"
  }

  depends_on = [aws_lambda_permission.allow_s3]
}
```

### Example 3: Event Source Mapping for Amazon SQS Queue Processing
Configure a Lambda function to continuously process messages from an SQS queue with batching and partial batch response.

```bash
aws lambda create-event-source-mapping \
  --function-name order-processor \
  --batch-size 10 \
  --maximum-batching-window-in-seconds 5 \
  --event-source-arn arn:aws:sqs:us-east-1:111122223333:IncomingOrdersQueue \
  --function-response-types "ReportBatchItemFailures"
```

**Python partial failure handling:**
```python
def lambda_handler(event, context):
    batch_item_failures = []
    
    for record in event["Records"]:
        message_id = record["messageId"]
        try:
            payload = json.loads(record["body"])
            process_order(payload)
        except Exception as e:
            # Report specific message ID to be left in SQS without failing entire batch
            batch_item_failures.append({"itemIdentifier": message_id})
            
    return {"batchItemFailures": batch_item_failures}
```

### Example 4: Configuring On-Failure Asynchronous Destinations
Route failed asynchronous events to an SNS topic for immediate alert notification.

```bash
aws lambda put-function-event-invoke-config \
  --function-name order-processor \
  --maximum-retry-attempts 2 \
  --maximum-event-age-in-seconds 3600 \
  --destination-config '{
    "OnFailure": {
      "Destination": "arn:aws:sns:us-east-1:111122223333:DeadLetterAlertTopic"
    }
  }'
```

### Example 5: Benchmarking Cold Start vs Warm Start Latencies
Execute a Node.js benchmark logging execution phases using `performance.now()`.

```javascript
// Global scope: Init Phase
const startTime = performance.now();
const { DynamoDBClient } = require("@aws-sdk/client-dynamodb");
const ddb = new DynamoDBClient({});
const initDuration = performance.now() - startTime;
let isFirstRun = true;

exports.handler = async (event, context) => {
  const invokeStart = performance.now();
  const wasCold = isFirstRun;
  isFirstRun = false;

  return {
    isColdStart: wasCold,
    initPhaseMs: wasCold ? initDuration : 0,
    invokePhaseMs: performance.now() - invokeStart,
    awsRequestId: context.awsRequestId
  };
};
```

---

## 6. Best Practices

1. **Initialize Outside the Handler**: Place database connections, SDK clients, and configuration parsing in global scope (outside `lambda_handler`). This code executes once during the **Init Phase** and is reused across warm invocations.
2. **Optimize Memory for Cost & Speed**: Because CPU scales linearly with memory, bumping a CPU-bound task from 256 MB to 1,769 MB (1 full vCPU) can reduce runtime by 80%, resulting in **lower total cost** despite the higher per-second rate.
3. **Always Return Partial Batch Responses for SQS**: When processing SQS queues, enable `ReportBatchItemFailures`. Returning failed message IDs individually prevents successfully processed messages from being re-delivered and re-executed.
4. **Enforce Least Privilege Execution Roles**: Never attach full access managed policies (`AdministratorAccess`, `AmazonS3FullAccess`). Scope IAM policies down to specific table ARNs and bucket prefixes.
5. **Set Realistic Timeouts**: Never leave timeouts at the default or maximum (15 minutes) for API-driven synchronous workloads. Set timeouts to 3–5 seconds so hanging external dependencies fail fast and prevent thread pool starvation.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Opening DB Connections Inside Handler** | Each invoke opens a new connection, exhausting database connection pools within seconds. | Initialize the connection pool in global scope outside the handler; reuse across invocations. |
| **Using Synchronous Invocation for Long Tasks** | Client or API Gateway drops connection after 29-second HTTP gateway timeout. | Return an HTTP 202 immediately and process long workloads asynchronously (via SQS or Step Functions). |
| **Ignoring the 1,769 MB vCPU Boundary** | Multi-threaded algorithms run slowly on 512 MB memory because Lambda only grants a fractional single core. | Allocate at least 1,769 MB RAM to allocate a full dedicated vCPU core. |
| **Failing Entire SQS Batches** | If 1 of 10 messages fails, all 10 messages are retried, causing duplicate processing. | Use `ReportBatchItemFailures` and return `{"batchItemFailures": [{"itemIdentifier": id}]}`. |
| **Storing Persistent State in `/tmp`** | Assuming files saved in `/tmp` persist permanently across weeks. | Treat `/tmp` as ephemeral scratch space. Persist permanent files to Amazon S3 or Amazon EFS. |
| **Writing Heavy Logging in Production** | Printing verbose debug JSON strings on high-volume functions inflates CloudWatch Ingestion bills. | Use structured JSON logging and configure dynamic log-level filters (e.g., `WARN`/`ERROR`). |

---

## 8. Performance, Security & Cost Notes

### Concurrency Mechanics & Limits
- **Regional Account Concurrency**: Default soft limit is **1,000 concurrent executions** per region (can be increased via Service Quotas).
- **Throttling Behavior**:
  - Synchronous calls receive an immediate `429 TooManyRequestsException`.
  - Asynchronous calls queue automatically inside Lambda's internal buffer for up to 6 hours with exponential backoff.
- **Reserved Concurrency**: Guarantees a dedicated pool of concurrency for critical functions while preventing unconstrained functions from starving the account limit.

### Security Hardening
- **Firecracker Isolation**: Each execution environment runs in a dedicated Firecracker MicroVM with separate Linux cgroups, namespaces, and seccomp filters. Memory is never shared between concurrent invocations or different functions.
- **No Inbound Network Ports**: Lambda has no public IP or listening network ports; it cannot be port-scanned or directly accessed via SSH.

### Cost Breakdown
- **Requests**: $0.20 per 1,000,000 requests.
- **Duration**: Billed in 1 ms increments. At 1 GB RAM, cost is ~$0.0000166667 per second ($0.06 per hour of continuous execution).
- **Free Tier**: 1,000,000 free requests and 3,200,000 seconds of compute time per month permanently.

---

## 9. Comparison Table

| Feature | AWS Lambda | AWS Fargate (ECS) | Amazon EC2 |
|:---|:---|:---|:---|
| **Primary Paradigm** | Serverless Functions (FaaS) | Serverless Containers (CaaS) | Virtual Machines (IaaS) |
| **Scaling Granularity** | Per-request (sub-second) | Per-task (15–45 seconds) | Per-instance (1–3 minutes) |
| **Maximum Runtime** | 15 minutes (900 seconds) | Indefinite | Indefinite |
| **Billing Model** | Per millisecond of execution | Per second of vCPU & RAM | Per second running instance |
| **Idle Cost** | $0.00 | $0.00 (when desired=0) | Full hourly rate while running |
| **Concurrency Ceiling** | Tens of thousands instantly | Bounded by cluster/service limit | Bounded by instance fleet size |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What happens under the hood during a Lambda cold start?</strong></summary>

**Answer:**
When an invocation arrives and no idle execution environment exists:
1. **Host Allocation**: The Lambda placement service selects an underlying physical worker host with available CPU/memory capacity.
2. **MicroVM Initialization**: Firecracker spins up a dedicated MicroVM sandbox with isolated Linux cgroups and namespaces (typically 5–15 ms).
3. **Deployment Package Download**: The function code zip or container layer is fetched from internal S3 storage and mounted into the MicroVM.
4. **Runtime Initialization**: The chosen language runtime (e.g., Node.js or Python) starts.
5. **Function Initialization**: Code located outside the handler runs (importing libraries, initializing SDKs, building connection pools).
6. **Invocation**: The handler function is finally invoked with the event payload.
</details>

<details>
<summary><strong>Q2: How does memory allocation directly affect CPU performance in AWS Lambda?</strong></summary>

**Answer:**
In AWS Lambda, CPU power cannot be provisioned independently; it is tied directly to memory allocation.
- At 128 MB RAM, the function receives a tiny fraction of a single vCPU.
- At exactly **1,769 MB RAM**, the function receives the equivalent of **1 full dedicated vCPU core**.
- At higher thresholds (e.g., 3,538 MB), the function gains access to 2 full vCPU cores and can run multi-threaded processes (e.g., Python `multiprocessing` or Java worker threads) in parallel, up to 6 vCPUs at 10,240 MB.
</details>

<details>
<summary><strong>Q3: How does error handling differ between Synchronous and Asynchronous invocations?</strong></summary>

**Answer:**
- **Synchronous**: The caller is actively connected and waiting. If the function throws an error or times out, Lambda returns the error stack trace immediately with a `4xx` or `5xx` HTTP status code. The caller is responsible for handling retries.
- **Asynchronous**: The caller receives an immediate `202 Accepted` and disconnects. The event enters an internal AWS queue. If execution fails, Lambda automatically retries twice (total 3 attempts) with exponential backoff. If all retries fail, the event is either dropped or delivered to a configured On-Failure Destination (SQS, SNS, EventBridge).
</details>

<details>
<summary><strong>Q4: What is the purpose of Reserved Concurrency vs. Provisioned Concurrency?</strong></summary>

**Answer:**
- **Reserved Concurrency**: Guarantees that a function has a dedicated slice of the account's total concurrency (e.g., reserving 200 of 1,000 slots). It also acts as a hard ceiling, preventing runaway invocations from exhausting the entire account's concurrency pool.
- **Provisioned Concurrency**: Pre-warms and maintains a specified number of execution environments in an active, initialized state (Init phase completed). Invocations routed to provisioned capacity experience **zero cold starts**, responding with double-digit millisecond latency consistently.
</details>

<details>
<summary><strong>Q5: Why should database connection pools be declared in global scope outside the handler?</strong></summary>

**Answer:**
Code in global scope executes during the **Init Phase**, which only runs once during a cold start. Subsequent invocations on the same warm MicroVM reuse the established execution environment.

If connection pools are created inside the handler, a new database connection will be opened on every single request, rapidly exhausting database connection limits (e.g., max connections in PostgreSQL or MySQL) and adding significant connection-handshake latency to every request.
</details>

<details>
<summary><strong>Q6: How does the `ReportBatchItemFailures` feature optimize SQS message processing?</strong></summary>

**Answer:**
Prior to `ReportBatchItemFailures`, if a Lambda function processed a batch of 10 SQS messages and 1 message failed, throwing an exception caused the entire batch of 10 messages to fail and return to the queue. This resulted in duplicate processing of the 9 successful messages.

With `ReportBatchItemFailures`, the function catches exceptions per item and returns an array of failed `itemIdentifier` IDs. Lambda acknowledges and deletes the successful messages from SQS, leaving only the failed messages visible in the queue for retry or dead-letter queue routing.
</details>

<details>
<summary><strong>Q7: What is the maximum payload size for AWS Lambda invocations?</strong></summary>

**Answer:**
- **Synchronous Invocation**: Maximum payload size is **6 MB** for both request and response.
- **Asynchronous Invocation**: Maximum event payload size is **256 KB**.
- For data exceeding these boundaries (e.g., large image files or multi-megabyte JSON datasets), implement the **Claim Check Pattern**: upload the raw data to Amazon S3 and pass the S3 bucket and object key in the Lambda event.
</details>

<details>
<summary><strong>Q8: What is the maximum execution duration for an AWS Lambda function, and how should longer workloads be handled?</strong></summary>

**Answer:**
The maximum hard timeout for an AWS Lambda function is **15 minutes (900 seconds)**.
Workloads exceeding 15 minutes cannot run in a single Lambda invocation. Architectural solutions include:
1. Orchestrating multiple chained Lambda steps using **AWS Step Functions**.
2. Migrating the long-running task to containerized batch compute using **AWS Batch** or **Amazon ECS on Fargate**.
3. Deconstructing the monolithic calculation into smaller chunked tasks distributed across an SQS worker queue.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Lambda Function via AWS CLI</strong></summary>

**Objective**: Package and deploy a minimal Python 3.12 function.

**Steps**:
```bash
cat <<'EOF' > index.py
def handler(event, context):
    return {"message": "Hello from AWS Lambda", "requestId": context.aws_request_id}
EOF

zip function.zip index.py

aws lambda create-function \
  --function-name hello-lambda \
  --runtime python3.12 \
  --role arn:aws:iam::111122223333:role/LambdaBasicExecutionRole \
  --handler index.handler \
  --zip-file fileb://function.zip
```
</details>

<details>
<summary><strong>Exercise 2: Benchmark Cold Start vs Warm Start Latency</strong></summary>

**Objective**: Observe execution duration differences across consecutive invokes.

**Command**:
```bash
# Invoke 1 (Cold)
aws lambda invoke --function-name hello-lambda --log-type Tail out1.json \
  --query 'LogResult' --output text | base64 --decode

# Invoke 2 (Warm)
aws lambda invoke --function-name hello-lambda --log-type Tail out2.json \
  --query 'LogResult' --output text | base64 --decode
```
*Compare the `Init Duration` line present only on the first call.*
</details>

<details>
<summary><strong>Exercise 3: Configure Reserved Concurrency</strong></summary>

**Objective**: Cap a function at 10 concurrent executions to protect downstream databases.

**Command**:
```bash
aws lambda put-function-concurrency \
  --function-name hello-lambda \
  --reserved-concurrent-executions 10
```
</details>

<details>
<summary><strong>Exercise 4: Test Asynchronous Retry and Dead-Letter Queuing</strong></summary>

**Objective**: Force an unhandled exception in an async function and verify retry count.

**Steps**:
1. Modify code to execute `raise Exception("Simulated Error")`.
2. Configure async invoke config: `--maximum-retry-attempts 1`.
3. Invoke with `--invocation-type Event`.
4. Inspect CloudWatch Logs to confirm exactly 2 invocations occurred before the event was routed to DLQ.
</details>

<details>
<summary><strong>Exercise 5: Attach an SQS Event Source Mapping with Partial Failures</strong></summary>

**Objective**: Wire an SQS queue to trigger Lambda with batch size 5.

**Steps**:
```bash
aws lambda create-event-source-mapping \
  --function-name hello-lambda \
  --batch-size 5 \
  --event-source-arn arn:aws:sqs:us-east-1:111122223333:MyTestQueue \
  --function-response-types ReportBatchItemFailures
```
</details>

<details>
<summary><strong>Exercise 6: Configure Ephemeral Storage to 2048 MB</strong></summary>

**Objective**: Expand `/tmp` disk space for scratch file caching.

**Command**:
```bash
aws lambda update-function-configuration \
  --function-name hello-lambda \
  --ephemeral-storage Size=2048
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                              AWS Lambda Operational Limits                            |
+---------------------------------------------------------------------------------------+
|  Dimension               | Minimum              | Maximum                             |
|--------------------------+----------------------+-------------------------------------|
|  Memory                  | 128 MB               | 10,240 MB (10 GB)                   |
|  vCPU Allocation         | Fractional           | Up to 6 vCPUs (scales with memory)  |
|  Timeout                 | 1 second             | 15 minutes (900 seconds)            |
|  Ephemeral Storage (/tmp)| 512 MB               | 10,240 MB (10 GB)                   |
|  Deployment Package Size | None (Container)     | 250 MB unzipped / 50 MB zipped      |
|  Container Image Size    | None                 | 10 GB (ECR hosted)                  |
|  Invocation Payload      | 0 bytes              | 6 MB (Sync) / 256 KB (Async)        |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Lambda Developer Guide](https://docs.aws.amazon.com/lambda/latest/dg/welcome.html)
- [Firecracker: Secure and Fast MicroVMs for Serverless](https://firecracker-microvm.github.io/)
- [AWS Lambda Execution Environment Lifecycle](https://docs.aws.amazon.com/lambda/latest/dg/runtimes-context.html)
- [Optimizing Lambda Memory with AWS Lambda Power Tuning](https://github.com/alexcasalboni/aws-lambda-power-tuning)
- [Serverless Land Architecture Patterns](https://serverlessland.com/)
