# Elasticsearch & OpenSearch Clients in Node.js

> Scalable distributed full-text search, aggregation pipelines, and log analytics using the official `@elastic/elasticsearch` and `@opensearch-project/opensearch` clients.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Clients** | `@elastic/elasticsearch` v8+, `@opensearch-project/opensearch` v2+ |
| **Use Cases** | Catalog Search, Log Aggregation, Geo-queries, Vector Search |

---

## 1. Connecting to Elasticsearch

```javascript
import { Client } from '@elastic/elasticsearch';

export const esClient = new Client({
  node: process.env.ELASTICSEARCH_URL || 'http://localhost:9200',
  auth: {
    apiKey: process.env.ELASTICSEARCH_API_KEY,
  },
  maxRetries: 3,
  requestTimeout: 10000,
});
```

---

## 2. Multi-Match Search with Fuzzy Matching

```javascript
export async function searchCatalog(queryText, categoryFilter) {
  const response = await esClient.search({
    index: 'products',
    body: {
      query: {
        bool: {
          must: [
            {
              multi_match: {
                query: queryText,
                fields: ['title^3', 'description', 'brand^2'],
                fuzziness: 'AUTO', // Handles typos
              },
            },
          ],
          filter: categoryFilter ? [{ term: { category: categoryFilter } }] : [],
        },
      },
      highlight: {
        fields: { title: {}, description: {} },
      },
    },
  });

  return response.hits.hits.map(hit => ({
    id: hit._id,
    score: hit._score,
    source: hit._source,
    highlight: hit.highlight,
  }));
}
```
