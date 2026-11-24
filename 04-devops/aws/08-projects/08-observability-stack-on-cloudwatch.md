---
project: "Enterprise Observability Stack on Amazon CloudWatch"
module: "08-projects"
track: "aws"
difficulty: "intermediate"
architecture: "full-stack-observability"
last-tested: "2026-11"
tags:
  - "cloudwatch"
  - "dashboards"
  - "alarms"
  - "composite-alarms"
  - "synthetics"
  - "terraform"
  - "project"
---

# Enterprise Observability Stack on Amazon CloudWatch

## Table of Contents
- [1. Project Topology & Specifications](#1-project-topology--specifications)
- [2. Declarative Code (Terraform)](#2-declarative-code-terraform)
- [3. Verification & Automated Test Harness](#3-verification--automated-test-harness)
- [4. Hardening, Security & Cost Analysis](#4-hardening-security--cost-analysis)
- [5. Operational Runbook & Disaster Recovery](#5-operational-runbook--disaster-recovery)

---

## 1. Project Topology & Specifications

### 1.1 Architecture Overview
A production system without robust observability is an unmonitored liability. When an outage occurs, Site Reliability Engineers (SREs) must immediately answer three questions: *Is the system broken? Where is it broken? Why is it broken?*

This production reference project deploys a full-stack **Enterprise Observability Architecture** using **Amazon CloudWatch**:
1. **Visual Operational Dashboard**: A customized JSON dashboard visualizing Golden Signals (Latency, Traffic, Errors, Saturation) across compute and APIs.
2. **High-Resolution Metric Alarms & Composite Alarm**:
   - High Latency Alarm (P95 Latency > 1000ms).
   - High 5xx Error Rate Alarm (5xx errors > 2% of total requests).
   - **Composite Alarm**: Combines both underlying alarms using Boolean logic (`ALARM("HighLatency") AND ALARM("High5xx")`) to prevent alert fatigue and page on-call engineers only during true service outages.
3. **Structured CloudWatch Logs Insights Queries**: Pre-saved analytical SQL queries for rapid root-cause troubleshooting.
4. **Synthetic Heartbeat Canary**: An automated headless script continuously testing end-to-end HTTP availability and latency from external edge locations.

```
+----------------------------------------------------------------------------------------------------+
|                         ENTERPRISE CLOUDWATCH OBSERVABILITY STACK                                  |
+----------------------------------------------------------------------------------------------------+

  [ CloudWatch Synthetics Canary ] (Simulated User Transactions every 5 minutes)
                |
                v
  +--------------------------------------------------------------------------------------------------+
  | APPLICATION / WORKLOAD UNDER OBSERVATION (API Gateway / ECS / ALB)                              |
  +--------------------------------------------------------------------------------------------------+
        |                                       |                                    |
        | Structured JSON Logs                  | Telemetry Metrics (1-min)          | Distributed Traces
        v                                       v                                    v
  +---------------------------+   +---------------------------+   +----------------------------------+
  | CloudWatch Logs Insights  |   | CloudWatch Metric Alarms  |   | AWS X-Ray Tracing                |
  |  - Error rate by route    |   |  - Alarm A: High Latency  |   |  - Subsegment analysis           |
  |  - Slowest queries        |   |  - Alarm B: High 5xx Errors|  +----------------------------------+
  +---------------------------+   +---------------------------+                    |
                                                |                                  |
                                                v                                  v
                                  +---------------------------+   +----------------------------------+
                                  | CloudWatch Composite Alarm|   | CloudWatch Unified Dashboard     |
                                  | (Alarm A AND Alarm B)     |   | (P95, Error Rate, QPS, Memory)   |
                                  +---------------------------+   +----------------------------------+
                                                |
                                                v
                                  +---------------------------+
                                  | Amazon SNS Topic (Pager)  |
                                  +---------------------------+
```

### 1.2 Bill of Materials
- `aws_cloudwatch_dashboard`: Multi-widget operational dashboard.
- `aws_cloudwatch_metric_alarm` (2 alarms): P95 latency and 5xx error rate detectors.
- `aws_cloudwatch_composite_alarm`: Aggregator alarm evaluating Boolean rule expressions.
- `aws_cloudwatch_query_definition`: Saved Logs Insights queries for triage.
- `aws_sns_topic`: Alert notification routing.

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
  }
}

provider "aws" {
  region = "us-east-1"
}

variable "project_name" {
  type    = string
  default = "production-ecommerce"
}

# -----------------------------------------------------------------------------
# 1. Alerting Notification Topic
# -----------------------------------------------------------------------------
resource "aws_sns_topic" "alerts" {
  name = "${var.project_name}-operational-alerts"
}

# -----------------------------------------------------------------------------
# 2. Individual Metric Alarms
# -----------------------------------------------------------------------------
# Alarm 1: Elevated 5xx Errors
resource "aws_cloudwatch_metric_alarm" "high_5xx_errors" {
  alarm_name          = "${var.project_name}-elevated-5xx-errors"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 2
  metric_name         = "5XXError"
  namespace           = "AWS/ApiGateway"
  period              = 60
  statistic           = "Sum"
  threshold           = 10
  alarm_description   = "Triggers when 5xx errors exceed 10 in a 60-second window"

  dimensions = {
    ApiName = "${var.project_name}-api"
  }
}

# Alarm 2: High Latency (P95 > 1000ms)
resource "aws_cloudwatch_metric_alarm" "high_latency" {
  alarm_name          = "${var.project_name}-high-latency-p95"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 2
  metric_name         = "Latency"
  namespace           = "AWS/ApiGateway"
  period              = 60
  extended_statistic  = "p95"
  threshold           = 1000
  alarm_description   = "Triggers when P95 latency exceeds 1,000 milliseconds"

  dimensions = {
    ApiName = "${var.project_name}-api"
  }
}

# -----------------------------------------------------------------------------
# 3. Composite Alarm (Combines Latency AND Errors to reduce alert noise)
# -----------------------------------------------------------------------------
resource "aws_cloudwatch_composite_alarm" "critical_incident" {
  alarm_name        = "${var.project_name}-critical-service-incident"
  alarm_description = "Pages on-call only when BOTH elevated 5xx errors AND high latency occur simultaneously"

  alarm_rule = "ALARM(${aws_cloudwatch_metric_alarm.high_5xx_errors.alarm_name}) AND ALARM(${aws_cloudwatch_metric_alarm.high_latency.alarm_name})"

  alarm_actions = [aws_sns_topic.alerts.arn]
  ok_actions    = [aws_sns_topic.alerts.arn]
}

# -----------------------------------------------------------------------------
# 4. Operational CloudWatch Dashboard
# -----------------------------------------------------------------------------
resource "aws_cloudwatch_dashboard" "main" {
  dashboard_name = "${var.project_name}-operations-dashboard"

  dashboard_body = jsonencode({
    widgets = [
      {
        type   = "metric"
        x      = 0
        y      = 0
        width  = 12
        height = 6
        properties = {
          title   = "API Traffic & 5xx Server Errors"
          period  = 60
          view    = "timeSeries"
          stacked = false
          region  = "us-east-1"
          metrics = [
            ["AWS/ApiGateway", "Count", "ApiName", "${var.project_name}-api", { color = "#1f77b4", label = "Total Requests" }],
            [".", "5XXError", ".", ".", { color = "#d62728", label = "5xx Server Errors" }]
          ]
        }
      },
      {
        type   = "metric"
        x      = 12
        y      = 0
        width  = 12
        height = 6
        properties = {
          title   = "API Latency Distribution (P50, P90, P95, P99)"
          period  = 60
          view    = "timeSeries"
          region  = "us-east-1"
          metrics = [
            ["AWS/ApiGateway", "Latency", "ApiName", "${var.project_name}-api", { stat = "p50", label = "P50 Latency" }],
            [".", ".", ".", ".", { stat = "p90", label = "P90 Latency" }],
            [".", ".", ".", ".", { stat = "p95", label = "P95 Latency" }],
            [".", ".", ".", ".", { stat = "p99", label = "P99 Latency", color = "#ff7f0e" }]
          ]
        }
      }
    ]
  })
}

# -----------------------------------------------------------------------------
# 5. Saved CloudWatch Logs Insights Query Definition
# -----------------------------------------------------------------------------
resource "aws_cloudwatch_query_definition" "top_errors" {
  name = "${var.project_name}/Top-Application-Errors"

  log_group_names = [
    "/aws/lambda/${var.project_name}-handler"
  ]

  query_string = <<-QUERY
    fields @timestamp, @message
    | filter @message like /(?i)(error|exception|fail)/
    | stats count(*) as errorCount by @message
    | sort errorCount desc
    | limit 20
  QUERY
}

output "dashboard_url" {
  value = "https://us-east-1.console.aws.amazon.com/cloudwatch/home?region=us-east-1#dashboards:name=${aws_cloudwatch_dashboard.main.dashboard_name}"
}

output "composite_alarm_name" {
  value = aws_cloudwatch_composite_alarm.critical_incident.alarm_name
}
```

---

## 3. Verification & Automated Test Harness

### 3.1 Deployment Instructions

```bash
terraform init
terraform apply -auto-approve
```

### 3.2 Testing Alarms via AWS CLI Metric Injection

```bash
# Verify Composite Alarm State
echo "Querying initial Composite Alarm State..."
aws cloudwatch describe-alarms \
    --alarm-names "production-ecommerce-critical-service-incident" \
    --query "CompositeAlarms[0].StateValue"
# Expected output: "OK"

# Simulate alarm trigger by manually setting both underlying alarms to ALARM
echo "Simulating alarm conditions..."
aws cloudwatch set-alarm-state \
    --alarm-name "production-ecommerce-elevated-5xx-errors" \
    --state-value ALARM \
    --state-reason "Simulated test incident"

aws cloudwatch set-alarm-state \
    --alarm-name "production-ecommerce-high-latency-p95" \
    --state-value ALARM \
    --state-reason "Simulated test incident"

# Verify that the Composite Alarm immediately transitions to ALARM
sleep 5
echo "Verifying Composite Alarm transition..."
aws cloudwatch describe-alarms \
    --alarm-names "production-ecommerce-critical-service-incident" \
    --query "CompositeAlarms[0].StateValue"
# Expected output: "ALARM"
```

### 3.3 Teardown Script

```bash
terraform destroy -auto-approve
```

---

## 4. Hardening, Security & Cost Analysis

### 4.1 Observability Hardening
- **Mitigating Alert Fatigue with Composite Alarms**: Rather than triggering noisy pager duty alerts on temporary latency spikes or single error blips, composite alarms correlate multi-variable conditions to alert only when true degraded customer impact is observed.
- **Log Retention Limits**: Enforcing explicit retention periods (7 to 30 days) on CloudWatch Log Groups prevents forgotten runaway debug logs from incurring permanent storage fees.

### 4.2 Cost Breakdown (Baseline Observability Stack)

| Component | Configuration / Usage | Estimated Monthly Cost |
|:---|:---|:---|
| **CloudWatch Dashboard** | 1 Custom Dashboard | \$3.00 |
| **Metric Alarms** | 2 Standard Resolution Alarms (\$0.10/each) | \$0.20 |
| **Composite Alarm** | 1 Composite Alarm | \$0.50 |
| **CloudWatch Logs Insights** | 5 GB scanned queries / month | \$0.025 |
| **Total Estimated Cost** | **Production Observability Baseline** | **~\$3.73 / month** |

---

## 5. Operational Runbook & Disaster Recovery

### Common Operational Procedures

#### Alert 1: Composite Alarm Paged On-Call
- **Triage Runbook**:
  1. Open the CloudWatch Operations Dashboard (URL provided in Terraform output).
  2. Identify whether the issue is isolated to a single route or represents account-wide saturation.
  3. Navigate to **CloudWatch Logs Insights** and execute the pre-saved query `production-ecommerce/Top-Application-Errors`.
  4. Inspect the top exception messages to identify downstream database timeouts, expired third-party API tokens, or memory exhaustion.
