# Full-Stack Cloud Observability: CloudWatch, X-Ray & OpenSearch

> **Module**: `06-operations`  
> **Topic**: Metrics, Logs Insights, Distributed Tracing, Synthetics & Managed Open Source (Prometheus/Grafana)  
> **Target Audience**: SREs, DevOps Architects, Platform Reliability Engineers  
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

In monolithic architectures, debugging a production issue was relatively straightforward: an engineer SSHed into a single server, opened `/var/log/application.log`, and inspected the stack trace. In modern distributed cloud architectures spanning dozens of microservices, serverless Lambda functions, API Gateways, container clusters (ECS/EKS), managed queues (SQS), and databases (Aurora/DynamoDB), tracking down a slow request or diagnosing a cascading failure requires modern **Observability**.

Observability is the ability to infer the internal health and state of a system based entirely on its external outputs. The three pillars of observability are:
1. **Metrics**: Aggregated numerical telemetry measured over time (CPU utilization, error rates, p99 latencies).
2. **Logs**: Timestamped, structured records of discrete events emitted by applications and infrastructure.
3. **Distributed Tracing**: End-to-end tracking of a single user request as it traverses asynchronous queues, reverse-proxies, and microservices across physical network boundaries.

AWS provides a cohesive, unified observability stack centered around **Amazon CloudWatch**, **AWS X-Ray**, and managed open-source engines (**Amazon Managed Service for Prometheus** and **Amazon Managed Grafana**).

---

## 2. Core Concepts

### 2.1 Amazon CloudWatch Metrics & Alarms
- **Metrics**: Time-series data points defined by a `Namespace` (e.g., `AWS/EC2`, `AWS/Lambda`), `MetricName` (e.g., `CPUUtilization`, `Duration`), and up to 30 `Dimensions` (key-value pairs like `InstanceId=i-12345`).
- **Resolution**:
  - *Standard Resolution*: 1-minute data granularity (Default).
  - *High-Resolution Metrics*: Down to 1-second intervals (ideal for rapid auto-scaling triggers).
- **Metric Math**: Enables custom mathematical operations across multiple metrics (e.g., calculating Error Rate: `(5xxErrors / TotalRequests) * 100`).
- **Composite Alarms**: Evaluates boolean conditions across multiple alarms (e.g., fire PagerDuty ONLY IF `HighCPU == ALARM` AND `High5xxRate == ALARM`), drastically cutting down alert fatigue.

### 2.2 CloudWatch Logs & Logs Insights
- **Hierarchy**: `Log Groups` contain multiple `Log Streams` (representing individual container instances or Lambda runtimes).
- **CloudWatch Logs Insights**: A serverless, interactive query engine capable of querying terabytes of log data in seconds using a purpose-built query syntax (`fields`, `filter`, `stats`, `sort`, `limit`).
- **Live Tail**: Streams live log lines into the terminal or console with sub-second latency during deployments or debugging sessions.

### 2.3 CloudWatch Specialized Insights & Synthetics
- **Container Insights**: Collects, aggregates, and summarizes compute metrics and diagnostic logs for Amazon ECS, EKS, and Kubernetes clusters.
- **Lambda Insights**: Deep diagnostic telemetry for serverless functions (monitoring memory utilization, cold starts, and CPU usage).
- **CloudWatch Synthetics**: Automated configurable scripts ("Canaries") running on NodeJS/Python Chromium headless browsers that ping APIs and simulate user clicks 24/7 to catch outages before real customers do.

### 2.4 AWS X-Ray (Distributed Tracing)
- Generates a complete end-to-end trace by injecting a standardized tracing header (`X-Amzn-Trace-Id`) into incoming HTTP requests.
- **Trace Anatomy**:
  - *Trace*: The end-to-end journey of a single request through the application stack.
  - *Segment*: Work performed by an individual compute resource (e.g., an EC2 instance or Lambda function).
  - *Subsegment*: Fine-grained work performed inside a segment (e.g., a specific SQL query, an S3 read, or a remote HTTP call).
- **Service Map**: A visual topology map showing all interconnected services, request throughput, and localized latency bottlenecks.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AWS DISTRIBUTED OBSERVABILITY ARCHITECTURE                                 |
|                                                                                                   |
|  [End-User Request: HTTP GET /checkout]                                                           |
|         |                                                                                         |
|         v (CloudWatch Synthetics Canary monitors 24/7)                                            |
|  +---------------------------------------------------------------------------------------------+  |
|  | AMAZON API GATEWAY                                                                          |  |
|  | 1. Generates `X-Amzn-Trace-Id: Root=1-5e...;Sampled=1`                                      |  |
|  | 2. Emits Metric: `Count`, `Latency`, `4XXError`, `5XXError` to CloudWatch                   |  |
|  | 3. Emits Access Log: JSON payload to CloudWatch Log Group `/aws/apigateway/prod`            |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Trace Context Propagated via HTTP Header)                                              |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS LAMBDA FUNCTION (Order Processor)                                                       |  |
|  | - AWS X-Ray Daemon captures function segment                                                |  |
|  | - Subsegment A: DynamoDB `PutItem` call (Latency: 8ms)                                       |  |
|  | - Subsegment B: External Payment Gateway HTTP POST (Latency: 450ms -> BOTTLENECK!)          |  |
|  | - Emits Structured JSON Log via Lambda Powertools to CloudWatch Logs                         |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         +---------------------------------------+----------------------------------+              |
|         |                                       |                                  |              |
|         v (Metrics & Logs Ingest)               v (Trace Segments)                 v              |
|  +-----------------------------+         +-------------------------------+  +------------------+  |
|  | CLOUDWATCH LOGS & METRICS   |         | AWS X-RAY SERVICE             |  | CLOUDWATCH ALARM|  |
|  | - CloudWatch Logs Insights   |         | - Constructs Service Map      |  | Composite Alarm: |  |
|  |   Query: `stats count(*)    |         | - Identifies 450ms bottleneck |  | MetricMath Error |  |
|  |   by status`                |         | - Highlights Error Root Cause |  | Rate > 2%        |  |
|  +-----------------------------+         +-------------------------------+  +--------+---------+  |
|                                                                                      |            |
|                                                                                      v            |
|                                                                             [PagerDuty / Slack]   |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 CloudWatch Logs Insights Query Examples

```sql
# 1. Calculate 95th and 99th percentile execution duration for Lambda functions
fields @timestamp, @duration
| filter @type = "REPORT"
| stats count(*) as invocations,
        avg(@duration) as avg_duration,
        pct(@duration, 95) as p95,
        pct(@duration, 99) as p99 by bin(5m)

# 2. Extract Top 10 IP addresses generating HTTP 5xx errors from API Gateway / Nginx logs
fields @timestamp, ip, status, uri
| filter status >= 500
| stats count(*) as error_count by ip
| sort error_count desc
| limit 10

# 3. Search for fatal application exceptions and extract stack traces
fields @timestamp, @message
| filter @message like /(?i)(exception|fatal|panic)/
| sort @timestamp desc
| limit 20
```

### 4.2 CLI Commands for Metrics and Alarms

```bash
# 1. Put custom application metric
aws cloudwatch put-metric-data \
    --namespace "CustomEcommerce" \
    --metric-data MetricName=OrdersPlaced,Value=1,Unit=Count,Dimensions='[{Name=Tier,Value=VIP}]'

# 2. Tail live CloudWatch logs from the terminal
aws logs tail "/aws/lambda/order-service-prod" --follow --format short
```

---

## 5. Practical Examples

### 5.1 CloudWatch Metric Math & Composite Alarm (Terraform)

```hcl
# Metric Alarm 1: High 5XX Error Rate Calculated via Metric Math
resource "aws_cloudwatch_metric_alarm" "api_error_rate_high" {
  alarm_name          = "api-gateway-error-rate-exceeded"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 3
  threshold           = 5 # 5% error rate
  alarm_description   = "Triggers when 5xx errors exceed 5% of total requests"

  metric_query {
    id          = "e1"
    expression  = "(m1 / m2) * 100"
    label       = "5XX Error Rate (%)"
    return_data = true
  }

  metric_query {
    id = "m1"
    metric {
      metric_name = "5XXError"
      namespace   = "AWS/ApiGateway"
      period      = 60
      stat        = "Sum"
      dimensions = {
        ApiName = "EnterpriseOrderApi"
      }
    }
  }

  metric_query {
    id = "m2"
    metric {
      metric_name = "Count"
      namespace   = "AWS/ApiGateway"
      period      = 60
      stat        = "Sum"
      dimensions = {
        ApiName = "EnterpriseOrderApi"
      }
    }
  }
}

# Metric Alarm 2: High Latency
resource "aws_cloudwatch_metric_alarm" "api_latency_high" {
  alarm_name          = "api-gateway-p95-latency-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 3
  threshold           = 2000 # 2000 ms (2 seconds)
  metric_name         = "Latency"
  namespace           = "AWS/ApiGateway"
  period              = 60
  extended_statistic  = "p95"

  dimensions = {
    ApiName = "EnterpriseOrderApi"
  }
}

# Composite Alarm: Only fire if Error Rate is High AND Latency is High (Eliminates Flapping)
resource "aws_cloudwatch_composite_alarm" "critical_api_degraded" {
  alarm_name        = "CRITICAL-API-Degraded-Production"
  alarm_description = "Fires PagerDuty when both errors and latency breach thresholds"

  alarm_rule = "ALARM(${aws_cloudwatch_metric_alarm.api_error_rate_high.alarm_name}) AND ALARM(${aws_cloudwatch_metric_alarm.api_latency_high.alarm_name})"

  alarm_actions = [var.sns_pagerduty_topic_arn]
}
```

### 5.2 Python Lambda with Distributed Tracing via AWS X-Ray & Powertools

```python
"""
Python Lambda handler demonstrating AWS X-Ray subsegment tracing and structured logging.
"""
from aws_lambda_powertools import Logger, Tracer
import boto3

logger = Logger(service="order-service")
tracer = Tracer(service="order-service")
dynamodb = boto3.resource("dynamodb")
orders_table = dynamodb.Table("Orders")

@tracer.capture_method
def save_order(order_data: dict):
    # AWS X-Ray automatically creates a subsegment for this function
    logger.info("Saving order to DynamoDB", extra={"orderId": order_data["id"]})
    orders_table.put_item(Item=order_data)

@logger.inject_lambda_context(log_event=True)
@tracer.capture_lambda_handler
def lambda_handler(event, context):
    order = event.get("body", {})
    
    # Custom X-Ray metadata and annotations
    tracer.put_annotation(key="PaymentStatus", value="PAID")
    tracer.put_metadata(key="OrderPayload", value=order)

    save_order(order)
    return {"statusCode": 200, "body": "Order processed successfully"}
```

### 5.3 CloudWatch Synthetics Canary Monitoring an API Endpoint (Terraform)

```hcl
resource "aws_synthetics_canary" "api_canary" {
  name                 = "api-health-canary"
  artifact_s3_location = "s3://${var.canary_artifacts_bucket_name}/canary-runs"
  execution_role_arn   = var.canary_role_arn
  handler              = "apiCanaryBlueprint.handler"
  runtime_version      = "syn-nodejs-puppeteer-7.0"

  schedule {
    expression = "rate(5 minutes)"
  }

  zip_file = "canary_scripts/api_canary.zip"

  run_config {
    timeout_in_seconds = 60
  }
}
```

---

## 6. Best Practices

1. **Emit Structured JSON Logs**: Never write plain text logs (`logger.info("Order processed for user 123")`). Always write structured JSON (`logger.info({"event": "OrderProcessed", "userId": "123", "amount": 99.50})`). CloudWatch Logs Insights parses JSON keys natively, enabling instant filtering without regex.
2. **Use Composite Alarms to Stop Alert Fatigue**: Never alert engineers on raw single-metric CPU or memory spikes. Combine symptoms (e.g., CPU $> 90\%$ AND HTTP 5xx $> 5\%$ AND Target Response Time $> 1\text{s}$) into a Composite Alarm.
3. **Configure Log Group Retention Policies**: CloudWatch Logs default retention is **Never Expire**. Failing to set a retention period (e.g., 30, 90, or 365 days) results in indefinite storage billing for forgotten test environments.
4. **Instrument with AWS Distro for OpenTelemetry (ADOT)**: Standardize on OpenTelemetry (OTel) instrumentation. ADOT collects metrics and traces vendor-neutrally, exporting to CloudWatch, X-Ray, Prometheus, or Datadog without vendor lock-in.
5. **Use High-Resolution Alarms for Auto Scaling**: Configure 10-second or 30-second evaluation intervals for container and EC2 autoscaling alarms to react to flash traffic surges in seconds.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Unbounded Log Retention | Setting retention to "Never Expire". | Gigabytes of stale logs accumulate forever; high storage costs. | Set retention period (e.g., 30-90 days) on all Log Groups. |
| Alerting on High CPU Alone | Paging on-call because an instance reached 95% CPU during a batch job. | Wakes up engineers for healthy, expected workload bursts. | Pair CPU alarms with latency or customer-facing error alarms in Composite Alarms. |
| Inadequate X-Ray Sampling Rules | Sampling 100% of requests in a system handling 50,000 req/sec. | Huge AWS X-Ray tracing bill. | Configure X-Ray Sampling Rules: 1 req/sec fixed + 5% reservoir. |
| Missing Dimension Precision | Calling `GetMetricData` without providing exact dimension key-values. | Returns empty metric data graph. | Supply the exact dimensions (e.g., `InstanceId`, `TableName`) matching emission. |
| Unstructured Log Spam | Developers emitting 50-line raw stack traces per error. | Slower query performance in Logs Insights. | Format logs as standardized JSON with error payloads. |
| Forgetting to Enable X-Ray on API Gateway | Enabling X-Ray on Lambda, but leaving API Gateway disabled. | Trace is split into two disconnected segments; no unified Service Map. | Enable `tracing_enabled = true` on API Gateway stages. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Sampling
- **X-Ray Sampling Rules**: To control costs and performance, X-Ray samples a subset of requests:
  - *Reservoir*: Minimum number of traces recorded per second (e.g., 1 trace/sec ensures continuous baseline visibility).
  - *Fixed Rate*: Percentage of subsequent requests sampled (e.g., 5%).

### 8.2 Security
- **CloudWatch Logs KMS Encryption**: Log groups can be encrypted with AWS KMS Customer Managed Keys, ensuring sensitive logs are protected at rest.
- **Log Masking / Data Protection**: CloudWatch Logs Data Protection policies scan incoming logs for PII (credit cards, API keys) using machine learning and automatically mask them in the console.

### 8.3 Cost Breakdown
- **CloudWatch Metrics**: \$0.30 per metric per month (first 10 metrics free).
- **CloudWatch Logs Ingestion**: \$0.50 per GB ingested; \$0.03 per GB storage.
- **Logs Insights Queries**: \$0.005 per GB of data scanned.
- **AWS X-Ray**: First 100,000 traces recorded/month free; \$5.00 per million traces recorded thereafter.

---

## 9. Comparison Table

| Dimension | Amazon CloudWatch | AWS X-Ray | Amazon Managed Prometheus / Grafana |
|---|---|---|---|
| **Primary Domain** | Metrics, Logs & Alarms | Distributed Tracing & Service Map | Open-source Metrics & Visualization |
| **Telemetry Type** | Time-series metrics & JSON logs | Request spans, subsegments, latency| PromQL metrics & Grafana dashboards |
| **Query Engine** | CloudWatch Logs Insights | X-Ray Trace Search | PromQL (Prometheus Query Language) |
| **Ecosystem Standard** | AWS Native | AWS Native (Supports OTel) | Open-source Kubernetes Standard |
| **Retention** | Configurable (1 day to indefinite)| 30 days (Fixed) | Configurable in AMP workspace |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the three pillars of observability and how are they implemented across AWS native services?</strong></summary>

1. **Metrics**: Time-series numerical values representing system state over time (CPU utilization, error rates, throughput). Implemented in **Amazon CloudWatch Metrics** with support for Metric Math and Composite Alarms.
2. **Logs**: Timestamped structured records of discrete events emitted by systems and applications. Implemented in **Amazon CloudWatch Logs** with high-speed query capabilities via **CloudWatch Logs Insights**.
3. **Traces**: End-to-end distributed tracking of an individual request through multi-tier microservices, containers, and serverless runtimes. Implemented in **AWS X-Ray** with automatic trace context propagation and visual Service Maps.
</details>

<details>
<summary><strong>Q2: How does AWS X-Ray propagate trace context across asynchronous services like SQS and SNS?</strong></summary>

When a request enters AWS (e.g., via an Application Load Balancer or API Gateway), AWS injects an HTTP header:
`X-Amzn-Trace-Id: Root=1-5e...;Parent=2b...;Sampled=1`
When an application publishes a message to **Amazon SQS** or **Amazon SNS**, the AWS SDK automatically copies this trace header into the SQS/SNS message system attributes. When downstream worker consumers receive the message, the X-Ray SDK extracts the trace context and links the consumer's execution segment back to the original root trace, preserving the complete end-to-end distributed trace.
</details>

<details>
<summary><strong>Q3: What is the purpose of a CloudWatch Composite Alarm and how does it prevent alert fatigue?</strong></summary>

A standard metric alarm triggers based on a single condition (e.g., `CPUUtilization > 85%`). However, a batch database backup or legitimate flash sale might cause CPU to spike without causing any customer impact. Alerting on-call engineers for isolated spikes creates "alert fatigue".
A **Composite Alarm** evaluates boolean logic (`AND`, `OR`, `NOT`) across multiple underlying alarms:
$$\text{Composite Alarm} = \text{HighCPU} \text{ AND } \text{High5xxErrors} \text{ AND } \text{TargetResponseTimeExceeded}$$
The on-call engineer is paged only when multiple failure indicators coincide, ensuring high-fidelity actionable alerts.
</details>

<details>
<summary><strong>Q4: How does CloudWatch Logs Insights query syntax work and why is structured JSON logging recommended?</strong></summary>

CloudWatch Logs Insights uses a dedicated piping query language:
```sql
fields @timestamp, @message, status, duration
| filter status >= 500
| stats count(*) by bin(5m)
```
When applications write logs as **structured JSON**, CloudWatch automatically parses top-level JSON keys into queryable fields without writing complex regular expressions (`parse @message /.../ as ...`). This drastically speeds up query execution, reduces scanned data costs, and allows immediate aggregation (`stats avg(duration) by userId`).
</details>

<details>
<summary><strong>Q5: What is an AWS X-Ray Sampling Rule and why is it necessary?</strong></summary>

In high-throughput distributed systems handling millions of requests per second, tracing 100% of requests is unnecessary and cost-prohibitive. 
An **X-Ray Sampling Rule** controls which requests are traced:
- **Reservoir**: The minimum number of requests per second that must be traced (e.g., 1 req/sec guarantees that low-traffic services always have baseline trace data).
- **Fixed Rate**: The percentage of subsequent requests sampled once the reservoir is satisfied (e.g., 5%).
Sampling rules can be scoped by HTTP method, URL path (`/checkout/*` vs `/healthz`), and service name.
</details>

<details>
<summary><strong>Q6: What are CloudWatch Synthetics Canaries and how do they differ from simple HTTP health checks?</strong></summary>

- **Simple HTTP Health Checks** (like Route 53 or ALB probes) merely verify that a web server responds with an HTTP 200 to a static URL. They do not test whether JavaScript executes, whether authentication works, or whether a database record can be written.
- **CloudWatch Synthetics Canaries** execute automated Puppeteer or Selenium scripts running inside a headless browser on a recurring schedule. A canary navigates your website, fills in forms, logs in, clicks through checkout flows, and takes screenshots, alerting your team if a critical user flow breaks before real customers report it.
</details>

<details>
<summary><strong>Q7: What is CloudWatch Metric Math?</strong></summary>

Metric Math enables developers to query multiple CloudWatch metrics and apply mathematical formulas to create new custom metrics. 
Common use cases include:
- Calculating percentage error rates: `(Errors / TotalRequests) * 100`.
- Aggregating capacity across multiple distinct Auto Scaling Groups: `SUM([m1, m2, m3])`.
- Inverting latency to measure throughput.
Metric Math formulas can be graphed on dashboards and evaluated directly by CloudWatch Alarms.
</details>

<details>
<summary><strong>Q8: What is CloudWatch Logs Data Protection?</strong></summary>

CloudWatch Logs Data Protection is an automated auditing capability that scans log streams in transit for sensitive information (Personally Identifiable Information - PII) such as credit card numbers, email addresses, and AWS secret keys. When detected, the sensitive strings are automatically masked in the CloudWatch console and CLI (e.g., replacing a credit card number with `[CREDIT_CARD]`), preventing accidental data leakage to developers viewing logs.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Query Logs with CloudWatch Logs Insights</strong></summary>

**Goal**: Execute an aggregation query on Lambda execution logs.
1. Open CloudWatch Logs Insights in the AWS Console.
2. Select an active Lambda Log Group.
3. Run: `fields @timestamp, @duration | filter @type = "REPORT" | stats avg(@duration), max(@duration) by bin(5m)`.
4. Inspect the generated visualization graph.
</details>

<details>
<summary><strong>Exercise 2: Create a Metric Math Alarm for Error Rates</strong></summary>

**Goal**: Build a percentage-based error alarm.
1. In CloudWatch Alarms, select `Create Alarm`.
2. Add Metric 1 (`5XXError`) and Metric 2 (`Count`) from API Gateway.
3. Add a math expression: `(m1 / m2) * 100`.
4. Set threshold to $> 2\%$ and attach an SNS notification topic.
</details>

<details>
<summary><strong>Exercise 3: Instrument a Python Function with AWS X-Ray</strong></summary>

**Goal**: Trace a function call and inspect subsegments.
1. Install `aws-xray-sdk` or `aws-lambda-powertools`.
2. Wrap a database function with `@tracer.capture_method`.
3. Invoke the function and open the AWS X-Ray Service Map.
4. Verify the database subsegment appears in the trace timeline.
</details>

<details>
<summary><strong>Exercise 4: Deploy an Automated Synthetics Canary</strong></summary>

**Goal**: Monitor website uptime via headless browser.
1. In CloudWatch Synthetics, click `Create Canary`.
2. Select "Heartbeat Monitoring" blueprint.
3. Target a public URL (e.g., `https://aws.amazon.com`).
4. Set schedule to run every 5 minutes and view the generated screenshots and latency waterfall graphs.
</details>

<details>
<summary><strong>Exercise 5: Configure Log Group Retention Policy via AWS CLI</strong></summary>

**Goal**: Eliminate indefinite storage billing.
1. Create a log group: `aws logs create-log-group --log-group-name "/app/test"`.
2. Set retention to 30 days: `aws logs put-retention-policy --log-group-name "/app/test" --retention-in-days 30`.
3. Verify retention status with `describe-log-groups`.
</details>

<details>
<summary><strong>Exercise 6: Construct a Multi-Metric Composite Alarm</strong></summary>

**Goal**: Prevent false alarms by chaining conditions.
1. Create Alarm A (CPU $> 80\%$) and Alarm B (Network In $> 100 \text{ MB}$).
2. Create a Composite Alarm evaluating `ALARM(AlarmA) AND ALARM(AlarmB)`.
3. Test triggering only Alarm A and verify the Composite Alarm remains in `OK` state.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| AWS OBSERVABILITY PILLARS QUICK REFERENCE                                                         |
+--------------------------+-----------------------+------------------------------------------------+
| Pillar                   | Primary AWS Service   | Key Tool / Interface                           |
+--------------------------+-----------------------+------------------------------------------------+
| Metrics                  | Amazon CloudWatch     | CloudWatch Dashboards, Metric Math, Alarms     |
| Logs                     | CloudWatch Logs       | CloudWatch Logs Insights, Live Tail, Filtering |
| Tracing                  | AWS X-Ray             | X-Ray Service Map, Subsegments, Sampling Rules |
| Synthetic Monitoring     | CloudWatch Synthetics | Puppeteer/Selenium Headless Canaries           |
| Open Source Stacks       | Amazon Managed Grafana| PromQL, Managed Prometheus (AMP), Grafana UI   |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Structured JSON Logging**: Mandatory for fast, serverless querying via Logs Insights.
- **Composite Alarms**: Eliminate alert fatigue by requiring multiple simultaneous failure indicators.
- **Distributed Tracing**: Identify cross-microservice latency bottlenecks using X-Ray.

---

## 13. Further Reading

- [Amazon CloudWatch User Guide](https://docs.aws.amazon.com/AmazonCloudWatch/latest/monitoring/WhatIsCloudWatch.html)
- [AWS X-Ray Developer Guide](https://docs.aws.amazon.com/xray/latest/devguide/aws-xray.html)
- [AWS Lambda Powertools (Python / TypeScript)](https://docs.powertools.aws.dev/)
- [OpenTelemetry on AWS Documentation](https://aws-otel.github.io/)
- [06-operations/05-ci-cd-to-aws.md](file:///04-devops/aws/06-operations/05-ci-cd-to-aws.md)
