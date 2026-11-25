# Module 12: Saving, Loading & Deployment

Welcome to **Module 12: Saving, Loading & Deployment** of the Machine Learning Curriculum track.

A trained model that remains trapped inside a Jupyter notebook has zero business value. Model deployment is the engineering bridge that transitions algorithmic mathematical weights into secure, high-throughput, and resilient production software services.

However, model persistence and serving present unique software engineering hazards: arbitrary remote code execution (RCE) vulnerabilities from unpickling untrusted files, training-serving skew from decoupled preprocessing scripts, silent numerical drift from unpinned library dependencies, and latency bottlenecks during real-time inference.

This module provides an exhaustive, production-grade guide to modern model persistence, containerization, and deployment architectures: from understanding the Pickle Virtual Machine and leveraging secure zero-code formats (`skops`, `safetensors`, `onnx`), to packaging atomic pipelines, designing FastAPI microservices, executing out-of-core batch scoring, hardening Docker containers, and managing enterprise model registries.

---

## Module Notes Overview

| Order | Note | Format | Level | Focus & Key Algorithms |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Model Persistence Overview](01-saving-models-overview.md) | Concept | Core | Serialization landscape; taxonomy of model formats (Object graph vs Safe allowlists vs Pure tensor vs Open DAG); artifact packaging envelope (binary + schema + env lock + metadata sidecar). |
| **02** | [Joblib, Pickle & Their Risks](02-joblib-and-pickle-and-their-risks.md) | Concept | Core | The Pickle Virtual Machine (PVM) architecture; `GLOBAL` and `REDUCE` opcodes; `__reduce__` exploit mechanics; Joblib array chunking and `mmap_mode='r'` memory-mapping; import path coupling fragility. |
| **03** | [Skops & Safe Persistence](03-skops-and-safe-persistence.md) | Concept | Core | What is `skops.io`?; decoupling structural JSON schema from binary `.npy` arrays; `get_untrusted_types()` static inspection; trusted type allowlists; zero-execution security guarantees. |
| **04** | [ONNX Export & Runtime](04-onnx-export-and-runtime.md) | Concept | Advanced | Open Neural Network Exchange specification; Protocol Buffers computational DAG; `skl2onnx` pipeline conversion; ONNX Runtime C++ engine; Execution Providers (CPU AVX-512, CUDA, TensorRT); operator fusion. |
| **05** | [PyTorch State Dict & Safetensors](05-pytorch-state-dict-and-safetensors.md) | Concept | Core | `state_dict` vs full model serialization; `torch.load` pickle risks; Hugging Face `safetensors` format; 8-byte header layout; zero-copy memory mapping (`mmap`); layer streaming via `safe_open`. |
| **06** | [Saving Full End-to-End Pipelines](06-saving-full-pipelines.md) | Concept | Core | Eliminating training-serving skew; packaging `ColumnTransformer` + estimator as a single atomic artifact; internal state persistence (medians, scalers, vocabularies); `OneHotEncoder(handle_unknown='ignore')`. |
| **07** | [Model Versioning & Registries](07-model-versioning-and-registries.md) | Concept | Core | The model provenance 6-tuple (binary hash, git SHA, data hash, hyperparameters, metrics, signature); lifecycle stages (`Registered` $\to$ `Staging` $\to$ `Production` $\to$ `Archived`); automated promotion gating; zero-downtime rollbacks. |
| **08** | [Model Cards & Documentation](08-model-cards-and-documentation.md) | Concept | Core | The Mitchell et al. (2019) Model Card standard (9 sections); EU AI Act Annex IV technical documentation compliance; sliced evaluation tables; Disparate Impact Ratio (DIR); human oversight gates. |
| **09** | [Serving with FastAPI](09-serving-with-fastapi.md) | Concept | Core | High-performance ASGI serving with Uvicorn; `@asynccontextmanager` lifespan model loading into `app.state`; Pydantic v2 schemas; avoiding the `async def` blocking trap via AnyIO worker thread pools; `/healthz` and `/ready` probes. |
| **10** | [High-Throughput Batch Scoring](10-batch-scoring.md) | Concept | Advanced | Out-of-core chunked streaming; PyArrow and Apache Parquet columnar projection pushdown; bounded memory consumption ($\mathcal{O}(B)$); atomic write pattern (`.tmp` $\to$ final); resumable state checkpoint ledgers. |
| **11** | [Containerising Models with Docker](11-containerising-models.md) | Concept | Core | Multi-stage Docker builds cutting 80% image bloat; non-root user execution (`USER appuser`); read-only container root filesystems; baked vs dynamically mounted model artifacts; Kubernetes deployment manifests. |
| **12** | [Compatibility & Upgrading Libraries](12-compatibility-and-upgrading-libraries.md) | Concept | Advanced | Python ML dependency drift crisis; NumPy 2.0 ABI disruption; unpickling `AttributeError` breakages; why re-pickling is an anti-pattern; golden benchmark parity regression testing ($\Delta_{\max} \le 10^{-6}$); lockfiles with `uv`. |
| **13** | [Loading Untrusted Models Safely](13-loading-untrusted-models-safely.md) | Concept | Advanced | Zero-Trust model ingestion; threat modeling (RCE, Trojan backdoors, DNS data exfiltration); static AST scanning with Fickling and `pickletools`; air-gapped microVM conversion sandboxes (`--network none`); mandatory safe format conversion. |

---

## Architectural Decision Flowchart

```
+---------------------------------------------------------------------------------------------------------+
|                                    MODEL PERSISTENCE & DEPLOYMENT TAXONOMY                              |
|                                                                                                         |
|                                         What framework and target environment?                          |
|                     /                                      |                               \            |
|                    /                                       |                                \           |
|      [Scikit-Learn / Tabular]                     [PyTorch / Deep Learning]              [Cross-Language|
|                 |                                          |                              High-Perf API]|
|        Deployment Setting:                        Deployment Setting:                           |       |
|        /                 \                        /                 \                           |       |
|   [Internal Python]   [Public Hub / Zero-Trust] [Internal Checkpoint] [Public / Production]     |       |
|          |                       |                      |                     |                 |       |
|   Atomic Pipeline          Skops Safe            state_dict() with      Safetensors         ONNX Runtime|
|    with Joblib             Persistence            weights_only=True     Zero-Copy mmap     (C++, Go, SIMD|
|   (requirements.lock)     (Trusted Allowlists)    (Architecture Split)  (Zero Code Exec)    TensorRT)   |
|                                                                                                         |
|   +-------------------------------------------------------------------------------------------------+   |
|   |                           ENTERPRISE PRODUCTION SERVING & GOVERNANCE GATES                      |   |
|   |                                                                                                 |   |
|   |   1. Security Gate: Zero-Trust static opcode scan (Fickling); reject unverified pickle RCEs     |   |
|   |   2. Serving Ingestion: FastAPI with lifespan startup into app.state; standard 'def' thread pool|   |
|   |   3. Container Hardening: Multi-stage Docker build, non-root USER appuser, read-only root FS   |   |
|   |   4. Governance Gate: Model Registry promotion based on primary metric lift + p99 latency SLA   |   |
|   |   5. Audit Transparency: Automated Mitchell et al. Model Card with EU AI Act Sliced Metrics     |   |
|   +-------------------------------------------------------------------------------------------------+   |
+---------------------------------------------------------------------------------------------------------+
```

---

## Module Standards & Verification

- **Code Standard**: All implementations strictly adhere to Python 3.11+, PyTorch 2.4+, ONNX Runtime 1.19+, Pydantic v2.8+, scikit-learn 1.9+, and NumPy 2.5+.
- **Zero Raw Binaries**: All code, configurations, Dockerfiles, and test harnesses are self-contained inside markdown code blocks. Zero stray `.pkl`, `.bin`, or `.onnx` files are persisted to disk.
- **Inference Invariance**: Deserialization routines verify exact numerical equivalence ($\| \hat{y}_{\text{loaded}} - \hat{y}_{\text{orig}} \| < 10^{-6}$) across all formats.

---

[← Previous Module: 11-prediction-and-postprocessing](../11-prediction-and-postprocessing/README.md) | [Curriculum Index](../INDEX.md) | [Next Module: 13-interpretability-fairness-and-limitations →](../13-interpretability-fairness-and-limitations/README.md)
