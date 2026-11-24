# Alerting, SLIs, and SLOs in Spring Boot Microservices

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 12-observability-and-operations |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Alerting Rule Reference](#4-syntax--alerting-rule-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
In distributed cloud systems, traditional threshold alerting (e.g., "Alert if CPU > 80% for 5 minutes") leads to widespread **alert fatigue**. On-call engineers receive dozens of non-actionable pages every night:
- Transient CPU spikes during scheduled batch jobs do not impact end-user experience.
- A single pod restarting gracefully triggers an alarm even though 49 other pods handle 100% of user traffic seamlessly.
- Conversely, catastrophic edge-case failures (e.g. 5% of users permanently failing checkout) go undetected because overall system CPU and memory appear normal.

### Historical Evolution
The Site Reliability Engineering (SRE) discipline pioneered at Google established a mathematical, user-centric approach to alerting based on:
- **SLIs** (Service Level Indicators)
- **SLOs** (Service Level Objectives)
- **Error Budgets & Burn Rate Alerting**

Rather than alerting on internal server implementation details (CPU, thread counts), modern Spring Boot systems alert strictly when **user happiness is degraded** or when the **error budget is burning too rapidly**.

### Production Relevance
Implementing SLO-based alerting guarantees that:
- Every alert sent to a human engineer is high-signal and actionable.
- Engineering teams can mathematically balance feature velocity against system stability using error budgets.
- Production incidents are detected within minutes before total SLA breaches occur.

---

## 2. Core Concepts

### The SRE Hierarchy: SLI $\rightarrow$ SLO $\rightarrow$ SLA
1. **SLI (Service Level Indicator)**: A quantifiable metric tracking service performance from the perspective of the user. Expressed as:
   $$\text{SLI} = \frac{\text{Good Events}}{\text{Total Events}} \times 100\%$$
   *Example*: Percentage of successful HTTP requests with latency $< 300\text{ms}$.
2. **SLO (Service Level Objective)**: A target reliability threshold agreed upon by engineering and product teams over a defined rolling window (e.g., 30 days).
   *Example*: 99.9% of requests over 30 days must be successful and take $< 300\text{ms}$.
3. **SLA (Service Level Agreement)**: A formal contract with external customers specifying legal or financial penalties (e.g. billing credits) if the SLO is breached.
4. **Error Budget**: The permissible room for failure over the SLO period:
   $$\text{Error Budget} = 100\% - \text{SLO}$$
   For a $99.9\%$ SLO, the error budget is $0.1\%$ ($1$ in every $1,000$ requests can fail).
5. **Burn Rate**: The rate at which the error budget is consumed. A burn rate of $1$ means the entire error budget will be consumed exactly at the end of the window (e.g. 30 days). A burn rate of $14.4$ consumes $100\%$ of the budget in 2 days (requiring immediate paging).

---

## 3. How It Works Internally

### Multi-Window Multi-Burn-Rate Alerting Architecture

```
PROMETHEUS ENGINE (Evaluates every 15s)
  |
  |  Scrapes /actuator/prometheus from Spring Boot pods
  v
+------------------------------------------------------------------------------------+
| 1. SLI CALCULATION                                                                 |
|    Total Requests: sum(rate(http_server_requests_seconds_count[1h]))               |
|    Errors (5xx)  : sum(rate(http_server_requests_seconds_count{status=~"5.."}[1h]))|
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 2. DUAL-WINDOW BURN RATE EVALUATION (Google SRE Standard)                          |
|                                                                                    |
|    Page Alert (Burn Rate 14.4 - Consumes 2% in 1 hr):                              |
|    - Condition 1: 1-hour window burn rate > 14.4 (Immediate crisis)                |
|    AND                                                                             |
|    - Condition 2: 5-minute window burn rate > 14.4 (Ensures issue is still active) |
+------------------------------------------------------------------------------------+
  |
  +---------> [Condition Met?]
  |                |
  | YES            v
  |         +------------------------------------------------------------------------+
  |         | 3. ALERTMANAGER ROUTING & DISPATCH                                     |
  |         |    - Deduplicates, groups, and silences matching alerts                |
  |         |    - Burn rate 14.4 (Critical) -> Pages PagerDuty / OpsGenie           |
  |         |    - Burn rate 3.0 (Warning)   -> Sends ticket / Slack notification    |
  |         +------------------------------------------------------------------------+
  v
NO: Discard. No human interrupted.
```

---

## 4. Syntax & Alerting Rule Reference

### Multi-Window Burn-Rate Alert Rules (`prometheus-alerts.yml`)

```yaml
groups:
  - name: spring-boot-slo-alerts
    rules:
      # SLO Target: 99.9% Availability (Error Budget = 0.1% = 0.001)

      # -------------------------------------------------------------
      # Alert 1: Critical Page (14.4x Burn Rate)
      # Consumes 2% of budget in 1 hour; 100% budget consumed in 2 days.
      # -------------------------------------------------------------
      - alert: ServiceAvailabilityCriticalBurnRate
        expr: |
          (
            sum(rate(http_server_requests_seconds_count{status=~"5.."}[1h]))
            /
            sum(rate(http_server_requests_seconds_count[1h]))
          ) > (14.4 * 0.001)
          and
          (
            sum(rate(http_server_requests_seconds_count{status=~"5.."}[5m]))
            /
            sum(rate(http_server_requests_seconds_count[5m]))
          ) > (14.4 * 0.001)
        for: 2m
        labels:
          severity: page
          tier: p1
        annotations:
          summary: "Critical Error Budget Burn Rate on {{ $labels.application }}"
          description: "Service is burning error budget at 14.4x rate (1h and 5m windows). 2% of 30-day budget consumed in 1 hour."

      # -------------------------------------------------------------
      # Alert 2: Warning Notification (3x Burn Rate)
      # Consumes 5% of budget in 12 hours; sends ticket / Slack message.
      # -------------------------------------------------------------
      - alert: ServiceAvailabilityWarningBurnRate
        expr: |
          (
            sum(rate(http_server_requests_seconds_count{status=~"5.."}[6h]))
            /
            sum(rate(http_server_requests_seconds_count[6h]))
          ) > (3.0 * 0.001)
          and
          (
            sum(rate(http_server_requests_seconds_count{status=~"5.."}[30m]))
            /
            sum(rate(http_server_requests_seconds_count[30m]))
          ) > (3.0 * 0.001)
        for: 15m
        labels:
          severity: ticket
          tier: p2
        annotations:
          summary: "Elevated Error Budget Burn Rate on {{ $labels.application }}"
          description: "Service is burning error budget at 3x rate. At this rate, the budget will be exhausted within 10 days."
```

---

## 5. Practical Examples

### Example 1: Latency SLI Tracking with Micrometer SLA Buckets
To evaluate a Latency SLI (e.g. $99\%$ of requests $< 200\text{ms}$), we configure histogram SLA boundaries in Spring Boot:

```yaml
# application.yml
management:
  metrics:
    distribution:
      sla:
        http.server.requests: 200ms # Creates http_server_requests_seconds_bucket{le="0.2"}
      percentiles-histogram:
        http.server.requests: true
```

**Prometheus Latency SLI Query**:
```promql
# Calculates percentage of requests faster than 200ms
(
  sum(rate(http_server_requests_seconds_bucket{le="0.2"}[30d]))
  /
  sum(rate(http_server_requests_seconds_count[30d]))
) * 100
```

### Example 2: Programmatically Tracking Custom Business SLI
```java
package com.example.observability.slo;

import io.micrometer.core.instrument.Counter;
import io.micrometer.core.instrument.MeterRegistry;
import org.springframework.stereotype.Service;

@Service
public class OrderCheckoutSLITracker {

    private final Counter totalCheckoutEvents;
    private final Counter successfulCheckoutEvents;

    public OrderCheckoutSLITracker(MeterRegistry registry) {
        this.totalCheckoutEvents = Counter.builder("business.checkout.events.total")
            .description("Total number of checkout initiation attempts")
            .register(registry);

        this.successfulCheckoutEvents = Counter.builder("business.checkout.events.good")
            .description("Successful checkouts completed in under 1.5 seconds")
            .register(registry);
    }

    public void recordCheckout(boolean success, long durationMillis) {
        totalCheckoutEvents.increment();
        // A "Good Event" meets both correctness AND latency criteria
        if (success && durationMillis <= 1500) {
            successfulCheckoutEvents.increment();
        }
    }
}
```

### Example 3: Alertmanager Routing Configuration (`alertmanager.yml`)
```yaml
global:
  resolve_timeout: 5m

route:
  group_by: ['application', 'alertname']
  group_wait: 30s
  group_interval: 5m
  repeat_interval: 4h
  receiver: 'default-slack'
  routes:
    # Critical pages route to PagerDuty
    - match:
        severity: page
      receiver: 'pagerduty-oncall'
    # Tickets and non-urgent warnings route to Slack
    - match:
        severity: ticket
      receiver: 'ops-tickets-slack'

receivers:
  - name: 'pagerduty-oncall'
    pagerduty_configs:
      - service_key: "${PAGERDUTY_INTEGRATION_KEY}"
        severity: 'critical'

  - name: 'ops-tickets-slack'
    slack_configs:
      - channel: '#prod-alerts'
        webhook_url: "${SLACK_WEBHOOK_URL}"
        title: "[{{ .Status | toUpper }}] {{ .CommonAnnotations.summary }}"
        text: "{{ .CommonAnnotations.description }}"
```

### Example 4: Anti-Pattern vs. Production Fix (Raw Threshold CPU Alerting)
```yaml
# ANTI-PATTERN: Alerting on volatile hardware metrics (CPU / RAM)
# Scenario: A nightly batch job or garbage collection spikes CPU to 85% for 6 minutes.
# Result: On-call engineer is paged at 3:00 AM! User experience was 100% unaffected!
- alert: HighCpuUsage
  expr: process_cpu_usage > 0.80
  for: 5m
  labels:
    severity: page # BAD: Causes alert fatigue and burnout!

# PRODUCTION FIX: Alert on User-Facing Symptoms (SLIs), not Causes!
# Page ONLY when user-facing requests are failing or exceeding latency budgets.
- alert: UserFacingCheckoutFailure
  expr: |
    (sum(rate(business_checkout_events_total[5m])) - sum(rate(business_checkout_events_good[5m])))
    / sum(rate(business_checkout_events_total[5m])) > 0.01
  for: 2m
  labels:
    severity: page # LEGITIMATE: Users cannot complete purchases!
```

### Example 5: Grafana Error Budget Gauge Expression
```promql
# Grafana Single-Stat Gauge: Remaining Error Budget (%) over 30 days
# SLO = 99.9% (Budget = 0.001)
100 - (
  (
    1 - (
      sum(rate(http_server_requests_seconds_count{status!~"5.."}[30d]))
      /
      sum(rate(http_server_requests_seconds_count[30d]))
    )
  ) / 0.001
) * 100
```

---

## 6. Best Practices & Production Guidelines

1. **Alert on Symptoms, Not Causes**: Page humans when users are hurting (high latency, high 5xx errors). Investigate causes (CPU, thread pool, GC) after opening the incident dashboard.
2. **Implement Dual-Window Multi-Burn-Rate Alerts**: Follow the Google SRE Workbook standard: require both a long window (e.g. 1 hour) and a short window (e.g. 5 minutes) to exceed the burn rate threshold. This prevents paging for spikes that have already resolved.
3. **Every Paged Alert Must Be Actionable**: If an on-call engineer receives an alert and says "there is nothing I can do," that alert must either be deleted or downgraded to a warning ticket.
4. **Link Runbooks in Every Alert**: Every Prometheus alert annotation must include a `runbook_url` pointing to a documented step-by-step diagnostic and remediation procedure.
5. **Use Error Budgets to Negotiate Velocity**: If a service exhausts its monthly error budget in the first two weeks, halt new risky feature deployments and focus engineering capacity exclusively on stability, testing, and technical debt.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Paging on CPU $> 80\%$ | CPU spikes during GC or batch jobs without harming users, causing on-call alert fatigue. | Alert on SLIs (latency, error rate). Demote CPU to dashboard-only metric. |
| Single-window burn rate alerts | A momentary 1-minute spike triggers an alert that fires after the system has already self-healed. | Use dual-window alerting (e.g. 1 hour AND 5 minutes burn rate). |
| Alerting without runbooks | On-call engineers waste critical triage time figuring out what the alert means and where logs are located. | Add `runbook_url` linking to exact confluence/markdown guides. |
| Calculating availability without volume weighting | Low-traffic nighttime periods skew availability metrics disproportionately. | Calculate SLIs as the ratio of sums: `sum(good) / sum(total)`. |
| Unbounded `for` duration (e.g., `for: 1h`) | Severe outages remain un-alerted for 59 minutes while waiting for the timer to elapse. | Use burn rate alerting with short `for` durations (1-2 minutes). |
| Forgetting to exclude client errors (4xx) from SLIs | User typos or malformed client requests (HTTP 400/404) deplete the service's error budget unfairly. | Track SLIs against HTTP 5xx errors (`status=~"5.."`), not 4xx. |

---

## 8. Performance & Security Notes

### Prometheus Rule Evaluation Overhead
- Multi-window queries covering 1-hour to 6-hour ranges can be computationally heavy for Prometheus TSDB.
- Leverage **Prometheus Recording Rules** (`record: job:http_requests:rate5m`) to pre-compute rates, ensuring alert rules execute in milliseconds.

### On-Call Sustainability
- A sustainable on-call rotation experiences $\le 2$ pages per 12-hour shift.
- If engineers receive $> 5$ pages per day, the system suffers from alert fatigue, increasing the likelihood of human error during catastrophic outages.

---

## 9. Comparison / When to Use What

| Alerting Paradigm | Trigger Mechanism | Target Audience | Noise / Fatigue Level |
|---|---|---|---|
| **Raw Threshold (Legacy)** | Static metric limit (CPU $> 80\%$, Heap $> 90\%$) | Operations / Sysadmins | **Very High** (Frequent false alarms) |
| **SLO Multi-Burn-Rate (Modern SRE)** | Mathematical error budget consumption rate | On-Call Engineers | **Ultra Low** (Only fires when users suffer) |
| **Anomaly Detection (AI/ML)** | Statistical deviation from historical baseline | Capacity Planners | Medium (Prone to false alarms on holidays) |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between an SLI, an SLO, and an SLA?
<details>
<summary>Click to view answer</summary>

- **SLI (Service Level Indicator)**: A measurable metric tracking real-time performance (e.g., "99.8% of HTTP requests returned 200 OK within 250ms").
- **SLO (Service Level Objective)**: An internal target set by engineering and product teams (e.g., "99.9% availability over a 30-day rolling window").
- **SLA (Service Level Agreement)**: A legally binding contract with customers that includes penalties or financial credits if reliability drops below a certain threshold (e.g., 99.5%).
</details>

### Q2: What is an Error Budget and how is it calculated?
<details>
<summary>Click to view answer</summary>

The Error Budget is the allowable room for unreliability in a service over a given period:
$$\text{Error Budget} = 100\% - \text{SLO}$$
For a $99.9\%$ SLO over 30 days, the error budget is $0.1\%$. If the service handles 10,000,000 requests per month, the error budget is $10,000$ failed requests. Once these 10,000 failures are consumed, the budget is exhausted.
</details>

### Q3: What is "Burn Rate" and why is it superior to static threshold alerting?
<details>
<summary>Click to view answer</summary>

Burn rate is the speed at which an application consumes its error budget. A burn rate of $1.0$ means the budget will be exhausted exactly at the end of the period (30 days). A burn rate of $14.4$ consumes $2\%$ of the budget in 1 hour and exhausts the entire budget in 2 days. Alerting on burn rate ensures engineers are paged based on the **severity of user impact**, eliminating false alarms from minor transient spikes.
</details>

### Q4: Why does Google SRE recommend Dual-Window Multi-Burn-Rate alerting?
<details>
<summary>Click to view answer</summary>

Single-window burn rate alerts suffer from two flaws:
1. If the window is short (e.g. 10m), the alert resets quickly, but misses slow budget leaks.
2. If the window is long (e.g. 1h), the alert continues firing for an hour even after the incident has resolved.
A **dual-window** alert requires both the long window (e.g. 1 hour) AND a short window (e.g. 5 minutes) to exceed the burn rate threshold. This guarantees immediate detection without lingering alerts after recovery.
</details>

### Q5: Should HTTP 4xx client errors be counted against a service's SLO?
<details>
<summary>Click to view answer</summary>

In almost all cases, **no**. HTTP 4xx responses (400 Bad Request, 401 Unauthorized, 404 Not Found) indicate invalid client requests, expired credentials, or bad URLs created by external clients. Including 4xx errors would allow an external attacker or broken client script to artificially exhaust your service's error budget. Availability SLIs should measure server errors (HTTP 5xx).
</details>

### Q6: What is "Alert Fatigue" and how do you combat it?
<details>
<summary>Click to view answer</summary>

Alert fatigue is the cognitive desensitization that occurs when on-call engineers are inundated with frequent, non-actionable, or false alarms. Engineers begin ignoring alerts, eventually missing genuine production crises. It is combated by:
- Deleting alerts that do not require immediate action.
- Transitioning from CPU/host threshold alerts to SLO burn-rate alerts.
- Routing non-critical warnings to ticket queues instead of paging phones.
</details>

### Q7: What are Prometheus Recording Rules and how do they benefit alerting?
<details>
<summary>Click to view answer</summary>

Recording rules allow Prometheus to precompute frequently needed, computationally expensive PromQL expressions (such as 1-hour request rates across 100 pods) and save the result as a new time-series. Alerting rules can then evaluate these precomputed series instantly, drastically reducing Prometheus CPU load and query timeouts.
</details>

### Q8: How should an engineering team respond when their Error Budget is exhausted?
<details>
<summary>Click to view answer</summary>

According to SRE principles:
1. Feature deployments are temporarily paused.
2. Development focus shifts exclusively to reliability, testing, automated rollbacks, and bug fixes.
3. Once the rolling window recovers and the budget is replenished, normal feature development resumes.
</details>

---

## 11. Practice Exercises

### Exercise 1: Calculate Remaining Error Budget (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: A service handled 5,000,000 requests in 30 days with a 99.9% SLO. During the month, 3,200 requests returned HTTP 500 errors. What was the total error budget, how many errors were consumed, and what percentage of the budget remains?

**Solution**:
1. $\text{Total Allowed Error Budget} = 5,000,000 \times (1 - 0.999) = 5,000,000 \times 0.001 = 5,000 \text{ errors}$.
2. $\text{Consumed Errors} = 3,200 \text{ errors}$.
3. $\text{Remaining Budget} = 5,000 - 3,200 = 1,800 \text{ errors}$.
4. $\text{Remaining Budget Percentage} = \frac{1,800}{5,000} \times 100\% = 36.0\%$.
</details>

### Exercise 2: Write Prometheus Recording Rule for HTTP Rate (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a Prometheus recording rule that computes the 5-minute request rate per application and status code, and another that precomputes total 5-minute request rate.

**Solution**:
```yaml
groups:
  - name: http_recording_rules
    rules:
      - record: job:http_server_requests:rate5m
        expr: sum(rate(http_server_requests_seconds_count[5m])) by (application, status)

      - record: job:http_server_requests_total:rate5m
        expr: sum(rate(http_server_requests_seconds_count[5m])) by (application)
```
</details>

### Exercise 3: Complete Multi-Window Alerting Rule for P99 Latency (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a Prometheus alert that triggers when the 99th percentile response time for `application="order-service"` exceeds 500ms across both a 30-minute window and a 5-minute window.

**Solution**:
```yaml
groups:
  - name: latency-slo-alerts
    rules:
      - alert: OrderServiceP99LatencyBreach
        expr: |
          (
            histogram_quantile(0.99, sum(rate(http_server_requests_seconds_bucket{application="order-service"}[30m])) by (le))
            > 0.500
          )
          and
          (
            histogram_quantile(0.99, sum(rate(http_server_requests_seconds_bucket{application="order-service"}[5m])) by (le))
            > 0.500
          )
        for: 2m
        labels:
          severity: page
          team: order-platform
        annotations:
          summary: "P99 Latency SLO breached on order-service"
          description: "Order service 99th percentile response time is > 500ms over both 30m and 5m sliding windows."
          runbook_url: "https://wiki.internal/runbooks/order-service-latency"
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Concept / Rule                              | Mathematical Definition / SRE Rule                          |
+---------------------------------------------+-------------------------------------------------------------+
| SLI (Indicator)                             | (Good Events / Total Events) * 100%                         |
| SLO (Objective)                             | Target reliability over rolling window (e.g. 99.9% / 30d)   |
| Error Budget                                | 100% - SLO (Room for failure without breaching contract)   |
| 14.4x Burn Rate Alert                       | Consumes 2% budget in 1 hour; requires IMMEDIATE PagerDuty  |
| 3.0x Burn Rate Alert                        | Consumes 5% budget in 12 hours; creates non-urgent ticket   |
| Dual-Window Evaluation                      | Requires both long window (1h) AND short window (5m)        |
| Runbook Hygiene                             | Every paged alert must contain a valid runbook_url link     |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Google SRE Book: Service Level Objectives
- Google SRE Workbook: Alerting on SLOs & Multi-Window Multi-Burn-Rate Alerts
- Prometheus Official Documentation: Alerting Rules
- Martin Fowler: On-Call Scheduling & Alert Fatigue Prevention