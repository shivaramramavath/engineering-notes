---
title: "Loading Machine Learning Models at Startup with Lifespan in FastAPI"
description: "Managing heavy ML model lifecycle, memory allocation, warmup routines, multi-model registries, dependency injection, and cleanup using FastAPI lifespan."
fastapi_version: "0.115+"
python_version: "3.12+"
free_threading_compatible: true
tested_with: "Python 3.12.8, FastAPI 0.115.6, torch 2.5.1, onnxruntime 1.20.1"
---

# Loading Machine Learning Models at Startup with Lifespan in FastAPI

## 1. Conceptual Foundation & Mental Model

Machine learning models are memory-intensive, computationally heavy assets. Large language models (LLMs), vision transformers, or gradient-boosted trees require hundreds of megabytes or gigabytes of RAM/VRAM, take seconds to load from disk, compile CUDA kernels on first invocation, and require explicit teardown to release memory.

In modern FastAPI, the **Lifespan Context Manager** is the authoritative mechanism for orchestrating this lifecycle:
1. **Deterministic Startup Initialization**: Model weights are loaded into memory *before* the ASGI server opens the network socket to accept incoming HTTP traffic. The application never serves requests in an uninitialized state.
2. **First-Inference Warmup**: Pre-executing synthetic dummy tensor inferences during startup triggers JIT compilation, optimizes computational graphs, and warms CUDA kernel caches before real users arrive.
3. **Structured Dependency Injection**: Models are stored in an encapsulated registry or `app.state` and injected cleanly into route handlers using `Depends()`, avoiding fragile global variables.
4. **Clean VRAM Teardown**: Upon receiving a termination signal (`SIGTERM`), the lifespan context executes code following `yield`, explicitly clearing PyTorch CUDA caches and releasing hardware accelerators.

```
[ Application Bootstrapping ]
             │
             ▼
[ Lifespan Startup Phase ]
   ├── 1. Download/Verify Model Checksums from Cloud Store
   ├── 2. Load Weights into Host RAM / GPU VRAM
   ├── 3. Execute Warmup Inference (Pre-compile CUDA Kernels)
   └── 4. Register Model in App State / Dependency Registry
             │
             ▼
[ Server Binds Network Socket -> Accepts Traffic ]
             │
[ Route Handlers Access Model via Depends(get_model) ]
             │
[ Server Receives SIGTERM ]
             │
             ▼
[ Lifespan Shutdown Phase ]
   ├── 1. Wait for In-Flight Inferences to Complete
   ├── 2. Delete Tensor References
   └── 3. Call torch.cuda.empty_cache() / Release Memory
```

---

## 2. Architecture & Internal Mechanics

```
+---------------------------------------------------------------+
|                      FastAPI Lifespan Scope                   |
|                                                               |
|  @asynccontextmanager                                         |
|  async def lifespan(app: FastAPI):                            |
|      # 1. Startup phase                                       |
|      model_registry = ModelRegistry()                         |
|      await model_registry.load_all()                          |
|      app.state.models = model_registry                        |
|      yield                                                    |
|      # 2. Shutdown phase                                      |
|      await model_registry.unload_all()                        |
+-------------------------------+-------------------------------+
                                │
        Dependency Injection    │ app.state.models
                                ▼
+---------------------------------------------------------------+
|                       Route Handler                           |
|  async def predict(                                           |
|      features: InputDTO,                                      |
|      models: ModelRegistry = Depends(get_model_registry)      |
|  ):                                                           |
|      return await models.predict("fraud_detector", features)  |
+---------------------------------------------------------------+
```

### Avoiding the Multi-Worker Memory Multiplication Trap
When running multi-worker processes (e.g. 4 Uvicorn workers), each worker process executes its own isolated `lifespan` handler. If a model consumes 4GB of RAM, 4 workers will consume $4 \times 4\text{GB} = 16\text{GB}$ of host memory! 

**Architectural Guidance**: For models exceeding 2GB:
- Run a single worker per container inside Kubernetes and scale pods horizontally.
- Or decouple inference into a dedicated model-serving engine (Triton Inference Server, vLLM, Ray Serve) accessed via gRPC or HTTP.

---

## 3. Comprehensive Syntax & Core Mechanics

### Encapsulated Model Registry Pattern
```python
from contextlib import asynccontextmanager
from typing import Any, Dict
from fastapi import FastAPI, Request, Depends
import joblib

class ModelRegistry:
    def __init__(self):
        self._models: Dict[str, Any] = {}

    def load(self, name: str, file_path: str):
        # Synchronous weight deserialization
        self._models[name] = joblib.load(file_path)

    def get(self, name: str) -> Any:
        if name not in self._models:
            raise KeyError(f"Model '{name}' is not loaded in registry")
        return self._models[name]

    def clear(self):
        self._models.clear()

def get_model_registry(request: Request) -> ModelRegistry:
    return request.app.state.model_registry
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: PyTorch Model Loading with CUDA Warmup and Graceful Cleanup
```python
from contextlib import asynccontextmanager
from fastapi import FastAPI, Depends, Request
import torch
import torchvision.models as models

class VisionModelHolder:
    def __init__(self):
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.model = None

    def load_and_warmup(self):
        print(f"Loading ResNet50 onto {self.device}...")
        self.model = models.resnet50(weights=models.ResNet50_Weights.DEFAULT)
        self.model.to(self.device)
        self.model.eval()

        # Warmup: Run dummy inference to compile CUDA graphs and allocate memory
        with torch.inference_mode():
            dummy = torch.randn(1, 3, 224, 224, device=self.device)
            for _ in range(3):
                _ = self.model(dummy)
        print("ResNet50 loaded and CUDA kernels warmed up.")

    def unload(self):
        print("Releasing model memory...")
        self.model = None
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
            torch.cuda.synchronize()

@asynccontextmanager
async def lifespan(app: FastAPI):
    holder = VisionModelHolder()
    holder.load_and_warmup()
    app.state.vision_model = holder
    yield
    holder.unload()

app = FastAPI(lifespan=lifespan)
```

### Scenario 2: Multi-Model Lazy Loading with Pre-Warmed Critical Paths
```python
from contextlib import asynccontextmanager
from fastapi import FastAPI, Depends, Request, HTTPException
import joblib
import os

class TieredModelRegistry:
    def __init__(self):
        self.critical_models = {}
        self.on_demand_models = {}

    def init_critical(self):
        # Pre-load Tier-1 low-latency models at startup
        self.critical_models["fraud"] = joblib.load("models/fraud.pkl")

    def get_model(self, name: str):
        if name in self.critical_models:
            return self.critical_models[name]
        
        # Lazy load Tier-2 models on first request
        if name not in self.on_demand_models:
            path = f"models/{name}.pkl"
            if not os.path.exists(path):
                raise HTTPException(status_code=404, detail="Model not found")
            self.on_demand_models[name] = joblib.load(path)
            
        return self.on_demand_models[name]

@asynccontextmanager
async def lifespan(app: FastAPI):
    registry = TieredModelRegistry()
    registry.init_critical()
    app.state.registry = registry
    yield
    registry.critical_models.clear()
    registry.on_demand_models.clear()

app = FastAPI(lifespan=lifespan)
```

### Scenario 3: Asynchronous Remote Model Download from Cloud Storage (S3 / GCS)
```python
import asyncio
from contextlib import asynccontextmanager
from fastapi import FastAPI
import boto3
import os

MODEL_DIR = "/tmp/models"
MODEL_FILE = os.path.join(MODEL_DIR, "recommender_v3.bin")

async def download_model_artifact():
    os.makedirs(MODEL_DIR, exist_ok=True)
    if not os.path.exists(MODEL_FILE):
        print("Downloading model weights from S3...")
        s3 = boto3.client("s3")
        # Run synchronous boto3 download in a thread
        await asyncio.to_thread(
            s3.download_file,
            "enterprise-models-bucket",
            "recommender_v3.bin",
            MODEL_FILE
        )
        print("Model downloaded successfully.")

@asynccontextmanager
async def lifespan(app: FastAPI):
    await download_model_artifact()
    # Load model from local disk after download
    app.state.model = joblib.load(MODEL_FILE)
    yield
    app.state.model = None

app = FastAPI(lifespan=lifespan)
```

### Scenario 4: Dependency Injection of Specific Model Subtypes
```python
from fastapi import FastAPI, Depends, Request

app = FastAPI()

def get_fraud_model(request: Request):
    return request.app.state.model_registry.get("fraud")

def get_churn_model(request: Request):
    return request.app.state.model_registry.get("churn")

@app.post("/predict/fraud")
async def evaluate_fraud(payload: dict, model = Depends(get_fraud_model)):
    score = model.predict([payload["features"]])[0]
    return {"fraud_score": float(score)}
```

### Scenario 5: Dynamic Health Check Reflecting Model Readiness
```python
from fastapi import FastAPI, Response, status

app = FastAPI()

@app.get("/health/ready")
async def readiness_check(request: Request):
    # Only report 200 OK once model is loaded in state
    if not hasattr(request.app.state, "model") or request.app.state.model is None:
        return Response(
            content='{"status": "INITIALIZING_MODELS"}',
            media_type="application/json",
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE
        )
    return {"status": "READY"}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Kubernetes Startup Probe Timeout During Large Model Downloads**:
   If downloading a 10GB LLM or diffusion model during `lifespan` takes 90 seconds, but Kubernetes `startupProbe` has `failureThreshold: 10` and `periodSeconds: 5` (total 50s), the kubelet will kill the container midway through the download. Ensure startup probe failure thresholds account for worst-case model download durations.

2. **Leaking PyTorch CUDA Memory Across Process Lifecycles**:
   Simply setting `model = None` does not immediately release GPU VRAM back to the NVIDIA driver; PyTorch's caching allocator holds reserved memory blocks. Always invoke `torch.cuda.empty_cache()` and `torch.cuda.synchronize()` in lifespan teardown.

3. **Global State Thread Race Conditions in Python 3.13t (nogil)**:
   When running free-threaded Python without the GIL, mutating a shared global model registry dictionary without locks while incoming requests access it can corrupt internal data structures. Always protect dynamic model registries with a `threading.Lock`.

4. **Warmup Inferences Failing Silently**:
   If dummy warmup tensors use incorrect input dimensions or types, warmup will raise an exception during startup, terminating the container before it ever binds to the port.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Loading model weights inside endpoint handlers | Adds 500ms - 10s latency to every user request | Load models once inside `lifespan` startup |
| Storing models in mutable module globals | Hard to mock, untracked teardown, testing contamination | Store models in `app.state` and inject via `Depends` |
| Skipping model warmup | First real user incurs 3-10x higher latency (kernel compile) | Run dummy tensor inference during startup |
| Running 8 workers on a 16GB host with a 3GB model | Memory exceeds physical RAM; pod killed by OOM killer | Run 1 worker per pod and scale pods horizontally |
| Omitting `torch.cuda.empty_cache()` on shutdown | GPU VRAM remains allocated; prevents new pods on node | Explicitly call `empty_cache()` in lifespan cleanup |
| Downloading weights from remote S3 on every pod boot | Slow deployments and high bandwidth egress costs | Bake weights into container or mount persistent volumes |

---

## 7. Performance & Scalability Considerations

- **Pre-Allocating VRAM with Warmup**: Running warmup inference not only compiles CUDA kernels, but also pre-allocates PyTorch's internal memory pool, eliminating memory allocation spikes during initial live traffic.
- **Persistent Volume Mounts (Kubernetes PVC)**: For models larger than 5GB, avoid downloading weights over HTTP during container startup. Mount an AWS EBS volume, Google Cloud Persistent Disk, or NFS share pre-populated with model weights directly into `/opt/models`.

---

## 8. Security & OWASP Guardrails

- **Cryptographic Model Hash Verification**: When downloading models from remote object storage during startup, compute the SHA-256 hash of the downloaded artifact and compare it against a verified configuration manifest before loading.
- **Model Poisoning Mitigation**: Ensure model storage buckets have strict IAM write restrictions and S3 Object Lock enabled to prevent unauthorized weight tampering.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from main import app

class MockModel:
    def predict(self, features):
        return [0.42]

@pytest.mark.asyncio
async def test_lifespan_model_dependency_injection():
    # Override model in app.state for fast, isolated unit testing
    app.state.model = MockModel()

    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        response = await client.post("/predict/churn", json={"features": [1, 2, 3]})
        assert response.status_code == 200
        assert response.json()["prediction"] == 0.42
```

---

## 10. Real-World Production Incident

**Incident**: An e-commerce image search engine experienced a 10-minute crashloop outage following a routine deployment of a new ResNet vision model.

**Root Cause**: The engineering team moved from a lightweight Scikit-Learn classifier to a 600MB PyTorch deep vision model. The deployment configuration maintained `workers = 8` inside Gunicorn on a 4GB RAM Kubernetes node. During container startup, all 8 workers executed `lifespan` simultaneously, each allocating 600MB for model weights and another 400MB for PyTorch runtime buffers. Total memory demand hit 8GB, instantly triggering the Linux kernel OOM killer. The pod terminated in `CrashLoopBackOff`.

**Resolution**: Reconfigured the deployment to run standalone Uvicorn with `workers = 1` per pod and adjusted Kubernetes pod resource limits to 1.5GB RAM. The service scaled out horizontally to 8 independent pods via HPA, resolving the OOM crashes and stabilizing latency.

---

## 11. Technical Interview Questions

<details>
<summary>1. Why is the <code>lifespan</code> context manager the recommended location for loading ML models in FastAPI?</summary>

The `lifespan` context manager provides a deterministic lifecycle contract:
1. It guarantees that model loading and verification execute completely *before* the application begins accepting incoming network requests.
2. It encapsulates model state within `app.state`, allowing models to be cleanly injected via dependencies rather than relying on mutable global variables.
3. It guarantees that cleanup and VRAM deallocation code (after `yield`) execute reliably when the application receives a termination signal.
</details>

<details>
<summary>2. What is "model warmup" and why is it essential for latency-sensitive applications?</summary>

Many machine learning frameworks (like PyTorch with CUDA or ONNX Runtime) use lazy initialization: they defer CUDA context creation, memory buffer allocation, and dynamic kernel compilation until the first inference is executed. If warmup is omitted, the first real user request incurs a massive latency penalty (often 1 to 5 seconds). Executing synthetic dummy inferences during application startup warms internal caches, ensuring all user requests experience fast, consistent latency.
</details>

<details>
<summary>3. Why should you avoid running multiple Uvicorn workers per container when serving large ML models?</summary>

In Python, multi-worker setups (e.g. Gunicorn with multiple Uvicorn workers) spawn independent operating system processes. Each worker process has its own isolated memory heap and loads its own complete copy of the model weights. If a model consumes 3GB of RAM, running 4 workers consumes 12GB of RAM. In containerized environments, it is far more memory-efficient to run 1 worker per container and use the container orchestrator (Kubernetes HPA) to scale pods horizontally.
</details>

<details>
<summary>4. How does <code>torch.cuda.empty_cache()</code> work during lifespan teardown?</summary>

PyTorch manages GPU memory using a caching allocator. When tensors are freed in Python, PyTorch does not immediately return the physical VRAM back to the NVIDIA driver; it retains the memory in a pool to accelerate future allocations. Calling `torch.cuda.empty_cache()` releases all unallocated cached VRAM back to the operating system and GPU driver, allowing other processes or pods to claim the memory.
</details>

<details>
<summary>5. How should a readiness probe interact with model loading in FastAPI?</summary>

The Kubernetes readiness probe (`/health/ready`) should inspect whether the model has finished loading and warming up in `app.state`. If the model is not yet loaded, the endpoint must return HTTP 503 Service Unavailable. This instructs the load balancer not to route user traffic to the container until the model is completely ready to serve predictions.
</details>

<details>
<summary>6. What is the risk of downloading model weights dynamically from S3 on every pod startup?</summary>

Dynamic downloads introduce external dependencies into pod cold starts. If S3 experiences network throttling, rate limits, or transient outages, new pods will fail to initialize. Furthermore, downloading multiple gigabytes per pod increases deployment rollout times from seconds to minutes. Best practices involve baking model weights into the container image, mounting persistent network volumes (EBS/NFS), or using a local node cache.
</details>

<details>
<summary>7. How do you inject an ML model into a route handler without using global variables?</summary>

Store the loaded model instance on `app.state` during the lifespan startup phase:
```python
app.state.my_model = loaded_model
```
Define a dependency function that extracts the model from the request:
```python
def get_model(request: Request):
    return request.app.state.my_model
```
Inject it into route handlers via `model = Depends(get_model)`.
</details>

<details>
<summary>8. How do you verify the integrity of a downloaded model file before loading it?</summary>

Compute the cryptographic hash (e.g. SHA-256) of the downloaded model file on disk using Python's `hashlib` module. Compare the calculated hash against an expected hash stored in configuration or an external manifest. If the hashes do not match, abort startup immediately to prevent loading corrupted or tampered weights.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic App State Model Registration
**Task**: Write a FastAPI application using `lifespan` that stores a dummy multiplier model `lambda x: x * 2` in `app.state.model` and serves it via an endpoint.
<details>
<summary>Solution</summary>

```python
from contextlib import asynccontextmanager
from fastapi import FastAPI, Request

@asynccontextmanager
async def lifespan(app: FastAPI):
    app.state.model = lambda x: x * 2
    yield
    app.state.model = None

app = FastAPI(lifespan=lifespan)

@app.get("/double/{val}")
async def double_value(val: int, request: Request):
    result = request.app.state.model(val)
    return {"result": result}
```
</details>

### Exercise 2 (Easy): Dummy Warmup Function
**Task**: Write a function that accepts an ONNX inference session and runs a warmup inference with a batch size of 1.
<details>
<summary>Solution</summary>

```python
import numpy as np
import onnxruntime as ort

def warmup_session(session: ort.InferenceSession, input_shape: tuple):
    input_name = session.get_inputs()[0].name
    dummy_tensor = np.zeros(input_shape, dtype=np.float32)
    # Run once to compile kernels
    _ = session.run(None, {input_name: dummy_tensor})
    print("Warmup inference executed.")
```
</details>

### Exercise 3 (Medium): Model Readiness Dependency
**Task**: Implement a dependency `ensure_model_ready` that verifies the model exists in `app.state`, raising an HTTP 503 error if it is not ready.
<details>
<summary>Solution</summary>

```python
from fastapi import Request, HTTPException, status

def ensure_model_ready(request: Request):
    model = getattr(request.app.state, "model", None)
    if model is None:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Model is still loading or unavailable"
        )
    return model
```
</details>

### Exercise 4 (Medium): Thread-Safe Model Registry
**Task**: Implement a thread-safe `ThreadSafeModelRegistry` class protected by a `threading.Lock` for Python 3.13 free-threading compatibility.
<details>
<summary>Solution</summary>

```python
import threading
from typing import Any

class ThreadSafeModelRegistry:
    def __init__(self):
        self._models = {}
        self._lock = threading.Lock()

    def register(self, name: str, model: Any):
        with self._lock:
            self._models[name] = model

    def get(self, name: str) -> Any:
        with self._lock:
            if name not in self._models:
                raise KeyError(f"Model '{name}' not found")
            return self._models[name]

    def clear(self):
        with self._lock:
            self._models.clear()
```
</details>

### Exercise 5 (Hard): Complete PyTorch Model Lifespan Manager
**Task**: Build a complete production lifespan manager that loads a TorchScript model, executes a 5-step warmup on GPU/CPU, handles SIGTERM teardown, and exposes a dependency provider.
<details>
<summary>Solution</summary>

```python
import torch
from contextlib import asynccontextmanager
from fastapi import FastAPI, Depends, Request

class TorchModelHolder:
    def __init__(self, model_path: str):
        self.model_path = model_path
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        self.model = None

    def start(self):
        print(f"Loading TorchScript model on {self.device}...")
        self.model = torch.jit.load(self.model_path, map_location=self.device)
        self.model.eval()

        # 5-step warmup
        with torch.inference_mode():
            dummy = torch.randn(1, 10, device=self.device)
            for _ in range(5):
                _ = self.model(dummy)
        print("Warmup complete.")

    def stop(self):
        print("Stopping model and clearing VRAM...")
        self.model = None
        if torch.cuda.is_available():
            torch.cuda.empty_cache()

@asynccontextmanager
async def lifespan(app: FastAPI):
    holder = TorchModelHolder("models/classifier.pt")
    holder.start()
    app.state.model_holder = holder
    yield
    holder.stop()

app = FastAPI(lifespan=lifespan)

def get_torch_model(request: Request):
    return request.app.state.model_holder.model

@app.post("/predict")
async def predict_endpoint(features: list[float], model = Depends(get_torch_model)):
    with torch.inference_mode():
        inp = torch.tensor([features], device=model.device)
        output = model(inp)
    return {"prediction": output.tolist()}
```
</details>

### Exercise 6 (Hard): Checksum-Verifying Model Downloader
**Task**: Write an async startup function that downloads a model from a URL, calculates its SHA-256 hash, compares it to an expected hash, and raises an exception if tampered.
<details>
<summary>Solution</summary>

```python
import hashlib
import httpx
import os

async def verify_and_download_model(url: str, target_path: str, expected_sha256: str):
    if os.path.exists(target_path):
        # Verify existing file hash
        hasher = hashlib.sha256()
        with open(target_path, "rb") as f:
            while chunk := f.read(65536):
                hasher.update(chunk)
        if hasher.hexdigest() == expected_sha256:
            print("Existing model verified.")
            return

    print("Downloading model...")
    hasher = hashlib.sha256()
    async with httpx.AsyncClient() as client:
        async with client.stream("GET", url) as resp:
            resp.raise_for_status()
            with open(target_path, "wb") as f:
                async for chunk in resp.aiter_bytes():
                    f.write(chunk)
                    hasher.update(chunk)

    calculated_hash = hasher.hexdigest()
    if calculated_hash != expected_sha256:
        os.remove(target_path)
        raise ValueError(
            f"SECURITY ERROR: Hash mismatch! Expected {expected_sha256}, got {calculated_hash}"
        )
    print("Model downloaded and cryptographically verified.")
```
</details>
