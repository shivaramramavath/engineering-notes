# AWS Batch & Step Functions Workflow Orchestration

> **Module**: `02-compute`  
> **Topic**: Distributed High-Throughput Batch Computing & Serverless Workflow Orchestration  
> **Target Audience**: Data Engineers, Cloud Architects, Distributed Systems Engineers  
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

Modern data architectures frequently handle asynchronous, long-running, and computationally intensive workloads: rendering 3D graphics, running financial risk simulations, training machine learning models, or processing terabytes of media files.

Running these workloads on standard web servers or synchronous Lambda functions causes operational failures:
- Lambda imposes a strict **15-minute maximum timeout**.
- Web servers and standard containers are not optimized to dynamically provision thousands of spot vCPUs, schedule task dependencies, and terminate instances immediately once computations finish.
- Writing custom retry logic, exception handling, and parallel execution branches inside monolithic application code leads to fragile, unmaintainable systems.

**AWS Batch** and **AWS Step Functions** form the industry-standard architecture for high-throughput distributed processing:
- **AWS Batch**: Plans, schedules, and executes containerized batch workloads across dynamically scaled fleets of EC2 instances, AWS Fargate, or Amazon EKS.
- **AWS Step Functions**: A low-code visual workflow orchestrator that coordinates distributed services, manages error handling and retries, and coordinates parallel execution paths using finite state machines.

---

## 2. Core Concepts

### 2.1 AWS Batch Architectural Primitives
- **Job**: A unit of work (e.g., a shell script or Docker container execution) submitted to AWS Batch. Transitions through states: `SUBMITTED -> PENDING -> RUNNABLE -> STARTING -> RUNNING -> SUCCEEDED / FAILED`.
- **Job Definition**: The blueprint declaring how a job runs: container image, vCPU and RAM requirements, IAM execution roles, environment variables, and mount points. Supports multi-node parallel jobs for distributed MPI workloads.
- **Job Queue**: Stores submitted jobs until compute resources become available. Queues have configurable priorities, scheduling policies (FIFO vs Fair Share), and map to one or more Compute Environments.
- **Compute Environment**: The compute cluster running the jobs:
  - **Managed**: AWS automatically provisions, scales, and terminates EC2 instances or Fargate tasks based on queue demand.
  - **Unmanaged**: You manually provision and configure the underlying container instances.
  - **Compute Types**: EC2 On-Demand, EC2 Spot, Fargate, Fargate Spot, or Amazon EKS.

### 2.2 AWS Step Functions State Machine Concepts
- **Amazon States Language (ASL)**: JSON-based declarative language defining state machine structure.
- **State Types**:
  - `Task`: Performs work (invokes Lambda, submits an AWS Batch job, queries DynamoDB).
  - `Choice`: Evaluates branching logic based on input data.
  - `Parallel`: Executes multiple distinct state branches concurrently.
  - `Map`: Loops over an array of items, executing the same state flow for each item.
    - **Inline Map**: In-memory iteration (max 40 concurrent iterations).
    - **Distributed Map**: High-throughput iteration (up to 10,000 concurrent executions) processing massive datasets directly from Amazon S3.
  - `Pass` / `Wait` / `Fail` / `Succeed`: Flow-control and terminal states.

### 2.3 Step Functions Workflow Types: Standard vs. Express
- **Standard Workflows**:
  - **Execution Duration**: Up to **1 year**.
  - **Execution Model**: **Exactly-once** execution.
  - **History & Auditing**: Full visual execution history in the console, step-by-step state inspection.
  - **Pricing**: Billed per state transition ($0.025 per 1,000 transitions).
- **Express Workflows**:
  - **Execution Duration**: Up to **5 minutes**.
  - **Execution Model**: **At-least-once** (Asynchronous) or **At-most-once** (Synchronous).
  - **History**: Logs streamed to CloudWatch Logs (no visual step replay in console).
  - **Pricing**: Billed per GB-second of compute and request count (ideal for high-volume microservice APIs processing 100,000+ events/sec).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|               Distributed Batch Orchestration Pipeline Topology                   |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Ingest Trigger: S3 Object Upload / EventBridge Cron ]                          |
|             |                                                                     |
|             v                                                                     |
|  +-----------------------------------------------------------------------------+  |
|  |                 AWS Step Functions State Machine (Standard)                 |  |
|  |                                                                             |  |
|  |   [ Step 1: Validate Payload (Lambda Task) ]                                |  |
|  |                       |                                                     |  |
|  |                       v                                                     |  |
|  |   [ Step 2: Submit Batch Processing Job (Synchronous Service Integration) ] |  |
|  |   (Resource: "arn:aws:states:::batch:submitJob.sync")                       |  |
|  +-----------------------|-----------------------------------------------------+  |
|                          |                                                        |
|                          | Submits Job & Waits for SUCCEEDED status               |
|                          v                                                        |
|  +-----------------------------------------------------------------------------+  |
|  |                             AWS Batch Service                               |  |
|  |                                                                             |  |
|  |  [ High-Priority Job Queue ]                                                |  |
|  |              |                                                              |  |
|  |              v (Evaluates pending vCPUs)                                    |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |  | Managed Compute Environment (Spot Fleet / Graviton Instances)         |  |  |
|  |  | - Automatically launches 50x c7g.4xlarge EC2 Spot instances         |  |  |
|  |  | - Pulls processing container from Amazon ECR                          |  |  |
|  |  | - Processes raw data chunk, writes output to S3                        |  |  |
|  |  | - Shuts down EC2 instances when Job Queue reaches 0 pending           |  |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  +-----------------------|-----------------------------------------------------+  |
|                          |                                                        |
|                          | Job Completes -> Step Functions resumes                |
|                          v                                                        |
|  +-----------------------------------------------------------------------------+  |
|  |   [ Step 3: Send SNS Completion Notification ]                              |  |
|  |                       |                                                     |  |
|  |                       v                                                     |  |
|  |   [ Step 4: Succeed State ]                                                 |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating AWS Batch Resources via CLI
```bash
# 1. Create Managed Compute Environment with Spot Instances
aws batch create-compute-environment \
  --compute-environment-name "spot-compute-env" \
  --type MANAGED \
  --state ENABLED \
  --compute-resources '{
    "type": "SPOT",
    "allocationStrategy": "SPOT_CAPACITY_OPTIMIZED",
    "minvCpus": 0,
    "maxvCpus": 256,
    "desiredvCpus": 0,
    "instanceTypes": ["optimal"],
    "subnets": ["subnet-01a", "subnet-01b"],
    "securityGroupIds": ["sg-0123456789abcdef0"],
    "instanceRole": "arn:aws:iam::111122223333:instance-profile/ecsInstanceRole"
  }' \
  --service-role "arn:aws:iam::111122223333:role/service-role/AWSBatchServiceRole"

# 2. Create Job Queue
aws batch create-job-queue \
  --job-queue-name "high-priority-queue" \
  --state ENABLED \
  --priority 1 \
  --compute-environment-order '[{"order": 1, "computeEnvironment": "spot-compute-env"}]'

# 3. Register Job Definition
aws batch register-job-definition \
  --job-definition-name "data-cruncher" \
  --type container \
  --container-properties '{
    "image": "111122223333.dkr.ecr.us-east-1.amazonaws.com/batch-worker:latest",
    "vcpus": 4,
    "memory": 8192,
    "command": ["python", "process.py", "--input", "Ref::inputFile"]
  }'
```

### 4.2 Step Functions ASL Definition with Synchronous Batch Integration
```json
{
  "Comment": "Data Processing Orchestration Pipeline",
  "StartAt": "SubmitBatchJob",
  "States": {
    "SubmitBatchJob": {
      "Type": "Task",
      "Resource": "arn:aws:states:::batch:submitJob.sync",
      "Parameters": {
        "JobName": "DailyDataProcessingJob",
        "JobQueue": "arn:aws:batch:us-east-1:111122223333:job-queue/high-priority-queue",
        "JobDefinition": "arn:aws:batch:us-east-1:111122223333:job-definition/data-cruncher:1",
        "Parameters": {
          "inputFile.$": "$.s3Uri"
        }
      },
      "Retry": [
        {
          "ErrorEquals": ["Batch.AWSBatchException"],
          "IntervalSeconds": 30,
          "MaxAttempts": 3,
          "BackoffRate": 2.0
        }
      ],
      "Catch": [
        {
          "ErrorEquals": ["States.ALL"],
          "Next": "HandleJobFailure"
        }
      ],
      "Next": "NotifySuccess"
    },
    "NotifySuccess": {
      "Type": "Task",
      "Resource": "arn:aws:states:::sns:publish",
      "Parameters": {
        "TopicArn": "arn:aws:sns:us-east-1:111122223333:ProcessingAlerts",
        "Message": "Batch job completed successfully."
      },
      "End": true
    },
    "HandleJobFailure": {
      "Type": "Task",
      "Resource": "arn:aws:states:::sns:publish",
      "Parameters": {
        "TopicArn": "arn:aws:sns:us-east-1:111122223333:ProcessingAlerts",
        "Message": "Batch job failed execution."
      },
      "End": true
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Declarative Terraform State Machine with Batch Integration
Provision an end-to-end Step Function coordinating an AWS Batch pipeline.

```hcl
resource "aws_iam_role" "step_functions_role" {
  name = "batch-orchestrator-sfn-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "states.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy" "sfn_batch_policy" {
  name = "sfn-batch-policy"
  role = aws_iam_role.step_functions_role.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect   = "Allow"
        Action   = ["batch:SubmitJob", "batch:DescribeJobs", "batch:TerminateJob"]
        Resource = "*"
      },
      {
        Effect   = "Allow"
        Action   = ["events:PutTargets", "events:PutRule", "events:DescribeRule"]
        Resource = "*"
      }
    ]
  })
}

resource "aws_sfn_state_machine" "batch_pipeline" {
  name     = "nightly-data-processing-pipeline"
  role_arn = aws_iam_role.step_functions_role.arn
  type     = "STANDARD"

  definition = jsonencode({
    Comment = "Orchestrate AWS Batch via Step Functions"
    StartAt = "ProcessDataBatch"
    States = {
      ProcessDataBatch = {
        Type     = "Task"
        Resource = "arn:aws:states:::batch:submitJob.sync"
        Parameters = {
          JobName       = "NightlyETL"
          JobQueue      = var.batch_job_queue_arn
          JobDefinition = var.batch_job_definition_arn
        }
        End = true
      }
    }
  })
}
```

### Example 2: Distributed Map Processing 100,000 S3 Objects in Parallel
Process an entire Amazon S3 bucket prefix containing hundreds of thousands of CSV files using Step Functions Distributed Map.

```json
{
  "StartAt": "ProcessMassiveS3Dataset",
  "States": {
    "ProcessMassiveS3Dataset": {
      "Type": "Map",
      "ItemProcessor": {
        "ProcessorConfig": {
          "Mode": "DISTRIBUTED",
          "ExecutionType": "EXPRESS"
        },
        "StartAt": "ProcessSingleFile",
        "States": {
          "ProcessSingleFile": {
            "Type": "Task",
            "Resource": "arn:aws:states:::lambda:invoke",
            "OutputPath": "$.Payload",
            "Parameters": {
              "FunctionName": "arn:aws:lambda:us-east-1:111122223333:function:ParseCsvItem",
              "Payload.$": "$"
            },
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
          "Bucket": "prod-financial-data-bucket",
          "Key": "2026/transactions.csv"
        }
      },
      "MaxConcurrency": 1000,
      "End": true
    }
  }
}
```

### Example 3: Multi-Branch Parallel Workflow with Dynamic Catching
Process video transcoding and audio transcription concurrently, aggregating results before publishing.

```json
{
  "StartAt": "ParallelMediaAnalysis",
  "States": {
    "ParallelMediaAnalysis": {
      "Type": "Parallel",
      "Branches": [
        {
          "StartAt": "TranscodeVideo",
          "States": {
            "TranscodeVideo": {
              "Type": "Task",
              "Resource": "arn:aws:states:::batch:submitJob.sync",
              "Parameters": {
                "JobName": "TranscodeVideoTask",
                "JobQueue": "arn:aws:batch:us-east-1:111122223333:job-queue/transcode-queue",
                "JobDefinition": "arn:aws:batch:us-east-1:111122223333:job-definition/ffmpeg-transcoder:1"
              },
              "End": true
            }
          }
        },
        {
          "StartAt": "TranscribeAudio",
          "States": {
            "TranscribeAudio": {
              "Type": "Task",
              "Resource": "arn:aws:states:::lambda:invoke",
              "Parameters": {
                "FunctionName": "arn:aws:lambda:us-east-1:111122223333:function:ExtractAudio"
              },
              "End": true
            }
          }
        }
      ],
      "Next": "PublishCombinedOutput"
    },
    "PublishCombinedOutput": {
      "Type": "Task",
      "Resource": "arn:aws:states:::sns:publish",
      "Parameters": {
        "TopicArn": "arn:aws:sns:us-east-1:111122223333:MediaReadyTopic",
        "Message": "Both video transcode and audio transcription finished successfully."
      },
      "End": true
    }
  }
}
```

### Example 4: Submitting an Array Batch Job for High-Density Iteration
Submit a single job definition that executes 1,000 independent array child tasks simultaneously.

```bash
aws batch submit-job \
  --job-name "parameter-sweep-analysis" \
  --job-queue "high-priority-queue" \
  --job-definition "data-cruncher:1" \
  --array-properties size=1000
```
*Inside the container, software reads the environment variable `AWS_BATCH_JOB_ARRAY_INDEX` (values `0` to `999`) to select its specific dataset partition.*

### Example 5: Handling Timeout and Callback Tokens (`waitForTaskToken`)
Pause a Step Functions workflow and wait indefinitely until an external human approval email link is clicked.

```json
{
  "StartAt": "WaitForManualApproval",
  "States": {
    "WaitForManualApproval": {
      "Type": "Task",
      "Resource": "arn:aws:states:::sqs:sendMessage.waitForTaskToken",
      "Parameters": {
        "QueueUrl": "https://sqs.us-east-1.amazonaws.com/111122223333/ApprovalQueue",
        "MessageBody": {
          "TaskToken.$": "$$.Task.Token",
          "OrderId.$": "$.orderId",
          "Amount.$": "$.totalAmount"
        }
      },
      "TimeoutSeconds": 86400,
      "Next": "ExecutePayment"
    },
    "ExecutePayment": {
      "Type": "Task",
      "Resource": "arn:aws:states:::lambda:invoke",
      "Parameters": { "FunctionName": "ProcessPayment" },
      "End": true
    }
  }
}
```

---

## 6. Best Practices

1. **Use Synchronous (`.sync`) Service Integrations**: When invoking AWS Batch or ECS from Step Functions, always append `.sync` to the resource ARN (e.g., `arn:aws:states:::batch:submitJob.sync`). Step Functions automatically manages polling, waits for terminal status, and captures failure logs without writing custom Lambda polling loops.
2. **Leverage Spot Capacity Optimized for AWS Batch**: Set `allocationStrategy: "SPOT_CAPACITY_OPTIMIZED"` in Batch Compute Environments. AWS Batch selects instances from Spot pools with the lowest historical interruption risk, maximizing job completion rates.
3. **Use Distributed Map for Large-Scale ETL**: For processing millions of files or gigabytes of CSV/JSON data, use Step Functions Distributed Map. It distributes iterations across serverless workers in parallel without requiring dedicated Apache Spark or Hadoop infrastructure.
4. **Choose Express Workflows for High-Throughput Microservices**: For workflows executing under 5 minutes at high invocation rates (e.g., payment validation APIs), choose **Express Workflows** to cut state transition costs by over 90%.
5. **Always Implement Exponential Backoff Retries**: Add a `Retry` block with `BackoffRate: 2.0` on every task state that touches external networks or databases to handle transient rate limits and connection drops gracefully.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Using Standard Workflows for IoT Ingest** | Millions of state transitions result in massive unexpected Step Functions monthly bills. | Use **Express Workflows**, billed by execution time and memory rather than state transitions. |
| **Omitting `.sync` on Batch Tasks** | Step Functions fires the job and immediately proceeds to the next state while the job is still queued. | Use `arn:aws:states:::batch:submitJob.sync` to ensure Step Functions waits for job completion. |
| **Setting Batch `minvCpus` Greater Than Zero** | EC2 instances run continuously 24/7 even when no jobs exist in the queue, incurring idle costs. | Set `minvCpus = 0` so the compute environment scales down to zero instances when the queue is empty. |
| **Hardcoding Single Instance Type in Batch** | When that specific EC2 instance type runs out of Spot capacity in the AZ, all batch jobs stall. | Specify `"optimal"` or list multiple diverse instance families (`["c7g", "m7g", "r7g"]`). |
| **Exceeding 256 KB State Payload Limit** | Step Functions throws `States.DataLimitExceeded` when passing massive JSON payloads between states. | Use the **Claim Check Pattern**: write heavy data to Amazon S3 and pass the S3 URI between states. |
| **Missing `TimeoutSeconds` on Human Tasks** | Workflows with `waitForTaskToken` remain in an active running state forever if nobody responds. | Always configure a reasonable `TimeoutSeconds` (e.g. 86400s) paired with a `Catch` handler. |

---

## 8. Performance, Security & Cost Notes

### Scaling & Concurrency Limits
- **AWS Batch Maximum vCPUs**: Governed by the `maxvCpus` setting on the Compute Environment and regional EC2 vCPU quotas.
- **Distributed Map Scale**: Can launch up to **10,000 parallel executions** simultaneously, achieving massive multi-gigabyte per-second processing throughput.

### Security Isolation
- **IAM PassRole Boundaries**: The Step Functions execution role requires `iam:PassRole` permissions to pass the execution role to AWS Batch and Lambda.
- **Container Sandbox**: AWS Batch runs each job in an isolated Docker container on ECS or Fargate, isolating host filesystem state between job runs.

### Cost Breakdown
- **AWS Batch**: 100% Free management service. You pay only for the raw EC2, Spot, or Fargate compute consumed.
- **Step Functions Standard**: $0.025 per 1,000 state transitions.
- **Step Functions Express**: $1.00 per 1,000,000 executions + $0.00001667 per GB-second of execution time.

---

## 9. Comparison Table

| Attribute | AWS Batch | AWS Lambda | AWS Step Functions |
|:---|:---|:---|:---|
| **Primary Purpose** | Long-running container batch compute | Event-driven short function compute | Distributed workflow orchestration |
| **Execution Limit** | Indefinite (days or weeks) | 15 minutes (900 seconds) | 1 year (Standard) / 5 min (Express)|
| **Compute Packaging** | Docker containers (ECR) | Zip code packages or containers | N/A (Coordinates other services) |
| **Instance Hardware** | Full EC2 hardware (GPUs, NVMe, HPC) | Abstracted MicroVM (vCPU/RAM only) | Serverless managed state engine |
| **Pricing Model** | Underlying EC2 / Fargate runtime | Per request + per millisecond duration| Per state transition or GB-second |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural difference between Step Functions Standard and Express Workflows?</strong></summary>

**Answer:**
- **Standard Workflows**: Built for long-running, durable, mission-critical orchestrations (up to 1 year duration). They enforce **exactly-once** task execution semantics, provide a visual step-by-step audit history in the AWS Management Console, and are billed per individual state transition ($0.025 per 1,000 transitions).
- **Express Workflows**: Built for high-volume, short-duration event processing (up to 5 minutes duration). They enforce **at-least-once** execution (asynchronous) or **at-most-once** (synchronous), stream execution logs exclusively to Amazon CloudWatch, and are billed based on execution duration and memory consumption. Express is significantly cheaper for workloads exceeding 100,000 executions per day.
</details>

<details>
<summary><strong>Q2: How does the `.sync` service integration pattern work in AWS Step Functions?</strong></summary>

**Answer:**
When a task state specifies a resource ending in `.sync` (such as `arn:aws:states:::batch:submitJob.sync` or `arn:aws:states:::ecs:runTask.sync`):
1. Step Functions calls the target service API to initiate the job.
2. Rather than immediately transitioning to the next state, Step Functions establishes an automated EventBridge rule listener tracking the job's execution status.
3. The execution pauses in the Task state until the target service emits a terminal state (`SUCCEEDED`, `FAILED`, `STOPPED`).
4. If the job succeeds, Step Functions injects the output into the next state. If it fails, Step Functions evaluates the task's `Retry` and `Catch` blocks. This completely eliminates manual polling Lambda functions.
</details>

<details>
<summary><strong>Q3: How does AWS Batch manage EC2 Spot instance capacity scaling?</strong></summary>

**Answer:**
In a managed compute environment configured with Spot instances:
- When jobs are submitted to a Job Queue, the Batch scheduler evaluates the total vCPU and memory demands of pending jobs.
- With `allocationStrategy: "SPOT_CAPACITY_OPTIMIZED"`, AWS Batch inspects real-time Spot capacity depth across all Availability Zones and launches instances from the Spot pools least likely to be interrupted.
- When instances are launched, the Batch container agent places containers onto instances until memory/CPU capacity is filled.
- When the queue empties and jobs complete, Batch automatically terminates idle Spot instances, scaling capacity back to `minvCpus` (typically 0).
</details>

<details>
<summary><strong>Q4: What is an AWS Batch Array Job and when should it be used?</strong></summary>

**Answer:**
An Array Job submits a single job definition that spawns up to 10,000 identical container tasks running in parallel.
Each child task receives an identical configuration except for a unique environment variable: `AWS_BATCH_JOB_ARRAY_INDEX` (ranging from `0` to `N-1`). Applications use this index to partition massive datasets (e.g., child task 45 processes slice 45 of a video file or partition 45 of a database table). Array jobs simplify parameter sweep simulations, monte carlo modeling, and distributed file processing without submitting thousands of individual API requests.
</details>

<details>
<summary><strong>Q5: What is the Step Functions Distributed Map state and how does it scale?</strong></summary>

**Answer:**
Distributed Map allows Step Functions to iterate over massive datasets stored in Amazon S3 (millions of objects, CSV rows, or JSON items).
Unlike standard in-memory Map states (which are capped at 40 concurrent iterations and limited by state payload boundaries), Distributed Map spawns up to **10,000 concurrent child workflow executions**. It reads the source data directly from Amazon S3, distributes chunks across child executions, and aggregates success/failure results into S3 output files, enabling serverless petabyte-scale data processing.
</details>

<details>
<summary><strong>Q6: How does the `waitForTaskToken` callback pattern work in Step Functions?</strong></summary>

**Answer:**
The `waitForTaskToken` pattern pauses a state machine execution until an external system delivers a completion signal:
1. A Task state specifies `.waitForTaskToken` (e.g., publishing to an SQS queue or calling an external webhook).
2. Step Functions generates a unique, cryptographically signed **Task Token** (`$$.Task.Token`) and includes it in the task payload.
3. The state machine pauses execution indefinitely (up to 1 year).
4. When the external system or human completes the task (e.g., clicking an approval link in an email), it calls `aws stepfunctions send-task-success --task-token <TOKEN> --task-output <JSON>`.
5. Step Functions receives the token, matches it to the paused execution, and resumes the workflow.
</details>

<details>
<summary><strong>Q7: What happens when an AWS Batch job running on an EC2 Spot instance receives a 2-minute interruption notice?</strong></summary>

**Answer:**
When AWS reclaims Spot capacity, the underlying EC2 host receives a 2-minute termination notice:
1. AWS Batch detects the interruption signal via EventBridge.
2. The running container receives a `SIGTERM` followed by a `SIGKILL`.
3. AWS Batch marks the job status as `FAILED` with a status reason indicating Spot interruption.
4. If configured with **Job Retry Strategies**, AWS Batch automatically re-queues the job and places it onto an alternate healthy Spot instance or On-Demand instance without losing the workflow.
</details>

<details>
<summary><strong>Q8: How do you prevent sensitive state payload data from leaking in Step Functions CloudWatch logs?</strong></summary>

**Answer:**
- In **Standard Workflows**, execution history is stored internally within Step Functions and encrypted using AWS KMS.
- In **Express Workflows**, logs are written directly to CloudWatch Logs. To prevent sensitive PII or credentials from being logged:
  - Configure the logging level to `ERROR` instead of `ALL`.
  - Disable data logging by setting `includeExecutionData: false` in the state machine logging configuration.
  - Avoid passing raw secrets through state inputs/outputs; pass secret ARNs and have worker tasks retrieve secrets directly from AWS Secrets Manager.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an AWS Batch Compute Environment via CLI</strong></summary>

**Objective**: Provision an ephemeral Spot compute environment scaling from 0 to 64 vCPUs.

**Command**:
```bash
aws batch create-compute-environment \
  --compute-environment-name "lab-spot-env" \
  --type MANAGED \
  --state ENABLED \
  --compute-resources '{
    "type": "SPOT",
    "allocationStrategy": "SPOT_CAPACITY_OPTIMIZED",
    "minvCpus": 0,
    "maxvCpus": 64,
    "instanceTypes": ["c7g.large", "m7g.large"],
    "subnets": ["subnet-01a"],
    "securityGroupIds": ["sg-01234"],
    "instanceRole": "arn:aws:iam::111122223333:instance-profile/ecsInstanceRole"
  }' \
  --service-role "arn:aws:iam::111122223333:role/AWSBatchServiceRole"
```
</details>

<details>
<summary><strong>Exercise 2: Create a Step Functions State Machine via CLI</strong></summary>

**Objective**: Deploy a minimal state machine testing Choice state branching.

**Command**:
```bash
cat <<'EOF' > definition.json
{
  "StartAt": "CheckValue",
  "States": {
    "CheckValue": {
      "Type": "Choice",
      "Choices": [
        {"Variable": "$.score", "NumericGreaterThan": 80, "Next": "HighPass"}
      ],
      "Default": "StandardPass"
    },
    "HighPass": {"Type": "Pass", "Result": "Distinction", "End": true},
    "StandardPass": {"Type": "Pass", "Result": "Passed", "End": true}
  }
}
EOF

aws stepfunctions create-state-machine \
  --name "ScoreEvaluator" \
  --definition file://definition.json \
  --role-arn "arn:aws:iam::111122223333:role/StepFunctionsExecutionRole"
```
</details>

<details>
<summary><strong>Exercise 3: Submit an AWS Batch Job via CLI</strong></summary>

**Objective**: Dispatch a container computation passing dynamic input parameters.

**Command**:
```bash
aws batch submit-job \
  --job-name "cli-test-run" \
  --job-queue "high-priority-queue" \
  --job-definition "data-cruncher:1" \
  --parameters '{"inputFile": "s3://my-bucket/dataset-01.parquet"}'
```
</details>

<details>
<summary><strong>Exercise 4: Test Step Functions Execution via CLI</strong></summary>

**Objective**: Start state machine execution with JSON input and inspect output.

**Command**:
```bash
EXECUTION_ARN=$(aws stepfunctions start-execution \
  --state-machine-arn "arn:aws:states:us-east-1:111122223333:stateMachine:ScoreEvaluator" \
  --input '{"score": 95}' \
  --query 'executionArn' --output text)

aws stepfunctions describe-execution --execution-arn "$EXECUTION_ARN"
```
</details>

<details>
<summary><strong>Exercise 5: Configure Automatic Retries on AWS Batch Job Definitions</strong></summary>

**Objective**: Configure automatic retries for Spot interruption exit codes.

**Command**:
```bash
aws batch register-job-definition \
  --job-definition-name "resilient-batch-job" \
  --type container \
  --retry-strategy '{"attempts": 3, "evaluateOnExit": [{"onStatusReason": "Host EC2*", "action": "RETRY"}]}' \
  --container-properties '{"image": "alpine", "vcpus": 1, "memory": 512, "command": ["echo", "test"]}'
```
</details>

<details>
<summary><strong>Exercise 6: Test Human Approval via `send-task-success`</strong></summary>

**Objective**: Resume a paused `waitForTaskToken` execution using a CLI callback.

**Steps**:
1. Start an execution containing `waitForTaskToken`.
2. Inspect the SQS queue and extract the `TaskToken` string.
3. Call `aws stepfunctions send-task-success --task-token <TOKEN> --task-output '{"approved": true}'`.
4. Observe the state machine resume and transition to the next state.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                         Batch & Step Functions Architectural Roles                    |
+---------------------------------------------------------------------------------------+
|  Service               | Responsibilities                                             |
|------------------------+--------------------------------------------------------------|
|  AWS Batch             | Dynamically scales EC2/Spot fleets to run Docker containers  |
|  Compute Environment   | Defines EC2 instance families, Spot allocation, min/max vCPUs|
|  Job Queue             | Manages priorities, scheduling policies (FIFO/FairShare)     |
|  Step Functions Std    | Long-running (up to 1yr), visual debugging, exactly-once     |
|  Step Functions Express| High-throughput (up to 5min), sub-second microservices      |
|  Distributed Map       | Petabyte-scale S3 file processing (up to 10k parallel tasks) |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Batch User Guide](https://docs.aws.amazon.com/batch/latest/userguide/what-is-batch.html)
- [AWS Step Functions Developer Guide](https://docs.aws.amazon.com/step-functions/latest/dg/welcome.html)
- [Amazon States Language Specification](https://states-language.net/)
- [Step Functions Distributed Map Patterns](https://docs.aws.amazon.com/step-functions/latest/dg/concepts-asl-use-map-state-distributed.html)
- [Optimizing AWS Batch with EC2 Spot Instances](https://aws.amazon.com/blogs/compute/optimizing-aws-batch-with-ec2-spot-instances/)
