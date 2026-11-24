---
project: "Event-Driven Order Processing Pipeline with EventBridge, SQS, and DLQ"
module: "08-projects"
track: "aws"
difficulty: "intermediate"
architecture: "event-driven-pipeline"
last-tested: "2026-11"
tags:
  - "eventbridge"
  - "sqs"
  - "dlq"
  - "lambda"
  - "cloudwatch-alarms"
  - "terraform"
  - "project"
---

# Event-Driven Order Processing Pipeline with EventBridge, SQS, and DLQ

## Table of Contents
- [1. Project Topology & Specifications](#1-project-topology--specifications)
- [2. Declarative Code (Terraform)](#2-declarative-code-terraform)
- [3. Verification & Automated Test Harness](#3-verification--automated-test-harness)
- [4. Hardening, Security & Cost Analysis](#4-hardening-security--cost-analysis)
- [5. Operational Runbook & Disaster Recovery](#5-operational-runbook--disaster-recovery)

---

## 1. Project Topology & Specifications

### 1.1 Architecture Overview
This production project implements a robust, asynchronous **Event-Driven Order Processing Pipeline** designed for high throughput, zero message loss, and automated failure isolation.

Key Architectural Guarantees:
- **Central Event Routing (Amazon EventBridge)**: Producers emit business domain events (`OrderPlaced`, `PaymentProcessed`) to a custom event bus. EventBridge routes events based on declarative JSON pattern matching.
- **Backpressure & Decoupling (Amazon SQS)**: Targets are decoupled via persistent SQS queues rather than direct Lambda invocations, preventing concurrency throttling and protecting downstream databases.
- **Automated Poison Pill Isolation (Dead-Letter Queue)**: If a message fails processing 3 times (e.g., due to malformed payload or transient downstream outage), SQS automatically moves it to an isolated **Dead-Letter Queue (DLQ)** with a 14-day retention.
- **Real-Time Operational Alerting**: A CloudWatch Alarm monitors DLQ depth (`ApproximateNumberOfMessagesVisible >= 1`) to alert on-call engineers immediately upon message failure.

```
+----------------------------------------------------------------------------------------------------+
|                         EVENT-DRIVEN ORDER PIPELINE TOPOLOGY                                       |
+----------------------------------------------------------------------------------------------------+

  [ Order Checkout Producer ]
             |
             | 1. PutEvents (JSON Event)
             v
  +--------------------------------------------------------------------------------------------------+
  | Amazon EventBridge (Custom Bus: `order-processing-bus`)                                          |
  |                                                                                                  |
  |  Rule: Source = "ecommerce.checkout" AND Detail-Type = "OrderPlaced"                             |
  +--------------------------------------------------------------------------------------------------+
             |
             | 2. Route matching event
             v
  +--------------------------------------------------------------------------------------------------+
  | Amazon SQS Queue (`order-fulfillment-queue`)                                                     |
  |  - Visibility Timeout: 60 seconds (6x Lambda timeout)                                            |
  |  - Redrive Policy: maxReceiveCount = 3 -> Redrive to DLQ                                         |
  +--------------------------------------------------------------------------------------------------+
        |                                                              |
        | 3. Event Source Mapping (Batch: 10)                          | 4. Max Retries Exceeded (3x)
        v                                                              v
  +---------------------------------------+      +---------------------------------------------------+
  | AWS Lambda Worker                     |      | Amazon SQS Dead-Letter Queue                      |
  | (`order-fulfillment-worker`)          |      | (`order-fulfillment-dlq`)                         |
  |  - Partial Batch Failure reporting    |      |  - Message Retention: 14 days                     |
  |  - Idempotent processing              |      +---------------------------------------------------+
  +---------------------------------------+                                |
                                                                           v
                                                 +---------------------------------------------------+
                                                 | Amazon CloudWatch Metric Alarm                    |
                                                 |  - Threshold: ApproximateMessagesVisible >= 1     |
                                                 +---------------------------------------------------+
```

### 1.2 Bill of Materials
- `aws_cloudwatch_event_bus`: Custom EventBridge bus.
- `aws_sqs_queue` & `aws_sqs_queue_policy`: Main processing queue and Dead-Letter Queue.
- `aws_cloudwatch_event_rule` & `aws_cloudwatch_event_target`: Declarative routing rule.
- `aws_lambda_function`: Node.js 20 ARM64 worker with partial batch failure reporting.
- `aws_lambda_event_source_mapping`: Binds SQS queue to Lambda worker.
- `aws_cloudwatch_metric_alarm`: High-priority alert triggered by messages landing in DLQ.

---

## 2. Declarative Code (Terraform)

Create `main.tf`:

```hcl
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.40"
    }
    archive = {
      source  = "hashicorp/archive"
      version = "~> 2.4"
    }
  }
}

provider "aws" {
  region = "us-east-1"
}

variable "project_name" {
  type    = string
  default = "order-pipeline"
}

# -----------------------------------------------------------------------------
# 1. Dead-Letter Queue & Main SQS Queue
# -----------------------------------------------------------------------------
resource "aws_sqs_queue" "dlq" {
  name                      = "${var.project_name}-dlq"
  message_retention_seconds = 1209600 # 14 days
}

resource "aws_sqs_queue" "main_queue" {
  name                       = "${var.project_name}-queue"
  visibility_timeout_seconds = 60
  message_retention_seconds  = 345600 # 4 days

  redrive_policy = jsonencode({
    deadLetterTargetArn = aws_sqs_queue.dlq.arn
    maxReceiveCount     = 3
  })
}

# SQS Queue Policy granting EventBridge SendMessage permission
resource "aws_sqs_queue_policy" "queue_policy" {
  queue_url = aws_sqs_queue.main_queue.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "AllowEventBridgeSend"
        Effect    = "Allow"
        Principal = { Service = "events.amazonaws.com" }
        Action    = "sqs:SendMessage"
        Resource  = aws_sqs_queue.main_queue.arn
        Condition = {
          ArnEquals = {
            "aws:SourceArn" = aws_cloudwatch_event_rule.order_placed.arn
          }
        }
      }
    ]
  })
}

# -----------------------------------------------------------------------------
# 2. EventBridge Bus and Routing Rule
# -----------------------------------------------------------------------------
resource "aws_cloudwatch_event_bus" "bus" {
  name = "${var.project_name}-bus"
}

resource "aws_cloudwatch_event_rule" "order_placed" {
  name           = "${var.project_name}-order-placed-rule"
  event_bus_name = aws_cloudwatch_event_bus.bus.name
  description    = "Captures OrderPlaced events and forwards to SQS"

  event_pattern = jsonencode({
    source      = ["ecommerce.checkout"]
    detail-type = ["OrderPlaced"]
    detail = {
      status = ["CONFIRMED"]
    }
  })
}

resource "aws_cloudwatch_event_target" "sqs_target" {
  rule           = aws_cloudwatch_event_rule.order_placed.name
  event_bus_name = aws_cloudwatch_event_bus.bus.name
  target_id      = "SendToOrderQueue"
  arn            = aws_sqs_queue.main_queue.arn
}

# -----------------------------------------------------------------------------
# 3. Lambda Worker with Partial Batch Failure Reporting
# -----------------------------------------------------------------------------
data "archive_file" "worker_zip" {
  type        = "zip"
  output_path = "${path.module}/worker.zip"

  source {
    content = <<-EOF
      export const handler = async (event) => {
        const batchItemFailures = [];

        for (const record of event.Records) {
          try {
            const body = JSON.parse(record.body);
            const detail = typeof body.detail === "string" ? JSON.parse(body.detail) : body.detail;
            
            console.log(`Processing Order: $${detail?.orderId}, Amount: $${detail?.amount}`);

            // Simulate poison pill scenario for testing DLQ
            if (detail?.orderId === "POISON_PILL") {
              throw new Error("Intentional processing failure for DLQ validation");
            }

            // Normal successful processing
            console.log(`Successfully processed order: $${detail?.orderId}`);
          } catch (err) {
            console.error(`Error processing record $${record.messageId}:`, err);
            batchItemFailures.push({ itemIdentifier: record.messageId });
          }
        }

        // Return failed items so SQS only retries those specific records
        return { batchItemFailures };
      };
    EOF
    filename = "index.mjs"
  }
}

resource "aws_iam_role" "worker_role" {
  name = "${var.project_name}-worker-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "lambda.amazonaws.com" }
    }]
  })
}

resource "aws_iam_policy" "worker_policy" {
  name = "${var.project_name}-worker-policy"

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "sqs:ReceiveMessage",
          "sqs:DeleteMessage",
          "sqs:GetQueueAttributes"
        ]
        Resource = aws_sqs_queue.main_queue.arn
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

resource "aws_iam_role_policy_attachment" "worker_attach" {
  role       = aws_iam_role.worker_role.name
  policy_arn = aws_iam_policy.worker_policy.arn
}

resource "aws_lambda_function" "worker" {
  filename         = data.archive_file.worker_zip.output_path
  source_code_hash = data.archive_file.worker_zip.output_base64sha256
  function_name    = "${var.project_name}-worker"
  role             = aws_iam_role.worker_role.arn
  handler          = "index.handler"
  runtime          = "nodejs20.x"
  architectures    = ["arm64"]
  timeout          = 10

  environment {
    variables = {
      NODE_OPTIONS = "--enable-source-maps"
    }
  }
}

resource "aws_lambda_event_source_mapping" "sqs_trigger" {
  event_source_arn                   = aws_sqs_queue.main_queue.arn
  function_name                      = aws_lambda_function.worker.arn
  batch_size                         = 10
  function_response_types            = ["ReportBatchItemFailures"]
}

# -----------------------------------------------------------------------------
# 4. CloudWatch Alarm on DLQ Depth
# -----------------------------------------------------------------------------
resource "aws_cloudwatch_metric_alarm" "dlq_alarm" {
  alarm_name          = "${var.project_name}-dlq-messages-visible"
  comparison_operator = "GreaterThanOrEqualToThreshold"
  evaluation_periods  = 1
  metric_name         = "ApproximateNumberOfMessagesVisible"
  namespace           = "AWS/SQS"
  period              = 60
  statistic           = "Maximum"
  threshold           = 1
  alarm_description   = "Triggers when poison pill messages land in the Dead Letter Queue"

  dimensions = {
    QueueName = aws_sqs_queue.dlq.name
  }
}

output "event_bus_name" {
  value = aws_cloudwatch_event_bus.bus.name
}

output "main_queue_url" {
  value = aws_sqs_queue.main_queue.id
}

output "dlq_url" {
  value = aws_sqs_queue.dlq.id
}
```

---

## 3. Verification & Automated Test Harness

### 3.1 Deployment Instructions

```bash
terraform init
terraform apply -auto-approve
BUS_NAME=$(terraform output -raw event_bus_name)
DLQ_URL=$(terraform output -raw dlq_url)
```

### 3.2 Automated Event Ingestion & DLQ Test

```bash
# Test 1: Publish Valid Order Event to EventBridge
echo "Publishing valid order..."
aws events put-events --entries "[{
  \"EventBusName\": \"${BUS_NAME}\",
  \"Source\": \"ecommerce.checkout\",
  \"DetailType\": \"OrderPlaced\",
  \"Detail\": \"{\\\"orderId\\\": \\\"ORD-9901\\\", \\\"status\\\": \\\"CONFIRMED\\\", \\\"amount\\\": 129.50}\"
}]"

# Test 2: Publish Poison Pill Event (Should fail 3 times and move to DLQ)
echo "Publishing poison pill event..."
aws events put-events --entries "[{
  \"EventBusName\": \"${BUS_NAME}\",
  \"Source\": \"ecommerce.checkout\",
  \"DetailType\": \"OrderPlaced\",
  \"Detail\": \"{\\\"orderId\\\": \\\"POISON_PILL\\\", \\\"status\\\": \\\"CONFIRMED\\\", \\\"amount\\\": -50.00}\"
}]"

# Wait for retries and redrive to complete (~45 seconds)
echo "Waiting for retries to exhaust and message to arrive in DLQ..."
sleep 45

# Test 3: Inspect DLQ for the Poison Pill message
echo "Checking DLQ for poisoned message..."
DLQ_MSG=$(aws sqs receive-message --queue-url "${DLQ_URL}" --max-number-of-messages 1 --wait-time-seconds 5)
echo "$DLQ_MSG"
```

### 3.3 Teardown Script

```bash
terraform destroy -auto-approve
```

---

## 4. Hardening, Security & Cost Analysis

### 4.1 Reliability & Security Highlights
- **ReportBatchItemFailures**: Prevents duplicate reprocessing. In a batch of 10 records, if 1 fails, only the failed record is returned to SQS.
- **Visibility Timeout Buffer**: SQS Visibility Timeout is set to 60 seconds (6x the Lambda timeout of 10s), preventing race conditions where messages are redelivered while Lambda is still executing.
- **14-Day DLQ Retention**: Messages that land in the DLQ remain safely stored for 14 days, giving operations engineers ample time to analyze root causes and redrive messages.

### 4.2 Cost Breakdown (1 Million Events / Month)

| Component | Usage / Configuration | Estimated Monthly Cost |
|:---|:---|:---|
| **EventBridge Custom Bus** | 1,000,000 events published | \$1.00 |
| **Amazon SQS** | 1,000,000 writes + 100,000 batch reads | \$0.45 (Free tier covers 1M) |
| **AWS Lambda Worker** | 100,000 batch invocations (ARM64, 128MB, 50ms) | \$0.02 |
| **CloudWatch Alarm** | 1 Metric Alarm | \$0.10 |
| **Total Estimated Cost** | **1 Million Events / Month** | **~\$1.57 / month** |

---

## 5. Operational Runbook & Disaster Recovery

### Common Operational Procedures

#### Alert 1: DLQ Depth Alarm Triggered (`ApproximateNumberOfMessagesVisible >= 1`)
- **Diagnosis**:
  1. Inspect the message attributes and error payload:
     ```bash
     aws sqs receive-message --queue-url "$DLQ_URL" --attribute-names All
     ```
  2. Inspect Lambda CloudWatch logs for stack traces matching the poisoned message ID.
- **Redrive Runbook**:
  Once the application bug or downstream service outage is resolved, redrive DLQ messages back to the primary queue using the native AWS SQS Redrive API:
  ```bash
  aws sqs start-message-move-task \
      --source-arn $(terraform output -raw dlq_arn) \
      --destination-arn $(terraform output -raw main_queue_arn)
  ```
