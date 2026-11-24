---
title: "SLOs, SLIs, Error Budgets, and Alerting Rules for FastAPI"
description: "Implement Site Reliability Engineering (SRE) principles in FastAPI: define SLIs, set realistic SLOs, track Error Budgets, and configure Google SRE multi-window multi-burn-rate PromQL alerts."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "prometheus-client 0.21+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# SLOs, SLIs, Error Budgets, and Alerting Rules for FastAPI

## 1. Conceptual Foundation & Mental Model

### The SRE Observability Triad: SLI, SLO, and SLA
In modern Site Reliability Engineering (SRE, pioneered by Google), alerting should not be based on raw CPU utilization or arbitrary threshold blips. Alerting on a server running at 85% CPU when all API requests are succeeding in 15ms creates **alert fatigue**, desensitizing on-call engineers.

Reliability is managed through three distinct constructs:
1. **Service Level Indicator (SLI)**: A carefully defined quantitative measure of service performance:
   $$\text{SLI} = \frac{\text{Good Events}}{\text{Total Events}} \times 100$$
   Example: The percentage of HTTP GET requests to `/orders` that return HTTP status codes `< 500` with a latency `< 250\text{ms}`.
2. **Service Level Objective (SLO)**: A target reliability metric agreed upon by engineering and product teams:
   Example: 99.9% of requests must meet the SLI over a 30-day rolling window.
3. **Service Level Agreement (SLA)**: A contractual agreement with customers that carries business consequences or financial penalties (e.g., service credits) if breached. The SLO is always set stricter than the SLA.

```
+-------------------------------------------------------------------------------+
|                       SLI / SLO / SLA HIERARCHY                               |
|                                                                               |
|  [SLI: What is measured]                                                      |
|  "99.94% of API requests completed successfully under 200ms this week."       |
|                                                                               |
|  [SLO: What the engineering team targets]                                     |
|  "Target: 99.90% over a 30-day rolling window." (Internal target)             |
|                                                                               |
|  [SLA: What legal/business promises to customers]                             |
|  "Guarantee: 99.50% uptime, or customer receives a 20% billing refund."      |
+-------------------------------------------------------------------------------+
```

### The Error Budget
An SLO of 99.9% implies an **Error Budget** of 0.1%:
$$\text{Error Budget} = 100\% - \text{SLO} = 0.1\%$$
If a FastAPI service serves 10,000,000 requests in a month, the application can tolerate up to **10,000 failed or slow requests** without violating its reliability promise. 

The Error Budget is a shared currency between Product and Engineering:
- **Budget Remaining**: High velocity; teams can deploy risky new features, refactor architecture, and experiment.
- **Budget Exhausted (Burned)**: Feature freezes; all engineering effort shifts to reliability, automated testing, and performance optimization.

---

## 2. Architecture & Internal Mechanics

Understanding how burn rate alerting detects outages based on the rate of Error Budget consumption.

```
+-----------------------------------------------------------------------------+
|                          ERROR BUDGET BURN RATES                            |
|                                                                             |
|  - 1x Burn Rate:  Consumes 100% of Error Budget over 30 days (Normal)       |
|  - 2x Burn Rate:  Consumes 100% of Error Budget in 15 days                  |
|  - 14.4x Burn:    Consumes 2% of budget in 1 hour -> PagerDuty Critical!    |
|  - 6x Burn:       Consumes 5% of budget in 6 hours -> Slack Warning!        |
+-----------------------------------------------------------------------------+
                                      │
                   Evaluated via Prometheus Time-Series
                                      ▼
+-----------------------------------------------------------------------------+
|                   GOOGLE SRE MULTI-WINDOW MULTI-BURN ALERT                  |
|                                                                             |
|  Short Window (e.g. 5m) AND Long Window (e.g. 1h) must BOTH burn rapidly!   |
|  - Prevents alerting on brief 30-second blips (Zero false positives)        |
|  - Detects catastrophic sudden outages within 2 minutes                     |
+-----------------------------------------------------------------------------+
                                      │
                   +------------------+------------------+
                   │ (14.4x Burn)                        │ (6x Burn)
                   ▼                                     ▼
        [PagerDuty: Wake Engineer]             [Slack: #eng-alerts]
```

---

## 3. Comprehensive Syntax & Core Mechanics

### Availability SLI Formula (PromQL)
Measuring the proportion of successful HTTP requests (non-5xx) over a rolling window:

```promql
sum(rate(http_requests_total{status!~"5.."}[30d]))
/
sum(rate(http_requests_total[30d]))
```

### Latency SLI Formula (PromQL)
Measuring the proportion of requests completing in under 200 milliseconds:

```promql
sum(rate(http_request_duration_seconds_bucket{le="0.2"}[30d]))
/
sum(rate(http_request_duration_seconds_count[30d]))
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Multi-Window Multi-Burn Rate Alerting Rules (Prometheus YAML)
Production Prometheus alerting rules implementing Google SRE's multi-window burn rate recommendations for a 99.9% availability SLO.

```yaml
# prometheus_slo_rules.yml
groups:
  - name: fastapi_slo_alerts
    rules:
      # Page Alert: 14.4x burn rate over 1h AND 5m (Consumes 2% budget in 1 hour)
      - alert: FastAPIAvailabilityBudgetBurningFast
        expr: |
          (
            sum(rate(http_requests_total{status=~"5.."}[1h]))
            /
            sum(rate(http_requests_total[1h]))
          ) > (1 - 0.999) * 14.4
          and
          (
            sum(rate(http_requests_total{status=~"5.."}[5m]))
            /
            sum(rate(http_requests_total[5m]))
          ) > (1 - 0.999) * 14.4
        for: 2m
        labels:
          severity: page
          tier: api-gateway
        annotations:
          summary: "FastAPI is burning 14.4x error budget rapidly"
          description: "High error rate over the last hour and 5 minutes. 2% of 30-day budget consumed in 1 hour."

      # Ticket Alert: 6x burn rate over 6h AND 30m (Consumes 5% budget in 6 hours)
      - alert: FastAPIAvailabilityBudgetBurningSlow
        expr: |
          (
            sum(rate(http_requests_total{status=~"5.."}[6h]))
            /
            sum(rate(http_requests_total[6h]))
          ) > (1 - 0.999) * 6
          and
          (
            sum(rate(http_requests_total{status=~"5.."}[30m]))
            /
            sum(rate(http_requests_total[30m]))
          ) > (1 - 0.999) * 6
        for: 15m
        labels:
          severity: ticket
          tier: api-gateway
        annotations:
          summary: "FastAPI is burning error budget at 6x normal rate"
          description: "Elevated error rate over the past 6 hours. Will exhaust budget in 5 days if unresolved."
```

---

### Example 2: In-Memory Error Budget Calculation Endpoint
Exposing an internal administrative route allowing engineers and CI/CD pipelines to inspect real-time Error Budget health.

```python
# app/api/v1/slo.py
from fastapi import APIRouter, Depends
from pydantic import BaseModel

router = APIRouter(prefix="/system/slo", tags=["SRE"])

class ErrorBudgetStatus(BaseModel):
    slo_target: float
    current_availability: float
    error_budget_total: float
    error_budget_consumed_pct: float
    status: str

# In production: query Prometheus HTTP API
async def fetch_prometheus_metric(query: str) -> float:
    return 0.9994  # Simulated: 99.94% availability currently

@router.get("/status", response_model=ErrorBudgetStatus)
async def get_slo_status():
    slo_target = 0.9990  # 99.9%
    total_budget = 1.0 - slo_target  # 0.001 (0.1%)
    
    current_avail = await fetch_prometheus_metric("current_sli_query")
    error_rate = max(0.0, 1.0 - current_avail)
    
    # Calculate % of error budget consumed
    consumed_pct = (error_rate / total_budget) * 100.0

    health_status = "HEALTHY"
    if consumed_pct > 100.0:
        health_status = "EXHAUSTED (FEATURE FREEZE)"
    elif consumed_pct > 75.0:
        health_status = "WARNING"

    return ErrorBudgetStatus(
        slo_target=slo_target * 100,
        current_availability=round(current_avail * 100, 3),
        error_budget_total=round(total_budget * 100, 3),
        error_budget_consumed_pct=round(consumed_pct, 1),
        status=health_status,
    )
```

---

### Example 3: Incident Severity Classification (P1 through P4)
Standardizing incident severity definitions and escalation pathways in code and runbooks.

```python
# app/core/sre/severity.py
from enum import StrEnum
from dataclasses import dataclass

class IncidentSeverity(StrEnum):
    P1_CRITICAL = "p1_critical"  # Complete outage; >50% users affected; Page on-call immediately
    P2_MAJOR = "p2_major"        # Core functionality degraded; >10% users affected; Page on-call
    P3_MINOR = "p3_minor"        # Non-critical feature broken; Workaround exists; Slack alert
    P4_TRIVIAL = "p4_trivial"    # Minor cosmetic defect; Jira ticket created during business hours

@dataclass
class IncidentPolicy:
    severity: IncidentSeverity
    target_response_time_minutes: int
    escalation_channel: str

INCIDENT_POLICIES = {
    IncidentSeverity.P1_CRITICAL: IncidentPolicy(IncidentSeverity.P1_CRITICAL, 5, "pagerduty-critical"),
    IncidentSeverity.P2_MAJOR: IncidentPolicy(IncidentSeverity.P2_MAJOR, 15, "pagerduty-major"),
    IncidentSeverity.P3_MINOR: IncidentPolicy(IncidentSeverity.P3_MINOR, 60, "slack-eng-alerts"),
    IncidentSeverity.P4_TRIVIAL: IncidentPolicy(IncidentSeverity.P4_TRIVIAL, 1440, "jira-backlog"),
}
```

---

### Example 4: Latency SLI PromQL Alerting (p95 Under 200ms)
Monitoring tail latency degradation against a defined Latency SLO.

```yaml
# prometheus_latency_slo.yml
groups:
  - name: fastapi_latency_slo
    rules:
      - alert: FastAPILatencySLOBreached
        expr: |
          (
            sum(rate(http_request_duration_seconds_bucket{le="0.2", route!~"/health.*"}[1h]))
            /
            sum(rate(http_request_duration_seconds_count{route!~"/health.*"}[1h]))
          ) < 0.95
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "FastAPI Latency SLO Breached (<95% under 200ms)"
          description: "Over the last hour, only {{ $value | printf '%.2f' }}% of requests met the 200ms target."
```

---

### Example 5: Testing SLO Compliance in End-to-End Test Harness
Asserting that an integration test suite satisfies latency and error SLOs under simulated load.

```python
# tests/test_slo_compliance.py
import pytest
import time
from fastapi.testclient import TestClient
from app.main import app

client = TestClient(app)

def test_endpoint_meets_latency_slo():
    """Validates that 99% of sample requests complete under 200ms."""
    durations = []
    
    for _ in range(100):
        start = time.perf_counter()
        resp = client.get("/api/v1/orders/123")
        durations.append(time.perf_counter() - start)
        assert resp.status_code == 200

    durations.sort()
    # 99th percentile index
    p99_latency = durations[98]
    print(f"\nMeasured p99 Latency: {p99_latency * 1000:.2f}ms")

    assert p99_latency < 0.20, f"SLO Violation: p99 latency was {p99_latency * 1000}ms (limit: 200ms)"
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Setting Unrealistic "100% Availability" SLOs**: Demanding 100% availability is mathematically impossible and economically disastrous. It implies an Error Budget of zero, meaning no deployments, no OS upgrades, and no infrastructure maintenance.
2. **Alert Fatigue from Single-Window Thresholds**: Setting an alert like `rate(5xx) > 0` triggers whenever a user abruptly closes a browser tab or inputs malformed data. Use **Multi-Window Multi-Burn-Rate** alerts to eliminate false positives.
3. **Evaluating Health Check Probes in User SLIs**: If Kubernetes readiness probes make 1,000 requests per minute to `/healthz` while user traffic is only 50 requests per minute, a 100% failure rate on user endpoints is diluted into a 95% overall success rate! Always filter out internal health routes (`route!~"/health.*"`).
4. **Window Misalignment in Low-Volume Services**: In low-traffic microservices (e.g., 1 request every 5 minutes), a single failed request causes availability to drop from 100% to 0%, triggering false alarms. Use larger rolling windows or count-based thresholds for low-volume endpoints.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Alerting on Raw CPU Utilization** | Pre-SRE infrastructure habits | Pages engineers at 3 AM when servers are healthy and users are happy. | Alert strictly on user-facing SLI burn rates and Error Budgets. |
| **Including Health Probes in SLIs** | Forgetting route exclusions | Dilutes actual user failure rates; masks real production outages. | Exclude `/healthz`, `/readyz`, and `/metrics` from SLI queries. |
| **Single-Window Alerts** | Using simple 5-minute thresholds | Fires on transient 30-second spikes; causes severe alert fatigue. | Implement Google SRE Multi-Window Multi-Burn Rate alerts. |
| **Setting SLO Target Equal to SLA** | Misunderstanding contractual risk | Any internal failure directly triggers financial customer penalties. | Set SLO significantly stricter than SLA (e.g. 99.9% SLO vs 99.5% SLA). |
| **Ignoring Error Budget Exhaustion** | Business ignoring engineering alerts | Technical debt multiplies; system collapses into chronic instability. | Enforce feature freezes when Error Budget is fully exhausted. |
| **Averaging Latency Across Endpoints** | Combining fast and slow routes | A 10ms health check masks a 5,000ms checkout bottleneck. | Define distinct SLIs for critical journeys (e.g., Checkout vs. Catalog). |

---

## 7. Performance & Scalability Considerations

- **Recording Rules for Complex PromQL**: Calculating 30-day aggregations (`rate[30d]`) on the fly on every dashboard refresh exhausts Prometheus CPU. Define **Prometheus Recording Rules** that pre-compute and store 5-minute and 1-hour error rates into pre-calculated time-series.
- **Python 3.13+ Free-Threading**: Fast, predictable request processing under free-threaded Python reduces tail latency jitter, helping services easily meet 99.9% p99 SLO targets.

---

## 8. Security & OWASP Guardrails

- **OWASP A04 (Insecure Design - DoS Mitigation)**: When an Error Budget begins burning rapidly due to a distributed denial-of-service (DDoS) attack or credential-stuffing wave, automated ops toggles should activate aggressive Cloudflare/WAF rate-limiting rules.
- **Audit Trails for SLO Overrides**: Document all decisions by management to override a feature freeze when an Error Budget has been exhausted.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_burn_rate_calculations.py
def calculate_burn_rate(error_rate: float, slo_target: float) -> float:
    error_budget = 1.0 - slo_target
    return error_rate / error_budget

def test_burn_rate_math():
    slo = 0.999  # 99.9% SLO -> 0.1% budget
    # If error rate is 1.44%, burn rate is 14.4x (2% budget consumed in 1 hour)
    burn = calculate_burn_rate(error_rate=0.0144, slo_target=slo)
    assert round(burn, 1) == 14.4
```

---

## 10. Real-World Case Study / Production Incident

A financial trading API had a contractually binding SLA guaranteeing 99.9% uptime. 

**The Incident**:
- Product management pushed three rapid feature releases in one week.
- A memory leak in the third release caused pods to restart every 4 hours.
- The engineering team had no formal Error Budget policy; they continued shipping features despite rising error rates.
- On the 24th day of the month, the platform breached the 99.9% SLO and dropped to 99.3% availability.
- Enterprise customers invoked their SLA penalty clauses, forcing the company to pay \$340,000 in customer billing refunds.

**The Post-Mortem Transformation**:
1. The company instituted a strict Error Budget policy: if the 30-day budget drops below 20%, all feature releases are halted automatically.
2. PromQL burn-rate alerts were configured. If the burn rate hits 14.4x, the on-call engineer is paged in under 2 minutes.
3. During the subsequent year, the platform maintained 99.96% availability with zero SLA financial penalties.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the fundamental difference between an SLI, an SLO, and an SLA?</summary>
- **SLI (Service Level Indicator)**: The actual quantitative metric measuring service performance (e.g., "99.92% of requests returned HTTP 200 in < 200ms").
- **SLO (Service Level Objective)**: The internal reliability goal agreed upon by engineering and product (e.g., "99.90% availability over a 30-day rolling window").
- **SLA (Service Level Agreement)**: The external contractual commitment with customers that includes financial penalties or credits if breached (e.g., "99.50% uptime guaranteed").
</details>

<details>
<summary>2. What is an "Error Budget", and how is it calculated?</summary>
The Error Budget represents the total amount of unreliability a service can tolerate before violating its SLO. It is calculated as:
$$\text{Error Budget} = 100\% - \text{SLO}$$
For a 99.9% SLO, the Error Budget is 0.1% of all requests. It provides a formal framework to balance innovation and feature releases against operational stability.
</details>

<details>
<summary>3. Why is alerting on high CPU or memory utilization considered an anti-pattern in modern SRE?</summary>
High CPU or memory utilization does not necessarily mean users are suffering. A server running at 90% CPU might be processing requests in 10ms with zero errors. Alerting on hardware utilization leads to alert fatigue and unnecessary on-call pages. SRE principles mandate alerting on user-facing symptoms (SLIs like Error Rate and Latency), using utilization metrics only for debugging root causes.
</details>

<details>
<summary>4. What is a "Burn Rate", and what does a 14.4x burn rate signify?</summary>
The Burn Rate measures how fast a service is consuming its Error Budget relative to normal:
- A **1x burn rate** consumes exactly 100% of the budget over the entire SLO window (e.g., 30 days).
- A **14.4x burn rate** consumes 2% of the total 30-day budget in just 1 hour. This indicates a catastrophic event requiring an immediate on-call page.
</details>

<details>
<summary>5. What is Google SRE's Multi-Window Multi-Burn-Rate alerting strategy?</summary>
Traditional threshold alerts either alert too slowly on massive outages or too quickly on brief transient spikes. Multi-window multi-burn-rate alerts evaluate both a short window (e.g., 5 minutes) and a long window (e.g., 1 hour). The alert fires only if the burn rate is elevated across *both* windows, virtually eliminating false positives while detecting severe outages within minutes.
</details>

<details>
<summary>6. What actions should an organization take when an Error Budget is exhausted?</summary>
When an Error Budget is exhausted, an agreed-upon policy takes effect:
1. Feature releases and deployments are frozen.
2. Engineering focus shifts exclusively to reliability, technical debt elimination, automated testing, and performance optimization.
3. Releases resume only after the rolling 30-day availability recovers above the SLO target.
</details>

<details>
<summary>7. Why must health check and probe endpoints be excluded from SLI calculations?</summary>
Internal Kubernetes health checks (`/healthz`, `/readyz`) execute continuously (e.g., every 2 seconds). Including them in SLI queries artificially inflates the total request count with trivial, fast requests, masking real user latency spikes and diluting failure rates on critical business routes.
</details>

<details>
<summary>8. How do you define a Latency SLI using Prometheus Histograms?</summary>
By calculating the proportion of requests in the duration bucket corresponding to the latency target:
```promql
sum(rate(http_request_duration_seconds_bucket{le="0.25"}[30d]))
/
sum(rate(http_request_duration_seconds_count[30d]))
```
This yields the exact percentage of requests completing within 250 milliseconds over the 30-day period.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Calculate Error Budget from SLO</summary>

**Task**: Write a Python function `get_allowed_failures(total_requests: int, slo_percent: float) -> int` that calculates how many requests can fail before breaching the budget.

**Solution**:
```python
def get_allowed_failures(total_requests: int, slo_percent: float) -> int:
    error_budget = (100.0 - slo_percent) / 100.0
    return int(total_requests * error_budget)

assert get_allowed_failures(1000000, 99.9) == 1000
assert get_allowed_failures(1000000, 99.0) == 10000
```
</details>

<details>
<summary>Exercise 2 (Easy): Availability SLI Calculation</summary>

**Task**: Given 99,850 successful requests and 150 failed requests, calculate the availability SLI percentage.

**Solution**:
```python
total = 99850 + 150
successful = 99850
sli = (successful / total) * 100.0
print(f"SLI: {sli:.3f}%")  # Evaluates to 99.850%
```
</details>

<details>
<summary>Exercise 3 (Medium): PromQL Availability Ratio</summary>

**Task**: Write a PromQL expression computing the availability percentage of the `api_requests_total` metric, where `status=~"5.."` denotes errors, over a 1-hour window.

**Solution**:
```promql
(
  sum(rate(api_requests_total{status!~"5.."}[1h]))
  /
  sum(rate(api_requests_total[1h]))
) * 100
```
</details>

<details>
<summary>Exercise 4 (Medium): Calculate Burn Rate from Incident Metrics</summary>

**Task**: Write a Python function `calculate_hours_to_exhaustion(slo_target: float, current_error_rate: float) -> float` determining how many hours until the 30-day budget is exhausted.

**Solution**:
```python
def calculate_hours_to_exhaustion(slo_target: float, current_error_rate: float) -> float:
    error_budget = 1.0 - slo_target
    if current_error_rate <= 0:
        return float("inf")
    burn_rate = current_error_rate / error_budget
    total_hours_in_30_days = 30 * 24  # 720 hours
    return total_hours_in_30_days / burn_rate

# If SLO is 99.9% and error rate is 2% (0.02), burn rate is 20x
# 720 / 20 = 36 hours until complete budget exhaustion
hours = calculate_hours_to_exhaustion(0.999, 0.02)
assert round(hours, 1) == 36.0
```
</details>

<details>
<summary>Exercise 5 (Hard): Prometheus Alerting Rule YAML for 14.4x Burn Rate</summary>

**Task**: Write a complete Prometheus alert rule named `APICriticalBudgetBurn` that pages if the 1-hour and 5-minute error rate exceeds a 14.4x burn rate on a 99.9% SLO.

**Solution**:
```yaml
groups:
  - name: slo_alerts
    rules:
      - alert: APICriticalBudgetBurn
        expr: |
          (
            sum(rate(http_requests_total{status=~"5.."}[1h]))
            /
            sum(rate(http_requests_total[1h]))
          ) > 0.0144
          and
          (
            sum(rate(http_requests_total{status=~"5.."}[5m]))
            /
            sum(rate(http_requests_total[5m]))
          ) > 0.0144
        for: 2m
        labels:
          severity: page
        annotations:
          summary: "API Error Budget burning at 14.4x rate"
```
</details>

<details>
<summary>Exercise 6 (Hard): FastAPI SRE Error Budget Health Gate</summary>

**Task**: Implement an internal FastAPI endpoint `/system/release-gate` that returns `{"allowed": false, "reason": "Error Budget Exhausted"}` if current availability is below 99.9%.

**Solution**:
```python
from fastapi import APIRouter

router = APIRouter()

async def get_current_sli() -> float:
    # Simulated Prometheus metric lookup
    return 0.9985  # 99.85% (Breached!)

@router.get("/system/release-gate")
async def release_gate():
    current_availability = await get_current_sli()
    target_slo = 0.9990

    if current_availability < target_slo:
        return {
            "allowed": False,
            "reason": "Error Budget Exhausted (Feature Freeze Active)",
            "current_sli": current_availability,
            "target_slo": target_slo,
        }
    return {
        "allowed": True,
        "current_sli": current_availability,
        "target_slo": target_slo,
    }
```
</details>
