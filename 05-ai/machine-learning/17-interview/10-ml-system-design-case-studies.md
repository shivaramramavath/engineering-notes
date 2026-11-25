# Note 10: Machine Learning System Design Case Studies

> **Topic ID:** `05-ai/machine-learning/17-interview/10-ml-system-design-case-studies`  
> **Target Audience:** Staff ML Engineers, Principal Architects, Technical Directors  
> **Prerequisites:** Note 01–15, Distributed Systems, High-Throughput Serving  
> **Status:** Production Verified

---

## 1. The Senior ML System Design Framework

In Staff and Principal ML System Design interviews (Meta, Google, Netflix, Uber), candidates are evaluated on their ability to architect scalable, resilient, end-to-end production systems under strict real-world constraints. Every system design interview follows a 6-step architectural framework:
1. **Scope & Clarify Requirements**: Functional goals, non-functional SLAs (latency, QPS, availability, freshness).
2. **High-Level System Topology**: End-to-end data pipelines, offline training, and online serving microservices.
3. **Data Engineering & Feature Pipelines**: Stream vs batch ingestion, dual feature stores, point-in-time correctness.
4. **Model Architecture & Inductive Biases**: Multi-stage funnel (candidate generation, ranking, re-ranking).
5. **Serving & Low-Latency Optimization**: Caching, vector search indexes, tensor quantization, fallback runbooks.
6. **Telemetry, Drift & Continuous Retraining**: Online experimentation, drift metrics, and champion-challenger gating.

---

## 2. Five Comprehensive Production Case Studies

### Case Study 1: Large-Scale Personalized Recommendation Engine (E-Commerce / Streaming)

#### 1. Requirements & Constraints
* **Functional Requirements**: Recommend a personalized list of 20 relevant items from a catalog of $100,000,000$ products on user home feed load.
* **Non-Functional Requirements**:
  * *Scale*: 200,000,000 Daily Active Users (DAU); Peak traffic: $50,000\text{ queries/second (QPS)}$.
  * *Latency SLA*: p99 response time $\le 50\text{ ms}$.
  * *Freshness*: Incorporate real-time in-session clicks within $\le 2\text{ seconds}$.

#### 2. End-to-End System Architecture

```mermaid
flowchart TD
    User["User Client (Mobile/Web)"] --> API["API Gateway (QPS: 50,000)"]
    API --> RecService["Recommendation Orchestrator (SLA <= 50ms)"]
    
    subgraph MultiStageFunnel ["3-Stage Recommendation Funnel"]
        RecService --> Stage1["Stage 1: Candidate Generation (100M -> 1,000 items in 15ms)"]
        Stage1 --> AnnIndex["HNSW Vector Search (Two-Tower Embeddings)"]
        Stage1 --> CollabIndex["Graph / Real-Time Collaborative Filtering"]
        
        AnnIndex & CollabIndex --> Stage2["Stage 2: Heavy Scoring & Ranking (1,000 -> 100 items in 25ms)"]
        Stage2 --> FeatureStore["Online Feature Store (Redis: User History & Context)"]
        Stage2 --> DeepRanker["Multi-Task Deep & Cross / DLRM Model"]
        
        DeepRanker --> Stage3["Stage 3: Business Logic & Diversity Re-Ranking (100 -> 20 items in 5ms)"]
        Stage3 --> Deduplication["Deduplication & Category Diversity (MMR)"]
        Stage3 --> FreshnessBooster["Exploration & Sponsoring Injection"]
    end
    
    Stage3 --> RecService
    RecService --> User
```

#### 3. Algorithmic Formulation & Serving Details
* **Candidate Generation (Two-Tower DNN)**:
  * User Tower outputs $\mathbf{u}(x_{\text{user}}) \in \mathbb{R}^{64}$; Item Tower outputs $\mathbf{v}(x_{\text{item}}) \in \mathbb{R}^{64}$.
  * Trained via sampled Softmax loss. Item embeddings are pre-computed offline and indexed in an Approximate Nearest Neighbor (ANN) index using Hierarchical Navigable Small World (**HNSW**).
  * Fast query: compute user vector online, query HNSW in $4\text{ms}$ to retrieve top 1,000 items.
* **Heavy Ranking (DLRM / Deep & Cross Network)**:
  * Predicts multiple objectives: $P(\text{Click})$ and $P(\text{Purchase} \mid \text{Click})$.
  * Utility function: $\text{Score} = P(\text{Click}) \times P(\text{Purchase} \mid \text{Click})^\alpha \times \text{Price}$.
* **Re-Ranking for Diversity**:
  * Uses Maximal Marginal Relevance (**MMR**) to prevent showing 20 items from the same category:
    $$\text{MMR} = \arg\max_{d_i \in R \setminus S} \left[ \lambda \text{Sim}_1(d_i, q) - (1 - \lambda) \max_{d_j \in S} \text{Sim}_2(d_i, d_j) \right]$$

---

### Case Study 2: Ultra Low-Latency Financial Fraud Detection Pipeline

#### 1. Requirements & Constraints
* **Functional Requirements**: Intercept credit card transactions in real-time, emit a fraud probability, and approve, decline, or route to 2FA / manual review.
* **Non-Functional Requirements**:
  * *Scale*: 10,000 transactions/sec peak; 500,000,000 transactions/day.
  * *Latency SLA*: Hard timeout at **$12\text{ ms}$** (strict banking payment processor deadline).
  * *Availability*: $99.999\%$ uptime (less than 5 minutes downtime/year).

#### 2. Architecture & Streaming Topology

```mermaid
flowchart TD
    POS["Card Reader / POS Terminal"] --> Switch["Payment Gateway Switch"]
    Switch --> Engine["Fraud Decisioning Engine (Timeout: 12ms)"]
    
    Engine --> Redis["In-Memory Online Feature Store (p99 < 1.5ms)"]
    Redis --> VectorAssemble["Assemble Feature Vector: [Lags, Velocity, Geo, Amount]"]
    
    VectorAssemble --> GBDT["Quantized GBDT / Treelite Compiled Model (p99 < 0.5ms)"]
    GBDT --> CostMatrix{"Cost-Sensitive Threshold Engine"}
    
    CostMatrix -->|Prob < 0.014| Approve["Approve (Instant)"]
    CostMatrix -->|0.014 <= Prob < 0.25| Challenge["Step-Up 2FA Challenge"]
    CostMatrix -->|Prob >= 0.25| Decline["Decline & Emit Alert"]
    
    Engine -.-> Kafka["Async Kafka Event Bus (Zero Latency Impact)"]
    Kafka --> Flink["Apache Flink: Streaming Window Velocity Counters"]
    Flink --> Redis
    Kafka --> DataLake["S3 Parquet / Snowflake (Offline Audit)"]
```

#### 3. Low-Latency Engineering & Asymmetric Cost Optimization
* **Sub-Millisecond Inference via Treelite**:
  * Standard Python GBDT takes $2-5\text{ms}$. By compiling the tree ensemble to a native branch-free C shared library via **Treelite**, inference executes in **$18\mu\text{s}$**, leaving $10\text{ms}$ budget for network transit and Redis lookups.
* **Streaming Feature Aggregations (Apache Flink)**:
  * Computes sliding window velocities: `txn_count_last_10m`, `distinct_countries_last_1h`, `velocity_spend_last_24h` updated in Redis within $50\text{ms}$ of transaction completion.
* **Bayes Optimal Thresholding**:
  * Asymmetric operational costs: $C_{\text{FP}} = \$5$ (friction/call), $C_{\text{FN}} = \$350$ (chargeback loss). Optimal threshold $\tau^* = \frac{5}{355} \approx 0.0141$.

---

### Case Study 3: Real-Time Search Ranking & Query Understanding

#### 1. Requirements & Constraints
* **Functional Requirements**: Return ranked document/product results for arbitrary search queries, supporting spelling correction, semantic intent expansion, and personalized ranking.
* **Non-Functional Requirements**:
  * *Scale*: 50,000 search queries/sec; $1,000,000,000$ indexable documents.
  * *Latency SLA*: p99 response time $\le 30\text{ ms}$.

#### 2. Multi-Stage Retrieval & Ranking Topology

```mermaid
flowchart LR
    Query["Search Query Text"] --> Intent["Query Understanding: Speller, Intent, Named Entity Recognition"]
    Intent --> Stage1["Stage 1: Lexical & Dense Retrieval (BM25 + Splade)"]
    Stage1 --> Top1000["1,000 Candidate Documents"]
    
    Top1000 --> Stage2["Stage 2: LightGBM LambdaMART Ranker (Features: BM25, CTR, PageRank)"]
    Stage2 --> Top50["Top 50 Documents"]
    
    Top50 --> Stage3["Stage 3: Cross-Encoder DistilBERT Re-Ranker"]
    Stage3 --> Top10["Top 10 Results with Position Debiasing"]
```

#### 3. Position Bias & LambdaMART Formulation
* **LambdaMART Ranking Optimization**:
  * Directly optimizes ranking metrics (NDCG) using virtual gradients (lambdas) that measure the change in NDCG achieved by swapping document pairs $i$ and $j$:
    $$\lambda_{ij} = \frac{-\sigma}{1 + e^{\sigma(s_i - s_j)}} |\Delta \text{NDCG}_{ij}|$$
* **Position Bias Correction (Inverse Propensity Scoring - IPS)**:
  * Users click higher-ranked items simply because they are displayed at the top, regardless of relevance:
    $$P(\text{Click} = 1 \mid d, k) = P(\text{Examined} \mid k) \times P(\text{Relevance} \mid d)$$
  * Examination probability $p_k$ is estimated via randomized position swap experiments. Objective loss is weighted by the inverse examination propensity $1 / p_k$, training the ranker on true unbiased relevance.

---

### Case Study 4: Real Estate Automated Valuation Model (AVM)

#### 1. Requirements & Constraints
* **Functional Requirements**: Predict market valuation for residential properties with mathematically calibrated prediction intervals for mortgage underwriting.
* **Non-Functional Requirements**:
  * *Accuracy*: Median Absolute Percentage Error ($\text{MdAPE}$) $\le 5.0\%$.
  * *Uncertainty*: Formally guaranteed $90\%$ coverage prediction intervals $[y_{\text{lower}}, y_{\text{upper}}]$.
  * *Auditability*: Explain every valuation with local SHAP waterfall charts for regulatory compliance.

#### 2. Model Architecture & Spatial Engineering

```mermaid
flowchart TD
    Property["Property Attributes (SqFt, Beds, Baths, Year)"] --> Preprocess["ColumnTransformer: RobustScaler + Imputers"]
    Geo["GPS Coordinates (Lat, Long)"] --> Spatial["Spatial Feature Engine: K-Nearest Sales & Hex H3 Index"]
    Macro["Macro Indicators: 30Y Mortgage Rates, Regional Case-Shiller Index"] --> Merged["Feature Vector X in R^d"]
    
    Preprocess & Spatial & Macro --> Merged
    Merged --> BaseEnsemble["HistGradientBoosting + CatBoost Ensemble"]
    BaseEnsemble --> PointEst["Point Estimate: Valuation Price USD"]
    
    Merged --> Conformal["Split Inductive Conformal Prediction Engine"]
    Conformal --> Intervals["Guaranteed 90% Price Range: [$450k, $510k]"]
    
    BaseEnsemble --> TreeSHAP["TreeSHAP Explainer (Sub-5ms waterfall)"]
```

#### 3. Spatial Encodings & Conformal Prediction Intervals
* **Spatial Feature Representation**:
  * Encodes geography using Uber's **H3 Hierarchical Hexagonal Spatial Index** (resolutions 7, 8, 9) mapped to target-encoded price per square foot with hierarchical Bayesian shrinkage.
* **Non-Parametric Split Conformal Prediction**:
  * Avoids assuming Gaussian error distributions.
  * Computes absolute non-conformity scores on held-out calibration set: $s_i = |y_i - \hat{f}(\mathbf{x}_i)|$.
  * Takes empirical $(1-\alpha)(1 + 1/n)$-th quantile $\hat{q}$:
    $$C(\mathbf{x}_{\text{query}}) = \left[ \hat{f}(\mathbf{x}_{\text{query}}) - \hat{q}, \, \hat{f}(\mathbf{x}_{\text{query}}) + \hat{q} \right]$$
  * **Mathematical Guarantee**: Coverage $\mathbb{P}(Y_{\text{true}} \in C(\mathbf{x})) \ge 1 - \alpha = 90\%$ holds under zero distributional assumptions.

---

### Case Study 5: Autonomous Driving 3D Perception & Object Detection

#### 1. Requirements & Constraints
* **Functional Requirements**: Detect, classify, and track 3D bounding boxes and velocity vectors for vehicles, pedestrians, and obstacles across 6 surround cameras.
* **Non-Functional Requirements**:
  * *Throughput*: 30 frames per second ($33.3\text{ ms}$ processing deadline per frame).
  * *Hardware Limit*: On-vehicle compute unit constrained to 60W power budget (embedded NVIDIA DRIVE Orin).

#### 2. Bird's Eye View (BEV) Architecture

```mermaid
flowchart TD
    subgraph MultiCamera ["Surround Camera Inputs (6 Views @ 30 FPS)"]
        C1["Front Camera"] & C2["Front-Left"] & C3["Front-Right"]
        C4["Rear Camera"] & C5["Rear-Left"] & C6["Rear-Right"]
    end
    
    MultiCamera --> 2DBackbone["Shared 2D Backbone: RegNet / ResNet-50 (FP16 / INT8)"]
    2DBackbone --> MultiScaleFeats["Multi-Scale 2D Feature Maps"]
    
    MultiScaleFeats --> BEVTransform["Lift-Splat-Shoot / Spatial Cross-Attention (2D -> 3D BEV)"]
    BEVTransform --> BEVGrid["Unified Bird's Eye View Representation: R^(H x W x C)"]
    
    BEVGrid --> TemporalFusion["Recurrent Temporal Fusion: GRU across prior frames"]
    TemporalFusion --> DetectionHeads["Task Heads (CenterPoint 3D Anchors)"]
    
    DetectionHeads --> BoundingBoxes["3D Boxes: (x, y, z, w, l, h, rot_z)"]
    DetectionHeads --> VelocityVectors["Kinematic Velocities: (v_x, v_y)"]
    DetectionHeads --> MapSegmentation["Drivable Area & Lane Boundary Semantic Masks"]
```

#### 3. Lift-Splat-Shoot & On-Device INT8 Execution
* **View Transformation (2D Image Space to 3D Physical Space)**:
  * Uses **Lift-Splat-Shoot (Philion & Baker, ECCV 2020)**:
    1. *Lift*: Predicts a categorical distribution over discrete depth bins $D$ for every pixel.
    2. *Splat*: Uses camera intrinsic $K$ and extrinsic $[R \mid t]$ matrices to project rays into a 3D frustum point cloud.
    3. *Shoot*: Voxel pooling collapses vertical heights along $z$ into a top-down 2D Bird's Eye View feature grid.
* **Hardware Quantization & Safety Runbook**:
  * Model weights are quantized to INT8 using TensorRT with Post-Training Quantization (PTQ) on vehicle hardware.
  * If the deep learning perception model encounters unexpected edge-case latency spikes ($> 30\text{ms}$), a low-compute classical radar-continuity heuristic intercepts control to execute emergency collision avoidance braking.
