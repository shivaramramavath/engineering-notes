# Amazon DynamoDB Advanced Data Modeling & Single Table Design

> **Module**: `03-storage-and-databases`  
> **Topic**: Single Table Design, Secondary Indexes, Streams, and In-Memory Caching  
> **Target Audience**: Senior Backend Engineers, Data Architects, Cloud Developers  
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

In traditional relational databases (RDBMS), data normalization divides information into dozens of discrete tables (e.g., `Users`, `Orders`, `OrderItems`, `Products`), using SQL joins at query runtime to reconstruct relational graphs. In high-scale distributed systems, relational joins require cross-node network shuffles, locking mechanisms, and heavy CPU overhead, causing severe latency spikes as data volumes reach terabytes.

In Amazon DynamoDB, high performance at internet scale requires **modeling for access patterns first**:
- **Single Table Design**: Co-locates multiple heterogeneous entity types (Users, Orders, Payments) within a **single DynamoDB table**. Relational joins are pre-computed during write time by grouping related items under the same Partition Key.
- **Single-Request Retrieval**: A single query can retrieve a Customer profile, their 5 most recent orders, and the order line items in a **single round-trip network request** taking under 5 milliseconds.
- **Event-Driven Architecture**: Uses **DynamoDB Streams** to stream item-level mutations to AWS Lambda functions for asynchronous replication, analytics pipelines, or cache invalidation.
- **Microsecond Acceleration**: Employs **DynamoDB Accelerator (DAX)** to deliver microsecond in-memory response times for read-intensive workloads.

---

## 2. Core Concepts

### 2.1 Single Table Design & Key Overloading
Instead of domain-specific column names like `UserId` or `OrderId`, Single Table Design uses generic primary keys:
- **`PK` (Partition Key)**: High-cardinality routing identifier (e.g., `USER#usr_1001`, `ORDER#ord_5002`).
- **`SK` (Sort Key)**: Hierarchical sorting attribute (e.g., `METADATA`, `ORDER#2026-11-25`, `ITEM#sku_991`).
- **Entity Identification (`Type`)**: An attribute indicating entity class (e.g., `Type: "USER"`, `Type: "ORDER"`).

### 2.2 Global Secondary Indexes (GSI) vs. Local Secondary Indexes (LSI)
- **Global Secondary Index (GSI)**:
  - Can define a **completely different Partition Key and Sort Key** from the base table.
  - Asynchronously replicated from the base table.
  - Up to **20 GSIs** per table.
  - Operates on its own dedicated throughput (WCU/RCU) or inherits on-demand mode.
  - Can be added, modified, or deleted at any time on an active table.
- **Local Secondary Index (LSI)**:
  - Uses the **same Partition Key** as the base table, but defines an **alternate Sort Key**.
  - Strongly consistent reads supported.
  - Shares the base table's provisioned throughput.
  - **Can only be created during initial table creation**; cannot be added later.
  - **10 GB Item Collection Limit**: The total size of all items sharing the same partition key value cannot exceed 10 GB.

### 2.3 Sparse Indexes
A Secondary Index is **sparse** if an item in the base table does not contain the attributes defined in the index's key schema:
- DynamoDB only populates an item into an index if the item contains the index's partition key and sort key.
- Items lacking these attributes are completely omitted from the index.
- Allows query engines to locate a tiny subset of records (e.g., finding only `Status = "FLAGGED_FOR_FRAUD"`) across billions of normal items without scanning the base table.

### 2.4 DynamoDB Streams
A time-ordered, sharded stream capturing item-level mutations:
- Emits records upon `PutItem`, `UpdateItem`, and `DeleteItem`.
- **Stream View Types**:
  - `KEYS_ONLY`: Only the key attributes of the modified item.
  - `NEW_IMAGE`: The entire item as it appears after the update.
  - `OLD_IMAGE`: The entire item as it appeared before the update.
  - `NEW_AND_OLD_IMAGES`: Both pre-mutation and post-mutation states.
- Exactly-once processing guarantee per shard when integrated with AWS Lambda.

### 2.5 Time to Live (TTL)
Automated item expiration mechanism:
- Designates a numeric attribute containing an epoch timestamp in seconds (e.g., `ExpiresAt: 1764038400`).
- DynamoDB automatically deletes expired items within 48 hours without consuming write capacity units (WCUs).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Single Table Design Data Layout Model                      |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Partition Key: PK ]       [ Sort Key: SK ]            [ Attributes / Payload ] |
|  -------------------------+---------------------------+-----------------------|
|  USER#1001                | METADATA                  | Name: "Alice", Email  |
|  USER#1001                | ORDER#2026-11-20#ORD-01   | Total: $120.00        |
|  USER#1001                | ORDER#2026-11-24#ORD-02   | Total: $45.50         |
|  USER#1001                | PAYMENT#VISA#4111         | Exp: "12/28"          |
|  -------------------------+---------------------------+-----------------------|
|  USER#1002                | METADATA                  | Name: "Bob", Email    |
|  USER#1002                | ORDER#2026-11-22#ORD-03   | Total: $310.00        |
|  -------------------------+---------------------------+-----------------------|
|  ORDER#ORD-01             | ITEM#SKU-99               | Qty: 2, Price: $50    |
|  ORDER#ORD-01             | ITEM#SKU-42               | Qty: 1, Price: $20    |
+-----------------------------------------------------------------------------------+
```

### Access Pattern Execution Flow
1. **Query Pattern 1**: "Fetch User profile and all their orders"
   - Query: `PK = "USER#1001" AND SK begins_with("ORDER#")` or `PK = "USER#1001"`
   - Result: Returns the user profile, orders, and payment methods in a single atomic request!
2. **Query Pattern 2**: "Fetch specific order line items"
   - Query: `PK = "ORDER#ORD-01" AND SK begins_with("ITEM#")`
   - Result: Returns all line items for that specific order.

---

## 4. Syntax & Usage

### 4.1 Creating a Table with GSI and TTL via AWS CLI
```bash
aws dynamodb create-table \
  --table-name EnterpriseCommerce \
  --attribute-definitions \
    AttributeName=PK,AttributeType=S \
    AttributeName=SK,AttributeType=S \
    AttributeName=GSI1PK,AttributeType=S \
    AttributeName=GSI1SK,AttributeType=S \
  --key-schema \
    AttributeName=PK,KeyType=HASH \
    AttributeName=SK,KeyType=RANGE \
  --billing-mode PAY_PER_REQUEST \
  --global-secondary-indexes '[
    {
      "IndexName": "GSI1",
      "KeySchema": [
        {"AttributeName": "GSI1PK", "KeyType": "HASH"},
        {"AttributeName": "GSI1SK", "KeyType": "RANGE"}
      ],
      "Projection": {
        "ProjectionType": "ALL"
      }
    }
  ]'

# Enable Time to Live (TTL) on attribute 'ExpiresAt'
aws dynamodb update-time-to-live \
  --table-name EnterpriseCommerce \
  --time-to-live-specification "Enabled=true,AttributeName=ExpiresAt"
```

### 4.2 Enabling DynamoDB Streams
```bash
aws dynamodb update-table \
  --table-name EnterpriseCommerce \
  --stream-specification StreamEnabled=true,StreamViewType=NEW_AND_OLD_IMAGES
```

---

## 5. Practical Examples

### Example 1: Boto3 Query Retrieving Multiple Related Entities in One Request
Fetch customer account metadata and all recent orders in a single sub-5ms query.

```python
import boto3
from boto3.dynamodb.conditions import Key

dynamodb = boto3.resource('dynamodb', region_name='us-east-1')
table = dynamodb.Table('EnterpriseCommerce')

def get_customer_dashboard(user_id):
    """
    Executes a single Query reading the user's item collection:
    - User Profile (SK: METADATA)
    - Orders (SK: ORDER#...)
    """
    response = table.query(
        KeyConditionExpression=Key('PK').eq(f"USER#{user_id}") & Key('SK').begins_with(""),
        ConsistentRead=True
    )
    
    items = response.get('Items', [])
    profile = None
    orders = []
    
    for item in items:
        if item['SK'] == 'METADATA':
            profile = item
        elif item['SK'].startswith('ORDER#'):
            orders.append(item)
            
    return {
        "profile": profile,
        "orders": orders,
        "consumedCapacity": response.get('ConsumedCapacity')
    }
```

### Example 2: Inverted Index (GSI) for Many-to-Many Relationships
In an e-commerce platform, finding all users who purchased a specific product SKU.

```
Base Table:
PK: USER#101     SK: PRODUCT#SKU-90    Title: "Mechanical Keyboard"
PK: USER#102     SK: PRODUCT#SKU-90    Title: "Mechanical Keyboard"

Inverted GSI1:
GSI1PK: PRODUCT#SKU-90    GSI1SK: USER#101
GSI1PK: PRODUCT#SKU-90    GSI1SK: USER#102
```

Querying the inverted index via Boto3:
```python
def get_users_for_product(sku):
    response = table.query(
        IndexName='GSI1',
        KeyConditionExpression=Key('GSI1PK').eq(f"PRODUCT#{sku}")
    )
    return response.get('Items', [])
```

### Example 3: DynamoDB Streams Triggering an Event-Driven Lambda Function
Lambda event processor reacting to order creation events.

```python
import json
import logging

logger = logging.getLogger()
logger.setLevel(logging.INFO)

def lambda_handler(event, context):
    for record in event['Records']:
        event_name = record['eventName'] # INSERT, MODIFY, REMOVE
        
        if event_name == 'INSERT':
            new_image = record['dynamodb']['NewImage']
            entity_type = new_image.get('Type', {}).get('S')
            
            if entity_type == 'ORDER':
                order_id = new_image['OrderId']['S']
                total = new_image['Total']['N']
                logger.info(f"New Order Detected: {order_id} for ${total}")
                # Trigger downstream notifications (SNS, EventBridge, Payment capture)
                
    return {"status": "SUCCESS"}
```

### Example 4: Sparse Index for High-Efficiency Exception Queuing
Query only failed orders without scanning millions of successful orders.

```python
# During write time:
# Successful orders DO NOT set the 'PendingReview' attribute.
# Failed orders set 'PendingReview = "YES"' and 'ReviewQueueDate = "2026-11-25"'.
# GSI2 Key Schema: GSI2PK = PendingReview, GSI2SK = ReviewQueueDate.

def get_failed_orders_queue():
    response = table.query(
        IndexName='GSI2',
        KeyConditionExpression=Key('GSI2PK').eq('YES')
    )
    return response.get('Items', [])
```

### Example 5: Integrating DynamoDB Accelerator (DAX) for Microsecond Reads
Connect to a DAX cluster to accelerate read performance by 10x without modifying query logic.

```python
import amazondax
import boto3

# Create a DAX client pointing to your DAX cluster endpoint
dax = amazondax.AmazonDaxClient.resource(
    endpoint_url='dax://my-dax-cluster.abc123.dax-clusters.us-east-1.amazonaws.com',
    region_name='us-east-1'
)

# Interact with the table identical to standard Boto3
table = dax.Table('EnterpriseCommerce')
response = table.get_item(Key={'PK': 'USER#1001', 'SK': 'METADATA'})
print(f"Retrieved user in microseconds: {response.get('Item')}")
```

---

## 6. Best Practices

1. **Model Data Around Access Patterns, Not Domain Entities**: Never design DynamoDB tables by normalizing business entities as you would in SQL. List all required application queries first, and design primary and secondary keys to satisfy those queries with `Query` operations.
2. **Use Generic Key Names (`PK`/`SK`)**: Always name table primary keys `PK` and `SK`. This allows a single table to store Users, Orders, Products, and Invoices without schema conflicts.
3. **Use Sparse Indexes to Eliminate Unnecessary Scans**: When indexing flags or rare events (e.g., active error logs, pending payments), only write index keys on items that require processing. This keeps index sizes small and queries lightning fast.
4. **Project Only Required Attributes in GSIs**: When creating a GSI, choose `ProjectionType = INCLUDE` with only necessary attributes (or `KEYS_ONLY`). Avoid `ALL` unless strictly necessary, as projecting all attributes doubles storage and write costs.
5. **Decouple Asynchronous Processing via DynamoDB Streams**: Never execute heavy business logic, external API calls, or email sending synchronously inside the write request path. Write the item to DynamoDB and trigger a Lambda function via Streams to process downstream steps asynchronously.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Using Multiple Tables for Relational Models** | Application must execute multiple sequential round-trip queries to fetch related entities, inflating latency. | Use **Single Table Design** to co-locate related records under a single Partition Key. |
| **Using LSIs Instead of GSIs** | Hitting the **10 GB Item Collection Limit** permanently blocks further writes to that partition key. | Use **Global Secondary Indexes (GSIs)**, which have no item collection size limits. |
| **Forgetting GSI Throttling Backpressure** | If a GSI runs out of WCUs, writes to the base table are throttled, failing client requests. | Ensure GSI provisioned capacity matches or exceeds base table write capacity, or use On-Demand mode. |
| **Overloading Sort Keys with Dynamic Regex** | Queries using complex substring filters fail because DynamoDB sort keys only support `begins_with` and `between`. | Structure composite sort keys with structured delimiters (e.g., `STATUS#ACTIVE#DATE#2026-11-25`). |
| **Using DAX for Write-Heavy Workloads** | DAX adds caching overhead without providing read benefits; write-through caching still writes to DynamoDB. | Reserve DAX for read-heavy workloads with repetitive queries (e.g., product catalogs, leaderboards). |
| **Assuming TTL Deletions Occur Instantly** | Applications serve expired data because TTL deletes items asynchronously within 48 hours. | Include an application-level filter check: `WHERE ExpiresAt > currentTime`. |

---

## 8. Performance, Security & Cost Notes

### GSI Replication Lag & Eventual Consistency
- Global Secondary Indexes are **strictly eventually consistent**. Writes to the base table typically replicate to GSIs within 10–50 milliseconds.
- You cannot perform strongly consistent reads against a GSI (`ConsistentRead=true` is rejected by the API).

### Security & Fine-Grained Access Control
- Restrict employee access to specific item partitions using IAM Policy Variables:
  ```json
  "Condition": {
    "ForAllValues:StringEquals": {
      "dynamodb:LeadingKeys": ["${cognito-identity.amazonaws.com:sub}"]
    }
  }
  ```

### Cost Dynamics
- **GSI Cost Multiplication**: Every time an item is created or updated in the base table, if the item contains attributes projected into 3 GSIs, DynamoDB writes to the base table AND all 3 GSIs, consuming **4x WCUs**.

---

## 9. Comparison Table

| Attribute | Base Table Query | Global Secondary Index (GSI) | Local Secondary Index (LSI) |
|:---|:---|:---|:---|
| **Partition Key** | Base PK | Can be different from base PK | Must be identical to base PK |
| **Sort Key** | Base SK | Can be different from base SK | Must be different from base SK |
| **Consistency** | Strongly or Eventually | Strictly Eventually Consistent | Strongly or Eventually Consistent |
| **Creation Timing** | At table creation | Anytime (online creation) | Table creation time ONLY |
| **Size Limit** | Unlimited | Unlimited | Max 10 GB per Item Collection |
| **Capacity Accounting**| Table WCU/RCU | Dedicated Index WCU/RCU | Consumes Base Table WCU/RCU |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the core philosophy behind Single Table Design in Amazon DynamoDB?</strong></summary>

**Answer:**
Single Table Design co-locates multiple distinct entity types (e.g., Customers, Orders, Products) into a single physical DynamoDB table.
- **Relational Joins at Read Time are Eliminated**: Instead of making multiple network round trips to query separate tables or forcing database engines to perform disk shuffles for joins, related items are grouped under the same Partition Key (`PK`).
- **Single-Round-Trip Latency**: A client can fetch a customer profile, their recent orders, and order line items with a single `Query` call in under 5 milliseconds.
- **Predictable Performance**: DynamoDB operates as an O(1) hash lookup engine; single table design leverages this to ensure performance never degrades regardless of dataset size.
</details>

<details>
<summary><strong>Q2: Why do Global Secondary Indexes strictly enforce eventual consistency?</strong></summary>

**Answer:**
GSIs are stored on completely separate physical storage partitions from the base table, potentially hosted on different physical hardware racks or Availability Zones within the region.
When an item is written to the base table, the write commits via a Paxos consensus quorum on the base table partition. DynamoDB asynchronously streams the mutation to the GSI partition storage nodes via an internal storage replication bus. Because requiring synchronous consensus across both the base table and all GSIs would introduce distributed deadlock risks and degrade write latency, GSI updates are asynchronous and eventually consistent.
</details>

<details>
<summary><strong>Q3: What is the 10 GB Item Collection Limit on Local Secondary Indexes (LSIs)?</strong></summary>

**Answer:**
In a table with an LSI, an **Item Collection** consists of all items that share the exact same Partition Key value. Because an LSI shares the same physical partition as the base table to support strongly consistent reads, the total size of all items in that item collection **cannot exceed 10 GB** (the physical size limit of a single DynamoDB partition). If an item collection grows beyond 10 GB, DynamoDB rejects further writes to that partition key with an error. For this reason, modern DynamoDB design heavily favors **GSIs** (which have no item collection size limits).
</details>

<details>
<summary><strong>Q4: How does a Sparse Index function and why does it save cost?</strong></summary>

**Answer:**
A Sparse Index is a Global Secondary Index where the index keys are only present on a small fraction of items in the base table. DynamoDB only writes an item to an index if the item contains the index's partition key and sort key attributes.
- **Cost Savings**: If only 1% of your items contain the index key, 99% of base table writes do not replicate to the GSI, avoiding WCU consumption and index storage fees.
- **Query Efficiency**: Instead of scanning millions of records to find a few pending tasks, a query against the sparse GSI scans only the 1% relevant items, returning results in milliseconds.
</details>

<details>
<summary><strong>Q5: What happens if a Global Secondary Index becomes throttled under Provisioned Capacity?</strong></summary>

**Answer:**
If a GSI's write capacity is under-provisioned and cannot keep up with base table write throughput:
- DynamoDB attempts to buffer mutations in an internal replication queue.
- If the replication queue fills, DynamoDB **throttles writes on the base table itself**, throwing `ProvisionedThroughputExceededException` back to the application.
- Therefore, in Provisioned Mode, GSI write capacity must always equal or exceed the base table write capacity, or the table must be switched to On-Demand Mode.
</details>

<details>
<summary><strong>Q6: How does DynamoDB Accelerator (DAX) handle write operations?</strong></summary>

**Answer:**
DAX is a **write-through** caching service:
- When an application executes `PutItem` or `UpdateItem` via the DAX client, DAX writes the mutation synchronously to the underlying DynamoDB table.
- Once the write to DynamoDB succeeds, DAX updates its own in-memory cache before returning a success code to the client.
- This ensures the cache does not become stale on writes, while reads (`GetItem`/`BatchGetItem`) are served directly from DAX memory with microsecond latency.
</details>

<details>
<summary><strong>Q7: Can Time to Live (TTL) be used for exact real-time application scheduling?</strong></summary>

**Answer:**
No. DynamoDB TTL deletes expired items in the background as a low-priority process without consuming user-provisioned throughput.
While items become logically expired the moment their epoch timestamp passes, DynamoDB typically physically deletes the item within **48 hours**. Applications relying on TTL must filter out expired items in application code (`WHERE ExpiresAt > currentTime`) if strict real-time expiration is required.
</details>

<details>
<summary><strong>Q8: What are the trade-offs of Single Table Design?</strong></summary>

**Answer:**
**Advantages**:
- Extreme query performance; zero relational join latency.
- Cost-effective (consolidates provisioned throughput onto one table).
- Atomic multi-entity transactions and single-round-trip data fetching.

**Trade-offs**:
- Steep learning curve; difficult for engineers accustomed to SQL.
- Inflexible ad-hoc querying; adding new, unplanned queries often requires creating new GSIs or running S3 analytics.
- Complex data validation and migration logic handled in application code rather than database constraints.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Single Table Design Table via CLI</strong></summary>

**Objective**: Provision a generic `PK`/`SK` table with GSI1.

**Command**:
```bash
aws dynamodb create-table \
  --table-name SingleTableStore \
  --attribute-definitions \
    AttributeName=PK,AttributeType=S \
    AttributeName=SK,AttributeType=S \
    AttributeName=GSI1PK,AttributeType=S \
    AttributeName=GSI1SK,AttributeType=S \
  --key-schema \
    AttributeName=PK,KeyType=HASH \
    AttributeName=SK,KeyType=RANGE \
  --billing-mode PAY_PER_REQUEST \
  --global-secondary-indexes '[{
    "IndexName": "GSI1",
    "KeySchema": [
      {"AttributeName": "GSI1PK", "KeyType": "HASH"},
      {"AttributeName": "GSI1SK", "KeyType": "RANGE"}
    ],
    "Projection": {"ProjectionType": "ALL"}
  }]'
```
</details>

<details>
<summary><strong>Exercise 2: Insert Heterogeneous Entities into Single Table</strong></summary>

**Objective**: Add a User entity and an Order entity under the same partition key.

**Commands**:
```bash
# Insert User profile
aws dynamodb put-item \
  --table-name SingleTableStore \
  --item '{
    "PK": {"S": "USER#101"},
    "SK": {"S": "METADATA"},
    "Type": {"S": "USER"},
    "Name": {"S": "Jane Doe"}
  }'

# Insert Order entity under same PK
aws dynamodb put-item \
  --table-name SingleTableStore \
  --item '{
    "PK": {"S": "USER#101"},
    "SK": {"S": "ORDER#2026-11-25#ORD-99"},
    "Type": {"S": "ORDER"},
    "Total": {"N": "89.50"},
    "GSI1PK": {"S": "ORDER#ORD-99"},
    "GSI1SK": {"S": "STATUS#PENDING"}
  }'
```
</details>

<details>
<summary><strong>Exercise 3: Query Multiple Entities with One Query</strong></summary>

**Objective**: Fetch both the user profile and all orders in a single CLI call.

**Command**:
```bash
aws dynamodb query \
  --table-name SingleTableStore \
  --key-condition-expression "PK = :pk" \
  --expression-attribute-values '{":pk": {"S": "USER#101"}}'
```
</details>

<details>
<summary><strong>Exercise 4: Query via Global Secondary Index</strong></summary>

**Objective**: Lookup the order directly by Order ID using GSI1.

**Command**:
```bash
aws dynamodb query \
  --table-name SingleTableStore \
  --index-name GSI1 \
  --key-condition-expression "GSI1PK = :gpk" \
  --expression-attribute-values '{":gpk": {"S": "ORDER#ORD-99"}}'
```
</details>

<details>
<summary><strong>Exercise 5: Enable DynamoDB Streams</strong></summary>

**Objective**: Activate `NEW_AND_OLD_IMAGES` stream capture.

**Command**:
```bash
aws dynamodb update-table \
  --table-name SingleTableStore \
  --stream-specification StreamEnabled=true,StreamViewType=NEW_AND_OLD_IMAGES
```
</details>

<details>
<summary><strong>Exercise 6: Test Sparse Index Filtering</strong></summary>

**Objective**: Create a GSI where only items containing `ErrorStatus` are indexed, and verify normal items do not appear.

**Steps**:
1. Add an item without `ErrorStatus`.
2. Add an item with `ErrorStatus = "CRITICAL"`.
3. Query the sparse index and verify that only the error item is returned.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                         Single Table Design Architecture Matrix                       |
+---------------------------------------------------------------------------------------+
|  Concept               | Purpose                           | Implementation           |
|------------------------+-----------------------------------+--------------------------|
|  Key Overloading       | Co-locating heterogeneous entities| Generic PK / SK names    |
|  One-to-Many Join      | Read parent + children in 1 query | Same PK, composite SKs   |
|  Global Secondary Idx  | Alternative access query paths    | Independent PK/SK (GSI)  |
|  Sparse Index          | Filter rare items without scans   | Populate index keys only |
|  DynamoDB Streams      | Async event-driven architecture   | Sharded mutation log     |
|  DAX                   | Microsecond read acceleration     | In-memory write-through  |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS DynamoDB Best Practices for Data Modeling](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/best-practices.html)
- [The DynamoDB Book by Alex DeBrie](https://www.dynamodbbook.com/)
- [Understanding DynamoDB Global Secondary Indexes](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/GSI.html)
- [Capturing Table Activity with DynamoDB Streams](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/Streams.html)
- [Amazon DynamoDB Accelerator (DAX) Architecture](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/DAX.html)
