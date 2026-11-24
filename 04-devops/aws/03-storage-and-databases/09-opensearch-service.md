# Amazon OpenSearch Service & OpenSearch Serverless Architecture

> **Module**: `03-storage-and-databases`  
> **Topic**: Distributed Search, Log Analytics, Vector Search, and Multi-Tier Storage  
> **Target Audience**: Search Engineers, Platform Architects, Observability Specialists  
> **Last Updated**: November 2026

---

## Navigation & Contents

- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Relational databases (RDS/Aurora) and key-value NoSQL stores (DynamoDB) excel at exact key lookups and structured transaction queries, but they struggle with complex unstructured search queries: fuzzy matching, multi-field relevance ranking, full-text stemming, autocomplete, and real-time log ingestion at hundreds of thousands of events per second.

**Amazon OpenSearch Service** (the community-driven open-source successor to Elasticsearch) is a fully managed distributed search and analytics suite.

It powers three foundational cloud use cases:
1. **Interactive Full-Text Search**: Provides instant search, faceted filtering, spelling correction, and BM25 relevance ranking for e-commerce catalogs, corporate intranets, and content management systems.
2. **Real-Time Operational Log Analytics**: Ingests, indexes, and visualizes application logs, VPC Flow Logs, and security audit traces using OpenSearch Dashboards (formerly Kibana).
3. **Vector Search & Generative AI**: Implements high-performance **k-Nearest Neighbor (k-NN)** vector indexes, acting as the retrieval engine for semantic search and Retrieval-Augmented Generation (RAG) pipelines.

---

## 2. Core Concepts

### 2.1 Cluster Topology: Dedicated Cluster Managers vs. Data Nodes
A provisioned OpenSearch domain separates node responsibilities:
- **Dedicated Cluster Manager (Master) Nodes**: Three master nodes distributed across Availability Zones. They do not store index data or process search requests; they handle cluster health, create/delete indexes, track node availability, and orchestrate shard rebalancing.
- **Data Nodes**: Store the index data (shards) and execute read, write, and search requests. Powered by memory-optimized or compute-optimized EC2 instances attached to EBS `gp3` or local NVMe storage.

### 2.2 Storage Tiering: Hot, UltraWarm, and Cold Storage
- **Hot Tier**: Fast SSD/NVMe storage attached directly to active data nodes. Handles high-speed writes and low-latency queries (typically retaining 1–7 days of active logs).
- **UltraWarm Tier**: Backed by Amazon S3 and specialized compute execution nodes. Lowers storage costs by up to **90%** compared to the Hot tier while keeping data read-only and queryable with sub-second latencies.
- **Cold Storage Tier**: Moves detached UltraWarm indexes into a completely serverless S3 state. Nodes consume zero compute resources when idle, costing only raw S3 storage rates while allowing indexes to be restored on demand within seconds.

### 2.3 Index Architecture: Shards, Replicas, and Inverted Index
- **Index**: A logical collection of JSON documents (analogous to a database table).
- **Inverted Index**: The core data structure mapping every distinct word/token to the list of documents containing that word, enabling sub-millisecond full-text searches.
- **Primary Shards**: The horizontal partitions of an index. Data is distributed across shards via `hash(_id) % number_of_shards`.
- **Replica Shards**: Exact duplicates of primary shards placed on different nodes and AZs for high availability and parallel read throughput.

### 2.4 Amazon OpenSearch Serverless
Introduced to eliminate cluster management:
- Decouples storage from compute completely.
- Data is stored durably in **Amazon S3**.
- Compute scales automatically in **OpenSearch Compute Units (OCUs)**: separate OCU pools for **Indexing** (ingestion) and **Search** (querying), ensuring high-volume log ingestion never slows down user-facing search queries.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Amazon OpenSearch Cluster Architecture                     |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Dedicated Cluster Manager Quorum (3x Master Nodes across 3 AZs) ]             |
|  (Orchestrates Cluster State, Shard Routing & Rebalancing - Zero Data Storage)    |
|                                                                                   |
|  -------------------------------------------------------------------------------  |
|  [ HOT STORAGE TIER (EBS GP3 / NVMe SSDs) ]                                       |
|                                                                                   |
|  +---------------------------+   +---------------------------+                    |
|  | Data Node 1 (AZ 1a)       |   | Data Node 2 (AZ 1b)       |                    |
|  | - Primary Shard P0        |   | - Primary Shard P1        |                    |
|  | - Replica Shard R1        |   | - Replica Shard R0        |                    |
|  +---------------------------+   +---------------------------+                    |
|                |                               |                                  |
|                v (Index State Management: ISM) v                                  |
|  -------------------------------------------------------------------------------  |
|  [ ULTRAWARM TIER (Amazon S3-Backed Direct Querying - 90% Cost Reduction) ]       |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  | UltraWarm Compute Node Fleet                                                |  |
|  | - Queries read-only indexes directly from Amazon S3                         |  |
|  +-----------------------------------------------------------------------------+  |
|                                        |                                          |
|                                        v (Archival Transition)                    |
|  -------------------------------------------------------------------------------  |
|  [ COLD STORAGE TIER (Detached Serverless S3 Storage - Zero Compute Cost) ]      |
+-----------------------------------------------------------------------------------+
```

### Inverted Index Construction
When indexing document: `{"id": 101, "title": "Deploy AWS Lambda"}`
```
Token Dictionary   Posting List (Document IDs)
----------------------------------------------
aws             -> [101, 104, 109]
deploy          -> [101, 102]
lambda          -> [101, 103, 105]
```
Searching for `"deploy AND lambda"` performs an intersection of posting lists `[101, 102] ∩ [101, 103, 105] = [101]` in microseconds without scanning document bodies!

---

## 4. Syntax & Usage

### 4.1 Creating an OpenSearch Domain via AWS CLI
```bash
aws opensearch create-domain \
  --domain-name "analytics-search" \
  --engine-version "OpenSearch_2.11" \
  --cluster-config '{
    "InstanceType": "r7g.large.search",
    "InstanceCount": 2,
    "DedicatedMasterEnabled": true,
    "DedicatedMasterType": "c7g.large.search",
    "DedicatedMasterCount": 3,
    "ZoneAwarenessEnabled": true,
    "ZoneAwarenessConfig": {"AvailabilityZoneCount": 2}
  }' \
  --ebs-options '{
    "EBSEnabled": true,
    "VolumeType": "gp3",
    "VolumeSize": 100,
    "Iops": 3000,
    "Throughput": 125
  }' \
  --node-to-node-encryption-options Enabled=true \
  --encryption-at-rest-options Enabled=true \
  --domain-endpoint-options EnforceHTTPS=true,TLSSecurityPolicy=Policy-Min-TLS-1-2-2019-07
```

### 4.2 Querying via REST API (Curl / OpenSearch Dashboards)
Search documents with multi-field matching, fuzzy matching, and highlighting:
```bash
curl -X POST "https://search-analytics.us-east-1.es.amazonaws.com/products/_search" \
  -H 'Content-Type: application/json' \
  -d '{
    "query": {
      "multi_match": {
        "query": "wirless headphone",
        "fields": ["title^3", "description"],
        "fuzziness": "AUTO"
      }
    },
    "highlight": {
      "fields": {
        "title": {}
      }
    }
  }'
```

---

## 5. Practical Examples

### Example 1: Declarative OpenSearch Serverless Collection with Terraform
Deploy an OpenSearch Serverless collection optimized for vector search and AI embeddings.

```hcl
resource "aws_opensearchserverless_security_policy" "encryption" {
  name        = "vector-encryption-policy"
  type        = "encryption"
  description = "Encryption policy for serverless vector store"

  policy = jsonencode({
    Rules = [
      {
        ResourceType = "collection"
        Resource     = ["collection/vector-knowledge-base"]
      }
    ]
    AWSOwnedKey = true
  })
}

resource "aws_opensearchserverless_security_policy" "network" {
  name        = "vector-network-policy"
  type        = "network"
  description = "Public access policy with fine-grained rules"

  policy = jsonencode([
    {
      Rules = [
        {
          ResourceType = "collection"
          Resource     = ["collection/vector-knowledge-base"]
        }
      ]
      AllowFromPublic = true
    }
  ])
}

resource "aws_opensearchserverless_collection" "vector_store" {
  name        = "vector-knowledge-base"
  type        = "VECTORSEARCH"
  description = "Vector database collection for RAG pipeline"

  depends_on = [
    aws_opensearchserverless_security_policy.encryption,
    aws_opensearchserverless_security_policy.network
  ]
}
```

### Example 2: Index State Management (ISM) Policy for Automated Log Tiering
Automate the lifecycle of log indexes: rollover at 50 GB or 30 days, move to UltraWarm after 7 days, and delete after 90 days.

```json
{
  "policy": {
    "description": "Enterprise log tiering policy",
    "default_state": "hot",
    "states": [
      {
        "name": "hot",
        "actions": [
          {
            "rollover": {
              "min_index_age": "30d",
              "min_primary_shard_size": "50gb"
            }
          }
        ],
        "transitions": [
          {
            "state_name": "warm",
            "conditions": {
              "min_index_age": "7d"
            }
          }
        ]
      },
      {
        "name": "warm",
        "actions": [
          {
            "warm_migration": {}
          }
        ],
        "transitions": [
          {
            "state_name": "delete",
            "conditions": {
              "min_index_age": "90d"
            }
          }
        ]
      },
      {
        "name": "delete",
        "actions": [
          {
            "delete": {}
          }
        ],
        "transitions": []
      }
    ]
  }
}
```

### Example 3: k-NN Vector Search Query for AI Embeddings
Execute a vector similarity search using cosine distance for Retrieval-Augmented Generation (RAG).

```json
POST /knowledge-index/_search
{
  "size": 5,
  "query": {
    "knn": {
      "embedding_vector": {
        "vector": [0.045, -0.012, 0.321, 0.089, -0.114, 0.542],
        "k": 5
      }
    }
  },
  "_source": ["document_id", "text_snippet", "source_url"]
}
```

### Example 4: Ingesting CloudWatch Logs via Kinesis Data Firehose
Stream application logs directly into OpenSearch with automated buffering.

```hcl
resource "aws_kinesis_firehose_delivery_stream" "opensearch_stream" {
  name        = "app-logs-to-opensearch"
  destination = "opensearch"

  opensearch_configuration {
    domain_arn = aws_opensearch_domain.search_cluster.arn
    role_arn   = aws_iam_role.firehose_delivery_role.arn
    index_name = "application-logs"
    type_name  = "_doc"

    s3_backup_mode = "FailedDocumentsOnly"

    s3_configuration {
      role_arn           = aws_iam_role.firehose_delivery_role.arn
      bucket_arn         = aws_s3_bucket.firehose_backup.arn
      buffering_size     = 5
      buffering_interval = 300
    }
  }
}
```

### Example 5: Recovering from Yellow or Red Cluster Health Status
Diagnose and resolve shard allocation failures via REST API.

```bash
# 1. Check cluster health status
curl -X GET "https://search-analytics.us-east-1.es.amazonaws.com/_cluster/health?pretty"

# 2. Identify unassigned shards
curl -X GET "https://search-analytics.us-east-1.es.amazonaws.com/_cat/shards?v&h=index,shard,prirep,state,unassigned.reason" | grep UNASSIGNED

# 3. Explain why a specific shard is not allocated
curl -X GET "https://search-analytics.us-east-1.es.amazonaws.com/_cluster/allocation/explain?pretty"
```

---

## 6. Best Practices

1. **Always Deploy 3 Dedicated Cluster Managers**: In production clusters with more than 2 data nodes, always enable 3 Dedicated Cluster Manager nodes across 3 Availability Zones. This prevents "split-brain" scenarios and prevents heavy query spikes on data nodes from destabilizing cluster quorum.
2. **Target 30 GB to 50 GB per Shard**: Avoid over-sharding. Target primary shard sizes between **30 GB and 50 GB** for log analytics, and **10 GB to 30 GB** for search workloads. Creating hundreds of tiny 100 MB shards consumes massive JVM heap memory and degrades query performance.
3. **Keep JVM Memory Pressure Below 75%**: Monitor the `JVMMemoryPressure` CloudWatch metric. If JVM memory pressure exceeds 75% continuously, garbage collection cycles stall, causing nodes to drop out of the cluster. Scale up memory or scale out data nodes immediately.
4. **Leverage Index State Management (ISM) for Lifecycle Transitions**: Automate data movement. Do not maintain separate scripts to delete or roll over indexes; use native ISM policies to roll over hot data and migrate to UltraWarm after 7–14 days.
5. **Use OpenSearch Serverless for Fluctuating Workloads**: For workloads with highly variable traffic (e.g., search applications used only during business hours, or intermittent security log ingestion), adopt **OpenSearch Serverless** to eliminate idle compute billing.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Over-Sharding (Thousands of Small Shards)** | High JVM memory consumption; nodes run out of heap memory and crash repeatedly. | Consolidate shards; maintain shard counts so each shard holds 30–50 GB of data. |
| **Omitting Dedicated Cluster Manager Nodes** | Data nodes performing heavy aggregation queries exhaust CPU, missing cluster heartbeats and causing cluster failure. | Deploy **3 Dedicated Cluster Manager nodes** across multiple AZs to isolate control plane health. |
| **Using Single-AZ in Production** | Availability zone hardware disruptions take the search domain and dashboards completely offline. | Enable **Zone Awareness** with 2 or 3 Availability Zones and at least 1 replica shard per primary. |
| **Ignoring `JVMMemoryPressure` Alarms** | When heap usage exceeds 85%, OpenSearch blocks write requests with `429 Too Many Requests`. | Set a CloudWatch alarm at 75% JVM memory pressure to trigger automated scaling before writes block. |
| **Scanning Full Text with Wildcard Prefixes (`*term`)** | Queries execute full-table disk sweeps, causing CPU spikes and 10+ second query latency. | Use standard full-text analyzers or edge n-grams for autocomplete instead of leading wildcards. |
| **Storing Inactive Historical Logs in Hot Tier** | Massive EBS storage costs ($0.08/GB/month) for older logs that are rarely or never queried. | Transition indexes older than 7 days to **UltraWarm** (backed by S3) using ISM policies. |

---

## 8. Performance, Security & Cost Notes

### JVM Heap Memory & Sizing
- OpenSearch runs on the Java Virtual Machine (JVM).
- The maximum recommended JVM heap size is **32 GB** (due to Java compressed object pointer limits). Allocating instances with 64 GB RAM assigns 32 GB to the JVM heap and leaves the remaining 32 GB for OS filesystem page caching.

### Security Enforcements
- **Fine-Grained Access Control (FGAC)**: Implements document-level and field-level security, allowing administrators to hide sensitive attributes (e.g., SSN, credit cards) based on IAM roles or SAML directory groups.
- **Node-to-Node Encryption**: Enforces TLS 1.3 across all internode communications.

### Cost Breakdown
- **Hot Data Nodes**: Hourly instance fee + EBS `gp3` storage and IOPS.
- **UltraWarm Tier**: ~$0.024 / GB / month (storage backed by Amazon S3) + hourly UltraWarm compute node fee.
- **OpenSearch Serverless**:
  - Ingestion: $0.24 per OCU-hour.
  - Search: $0.24 per OCU-hour. Minimum 2 OCUs for indexing and 2 OCUs for search ($0.96/hour baseline).

---

## 9. Comparison Table

| Attribute | Amazon OpenSearch Service | Amazon OpenSearch Serverless | Amazon Aurora PostgreSQL |
|:---|:---|:---|:---|
| **Primary Architecture** | Provisioned EC2/EBS Clusters | Serverless Decoupled S3 Tier | Relational Database (SQL) |
| **Data Indexing** | Inverted Index & k-NN Vectors | Inverted Index & k-NN Vectors | B-Tree & GIN Indexes |
| **Scaling Mechanism** | Cluster resize (Add nodes/EBS)| Automatic OCU Scaling | Serverless v2 ACUs |
| **Full-Text Ranking** | Native BM25 Relevance Scoring | Native BM25 Relevance Scoring | Basic `to_tsvector` full-text |
| **Vector Search (k-NN)**| Native k-NN Plugin | Native Vector Collection | `pgvector` extension |
| **Idle Hourly Cost** | Minimum ~$50/month (T3 nodes)| Minimum ~$700/month (4 OCUs) | Scale down to 0.5 ACU (~$43/mo)|

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the purpose of an Inverted Index in Amazon OpenSearch?</strong></summary>

**Answer:**
An Inverted Index is the core data structure that enables sub-millisecond full-text search.
Instead of scanning documents row-by-row to see if they contain a word (which is an O(N) operation), an inverted index tokenizes all text during write time and creates a dictionary mapping every unique word (token) to the list of document IDs (posting lists) where that word appears:
$$\text{Word} \rightarrow [\text{Doc 1}, \text{Doc 14}, \text{Doc 99}]$$
When a user searches for multiple terms, OpenSearch simply performs set intersections or unions of the relevant posting lists, returning ranked results across millions of documents in single-digit milliseconds.
</details>

<details>
<summary><strong>Q2: Why should an OpenSearch cluster have exactly 3 Dedicated Cluster Manager nodes?</strong></summary>

**Answer:**
Dedicated Cluster Manager nodes handle cluster orchestration: creating indexes, tracking node membership, mapping shard locations, and orchestrating rebalancing:
1. **Performance Isolation**: Keeping master nodes dedicated ensures that heavy analytical queries or ingestion spikes on Data Nodes do not starve the master of CPU or memory, which would cause the cluster to lose state and crash.
2. **Split-Brain Mitigation**: Distributed consensus algorithms (like Raft or Zen Discovery) require an **odd number of voting nodes** to achieve a strict majority ($N/2 + 1$). Having 3 master nodes ensures that if a network partition isolates 1 node, the remaining 2 nodes form a quorum, preventing two halves of the cluster from acting as competing masters and corrupting state.
</details>

<details>
<summary><strong>Q3: What causes an OpenSearch cluster status to transition to "Yellow" or "Red"?</strong></summary>

**Answer:**
- **Green**: All primary shards and all replica shards are actively allocated to data nodes across the cluster.
- **Yellow**: All **primary shards are allocated**, but **at least one replica shard is unassigned**. This typically occurs if a data node crashes, or during initial cluster provisioning when there are fewer nodes than configured replicas. The cluster is fully functional for reads and writes, but lacks high-availability redundancy.
- **Red**: At least **one primary shard is missing or unassigned**. Data is missing or inaccessible. Read queries return partial results or errors, and writes to affected indexes fail. This occurs if multiple nodes crash simultaneously or an EBS volume suffers unrecoverable corruption.
</details>

<details>
<summary><strong>Q4: How does the UltraWarm storage tier achieve up to 90% cost savings compared to Hot storage?</strong></summary>

**Answer:**
In the Hot tier, data nodes store indexes on high-performance Amazon EBS `gp3` volumes or local NVMe SSDs, which costs ~$0.08–$0.12 per GB-month plus instance hourly fees.
The **UltraWarm** tier completely decouples storage from compute:
- The actual index data is stored durably in **Amazon S3** (at standard S3 storage rates of ~$0.024/GB-month).
- UltraWarm compute nodes download only metadata and lightweight index caches locally.
- When an application executes a query targeting a warm index, UltraWarm nodes stream required data chunks directly from S3 using custom caching algorithms. This maintains sub-second query performance while cutting total storage bills by up to 90%.
</details>

<details>
<summary><strong>Q5: What is the optimal shard sizing guideline for Amazon OpenSearch?</strong></summary>

**Answer:**
- For **Log Analytics**: Target primary shard sizes between **30 GB and 50 GB**.
- For **Search Workloads**: Target primary shard sizes between **10 GB and 30 GB**.
- **The Rule of 20 Shards per GB of Heap**: As a general rule, a node should not host more than 20–25 shards per gigabyte of allocated JVM heap memory (e.g., a node with 32 GB heap should host no more than 600–800 shards total). Having thousands of tiny shards consumes excessive heap memory for cluster state and Lucene segment metadata, leading to JVM memory pressure and cluster crashes.
</details>

<details>
<summary><strong>Q6: How does OpenSearch Serverless decouple indexing and search compute?</strong></summary>

**Answer:**
Traditional OpenSearch clusters run indexing and search on the same shared data nodes. When a massive flood of log data arrives, indexing consumes 100% of the CPU and memory, causing user-facing search queries to time out.

OpenSearch Serverless completely decouples the two workloads into independent **OpenSearch Compute Unit (OCU)** pools:
- **Indexing OCUs**: Dedicated purely to parsing, tokenizing, and writing new documents to Amazon S3.
- **Search OCUs**: Dedicated purely to serving search queries and executing aggregations.
- Ingestion spikes automatically scale the Indexing OCU pool without impacting Search OCU capacity, guaranteeing consistent search latencies.
</details>

<details>
<summary><strong>Q7: What is the purpose of the k-NN plugin in Amazon OpenSearch?</strong></summary>

**Answer:**
The k-Nearest Neighbor (k-NN) plugin enables OpenSearch to function as a high-performance **Vector Database**:
- Applications generate mathematical vector embeddings (high-dimensional floating-point arrays) from text, images, or audio using deep learning models (such as Amazon Titan or OpenAI embeddings).
- OpenSearch indexes these vectors using specialized algorithms (Hierarchical Navigable Small World - HNSW, or Inverted File with Product Quantization - IVF-PQ).
- During search, OpenSearch calculates cosine similarity or Euclidean distance to return the most semantically relevant documents, forming the core retrieval engine for Generative AI and RAG architectures.
</details>

<details>
<summary><strong>Q8: What is Index State Management (ISM) and why is it preferred over external cron scripts?</strong></summary>

**Answer:**
Index State Management (ISM) is a native, distributed lifecycle automation engine built directly into OpenSearch:
- It eliminates fragile external Python or shell cron jobs that execute manual `curl -XDELETE` commands against the API.
- ISM operates internally within the cluster, executing deterministic state machines based on index age, primary shard size, or document count.
- It orchestrates automated rollovers, merges segments via `force_merge`, moves indexes between Hot, UltraWarm, and Cold tiers, and purges expired logs reliably.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an Index with Custom Shards and Replicas via REST API</strong></summary>

**Objective**: Provision an index with 2 primary shards and 1 replica.

**Command**:
```bash
curl -X PUT "https://<DOMAIN-ENDPOINT>/products" \
  -H 'Content-Type: application/json' \
  -d '{
    "settings": {
      "number_of_shards": 2,
      "number_of_replicas": 1
    },
    "mappings": {
      "properties": {
        "title": {"type": "text"},
        "sku": {"type": "keyword"},
        "price": {"type": "float"}
      }
    }
  }'
```
</details>

<details>
<summary><strong>Exercise 2: Index and Query Full-Text Documents</strong></summary>

**Objective**: Insert sample documents and execute a fuzzy match query.

**Commands**:
```bash
# Insert document
curl -X POST "https://<DOMAIN-ENDPOINT>/products/_doc/1" \
  -H 'Content-Type: application/json' \
  -d '{"title": "Apple MacBook Pro 16", "sku": "MBP-16", "price": 2499.00}'

# Execute fuzzy query
curl -X POST "https://<DOMAIN-ENDPOINT>/products/_search" \
  -H 'Content-Type: application/json' \
  -d '{"query": {"match": {"title": {"query": "makbook", "fuzziness": "AUTO"}}}}'
```
</details>

<details>
<summary><strong>Exercise 3: Check Cluster Health and Shard Allocation</strong></summary>

**Objective**: Inspect cluster status and shard distribution across nodes.

**Commands**:
```bash
# Cluster Health
curl -X GET "https://<DOMAIN-ENDPOINT>/_cluster/health?pretty"

# Shard Distribution
curl -X GET "https://<DOMAIN-ENDPOINT>/_cat/shards?v"
```
</details>

<details>
<summary><strong>Exercise 4: Apply an Index State Management (ISM) Policy</strong></summary>

**Objective**: Bind an automated rollover policy to a log index template.

**Command**:
```bash
curl -X PUT "https://<DOMAIN-ENDPOINT>/_plugins/_ism/policies/daily-log-policy" \
  -H 'Content-Type: application/json' \
  -d '{
    "policy": {
      "description": "Rollover after 50GB",
      "default_state": "hot",
      "states": [
        {"name": "hot", "actions": [{"rollover": {"min_index_age": "7d"}}], "transitions": [{"state_name": "delete", "conditions": {"min_index_age": "30d"}}]},
        {"name": "delete", "actions": [{"delete": {}}], "transitions": []}
      ]
    }
  }'
```
</details>

<details>
<summary><strong>Exercise 5: Configure Fine-Grained Access Control Role</strong></summary>

**Objective**: Restrict a role to read-only permissions on a specific index.

**Command**:
```bash
curl -X PUT "https://<DOMAIN-ENDPOINT>/_plugins/_security/api/roles/analyst_role" \
  -H 'Content-Type: application/json' \
  -d '{
    "cluster_permissions": ["cluster_composite_ops_ro"],
    "index_permissions": [{
      "index_patterns": ["products-*"],
      "allowed_actions": ["read", "search"]
    }]
  }'
```
</details>

<details>
<summary><strong>Exercise 6: Test k-NN Vector Index Creation</strong></summary>

**Objective**: Provision an index supporting 128-dimensional vector embeddings.

**Command**:
```bash
curl -X PUT "https://<DOMAIN-ENDPOINT>/vector-items" \
  -H 'Content-Type: application/json' \
  -d '{
    "settings": {"index.knn": true},
    "mappings": {
      "properties": {
        "item_vector": {
          "type": "knn_vector",
          "dimension": 128,
          "method": {
            "name": "hnsw",
            "space_type": "cosinesimil",
            "engine": "nmslib"
          }
        }
      }
    }
  }'
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                             OpenSearch Architectural Checklist                        |
+---------------------------------------------------------------------------------------+
|  Component             | Standard Production Setting       | Key Rationale            |
|------------------------+-----------------------------------+--------------------------|
|  Cluster Managers      | 3 Dedicated Master Nodes across AZ| Prevents split-brain & OOM|
|  Data Nodes            | Graviton R7g / M7g instances      | Up to 25% better price/p |
|  Primary Shard Size    | 30 GB to 50 GB per shard          | Prevents over-sharding   |
|  Lifecycle Automation  | Index State Management (ISM)      | Automated tiering/delete |
|  Storage Tiering       | UltraWarm & Cold Storage          | Up to 90% cost reduction |
|  Vector Search         | k-NN Plugin (HNSW Algorithm)      | Semantic search & RAG    |
|  Serverless Option     | OpenSearch Serverless (OCUs)      | Decouples ingest & search|
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon OpenSearch Service Developer Guide](https://docs.aws.amazon.com/opensearch-service/latest/developerguide/what-is.html)
- [OpenSearch Serverless Documentation](https://docs.aws.amazon.com/opensearch-service/latest/developerguide/serverless.html)
- [Managing Index Lifecycles with Index State Management (ISM)](https://opensearch.org/docs/latest/im-plugin/ism/index/)
- [k-NN Vector Search in OpenSearch](https://opensearch.org/docs/latest/search-plugins/knn/index/)
- [Sizing Amazon OpenSearch Service Domains](https://docs.aws.amazon.com/opensearch-service/latest/developerguide/sizing.html)
