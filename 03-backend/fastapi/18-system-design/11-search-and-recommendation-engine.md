# System Design Blueprint: Real-Time Search & Recommendation Engine

## Metadata

- **Topic**: Full-Text Search, Typeahead Autocomplete & Change Data Capture (CDC) Sync
- **Scale Requirements**: 50 Million searchable catalog documents, 25,000 search queries per second, sub-20ms p95 latency
- **Target SLA**: Near-real-time index freshness (< 1 second lag from primary DB to OpenSearch), 99.99% search availability
- **Tech Stack**: FastAPI, OpenSearch / Elasticsearch 8+, PostgreSQL, Debezium / Kafka Connect, Redis (Typeahead Cache)

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Fuzzy Full-Text Search**: Typo-tolerant keyword search across titles, descriptions, categories, and tags.
2. **Typeahead Autocomplete**: Prefix suggestions rendered in < 10ms as user types in search box.
3. **Change Data Capture (CDC)**: Automated synchronization from PostgreSQL to OpenSearch with zero dual-write inconsistencies.
4. **Faceted Filtering & Sorting**: Filter by price, brand, rating, and geographic proximity with dynamic aggregation counts.

---

## 2. High-Level Architecture Diagram

```
       [USER SEARCH PATH]                                [DATA SYNC (CDC) PATH]
                |                                                  |
                | GET /api/v1/search?q=laptop                      | PostgreSQL CRUD Operation
                v                                                  v
     +----------+----------+                            +----------+----------+
     | FastAPI Search Gateway                          | PostgreSQL Database |
     +----------+----------+                            | (Write-Ahead Log)   |
                |                                       +----------+----------+
                | 1. Check Redis Query Cache                       |
                v                                                  | WAL Streaming
     +----------+----------+                                       v
     | Redis Cache (Hot Qs)|                            +----------+----------+
     +----------+----------+                            | Debezium Connector  |
                |                                       +----------+----------+
                | Cache Miss                                       |
                v                                                  v
     +----------+----------+                            +----------+----------+
     | OpenSearch Cluster  | <------------------------- | Apache Kafka Stream |
     | (Inverted Index +   |    Bulk Indexer Worker     +---------------------+
     |  BM25 Relevance)    |
     +---------------------+
```

---

## 3. OpenSearch Query Construction Blueprint

```python
from fastapi import APIRouter, Depends, Query
from opensearchpy import AsyncOpenSearch
from pydantic import BaseModel

router = APIRouter(prefix="/search", tags=["Search"])
opensearch_client = AsyncOpenSearch(hosts=[{"host": "localhost", "port": 9200}])


class SearchResult(BaseModel):
    id: str
    title: str
    price: float
    score: float


@router.get("/products", response_model=list[SearchResult])
async def search_products(
    q: str = Query(..., min_length=2),
    min_price: float | None = None,
    max_price: float | None = None,
):
    must_conditions = [
        {
            "multi_match": {
                "query": q,
                "fields": ["title^3", "description", "tags^2"],
                "fuzziness": "AUTO",
            }
        }
    ]

    filters = []
    if min_price is not None or max_price is not None:
        price_range = {}
        if min_price is not None:
            price_range["gte"] = min_price
        if max_price is not None:
            price_range["lte"] = max_price
        filters.append({"range": {"price": price_range}})

    query_body = {
        "query": {"bool": {"must": must_conditions, "filter": filters}},
        "size": 20,
    }

    res = await opensearch_client.search(index="products", body=query_body)
    hits = res["hits"]["hits"]
    return [
        SearchResult(
            id=hit["_id"],
            title=hit["_source"]["title"],
            price=hit["_source"]["price"],
            score=hit["_score"],
        )
        for hit in hits
    ]
```

---

## 4. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Dual-Write Inconsistency** | App writes to Postgres and OpenSearch in separate calls; OpenSearch call fails, causing search index drift. | **Abolish Dual-Writes**: Use **Debezium Change Data Capture (CDC)** to stream DB WAL events to Kafka and bulk-index to OpenSearch asynchronously. |
| **Deep Pagination Attack (`from: 100000, size: 50`)** | OpenSearch node runs out of heap memory coordinating results across shards. | Enforce max offset limit (`from <= 1000`); use **`search_after` cursor pagination** for deep historical exports. |
| **OpenSearch Cluster Garbage Collection Pause** | JVM Stop-the-World GC freeze causes 10-second latency spikes. | Restrict JVM heap to maximum 31GB (preserving Compressed OOPs); offload read replicas across multiple dedicated data nodes. |
