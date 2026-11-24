---
title: "Machine Learning Model Versioning, Shadow Deployments, and A/B Testing in FastAPI"
description: "Architecting enterprise ML lifecycle management: MLflow, DVC, canary releases, shadow (dark) traffic shadowing, A/B split testing, and model registry integration in FastAPI."
fastapi_version: "0.115+"
python_version: "3.12+"
free_threading_compatible: true
tested_with: "Python 3.12.8, FastAPI 0.115.6, mlflow-skinny 2.18.0"
---

# Machine Learning Model Versioning, Shadow Deployments, and A/B Testing in FastAPI

## 1. Conceptual Foundation & Mental Model

Deploying an updated machine learning model is fundamentally more hazardous than deploying standard application code. A new model may compile cleanly, pass unit tests, and achieve high offline validation accuracy, yet degrade severely in production due to **data distribution drift**, latency regressions, or subtle edge-case hallucinations.

To mitigate this operational risk, mature MLOps architectures decouple model release from full production exposure using three core deployment strategies:
1. **A/B Testing (Split Testing)**: User traffic is divided deterministically (e.g. 50% Model A / 50% Model B) based on stable user identifiers. Business metrics (click-through rate, conversion rate, user retention) are measured across variants to determine statistical superiority.
2. **Shadow Deployments (Dark Traffic)**: 100% of live user traffic is served by the production Model A (Baseline). Concurrently, an asynchronous copy of the request payload is forwarded in the background ("shadowed") to the candidate Model B. Model B's outputs and latency are logged and compared against Model A without ever affecting user responses.
3. **Canary Releases**: Incrementally shifting traffic (e.g. 5% $\to$ 20% $\to$ 100%) to a new model version while actively monitoring automated rollback triggers (error rates, p99 latency spikes, prediction distribution drift).

```
Shadow Deployment Architecture:
[ Live User Request ] ──► [ FastAPI Ingress Gateway ]
                                  │
         ┌────────────────────────┴────────────────────────┐
         ▼ (Primary Synchronous Path)                      ▼ (Asynchronous Background Task)
[ Production Model A (v1.2) ]                    [ Candidate Model B (v2.0) - Shadow ]
         │                                                 │
         ▼                                                 ▼
[ Return 200 OK to User ]                        [ Log Output & Latency to MLflow ]
(Zero user latency impact!)                      (Compare accuracy & drift safely!)
```

---

## 2. Architecture & Internal Mechanics

```
+---------------------------------------------------------------+
|                       FastAPI Route Layer                     |
|  @app.post("/predict")                                        |
|  async def predict(data: InputDTO, user_id: str):             |
+-------------------------------+-------------------------------+
                                │
                                ▼
+---------------------------------------------------------------+
|                   MLflow Model Registry Store                 |
|  - Tracks Model Lineage, Git Commit, S3 Weight URIs           |
|  - Models: "fraud_detector" (Stage: Production, v3)           |
|  - Models: "fraud_detector" (Stage: Candidate, v4)            |
+-------------------------------+-------------------------------+
                                │
         ┌──────────────────────┴──────────────────────┐
         ▼ (Determined by MurmurHash / User ID)        ▼
+--------------------------------+   +--------------------------------+
|       Model A (Control)        |   |       Model B (Treatment)      |
|  - Current Production Model    |   |  - New Candidate Model         |
|  - Logs variant: "A" to StatsD |   |  - Logs variant: "B" to StatsD |
+--------------------------------+   +--------------------------------+
```

### Deterministic User Hashing for A/B Testing
A common pitfall is using random numbers (`random.random() < 0.5`) to route users to models. If a user refreshes the page, they alternate between Model A and Model B, causing inconsistent user experiences.

Instead, route users using a deterministic hash of their unique user ID:
$$\text{Bucket} = \text{hash}(\text{user\_id} + \text{experiment\_salt}) \pmod{100}$$
If $\text{Bucket} < 50$, route to Model A; otherwise, route to Model B. A given user consistently receives the same model variant throughout the entire experiment.

---

## 3. Comprehensive Syntax & Core Mechanics

### Deterministic Bucket Allocation Function
```python
import mmh3 # MurmurHash3 for fast, uniform 32-bit hashing

def assign_experiment_variant(user_id: str, experiment_name: str, split_percentage: int = 50) -> str:
    # Hash combining user ID and experiment salt
    hash_value = mmh3.hash(f"{experiment_name}:{user_id}")
    bucket = abs(hash_value) % 100
    return "treatment" if bucket < split_percentage else "control"
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: A/B Testing Endpoint with Deterministic Variant Assignment
```python
from fastapi import FastAPI, Header, HTTPException, Depends
from pydantic import BaseModel
import mmh3

app = FastAPI()

class FeaturesDTO(BaseModel):
    features: list[float]

class PredictionResponse(BaseModel):
    prediction: float
    model_version: str
    experiment_variant: str

# Mock models
def predict_model_v1(features: list[float]) -> float:
    return sum(features) * 1.0

def predict_model_v2(features: list[float]) -> float:
    return sum(features) * 1.25

@app.post("/predict/churn", response_model=PredictionResponse)
async def predict_churn(
    data: FeaturesDTO,
    x_user_id: str = Header(...)
):
    # Determine variant using MurmurHash3 (50/50 split)
    hash_val = abs(mmh3.hash(f"churn_exp_2026:{x_user_id}")) % 100
    
    if hash_val < 50:
        variant = "control"
        version = "v1.2.0"
        score = predict_model_v1(data.features)
    else:
        variant = "treatment"
        version = "v2.0.0"
        score = predict_model_v2(data.features)

    # Log assignment to telemetry for conversion correlation
    print(f"User {x_user_id} evaluated by {version} ({variant})")

    return PredictionResponse(
        prediction=round(score, 4),
        model_version=version,
        experiment_variant=variant
    )
```

### Scenario 2: Shadow Deployment (Dark Traffic) via `BackgroundTasks`
```python
from fastapi import FastAPI, BackgroundTasks
from pydantic import BaseModel
import asyncio
import time

app = FastAPI()

class InferenceInput(BaseModel):
    features: list[float]

def run_candidate_shadow_model(features: list[float], primary_score: float):
    t0 = time.perf_counter()
    # Execute candidate model v2 in background without affecting user response
    candidate_score = sum(features) * 1.15
    duration_ms = (time.perf_counter() - t0) * 1000

    # Log comparison metrics to telemetry/MLflow
    divergence = abs(primary_score - candidate_score)
    print(f"[SHADOW] v2 Duration: {duration_ms:.2f}ms | Divergence: {divergence:.4f}")

@app.post("/predict/fraud")
async def predict_fraud(payload: InferenceInput, bg: BackgroundTasks):
    # 1. Primary Model v1 (Serves user)
    primary_score = sum(payload.features) * 1.0

    # 2. Shadow Model v2 (Fire-and-forget background execution)
    bg.add_task(run_candidate_shadow_model, payload.features, primary_score)

    # 3. User receives response immediately from Primary Model
    return {"fraud_score": primary_score, "version": "v1.0.0"}
```

### Scenario 3: Dynamic Model Version Routing via MLflow Model Registry
```python
from fastapi import FastAPI, HTTPException
import mlflow.pyfunc
from contextlib import asynccontextmanager

models = {}

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Connect to MLflow Tracking Server and pull production & staging models
    print("Connecting to MLflow Registry...")
    # models["production"] = mlflow.pyfunc.load_model("models:/fraud_classifier/Production")
    # models["staging"] = mlflow.pyfunc.load_model("models:/fraud_classifier/Staging")
    models["production"] = lambda x: [0.2]
    models["staging"] = lambda x: [0.4]
    yield
    models.clear()

app = FastAPI(lifespan=lifespan)

@app.post("/models/{model_alias}/predict")
async def predict_by_alias(model_alias: str, features: list[float]):
    if model_alias not in models:
        raise HTTPException(status_code=404, detail=f"Model alias '{model_alias}' not found")
        
    prediction = models[model_alias]([features])
    return {"alias": model_alias, "result": prediction[0]}
```

### Scenario 4: Automated Canary Rollback on High Failure Rate
```python
import time
from fastapi import FastAPI, Header, HTTPException, status

app = FastAPI()

# Canary state tracking
canary_metrics = {"total": 0, "errors": 0}
CANARY_ACTIVE = True

@app.post("/canary/predict")
async def canary_predict(x_user_id: str = Header(...)):
    global CANARY_ACTIVE
    
    # If error rate exceeds 2%, trip circuit breaker and abort canary
    if canary_metrics["total"] > 100:
        error_rate = canary_metrics["errors"] / canary_metrics["total"]
        if error_rate > 0.02:
            CANARY_ACTIVE = False
            print("ALERT: Canary error rate exceeded 2%! Automatically rolling back to baseline.")

    use_canary = CANARY_ACTIVE and (abs(hash(x_user_id)) % 100 < 10)
    version = "v2.0-canary" if use_canary else "v1.0-stable"

    canary_metrics["total"] += 1
    # Process prediction...
    return {"version": version}
```

### Scenario 5: Canary Routing via Kubernetes Header Routing (Istio / Nginx)
```yaml
# VirtualService routing 10% of traffic to Canary based on HTTP headers
apiVersion: networking.istio.io/v1alpha3
kind: VirtualService
metadata:
  name: ml-inference-routing
spec:
  hosts:
    - "ml-api.enterprise.internal"
  http:
    - route:
        - destination:
            host: ml-service
            subset: v1-production
          weight: 90
        - destination:
            host: ml-service
            subset: v2-canary
          weight: 10
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Prediction Drift Going Undetected in Shadow Deployments**:
   If a shadow deployment logs outputs to a database or file system without automated divergence alerting, severe data drift or degradation can go unnoticed for weeks. Always compute real-time statistical divergence (e.g. Kolmogorov-Smirnov test or Population Stability Index) between primary and shadow outputs.

2. **Shadow Traffic Overloading Downstream Infrastructure**:
   Duplicating 100% of production traffic to a shadow model doubles compute and database query volume. If downstream dependencies (databases, feature stores, Redis caches) are not sized for 2x load, shadow traffic will overwhelm production databases.

3. **User Experience Inconsistency in A/B Testing**:
   Using session-based or random assignment causes a user to see Model A on their laptop and Model B on their phone, or see different outputs on page refresh. Always anchor assignment to immutable user identifiers (`user_id`).

4. **Resource Leaks from Shadow Tasks**:
   Executing shadow predictions via untracked `asyncio.create_task` during heavy traffic surges will accumulate thousands of background tasks in RAM, causing container out-of-memory crashes. Use bounded queues.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Random assignment (`random.random()`) in A/B tests | Inconsistent predictions for the same user across requests | Use deterministic hashing on `user_id` |
| Serving shadow predictions to users | Users receive experimental, unvalidated model outputs | Return primary output; shadow executes purely in background |
| Big-bang 100% replacement of ML models | Edge-case degradation impacts all users simultaneously | Use Canary rollouts (5% $\to$ 20% $\to$ 100%) |
| Omitting automated canary rollback triggers | Outages require human discovery and manual intervention | Automatically roll back if error rate or p99 spikes |
| Duplicating heavy write operations in shadow models | Shadow predictions create duplicate orders or audit records | Mock or isolate state mutations in shadow executions |
| Failing to version feature preprocessing logic alongside weights | Feature pipeline drift causes silent prediction corruption | Package feature transformers with model artifacts |

---

## 7. Performance & Scalability Considerations

- **Asynchronous Shadow Execution**: Always offload shadow model execution to background queues (ARQ, Celery) or non-blocking tasks (`BackgroundTasks`). Primary user responses must never wait for shadow model computation to finish.
- **Statistical Significance in A/B Tests**: Ensure A/B test sample sizes are calculated in advance (power analysis) to guarantee statistical validity before declaring a candidate model superior.

---

## 8. Security & OWASP Guardrails

- **Data Privacy in Shadow Logs**: Do not log raw prompt strings containing customer PII in shadow evaluation telemetry. Store anonymized feature vectors or masked hashes.
- **Canary Blast Radius Limitation**: Limit initial canary traffic to internal employee accounts or non-critical enterprise tenants before routing general public traffic.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from main import app

@pytest.mark.asyncio
async def test_ab_testing_deterministic_assignment():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Request with User 123
        resp1 = await client.post("/predict/churn", json={"features": [1.0, 2.0]}, headers={"X-User-Id": "user_123"})
        variant1 = resp1.json()["experiment_variant"]

        # Repeated request with same User 123 must receive IDENTICAL variant
        resp2 = await client.post("/predict/churn", json={"features": [1.0, 2.0]}, headers={"X-User-Id": "user_123"})
        variant2 = resp2.json()["experiment_variant"]

        assert variant1 == variant2, "A/B assignment must be strictly deterministic per user!"
```

---

## 10. Real-World Production Incident

**Incident**: A credit card fraud detection model update caused a 25% drop in transaction approval rates, blocking $12M in legitimate holiday transactions.

**Root Cause**: The engineering team released Model v3 directly to 100% of production traffic without a shadow phase or canary rollout. In offline testing, Model v3 achieved 99% accuracy on historical data. However, in production, holiday shopping patterns differed significantly from training data. The model misclassified standard holiday gift purchases as fraudulent.

**Resolution**: Reverted to Model v2 immediately via automated canary rollback. The team instituted a mandatory 1-week Shadow Deployment policy: all future models must shadow production traffic for 7 days to compare approval distributions against live baseline traffic before any user-facing rollout.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the fundamental difference between a Shadow Deployment and an A/B Test?</summary>

- **Shadow Deployment (Dark Launch)**: 100% of live traffic is served by the existing production model. An asynchronous duplicate of the request is sent to the candidate model in the background. The candidate's output is logged for analysis and never returned to the user. There is zero risk to user experience.
- **A/B Test**: Live users are split into two or more cohorts (Control vs Treatment). Each cohort receives real, user-facing responses from a different model variant. Business and accuracy metrics are compared across cohorts to measure real-world performance.
</details>

<details>
<summary>2. Why should user assignment in A/B testing use MurmurHash or SHA-256 rather than Python's built-in <code>hash()</code>?</summary>

Python's built-in `hash()` function incorporates randomized hash seeding (`PYTHONHASHSEED`) on process startup for security. Consequently, `hash("user_123")` will produce completely different integers across different Uvicorn worker processes or container pods. Cryptographic or non-cryptographic stable algorithms (like MurmurHash3 or MD5) produce identical, deterministic hash outputs across all processes, languages, and servers.
</details>

<details>
<summary>3. What is "Prediction Distribution Drift" and how can it be detected in production?</summary>

Prediction distribution drift occurs when the statistical distribution of model outputs changes over time, often signaling that input feature distributions have shifted (covariate shift) or relationships have evolved (concept drift). It is detected by logging prediction distributions (e.g. classification class proportions or regression probability histograms) and running statistical distance tests (Kolmogorov-Smirnov test, Population Stability Index) against a baseline.
</details>

<details>
<summary>4. What are the operational hazards of shadow deployments on downstream systems?</summary>

If a shadow deployment duplicates requests to a model that performs side effects (e.g. writing records to a database, incrementing counters in Redis, charging payment gateways, or enqueuing notifications), shadow traffic will cause duplicate transactions. Shadow execution paths must be strictly read-only and decoupled from production mutating side effects.
</details>

<details>
<summary>5. How does a Canary Deployment minimize the blast radius of a bad ML model?</summary>

A canary deployment introduces the candidate model to a tiny initial fraction of live users (e.g. 2% to 5%). If the new model contains bugs, causes latency spikes, or generates invalid outputs, only that tiny fraction of users is affected. Automated monitoring halts the rollout and rolls back traffic before the wider user base experiences any impact.
</details>

<details>
<summary>6. How does the MLflow Model Registry integrate with FastAPI deployments?</summary>

The MLflow Model Registry provides a centralized repository for tracking model versions, metadata, and lifecycle stages ("Staging", "Production", "Archived"). A FastAPI application can query the registry at startup or on rotation events, downloading the verified artifact designated for the "Production" alias and loading it into memory dynamically without rebuilding container images.
</details>

<details>
<summary>7. What is "Statistical Power" in an A/B test and why does it dictate test duration?</summary>

Statistical power (typically 80%) is the probability that an A/B test will detect a real difference between variants when one exists (avoiding false negatives / Type II errors). Power depends on sample size, baseline conversion rate, and Minimum Detectable Effect (MDE). Running an A/B test with insufficient traffic sample size leads to statistically invalid conclusions.
</details>

<details>
<summary>8. How do you prevent shadow model execution from degrading the latency of the primary model?</summary>

Execute the shadow prediction completely out-of-band:
1. In FastAPI, pass the shadow prediction to `BackgroundTasks.add_task()`, which runs after the primary response has been flushed to the client socket.
2. Or dispatch the shadow payload to an external asynchronous message broker (Redis Streams, AWS SQS) consumed by a dedicated shadow worker pool.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Deterministic Hash Variant Assigner
**Task**: Write a function that takes a `user_id` and returns `"A"` for 50% of users and `"B"` for 50% of users using `hashlib.md5`.
<details>
<summary>Solution</summary>

```python
import hashlib

def assign_variant(user_id: str) -> str:
    digest = hashlib.md5(user_id.encode("utf-8")).hexdigest()
    # Take first 4 hex chars -> int (0 to 65535)
    val = int(digest[:4], 16)
    return "A" if (val % 100) < 50 else "B"

assert assign_variant("usr_001") in ["A", "B"]
# Strictly deterministic
assert assign_variant("usr_001") == assign_variant("usr_001")
```
</details>

### Exercise 2 (Easy): Non-Blocking Background Shadow Logger
**Task**: Write a FastAPI route that calculates a primary prediction, schedules a shadow calculation using `BackgroundTasks`, and returns the primary result.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, BackgroundTasks

app = FastAPI()

def log_shadow(val: float):
    shadow_val = val * 1.05
    print(f"Shadow computed: {shadow_val}")

@app.get("/calc/{val}")
async def calc(val: float, bg: BackgroundTasks):
    primary = val * 1.0
    bg.add_task(log_shadow, val)
    return {"result": primary}
```
</details>

### Exercise 3 (Medium): 3-Way A/B/C Traffic Splitter
**Task**: Build a function that deterministically splits traffic across three variants: Control (60%), Treatment A (20%), Treatment B (20%).
<details>
<summary>Solution</summary>

```python
import hashlib

def split_three_way(user_id: str, salt: str = "exp_v1") -> str:
    key = f"{salt}:{user_id}".encode("utf-8")
    val = int(hashlib.sha256(key).hexdigest()[:4], 16) % 100

    if val < 60:
        return "control"       # 60%
    elif val < 80:
        return "treatment_a"   # 20%
    else:
        return "treatment_b"   # 20%
```
</details>

### Exercise 4 (Medium): Latency and Divergence Comparison Tracker
**Task**: Write a function that records primary and shadow prediction outputs and execution durations, printing a warning if divergence exceeds 10%.
<details>
<summary>Solution</summary>

```python
def compare_shadow_results(primary: float, shadow: float, p_time: float, s_time: float):
    divergence = abs(primary - shadow)
    pct_diff = (divergence / max(1e-6, abs(primary))) * 100
    
    print(f"Primary: {primary:.2f} ({p_time*1000:.1f}ms) | Shadow: {shadow:.2f} ({s_time*1000:.1f}ms)")
    if pct_diff > 10.0:
        print(f"WARNING: High prediction divergence ({pct_diff:.1f}%) detected!")
```
</details>

### Exercise 5 (Hard): Complete A/B Tested Recommendation Endpoint
**Task**: Build an endpoint `/recommend` that assigns users to Algorithm A or Algorithm B based on an `X-User-Id` header, executing the appropriate model and returning results with model version metadata.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, Header, HTTPException
import hashlib

app = FastAPI()

def model_a_recommend(user_id: str) -> list[str]:
    return ["item_popular_1", "item_popular_2"]

def model_b_recommend(user_id: str) -> list[str]:
    return ["item_personalized_A", "item_personalized_B"]

@app.get("/recommend")
async def get_recommendations(x_user_id: str = Header(...)):
    digest = hashlib.md5(f"rec_test:{x_user_id}".encode()).hexdigest()
    bucket = int(digest[:4], 16) % 100

    if bucket < 50:
        variant = "control"
        version = "heuristic_v1"
        items = model_a_recommend(x_user_id)
    else:
        variant = "treatment"
        version = "neural_collaborative_v2"
        items = model_b_recommend(x_user_id)

    return {
        "user_id": x_user_id,
        "variant": variant,
        "model_version": version,
        "recommendations": items
    }
```
</details>

### Exercise 6 (Hard): Automated Canary Rollback Circuit Breaker Class
**Task**: Build an automated Canary Controller class that tracks requests and errors; if error rate exceeds 5% over 50 requests, automatically switches all traffic to baseline.
<details>
<summary>Solution</summary>

```python
class CanaryController:
    def __init__(self, failure_threshold: float = 0.05, min_samples: int = 50):
        self.failure_threshold = failure_threshold
        self.min_samples = min_samples
        self.total_requests = 0
        self.error_count = 0
        self.is_rolled_back = False

    def record_outcome(self, is_error: bool):
        if self.is_rolled_back:
            return

        self.total_requests += 1
        if is_error:
            self.error_count += 1

        if self.total_requests >= self.min_samples:
            error_rate = self.error_count / self.total_requests
            if error_rate > self.failure_threshold:
                self.is_rolled_back = True
                print(f"CIRCUIT TRIPPED: Error rate {error_rate:.2%} exceeded threshold. Canary aborted!")

    def should_route_to_canary(self, user_hash_val: int) -> bool:
        if self.is_rolled_back:
            return False
        return (user_hash_val % 100) < 10 # 10% canary
```
</details>
