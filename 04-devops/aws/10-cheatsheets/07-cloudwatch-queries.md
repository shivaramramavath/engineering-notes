---
topic: "Amazon CloudWatch Logs Insights Query Recipes Cheatsheet"
module: "10-cheatsheets"
track: "aws"
difficulty: "reference"
last-tested: "2026-11"
tags:
  - "cheatsheet"
  - "cloudwatch"
  - "logs-insights"
  - "observability"
  - "queries"
  - "troubleshooting"
---

# Amazon CloudWatch Logs Insights Query Recipes Cheatsheet

A high-density reference card of the 10 most essential production CloudWatch Logs Insights queries for rapid root-cause analysis across Lambda, API Gateway, VPC Flow Logs, and CloudTrail.

---

## 1. Top Application Errors Grouped by Exception

```sql
fields @timestamp, @message
| filter @message like /(?i)(error|exception|fail|fatal)/
| parse @message /(?i)(error|exception): (?<errorMessage>[^\n]+)/
| stats count(*) as errorCount by errorMessage
| sort errorCount desc
| limit 25
```

---

## 2. AWS Lambda Cold Start Analysis

Find functions experiencing high cold start initiation overhead:

```sql
filter @type = "REPORT"
| fields @requestId, @duration, @billedDuration, @initDuration, @maxMemoryUsed
| filter ispresent(@initDuration)
| stats count(*) as coldStarts, 
        avg(@initDuration) as avgColdStartMs, 
        max(@initDuration) as maxColdStartMs, 
        p95(@initDuration) as p95ColdStartMs by bin(1h)
```

---

## 3. Slowest Lambda Invocations (Latency Outliers)

```sql
filter @type = "REPORT"
| fields @requestId, @duration, @billedDuration, @maxMemoryUsed
| sort @duration desc
| limit 20
```

---

## 4. Lambda Memory Over-Allocation vs OOM Risk

Identify functions using less than 20% of allocated memory (waste) or over 90% (OOM risk):

```sql
filter @type = "REPORT"
| fields @requestId, @memorySize, @maxMemoryUsed
| parse @maxMemoryUsed /(?<used>\d+)/
| parse @memorySize /(?<allocated>\d+)/
| fields (used / allocated) * 100 as memoryUtilizationPercent
| filter memoryUtilizationPercent > 90 or memoryUtilizationPercent < 20
| stats avg(memoryUtilizationPercent) as avgUtilPercent, 
        max(memoryUtilizationPercent) as maxUtilPercent, 
        count(*) as count by bin(1h)
```

---

## 5. API Gateway 5xx Breakdown by Route and Source IP

```sql
fields @timestamp, routeKey, status, sourceIp, responseLatency
| filter status >= 500
| stats count(*) as errorCount, 
        avg(responseLatency) as avgLat, 
        p95(responseLatency) as p95Lat by routeKey, status
| sort errorCount desc
| limit 20
```

---

## 6. VPC Flow Logs: Top Rejected External IPs (Port Scans & Probing)

```sql
fields @timestamp, srcAddr, dstAddr, dstPort, protocol, action
| filter action = "REJECT"
| stats count(*) as rejectedPackets by srcAddr, dstPort
| sort rejectedPackets desc
| limit 25
```

---

## 7. VPC Flow Logs: Top Talkers by Bandwidth

Identify instances or IPs generating the highest data egress:

```sql
fields @timestamp, srcAddr, dstAddr, bytes
| stats sum(bytes) / (1024 * 1024) as megabytesTransferred by srcAddr, dstAddr
| sort megabytesTransferred desc
| limit 20
```

---

## 8. CloudTrail: Who Deleted or Modified Cloud Resources?

```sql
fields @timestamp, eventName, userIdentity.arn, eventSource, responseElements
| filter eventName like /(?i)(delete|terminate|destroy|stop|drop)/
| stats count(*) as deleteAttempts by eventName, userIdentity.arn
| sort deleteAttempts desc
| limit 25
```

---

## 9. CloudTrail: Unauthorized API Calls (`AccessDenied` Hunt)

```sql
fields @timestamp, eventName, eventSource, userIdentity.arn, errorCode, errorMessage
| filter errorCode in ["AccessDenied", "UnauthorizedOperation", "Client.UnauthorizedOperation"]
| stats count(*) as failures by eventSource, eventName, userIdentity.arn
| sort failures desc
| limit 20
```

---

## 10. Amazon ECS Container Crashes & OOM Kills

Querying `/aws/ecs/` or FireLens log streams for unexpected exit codes:

```sql
fields @timestamp, @logStream, @message
| filter @message like /(?i)(SIGSEGV|SIGKILL|exit code 137|OutOfMemory|Segmentation fault)/
| sort @timestamp desc
| limit 30
```
