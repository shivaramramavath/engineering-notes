---
title: "Feature Flags and Dynamic Configuration in FastAPI"
description: "Implement production feature flags and toggles in FastAPI: release toggles, canary rollouts, percentage-based targeting, user segmentation, and Redis-backed dynamic configuration."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "pydantic 2.10+, fastapi 0.115+, redis 5.2+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Feature Flags and Dynamic Configuration in FastAPI

## 1. Conceptual Foundation & Mental Model

### Decoupling Deployment from Release
In traditional software delivery, deploying code to production servers is synonymous with releasing the feature to users. If a newly deployed feature contains a catastrophic bug or degrades database performance, remediation requires rolling back the deployment, reverting Git commits, or applying an emergency hotfix under extreme pressure.

**Feature Flags** (also known as Feature Toggles) decouple **code deployment** from **feature release**:
- **Deployment**: The binary or container is safely deployed to production with the new code path dormant behind a feature toggle.
- **Release**: The feature is activated dynamically at runtime for 1% of users, then 10%, then 100%, without redeploying or restarting server instances.
- **Instant Kill Switch**: If p99 latency spikes or errors exceed thresholds, an automated monitor or on-call engineer toggles the flag to `False` in Redis or an administrative dashboard, instantly disabling the code path in milliseconds.

```
+-------------------------------------------------------------------------------+
|                       FEATURE TOGGLE TAXONOMY (Martin Fowler)                 |
|                                                                               |
|  1. Release Toggles    -> Enables trunk-based development; conceals WIP code  |
|  2. Experiment Toggles -> A/B testing user conversions, algorithmic variants  |
|  3. Ops Toggles        -> Graceful degradation (e.g., disable AI search under)|
|  4. Permission Toggles -> Gating beta features to VIP or internal staff       |
+-------------------------------------------------------------------------------+
```

---

## 2. Architecture & Internal Mechanics

Understanding the evaluation lifecycle of a feature flag inside FastAPI's request pipeline.

```
[HTTP Request with Auth Token]
              |
              v
[FastAPI Dependency: get_current_user]
              |
              v (User Context: id="usr_42", email="alice@corp.com", tenant="acme")
[Feature Flag Dependency: is_feature_enabled("v2_checkout")]
              |
              +---> 1. Check Local In-Memory Cache (0.01ms)
              |
              +---> 2. Fallback to Redis / Central Store (1-2ms)
              |
              v
[Evaluation Engine: Consistent Hashing / Rule Evaluation]
  - Is tenant "acme" in whitelist? -> YES
  - Hash(usr_42) % 100 < rollout_percentage? -> Evaluated
              |
              +-----------------------+
              | (Enabled)             | (Disabled)
              v                       v
     [Execute V2 Pipeline]   [Execute Legacy Pipeline]
```

### Deterministic Percentage Rollouts via Consistent Hashing
When rolling out a feature to 20% of users, evaluating `random.random() < 0.20` is a catastrophic anti-pattern: the user would experience Feature V2 on one request and Feature V1 on the next. 

Instead, use **Deterministic Hashing**:
$$\text{Bucket} = \text{CRC32}(\text{user\_id} + \text{flag\_key}) \pmod{100}$$
If $\text{Bucket} < 20$, the user consistently receives the new feature across all sessions and devices.

---

## 3. Comprehensive Syntax & Core Mechanics

### Feature Flag Provider Interface

```python
from typing import Protocol, Optional
from dataclasses import dataclass

@dataclass(frozen=True)
class UserContext:
    user_id: str
    email: str
    tenant_id: str
    is_beta_tester: bool = False

class FeatureFlagService(Protocol):
    async def is_enabled(self, flag_key: str, context: Optional[UserContext] = None) -> bool: ...
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Deterministic Rollout Calculation Engine
A pure Python module calculating user percentage buckets using CRC32 hashing.

```python
# app/core/feature_flags/engine.py
import zlib
from typing import Optional
from dataclasses import dataclass

@dataclass(frozen=True)
class UserContext:
    user_id: str
    email: str
    tenant_id: str
    is_beta_tester: bool = False

def calculate_user_bucket(user_id: str, flag_name: str) -> int:
    """Computes a deterministic integer bucket from 0 to 99."""
    seed = f"{user_id}:{flag_name}".encode("utf-8")
    return zlib.crc32(seed) % 100

def evaluate_flag_rules(
    flag_data: dict,
    context: Optional[UserContext] = None
) -> bool:
    """Evaluates flag rules: Master Switch -> Whitelist -> Percentage."""
    # 1. Master Kill Switch
    if not flag_data.get("is_active", False):
        return False

    if not context:
        return flag_data.get("default_for_anonymous", False)

    # 2. Beta Tester / Internal Whitelist
    if context.is_beta_tester and flag_data.get("enable_for_beta", False):
        return True

    whitelisted_users = flag_data.get("whitelisted_user_ids", [])
    if context.user_id in whitelisted_users:
        return True

    # 3. Deterministic Percentage Rollout
    rollout_pct = flag_data.get("rollout_percentage", 0)
    if rollout_pct >= 100:
        return True
    if rollout_pct <= 0:
        return False

    user_bucket = calculate_user_bucket(context.user_id, flag_data["name"])
    return user_bucket < rollout_pct
```

---

## 2. Example 2: In-Memory / Redis-Backed Feature Flag Service
Supporting live runtime updates without restarting FastAPI worker processes.

```python
# app/core/feature_flags/service.py
from typing import Dict, Optional
from app.core.feature_flags.engine import UserContext, evaluate_flag_rules

class InMemoryFeatureFlagService:
    def __init__(self):
        # Simulated centralized flag store (e.g., Redis or Unleash)
        self._flags: Dict[str, dict] = {
            "new_recommendation_engine": {
                "name": "new_recommendation_engine",
                "is_active": True,
                "rollout_percentage": 25,  # 25% Canary rollout
                "enable_for_beta": True,
                "whitelisted_user_ids": ["admin_1", "vip_user_99"],
            },
            "experimental_checkout": {
                "name": "experimental_checkout",
                "is_active": False,  # Master kill switch engaged
                "rollout_percentage": 100,
            }
        }

    async def is_enabled(self, flag_key: str, context: Optional[UserContext] = None) -> bool:
        flag_data = self._flags.get(flag_key)
        if not flag_data:
            return False
        return evaluate_flag_rules(flag_data, context)

    def update_flag(self, flag_key: str, is_active: bool, rollout_pct: int) -> None:
        """Dynamic runtime update hook."""
        if flag_key in self._flags:
            self._flags[flag_key]["is_active"] = is_active
            self._flags[flag_key]["rollout_percentage"] = rollout_pct

feature_flag_service = InMemoryFeatureFlagService()
```

---

### Example 3: FastAPI Dependency Injection for Flag Evaluation
Creating clean dependencies that inject flag evaluation directly into router parameters.

```python
# app/api/dependencies.py
from typing import Callable
from fastapi import Depends, Header
from app.core.feature_flags.engine import UserContext
from app.core.feature_flags.service import feature_flag_service

# Simulated authentication context provider
async def get_current_user_context(
    x_user_id: str = Header(default="anonymous"),
    x_tenant_id: str = Header(default="default_tenant"),
    x_beta_tester: bool = Header(default=False),
) -> UserContext:
    return UserContext(
        user_id=x_user_id,
        email=f"{x_user_id}@example.com",
        tenant_id=x_tenant_id,
        is_beta_tester=x_beta_tester,
    )

def require_flag(flag_name: str) -> Callable:
    """Dependency factory checking if a feature is enabled for the current user."""
    async def flag_checker(
        context: UserContext = Depends(get_current_user_context)
    ) -> bool:
        return await feature_flag_service.is_enabled(flag_name, context)
    return flag_checker
```

---

### Example 4: Dynamic Branching and Gated Endpoints
Demonstrating both route gating (HTTP 404/403 if disabled) and internal algorithmic branching based on flags.

```python
# app/api/v1/recommendations.py
from fastapi import APIRouter, Depends, HTTPException, status
from app.api.dependencies import require_flag, get_current_user_context
from app.core.feature_flags.engine import UserContext
from app.core.feature_flags.service import feature_flag_service

router = APIRouter(prefix="/recommendations", tags=["Recommendations"])

# Pattern A: Algorithmic Branching inside route
@router.get("/")
async def get_recommendations(
    context: UserContext = Depends(get_current_user_context),
):
    use_new_engine = await feature_flag_service.is_enabled("new_recommendation_engine", context)
    
    if use_new_engine:
        # High-performance neural collaborative filtering (V2)
        return {
            "engine": "v2_neural",
            "items": ["Item-AI-101", "Item-AI-202"],
            "user_id": context.user_id,
        }
    
    # Legacy rule-based fallback (V1)
    return {
        "engine": "v1_heuristic",
        "items": ["Item-Classic-1", "Item-Classic-2"],
        "user_id": context.user_id,
    }

# Pattern B: Entire Route Gated behind Feature Flag
@router.post("/experimental-checkout")
async def experimental_checkout(
    is_active: bool = Depends(require_flag("experimental_checkout")),
):
    if not is_active:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="The requested feature is currently disabled or unavailable in this region."
        )
    return {"status": "CHECKOUT_PROCESSED_VIA_V2"}
```

---

### Example 5: Testing Flag Variations with Dependency Overrides
Testing both branches of a feature flag deterministically using FastAPI's `dependency_overrides`.

```python
# tests/test_feature_flags.py
from fastapi.testclient import TestClient
from app.main import app
from app.core.feature_flags.engine import calculate_user_bucket
from app.core.feature_flags.service import feature_flag_service

client = TestClient(app)

def test_deterministic_bucket_is_consistent():
    # CRC32 hash must produce the exact same bucket every time
    bucket_1 = calculate_user_bucket("user_123", "new_engine")
    bucket_2 = calculate_user_bucket("user_123", "new_engine")
    assert bucket_1 == bucket_2
    assert 0 <= bucket_1 < 100

def test_recommendation_branches_for_beta_tester():
    # Beta tester header triggers V2 engine
    headers = {"x-user-id": "beta_bob", "x-beta-tester": "true"}
    resp = client.get("/api/v1/recommendations/", headers=headers)
    assert resp.status_code == 200
    assert resp.json()["engine"] == "v2_neural"

def test_recommendation_branches_for_regular_user_outside_canary():
    # User outside 25% rollout receives V1
    # We find a user whose bucket >= 25
    user_id = "test_user_outside"
    while calculate_user_bucket(user_id, "new_recommendation_engine") < 25:
        user_id += "1"

    headers = {"x-user-id": user_id, "x-beta-tester": "false"}
    resp = client.get("/api/v1/recommendations/", headers=headers)
    assert resp.status_code == 200
    assert resp.json()["engine"] == "v1_heuristic"

def test_runtime_kill_switch_instantly_disables_feature():
    # Engage kill switch
    feature_flag_service.update_flag("new_recommendation_engine", is_active=False, rollout_pct=0)

    # Even beta testers now receive the fallback V1
    headers = {"x-user-id": "beta_bob", "x-beta-tester": "true"}
    resp = client.get("/api/v1/recommendations/", headers=headers)
    assert resp.status_code == 200
    assert resp.json()["engine"] == "v1_heuristic"
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Flag Debt & Code Rot**: When a feature reaches 100% rollout and stabilizes, the flag check, legacy code path, and configuration keys must be deleted. Leaving obsolete flags in code creates dead execution paths and technical debt that complicates future refactoring.
2. **Central Store Failure (Redis Outage)**: If the central Redis or Unleash instance fails and the application does not have defensive fallbacks, every HTTP request will raise an unhandled connection error. Always implement a local in-memory fallback with safe default values.
3. **Inconsistent Anonymous State**: Evaluating percentage flags for unauthenticated users based on IP addresses can cause multiple users behind the same corporate NAT proxy to be lumped into the exact same bucket. Use a client-side persistent device UUID cookie for anonymous targeting.
4. **Cascading Database Overload on Full Rollout**: When flipping a flag from 10% to 100%, if the new feature executes an unindexed database query, database CPU can jump to 100% instantly. Ramp up rollouts gradually (10% $\to$ 25% $\to$ 50% $\to$ 100%) while observing database telemetry.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Using `random.random()` for Canary Rollouts** | Lack of consistent hashing | User experiences differing feature versions across back-to-back clicks. | Use deterministic hashing on User ID (`crc32(user_id + flag_name) % 100`). |
| **Hardcoding Flag Defaults to `True`** | Over-optimism | If the configuration store is unreachable, untested features launch automatically. | Safe fallback: default flags to `False` (safe failure mode). |
| **Querying Redis on Every Single Request** | Omitting local caching | Adds 2–5ms network latency to every HTTP endpoint. | Cache flag definitions in process memory with a 5–10 second TTL. |
| **Leaving Flags in Code for Years** | Lack of cleanup hygiene | Massive technical debt, branching complexity, and dead code proliferation. | Add a ticket with an expiration date to delete the flag upon 100% rollout. |
| **Evaluating Heavy Flags in Deep Domain Entities** | Leaking infrastructure into domain | Domain models become untestable without flag configurations. | Evaluate flags in the Application/Service layer; pass resolved boolean settings down. |
| **Toggling Flags to 100% Instantly Under Load** | Skipping canary stages | Overwhelms backend caches and downstream microservices. | Ramp rollouts incrementally: 5%, 25%, 50%, 100% over hours or days. |

---

## 7. Performance & Scalability Considerations

- **In-Memory Cache with Async Refresh**: For high-throughput systems processing 20,000+ RPS, querying Redis on every request introduces unnecessary network overhead. Store flag rules in process memory and update them asynchronously via Redis Pub/Sub invalidation or a background ticker.
- **Microsecond Hashing Speed**: Evaluating `zlib.crc32()` in Python takes approximately 150 nanoseconds—vastly faster than cryptographic hashes like SHA-256 (3,000 nanoseconds).
- **Free-Threading in Python 3.13t**: In-memory flag evaluations are read-only dictionary lookups and mathematical hashes, running concurrently across multiple threads without GIL serialization.

---

## 8. Security & OWASP Guardrails

- **OWASP A01 (Broken Access Control - Tamper Prevention)**: Never allow clients to enable features by simply passing a header or query parameter (`?enable_admin_v2=true`). Flag evaluation rules must verify cryptographic session tokens or server-side user identities.
- **Audit Logging for Flag Toggles**: Changing a feature flag in production is equivalent to deploying code. All flag state changes must be logged with the operator's identity, timestamp, previous value, new value, and reason.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_flag_recipes.py
import pytest
from app.core.feature_flags.engine import calculate_user_bucket

def test_bucket_distribution_uniformity():
    """Validates that CRC32 hashing distributes 10,000 users evenly across 100 buckets."""
    buckets = [0] * 100
    for i in range(10000):
        b = calculate_user_bucket(f"user_{i}", "test_flag")
        buckets[b] += 1

    # Expected ~100 users per bucket (assert within reasonable variance: 60-140)
    for count in buckets:
        assert 60 <= count <= 140
```

---

## 10. Real-World Case Study / Production Incident

A major online streaming service deployed a new machine-learning recommendation algorithm expected to increase user engagement. The code was deployed behind an environment variable `USE_NEW_ML_MODEL=true`.

**The Incident**:
1. To enable the feature, DevOps updated the Kubernetes deployment environment variable, triggering a rolling restart of all 200 API pods.
2. The new ML model immediately overwhelmed the internal vector database cluster with unindexed queries, causing p99 latencies to skyrocket from 40ms to 9,000ms.
3. Because the toggle required a redeployment, the DevOps team had to update the ConfigMap and trigger another rolling restart.
4. During the 14 minutes required for the pods to terminate and restart, hundreds of thousands of users experienced connection timeouts and 504 Gateway Timeouts.

**The Solution**:
The platform migrated to dynamic runtime feature flags:
- Built a Redis-backed flag evaluator polled by in-memory worker threads.
- Implemented an automated Canary monitor: if error rates exceeded 1%, the monitoring system dispatched a Redis Pub/Sub message turning the flag off in under 50 milliseconds across the entire global cluster without restarting a single pod.

---

## 11. Technical Interview Questions

<details>
<summary>1. What are the four primary categories of feature toggles defined by Martin Fowler?</summary>
1. **Release Toggles**: Allow unfinished code to be merged into trunk and deployed to production safely while remaining hidden from users.
2. **Experiment Toggles**: Used for A/B multivariate testing to determine which algorithmic or UI variant performs better.
3. **Ops Toggles**: Operational circuit breakers and kill switches used to gracefully degrade performance under heavy system load.
4. **Permission Toggles**: Control feature access based on user tier, VIP status, or internal employee access.
</details>

<details>
<summary>2. Why should percentage-based rollouts use deterministic hashing rather than `random.random()`?</summary>
Using `random.random()` results in non-deterministic evaluations: a user refreshing the page or navigating to another screen would alternate randomly between the new feature and the old feature. Deterministic hashing (`crc32(user_id + flag_name) % 100`) ensures that a user is consistently assigned to the exact same bucket across sessions, devices, and requests.
</details>

<details>
<summary>3. What is "Feature Flag Debt", and how can engineering teams manage it?</summary>
Feature flag debt occurs when feature toggles and their obsolete legacy code paths remain in the codebase long after a feature has stabilized at 100% rollout. This creates code bloat, testing complexity, and confusion. It is managed by attaching expiration dates/SLAs to flags, setting automated alerts for flags active for >60 days, and scheduling cleanup tasks in the sprint immediately following full rollout.
</details>

<details>
<summary>4. How can you implement a feature flag kill switch in FastAPI without restarting worker processes?</summary>
By storing feature flag state in an external dynamic store (e.g., Redis, PostgreSQL, or an external flag management service like Unleash/LaunchDarkly) and having FastAPI workers read the flag at runtime (or subscribe to Redis Pub/Sub invalidation events to update an in-memory cache). Toggling the flag in the database updates all worker nodes in milliseconds without restarts.
</details>

<details>
<summary>5. How should feature flags be tested in unit and integration test suites?</summary>
Both the enabled and disabled code paths must be explicitly tested. Using FastAPI's `app.dependency_overrides`, test suites can force the flag dependency to return `True` for one test and `False` for another, ensuring 100% branch coverage across both implementations.
</details>

<details>
<summary>6. Why is CRC32 preferred over MD5 or SHA-256 for feature flag bucket calculation?</summary>
Cryptographic hashes like MD5 and SHA-256 are computationally heavy, designed to resist collision attacks. For feature flag evaluation, cryptographic security is unnecessary; the only requirement is a uniform pseudo-random distribution. CRC32 computes in ~150 nanoseconds (20x faster than SHA-256), introducing virtually zero CPU overhead into the request pipeline.
</details>

<details>
<summary>7. What is the difference between client-side and server-side feature flags?</summary>
Client-side flags evaluate in the browser or mobile application (controlling UI visibility, button colors, layouts). Server-side flags evaluate on the backend API (controlling business algorithms, database query paths, third-party vendor selection, and API endpoint availability). Server-side flags are strictly required for security-sensitive logic.
</details>

<details>
<summary>8. How do you handle feature flag evaluation for unauthenticated (anonymous) visitors?</summary>
Assign the anonymous visitor a persistent unique identifier stored in a cryptographically signed HTTP-only cookie (e.g., `device_id` or `anonymous_session_id`). Use this cookie identifier as the hashing seed for deterministic percentage evaluation.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Basic Boolean Feature Flag Dependency</summary>

**Task**: Implement a FastAPI dependency `is_beta_enabled` that reads a query parameter `?beta=true` and returns a boolean.

**Solution**:
```python
from fastapi import FastAPI, Depends

app = FastAPI()

def is_beta_enabled(beta: bool = False) -> bool:
    return beta

@app.get("/features")
def list_features(beta_active: bool = Depends(is_beta_enabled)):
    if beta_active:
        return {"features": ["stable", "beta_v2"]}
    return {"features": ["stable"]}
```
</details>

<details>
<summary>Exercise 2 (Easy): CRC32 Percentage Bucket Calculator</summary>

**Task**: Write a function `is_in_rollout(user_id: str, flag: str, percentage: int) -> bool` using `zlib.crc32`.

**Solution**:
```python
import zlib

def is_in_rollout(user_id: str, flag: str, percentage: int) -> bool:
    if percentage >= 100:
        return True
    if percentage <= 0:
        return False
    bucket = zlib.crc32(f"{user_id}:{flag}".encode("utf-8")) % 100
    return bucket < percentage
```
</details>

<details>
<summary>Exercise 3 (Medium): Redis-Backed Feature Flag Client</summary>

**Task**: Write an async class `RedisFeatureFlagStore` with `async def is_enabled(self, flag: str) -> bool` that reads from a Redis key `flag:{name}` (storing `"1"` or `"0"`). If Redis fails, return `False` safely.

**Solution**:
```python
class RedisFeatureFlagStore:
    def __init__(self, redis_client):
        self._redis = redis_client

    async def is_enabled(self, flag_name: str) -> bool:
        try:
            val = await self._redis.get(f"flag:{flag_name}")
            return val == "1" or val == b"1"
        except Exception:
            return False  # Safe default on outage
```
</details>

<details>
<summary>Exercise 4 (Medium): Route Gating with 404 Guard</summary>

**Task**: Create a FastAPI dependency `assert_feature_active(flag_name: str)` that raises `HTTPException(404, "Feature not found")` if the flag is disabled.

**Solution**:
```python
from fastapi import HTTPException, status

def assert_feature_active(flag_name: str, is_active: bool):
    if not is_active:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Feature '{flag_name}' is currently disabled."
        )
```
</details>

<details>
<summary>Exercise 5 (Hard): In-Memory Flag Cache with TTL</summary>

**Task**: Implement an in-memory cached flag store that queries a slow database or Redis only once every 10 seconds, returning cached results in between.

**Solution**:
```python
import time

class CachedFlagStore:
    def __init__(self, fetch_remote_func, ttl_seconds: float = 10.0):
        self._fetch_remote = fetch_remote_func
        self._ttl = ttl_seconds
        self._cache = {}
        self._last_fetched = 0.0

    async def get_flags(self) -> dict:
        now = time.time()
        if now - self._last_fetched > self._ttl or not self._cache:
            self._cache = await self._fetch_remote()
            self._last_fetched = now
        return self._cache

    async def is_enabled(self, flag_name: str) -> bool:
        flags = await self.get_flags()
        return flags.get(flag_name, False)
```
</details>

<details>
<summary>Exercise 6 (Hard): A/B Testing Variant Assignment</summary>

**Task**: Implement a function `assign_ab_variant(user_id: str, experiment_name: str, variants: list[str]) -> str` that deterministically assigns users to one of N variants with equal probability.

**Solution**:
```python
import zlib

def assign_ab_variant(user_id: str, experiment_name: str, variants: list[str]) -> str:
    if not variants:
        raise ValueError("Must provide at least one variant")
    seed = f"{user_id}:{experiment_name}".encode("utf-8")
    index = (zlib.crc32(seed) % len(variants))
    return variants[index]

# Verification
def test_variants():
    v1 = assign_ab_variant("usr_1", "checkout_experiment", ["control", "variant_a", "variant_b"])
    v2 = assign_ab_variant("usr_1", "checkout_experiment", ["control", "variant_a", "variant_b"])
    assert v1 == v2  # Must be deterministic
    assert v1 in ["control", "variant_a", "variant_b"]
```
</details>
