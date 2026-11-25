# Module 11: Prediction & Postprocessing

Welcome to **Module 11: Prediction & Postprocessing** of the Machine Learning Curriculum track.

Deploying a model into production involves far more than simply calling `.predict()`. In industrial environments, models are probabilistic, unconstrained function approximators that interact with live, messy, and adversarial data. Uncalibrated probabilities, unexpected inputs, out-of-bounds predictions, and strict latency service level agreements (SLAs) present severe engineering challenges that cannot be resolved during model training alone.

This module covers the complete software and mathematical layer situated between raw model weights and end-user business decisions: offline batch vs online streaming architectures, Bayes-optimal cost-utility thresholding, Platt and Isotonic calibration, rule cascades, conformal prediction, real-time explainability, schema validation with Pydantic v2, and hardware-accelerated latency optimization.

---

## Module Notes Overview

| Order | Note | Format | Level | Focus & Key Algorithms |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Inference Fundamentals](01-inference-basics.md) | Concept | Core | Mathematical formulation of inference; discriminative vs generative scoring; compute, memory, and latency constraints; serving architecture patterns (in-process, microservice, edge). |
| **02** | [Batch vs Online Inference](02-batch-vs-online-inference.md) | Concept | Core | Throughput vs latency trade-offs; BigQuery/Spark batch pipelines vs FastAPI/gRPC real-time microservices; hybrid lambda/kappa architectures; feature store synchronization. |
| **03** | [Decision Thresholds & Calibration](03-thresholds-and-decision-rules.md) | Concept | Core | Expected value framework; Bayes-optimal threshold $\tau^*$; Platt scaling (logistic) and Isotonic regression (PAVA); Expected Calibration Error (ECE) and Brier score. |
| **04** | [Postprocessing Pipelines](04-postprocessing-pipelines.md) | Concept | Core | Bounded regression clamping and winsorization; deterministic priority rule cascades; ensemble score blending; Maximal Marginal Relevance (MMR) recommendation diversity re-ranking. |
| **05** | [Uncertainty Estimation & Conformal Prediction](05-uncertainty-and-confidence.md) | Concept | Advanced | Epistemic vs aleatoric uncertainty; Temperature scaling; Split Conformal Prediction with finite-sample distribution-free coverage ($1-\alpha$); Conformalized Quantile Regression (CQR). |
| **06** | [Explaining Predictions at Inference Time](06-explaining-predictions-at-inference.md) | Concept | Advanced | Regulatory adverse action requirements (ECOA/GDPR); closed-form linear attributions ($O(d)$); Fast TreeSHAP ($O(TLD^2)$); actionable counterfactuals; asynchronous explainability offloading. |
| **07** | [Input Validation & Schema Checks](07-input-validation-and-schema-checks.md) | Concept | Core | Pydantic v2 runtime schema enforcement; range/null/type guardrails; safe inference-time missing value imputation; out-of-vocabulary (OOV) categorical tokens; data contracts. |
| **08** | [Inference Latency & Throughput Optimization](08-latency-and-throughput.md) | Concept | Core | Little's Law ($\lambda = L / W$); Roofline model; SIMD AVX-512 acceleration; ONNX Runtime operator fusion; INT8 post-training quantization; dynamic batching queues; sub-ms prediction caching. |

---

## Architectural Decision Flowchart

```
+---------------------------------------------------------------------------------------------------------+
|                                PRODUCTION INFERENCE PIPELINE ARCHITECTURE                               |
|                                                                                                         |
|                                      Inbound Client Request / Data Stream                               |
|                                                        |                                                |
|   [Stage 1: Validation] ---> Pydantic v2 Schema Gate (Range, Type, Null & Cross-Field Consistency)      |
|                                   | (Reject HTTP 422 on violation)                                      |
|                                   v                                                                     |
|   [Stage 2: Preparation] --> Feature Alignment & Ingestion (Imputation with Train Statistics + OOV)    |
|                                   |                                                                     |
|                                   v                                                                     |
|   [Stage 3: Acceleration] -> Dynamic Batcher Queue (e.g. 5ms window or 64 batch size)                 |
|                                   |                                                                     |
|                                   v                                                                     |
|   [Stage 4: Execution] ----> Accelerated Inference Engine (ONNX Runtime INT8 / TreeSHAP / SIMD)         |
|                                   |                                                                     |
|                                   v                                                                     |
|   [Stage 5: Calibration] --> Calibration & Uncertainty Layer (Platt / Isotonic / Conformal Interval)    |
|                                   |                                                                     |
|                                   v                                                                     |
|   [Stage 6: Postprocess] --> Deterministic Rule Cascade & Bounded Clamping (Legal / Compliance Overrides) |
|                                   |                                                                     |
|                                   v                                                                     |
|   [Stage 7: Serving] ------> Audit-Logged Response Envelope (Decision + Calibrated Prob + Attributions)|
+---------------------------------------------------------------------------------------------------------+
```

---

## Module Standards & Verification

- **Code Standard**: All implementations strictly adhere to Python 3.11+, Pydantic v2.8+, scikit-learn 1.9+, and NumPy 2.5+.
- **Finite-Sample Validity**: Conformal prediction routines verify coverage guarantees empirically ($P(Y \in C(X)) \ge 1 - \alpha$).
- **Latency Budgets**: All real-time serving algorithms (linear attributions, clamping, validation) are profiled and verified to execute within sub-millisecond to sub-10ms production budgets.

---

[← Previous Module: 10-evaluation-and-metrics](../10-evaluation-and-metrics/README.md) | [Curriculum Index](../INDEX.md) | [Next Module: 12-saving-loading-and-deployment →](../12-saving-loading-and-deployment/README.md)
