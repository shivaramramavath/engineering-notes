# AWS Step Functions & Serverless Workflow Orchestration

> **Module**: `05-app-services`  
> **Topic**: State Machines, Standard vs Express, Amazon States Language, Distributed Map & Saga Pattern  
> **Target Audience**: Cloud Architects, Distributed Systems Engineers, Serverless Developers  
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

In distributed microservice architectures, business processes often span multiple steps, databases, and third-party APIs (e.g., an order checkout process requires payment charging, inventory reservation, fraud scoring, and notification dispatch). Managing this multi-step coordination inside application code or chains of Lambda functions leads to significant complexity:
- **Hardcoded Chaining**: Functions calling functions tightly couple services and make failure debugging excruciating.
- **Lost State**: If a worker crashes mid-process, tracking which steps succeeded and rolling back partial changes requires custom database state management.
- **Idle Waiting Costs**: A Lambda function waiting for a 10-minute third-party human approval or database batch job burns money for idle execution time.

**AWS Step Functions** is a serverless visual workflow orchestrator that lets developers coordinate distributed components, automate business processes, build serverless applications, and execute resilient distributed transactions (the **Saga Pattern**) using visual finite state machines.

---

## 2. Core Concepts

### 2.1 State Machine Types: Standard vs Express Workflows
1. **Standard Workflows**:
   - **Execution Duration**: Can run for up to **1 year**.
   - **Execution Semantics**: **Exactly-once execution** (never duplicates execution of states).
   - **Pricing**: Billed per state transition (\$0.025 per 1,000 state transitions).
   - **Audit & Debugging**: Visual step-by-step execution history retained in the console for 90 days.
   - **Use Cases**: Long-running business processes, human approvals, ETL pipelines, order fulfillment sagas.
2. **Express Workflows**:
   - **Execution Duration**: Up to **5 minutes**.
   - **Execution Semantics**: At-least-once execution (Asynchronous) or At-most-once (Synchronous).
   - **Pricing**: Billed by memory and duration (similar to Lambda) + request count.
   - **Throughput**: Extremely high throughput ($>100,000$ executions/second).
   - **Use Cases**: High-volume IoT data ingestion, real-time microservice APIs, payment processing streams.

### 2.2 Amazon States Language (ASL)
State machines are defined declaratively in JSON or YAML using Amazon States Language. Key state types include:
- `Task`: Performs work by invoking an AWS service (Lambda, ECS, DynamoDB, SQS, Batch, Bedrock).
- `Choice`: Evaluates JSONPath/JSONata comparison rules and branches execution conditionally.
- `Wait`: Delays execution for a specific duration or until a timestamp.
- `Parallel`: Spawns concurrent execution branches.
- `Map`: Runs a set of workflow steps on each item of an input array.
- `Pass`: Passes input directly to output, or injects mock data.
- `Fail` / `Succeed`: Terminates execution with an explicit error or success code.

### 2.3 Task Token Callbacks (`.waitForTaskToken`)
- Enables asynchronous decoupling for workflows waiting on external systems or human actions.
- Step Functions generates a unique `TaskToken` and pauses execution (incurring **zero compute cost**).
- Days or weeks later, an external system or human completes the task and calls `SendTaskSuccess` or `SendTaskFailure` with the token, resuming the state machine.

### 2.4 Distributed Map
- A high-scale iteration state capable of processing millions of records stored in Amazon S3 concurrently.
- Spawns up to **10,000 parallel child workflow executions**, transforming big data processing without managing EMR or Spark clusters.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AWS STEP FUNCTIONS SAGA PATTERN ORCHESTRATION                              |
|                                                                                                   |
|  [Execution Started: Order Placed]                                                                |
|         |                                                                                         |
|         v                                                                                         |
|  [State: ProcessPayment] ----(Error: PaymentFailed)----------------------------+                  |
|         |                                                                      |                  |
|         v (Success)                                                            |                  |
|  [State: ReserveInventory] --(Error: OutOfStock)--------+                      |                  |
|         |                                               |                      |                  |
|         v (Success)                                     |                      |                  |
|  [State: ChargeCreditCard] --(Error: CardDeclined)----+ |                      |                  |
|         |                                             | |                      |                  |
|         v (Success)                                   | |                      |                  |
|  [State: HumanApprovalCheck]                          | |                      |                  |
|  - Generates TaskToken                                | |                      |                  |
|  - Sends Email with Approve/Reject link               | |                      |                  |
|  - State machine PAUSES for up to 14 days             | |                      |                  |
|  - (Zero cost during wait!)                           | |                      |                  |
|         |                                             | |                      |                  |
|         v (Admin clicks Approve -> SendTaskSuccess)   | |                      |                  |
|  [State: ShipOrder]                                   | |                      |                  |
|         |                                             | |                      |                  |
|         v                                             v v                      v                  |
|  [State: ExecutionSucceeded]                [COMPENSATING TRANSACTION SAGA]                       |
|                                             1. RefundCreditCard (if charged)                      |
|                                             2. ReleaseInventory (if reserved)                     |
|                                             3. NotifyCustomerOfCancellation                       |
|                                                       |                                           |
|                                                       v                                           |
|                                             [State: ExecutionFailed]                              |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Starting a State Machine Execution via AWS CLI

```bash
# 1. Start execution of a Standard Workflow
aws stepfunctions start-execution \
    --state-machine-arn "arn:aws:states:us-east-1:123456789012:stateMachine:OrderFulfillmentSaga" \
    --name "order-execution-$(date +%s)" \
    --input '{"orderId": "ord-8831", "amount": 299.00, "customerEmail": "user@example.com"}'

# 2. Check Execution Status
aws stepfunctions describe-execution \
    --execution-arn "arn:aws:states:us-east-1:123456789012:execution:OrderFulfillmentSaga:order-execution-1700000000"
```

### 4.2 Resuming a Paused Workflow via Task Token

```bash
# External worker sends task success to resume a paused state machine
aws stepfunctions send-task-success \
    --task-token "AAAAKgAAAAIAAAAAAAAAA..." \
    --output '{"status": "APPROVED", "reviewedBy": "manager@company.com"}'
```

---

## 5. Practical Examples

### 5.1 E-Commerce Saga Orchestration with Retry, Catch & Rollback (Terraform & ASL)

```hcl
resource "aws_sfn_state_machine" "order_saga" {
  name     = "OrderFulfillmentSaga"
  role_arn = var.sfn_role_arn
  type     = "STANDARD"

  definition = jsonencode({
    Comment = "E-Commerce Saga with automated compensation rollback"
    StartAt = "ProcessPayment"
    States = {
      ProcessPayment = {
        Type     = "Task"
        Resource = "arn:aws:states:::lambda:invoke"
        Parameters = {
          "FunctionName" = var.process_payment_lambda_arn
          "Payload.$"    = "$"
        }
        ResultPath = "$.paymentResult"
        Retry = [
          {
            ErrorEquals     = ["Lambda.ServiceException", "Lambda.AWSLambdaException"]
            IntervalSeconds = 2
            MaxAttempts     = 3
            BackoffRate     = 2.0
          }
        ]
        Catch = [
          {
            ErrorEquals = ["States.ALL"]
            Next        = "HandlePaymentFailure"
          }
        ]
        Next = "ReserveInventory"
      }

      ReserveInventory = {
        Type     = "Task"
        Resource = "arn:aws:states:::lambda:invoke"
        Parameters = {
          "FunctionName" = var.reserve_inventory_lambda_arn
          "Payload.$"    = "$"
        }
        ResultPath = "$.inventoryResult"
        Catch = [
          {
            ErrorEquals = ["States.ALL"]
            Next        = "CompensatePayment" # Trigger rollback
          }
        ]
        Next = "OrderSucceeded"
      }

      CompensatePayment = {
        Type     = "Task"
        Resource = "arn:aws:states:::lambda:invoke"
        Parameters = {
          "FunctionName" = var.refund_payment_lambda_arn
          "Payload.$"    = "$"
        }
        Next = "OrderFailed"
      }

      HandlePaymentFailure = {
        Type = "Pass"
        Next = "OrderFailed"
      }

      OrderSucceeded = {
        Type = "Succeed"
      }

      OrderFailed = {
        Type  = "Fail"
        Cause = "Order fulfillment failed. Rollback executed."
        Error = "OrderFulfillmentError"
      }
    }
  })
}
```

### 5.2 Human-in-the-Loop Task Token Callback Pattern

```json
{
  "WaitForManagerApproval": {
    "Type": "Task",
    "Resource": "arn:aws:states:::sqs:sendMessage.waitForTaskToken",
    "Parameters": {
      "QueueUrl": "https://sqs.us-east-1.amazonaws.com/123456789012/approvals-queue",
      "MessageBody": {
        "orderId.$": "$.orderId",
        "amount.$": "$.amount",
        "taskToken.$": "$$.Task.Token"
      }
    },
    "TimeoutSeconds": 86400,
    "Next": "ProcessApprovedOrder",
    "Catch": [
      {
        "ErrorEquals": ["States.Timeout"],
        "Next": "EscalateApproval"
      }
    ]
  }
}
```

### 5.3 Distributed Map Processing Millions of S3 Objects

```json
{
  "ProcessLargeS3Dataset": {
    "Type": "Map",
    "ItemProcessor": {
      "ProcessorConfig": {
        "Mode": "DISTRIBUTED",
        "ExecutionType": "EXPRESS"
      },
      "StartAt": "ValidateAndTransformRecord",
      "States": {
        "ValidateAndTransformRecord": {
          "Type": "Task",
          "Resource": "arn:aws:states:::lambda:invoke",
          "OutputPath": "$.Payload",
          "End": true
        }
      }
    },
    "ItemReader": {
      "Resource": "arn:aws:states:::s3:getObject",
      "ReaderConfig": {
        "InputType": "CSV",
        "CSVHeaderLocation": "FIRST_ROW"
      },
      "Parameters": {
        "Bucket": "enterprise-raw-data-lake-2026",
        "Key": "incoming-transactions.csv"
      }
    },
    "MaxConcurrency": 1000,
    "ItemBatcher": {
      "MaxItemsPerBatch": 100
    },
    "ResultWriter": {
      "Resource": "arn:aws:states:::s3:putObject",
      "Parameters": {
        "Bucket": "enterprise-processed-data-lake-2026",
        "Prefix": "step-functions-output/"
      }
    },
    "End": true
  }
}
```

### 5.4 Choice State with Multiple Business Branches

```json
{
  "EvaluateRiskScore": {
    "Type": "Choice",
    "Choices": [
      {
        "Variable": "$.riskScore",
        "NumericGreaterThan": 80,
        "Next": "RejectOrderFraud"
      },
      {
        "Variable": "$.riskScore",
        "NumericGreaterThan": 50,
        "Next": "TriggerManualReview"
      }
    ],
    "Default": "AutoApproveOrder"
  }
}
```

---

## 6. Best Practices

1. **Use Step Functions Instead of Lambda Orchestration**: Never write code in a Lambda function whose only job is to invoke other Lambda functions and wait for results. Step Functions eliminates idle execution billing and provides built-in visual auditing.
2. **Implement Compensating Transactions (Saga Pattern)**: In distributed architectures where ACID database transactions across multiple microservices are impossible, use Step Functions `Catch` blocks to invoke compensating operations that rollback earlier changes upon downstream failure.
3. **Always Set `TimeoutSeconds` on Every Task**: Default task timeout is 1 year. If a backend Lambda function or external worker hangs indefinitely, your state machine will remain blocked. Always set an explicit task timeout (e.g., 60 seconds).
4. **Use Express Workflows for High-Throughput / Short-Lived Pipelines**: If your workload processes $>100$ events/second and completes in $<5$ minutes, use Express Workflows to save up to 90% over Standard Workflow state transition costs.
5. **Direct Service Integrations (Optimized Integrations)**: Call AWS services (DynamoDB `putItem`, SQS `sendMessage`, EventBridge `putEvents`) directly from Step Functions ASL tasks without provisioning an intermediate Lambda function.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| State Machine Stuck for 1 Year | Missing `TimeoutSeconds` on a task calling external worker. | Execution remains in `RUNNING` state for 365 days. | Add `TimeoutSeconds: 300` on all `Task` states. |
| Exceeding 256 KB Execution History Limit | Passing massive raw payloads through JSONPath state context. | State machine fails with `States.DataLimitExceeded`. | Store heavy payloads in S3 and pass S3 URI pointers in state data. |
| Using Standard Workflow for Streaming High-TPS | Running 10,000 Standard Workflows per second. | Extreme state transition billing (\$0.025/1,000 transitions). | Switch high-volume pipelines to **Express Workflows**. |
| Unhandled Exceptions in Sagas | Forgetting `Catch` blocks on a critical downstream task. | State machine terminates abruptly without executing compensating rollback. | Wrap all tasks in `Catch: ["States.ALL"]` directing to compensating rollbacks. |
| Inverting InputPath / OutputPath / ResultPath | Confusion over ASL JSONPath data transformation filters. | Output overwrites input state or loses required upstream metadata. | Use `ResultPath: "$.resultKey"` to append output without losing input. |
| Missing TaskToken Expiry | Using `.waitForTaskToken` without a timeout. | External worker dies without calling callback; state hangs. | Always pair `.waitForTaskToken` with `HeartbeatSeconds` or `TimeoutSeconds`. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Throughput
- **Standard Workflows**: Up to 2,000 execution starts/second. Best for transactional state integrity.
- **Express Workflows**: Up to 100,000 execution starts/second. Synchronous invocations return responses in under 20 milliseconds.

### 8.2 Security & IAM
- **Execution Role**: Step Functions assumes an IAM service role granting permissions to invoke target resources (Lambda, SQS, DynamoDB).
- **Audit Compliance**: Every single state execution, timestamp, input payload, output payload, and failure stack trace is permanently logged in AWS CloudTrail and CloudWatch Logs.

### 8.3 Cost Breakdown
- **Standard Workflows**: \$0.025 per 1,000 state transitions. First 4,000 transitions/month free.
- **Express Workflows**:
  - \$1.00 per 1 million requests.
  - \$0.00001667 per GB-second of execution duration.

---

## 9. Comparison Table

| Dimension | Standard Workflows | Express Workflows |
|---|---|---|
| **Maximum Duration** | 1 year | 5 minutes |
| **Execution Rate** | 2,000 starts / sec | 100,000+ starts / sec |
| **Execution Semantics**| Exactly-once | At-least-once (Async) / At-most-once (Sync) |
| **Pricing Model** | Per State Transition (\$0.025/1,000)| Duration & Memory + Requests |
| **Execution History** | Visual Console (90 days) | CloudWatch Logs Insights |
| **Task Token Callback**| Supported (`.waitForTaskToken`) | Not supported |
| **Primary Use Case** | Business workflows, human approval| High-volume data processing & microservices |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How does the Saga Pattern work in AWS Step Functions to manage distributed transactions?</strong></summary>

In distributed cloud microservices, traditional two-phase commit (2PC) database transactions are impractical. The **Saga Pattern** structures a business process into a series of local transactions across independent services. 
In AWS Step Functions:
1. Each step executes a task (e.g., Reserve Inventory -> Charge Card -> Generate Shipping Label).
2. Each task has a `Catch` handler configured for `States.ALL` or specific business exceptions.
3. If an error occurs (e.g., Credit Card Declined), the state machine enters a **Compensating Transaction branch** that undoes earlier operations in reverse order (e.g., Release Inventory -> Mark Order Cancelled).
This guarantees eventual consistency across distributed datastores.
</details>

<details>
<summary><strong>Q2: How does the .waitForTaskToken pattern enable zero-cost human approvals?</strong></summary>

When a task resource is configured with the `.waitForTaskToken` suffix (e.g., `arn:aws:states:::sqs:sendMessage.waitForTaskToken`), Step Functions injects a unique context token (`$$.Task.Token`) into the message payload and pauses state machine execution. 
- While paused, the state machine incurs **zero compute or transition costs**.
- The token is emailed to an approver or displayed in an administrative dashboard.
- When the human approves or rejects the request, an external API calls `SendTaskSuccess(taskToken, payload)` or `SendTaskFailure(taskToken, error)`.
- Step Functions matches the token to the paused execution and instantly resumes workflow progression.
</details>

<details>
<summary><strong>Q3: Explain the difference between InputPath, Parameters, ResultSelector, ResultPath, and OutputPath in ASL.</strong></summary>

These ASL fields filter and reshape JSON data entering and leaving a state:
1. **`InputPath`**: Selects a portion of the incoming raw JSON state to pass to the task (e.g., `$.order`).
2. **`Parameters`**: Constructs a custom key-value payload passed to the service integration.
3. **`ResultSelector`**: Filters and reshapes the raw response payload returned by the target resource.
4. **`ResultPath`**: Specifies where in the original input state the task result should be inserted (e.g., `$.paymentResult`), preserving existing state fields.
5. **`OutputPath`**: Filters the combined state before passing it to the next step (e.g., `$.customer`).
</details>

<details>
<summary><strong>Q4: When should you choose an Express Workflow over a Standard Workflow?</strong></summary>

Choose an **Express Workflow** when:
1. High throughput is required (thousands of events per second).
2. Workflows complete in under 5 minutes.
3. You need synchronous request-response execution (e.g., fronted by an API Gateway).
4. Cost optimization for high transaction volume is paramount.
Choose a **Standard Workflow** for long-running processes (up to 1 year), visual step-by-step console debugging, human approval callbacks (`.waitForTaskToken`), and strict exactly-once execution guarantees.
</details>

<details>
<summary><strong>Q5: What is Distributed Map in Step Functions and how does it compare to EMR or Glue?</strong></summary>

**Distributed Map** allows Step Functions to iterate over large datasets (such as a multi-terabyte CSV, JSON, or Parquet file in S3, or thousands of S3 bucket objects) by launching up to **10,000 parallel child workflow executions**. 
Unlike EMR or Glue, which require provisioning and tuning cluster nodes, managing JVM memory, or writing PySpark code, Distributed Map is entirely serverless. It coordinates native Lambda functions or container tasks with built-in concurrency controls, failure isolation, and automated retries.
</details>

<details>
<summary><strong>Q6: How does Step Functions handle retries with exponential backoff?</strong></summary>

In ASL, tasks support a `Retry` array with configurable parameters:
```json
"Retry": [
  {
    "ErrorEquals": ["DatabaseUnavailableException", "Lambda.TooManyRequestsException"],
    "IntervalSeconds": 1,
    "MaxAttempts": 4,
    "BackoffRate": 2.0
  }
]
```
If an eligible error occurs, Step Functions automatically waits 1s, then 2s, then 4s, then 8s before failing over to the `Catch` block, preventing downstream service exhaustion during temporary blips.
</details>

<details>
<summary><strong>Q7: What is an optimized direct service integration in Step Functions?</strong></summary>

An optimized integration allows Step Functions to directly call over 200 AWS services and 9,000 API actions directly from the state machine definition without writing custom Lambda code. For example, Step Functions can read from DynamoDB (`dynamodb:getItem`), publish to SNS (`sns:publish`), put records to EventBridge (`events:putEvents`), or run ECS container tasks (`ecs:runTask.sync`) natively.
</details>

<details>
<summary><strong>Q8: What is the maximum payload size supported between Step Functions states and how do you bypass it?</strong></summary>

The maximum input/output payload size for a state transition is **256 KB**. If your task produces a large payload (e.g., a high-resolution image analysis or large JSON dump), store the payload in an Amazon S3 bucket and pass the S3 bucket name and object key (`{"bucket": "my-bucket", "key": "data.json"}`) in the state transition payload (Claim Check Pattern).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Basic Two-Step State Machine with Choice Branching</strong></summary>

**Goal**: Build a functional state machine in the AWS Console.
1. Create a Standard state machine with a `Pass` state injecting `{"score": 85}`.
2. Add a `Choice` state: if `score > 70` go to `SucceedState`, else go to `FailState`.
3. Execute the state machine with different inputs and observe execution paths.
</details>

<details>
<summary><strong>Exercise 2: Implement Exponential Backoff Retries</strong></summary>

**Goal**: Verify retry mechanics on failure.
1. Deploy a Lambda function that always throws an error.
2. Configure a `Task` state with `IntervalSeconds: 2`, `MaxAttempts: 3`, `BackoffRate: 2.0`.
3. Run execution and inspect execution history timestamps: verify 2s, 4s, 8s wait intervals.
</details>

<details>
<summary><strong>Exercise 3: Test Asynchronous Task Token Callbacks</strong></summary>

**Goal**: Pause and resume a workflow using a Task Token.
1. Define a state using `.waitForTaskToken`.
2. Output the token to a CloudWatch log or SQS queue.
3. Verify state execution is `In Progress` / paused.
4. Call `aws stepfunctions send-task-success --task-token <TOKEN> --output '{"status":"ok"}'`.
5. Observe the workflow resume immediately and succeed.
</details>

<details>
<summary><strong>Exercise 4: Direct DynamoDB Service Integration</strong></summary>

**Goal**: Write to DynamoDB without a Lambda function.
1. Create a DynamoDB table `Orders`.
2. Configure a Step Functions task with resource `arn:aws:states:::dynamodb:putItem`.
3. Pass `{"TableName": "Orders", "Item": {"orderId": {"S.$": "$.id"}}}`.
4. Execute and verify the item appears in the DynamoDB table.
</details>

<details>
<summary><strong>Exercise 5: Build a Parallel State with Error Catching</strong></summary>

**Goal**: Run two independent tasks concurrently.
1. Create a `Parallel` state with two branches: Branch A (succeeds) and Branch B (fails).
2. Attach a `Catch` block to the `Parallel` state.
3. Verify that failure in Branch B triggers the catch block cleanly.
</details>

<details>
<summary><strong>Exercise 6: Execute an Express Workflow from API Gateway</strong></summary>

**Goal**: Synchronous, sub-second API execution.
1. Create an Express Workflow that returns a calculated price.
2. Configure an HTTP API Gateway with direct Step Functions integration (`StartSyncExecution`).
3. Invoke the API with `curl` and verify the HTTP response contains the state machine output.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| STEP FUNCTIONS ASL COMMON STATE PATTERNS                                                          |
+--------------------------+-----------------------+------------------------------------------------+
| State Type               | Primary Purpose       | Key Configuration Attributes                   |
+--------------------------+-----------------------+------------------------------------------------+
| Task                     | Performs units of work| Resource, Parameters, ResultPath, Retry, Catch |
| Choice                   | Conditional branching | Variable, Comparison Operators (String, Numeric|
| Wait                     | Delays execution      | Seconds, Timestamp, SecondsPath                |
| Parallel                 | Concurrent branches   | Branches array                                 |
| Map (Inline)             | Array iteration       | ItemsPath, ItemProcessor                       |
| Map (Distributed)        | Massive S3 batch ETL  | Mode: DISTRIBUTED, ItemReader, ResultWriter    |
| Pass                     | Mock / inject data    | Result, ResultPath                             |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Standard**: Long duration (1 year), exactly-once, visual auditing.
- **Express**: Ultra-fast (< 5 min), high-volume microservices.
- **Saga Pattern**: Resilient compensating rollbacks for distributed microservices.

---

## 13. Further Reading

- [AWS Step Functions Developer Guide](https://docs.aws.amazon.com/step-functions/latest/dg/welcome.html)
- [Amazon States Language (ASL) Specification](https://states-language.net/)
- [Step Functions Distributed Map Architecture](https://aws.amazon.com/blogs/aws/step-functions-distributed-map-a-serverless-solution-for-large-scale-parallel-data-processing/)
- [Serverless Saga Pattern with AWS Step Functions](https://aws.amazon.com/blogs/compute/building-a-serverless-distributed-application-using-a-saga-pattern/)
- [05-app-services/01-sqs.md](file:///04-devops/aws/05-app-services/01-sqs.md)
