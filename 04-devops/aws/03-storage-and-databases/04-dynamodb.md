# Amazon DynamoDB Architecture & Core Fundamentals

> **Module**: `03-storage-and-databases`  
> **Topic**: Fully Managed Serverless NoSQL Database Architecture  
> **Target Audience**: Backend Developers, Cloud Architects, Database Engineers  
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

Traditional relational database management systems (RDBMS) scale vertically: as data volumes and concurrent connections grow, organizations must upgrade to larger instances with more CPU and RAM. When single-server scaling limits are reached, horizontal sharding across multiple relational instances requires immense application-level complexity and introduces operational fragility.

**Amazon DynamoDB** is a fully managed, serverless, key-value and document NoSQL database service designed for internet-scale applications. It delivers predictable, single-digit millisecond latency at any scale—whether serving 10 requests per second or 100 million requests per second.

Key architectural pillars of DynamoDB include:
- **Serverless Operations**: No virtual servers to provision, patch, or manage; zero storage capacity management.
- **Predictable Single-Digit Millisecond Latency**: Hardware-accelerated SSD storage and internal partition routing ensure consistent performance regardless of database size (from 10 GB to 100+ TB).
- **Fine-Grained Elastic Billing**: Supports both **On-Demand Capacity** (pay per request, scaling instantly to zero) and **Provisioned Capacity** with automated target tracking.
- **Enterprise High Availability**: Data is automatically replicated synchronously across a minimum of three Availability Zones within the AWS Region.

---

## 2. Core Concepts

### 2.1 Data Modeling Primitives
- **Table**: A collection of data items. DynamoDB has no predefined schema except for the primary key attributes.
- **Item**: A record composed of attributes (analogous to a row in a relational database). Maximum item size is **400 KB** (including attribute names and binary values).
- **Attribute**: A fundamental data element (e.g., String, Number, Binary, Boolean, Null, List, Map, Set).

### 2.2 Primary Key Architecture
Every table must define a Primary Key during creation. Two key designs are supported:
1. **Simple Primary Key (Partition Key Only)**:
   - Also known as the **Hash Key**.
   - DynamoDB runs the partition key value through an internal cryptographic hash function to determine the exact physical storage partition where the item is stored.
   - Every item must have a unique partition key value.
2. **Composite Primary Key (Partition Key + Sort Key)**:
   - Also known as **Hash & Range Key**.
   - Items with the same Partition Key are stored together on the same physical partition, sorted in ascending order by the Sort Key (e.g., `PK: CustomerID`, `SK: OrderDate#OrderID`).
   - Allows complex range queries (`begins_with`, `between`, `<`, `>`).

### 2.3 Read & Write Capacity Units (RCU / WCU)
Under **Provisioned Capacity Mode**, throughput is quantified in capacity units:
- **Write Capacity Unit (WCU)**:
  - 1 WCU represents **one write request per second for an item up to 1 KB** in size.
  - An item of 2.5 KB requires 3 WCUs (rounded up to the next 1 KB increment).
  - Transactional writes consume **2 WCUs per 1 KB**.
- **Read Capacity Unit (RCU)**:
  - **Strongly Consistent Read**: 1 RCU represents **one read per second for an item up to 4 KB**.
  - **Eventually Consistent Read**: 1 RCU represents **two reads per second for items up to 4 KB** (50% cheaper).
  - Transactional reads consume **2 RCUs per 4 KB**.

### 2.4 Capacity Modes: On-Demand vs. Provisioned
- **On-Demand Mode**: Pay strictly per read and write request. Ideal for spiky, unpredictable workloads or applications scaling up from zero.
- **Provisioned Mode**: Specify dedicated RCUs and WCUs. Paired with Application Auto Scaling to adjust capacity based on utilization. Significantly cheaper for predictable, steady-state workloads.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Amazon DynamoDB Internal Architecture                      |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Client / AWS SDK Request ] (PutItem / GetItem / Query)                         |
|             |                                                                     |
|             v (HTTPS / TLS 1.3)                                                   |
|  +-----------------------------------------------------------------------------+  |
|  | Request Router (Fleet of Stateless Gateway Proxies)                         |  |
|  | - Authenticates IAM Signature (SigV4)                                       |  |
|  | - Evaluates Account & Table Throughput Quotas                              |  |
|  | - Hashes Partition Key: Hash(PK) mod PartitionCount                        |  |
|  +-----------------------------------------------------------------------------+  |
|             |                                                                     |
|             v Routes directly to Storage Node Leader                              |
|  +-----------------------------------------------------------------------------+  |
|  | Physical Storage Partition (Paxos Consensus Group across 3 AZs)             |  |
|  |                                                                             |  |
|  |  +-------------------+   +--------------------+   +----------------------+  |  |
|  |  | AZ us-east-1a     |   | AZ us-east-1b      |   | AZ us-east-1c        |  |  |
|  |  | Storage Node (L)  |-->| Storage Node (F)   |-->| Storage Node (F)     |  |  |
|  |  | (Leader Replica)  |   | (Follower Replica) |   | (Follower Replica)   |  |  |
|  |  +-------------------+   +--------------------+   +----------------------+  |  |
|  |            |                                                                |  |
|  |            v                                                                |  |
|  |  [ SSD Storage Engine: B-Tree Index + Append-Only Commit Log ]              |  |
|  |  - Max Partition Capacity: 10 GB Storage                                    |  |
|  |  - Max Partition Throughput: 1,000 WCU / 3,000 RCU                          |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

### Partition Splitting Mechanics
DynamoDB automatically allocates physical partitions based on two hard constraints:
1. **Storage Constraint**: Each physical partition holds a maximum of **10 GB of data**. When a partition exceeds 10 GB, DynamoDB splits the partition in half automatically.
2. **Throughput Constraint**: Each physical partition supports a maximum of **1,000 WCUs** or **3,000 RCUs**. If provisioned throughput exceeds this boundary, DynamoDB partitions the keyspace across additional nodes.

---

## 4. Syntax & Usage

### 4.1 Creating a DynamoDB Table via AWS CLI
```bash
aws dynamodb create-table \
  --table-name Orders \
  --attribute-definitions \
    AttributeName=CustomerID,AttributeType=S \
    AttributeName=OrderTimestamp,AttributeType=S \
  --key-schema \
    AttributeName=CustomerID,KeyType=HASH \
    AttributeName=OrderTimestamp,KeyType=RANGE \
  --billing-mode PAY_PER_REQUEST \
  --point-in-time-recovery-specification PointInTimeRecoveryEnabled=true \
  --tags Key=Environment,Value=Production
```

### 4.2 Executing a Query with Condition Expressions (CLI)
Query all orders for customer `CUST-9921` placed after January 1, 2026:
```bash
aws dynamodb query \
  --table-name Orders \
  --key-condition-expression "CustomerID = :cid AND OrderTimestamp > :afterDate" \
  --expression-attribute-values '{
    ":cid": {"S": "CUST-9921"},
    ":afterDate": {"S": "2026-01-01T00:00:00Z"}
  }' \
  --consistent-read \
  --return-consumed-capacity TOTAL
```

---

## 5. Practical Examples

### Example 1: High-Performance Boto3 PutItem with Idempotency & Conditions
Python script inserting an order item only if it does not already exist (`attribute_not_exists`).

```python
import boto3
from botocore.exceptions import ClientError

dynamodb = boto3.resource('dynamodb', region_name='us-east-1')
table = dynamodb.Table('Orders')

def create_order(customer_id, order_id, timestamp, total_amount):
    try:
        response = table.put_item(
            Item={
                'CustomerID': customer_id,
                'OrderTimestamp': f"{timestamp}#{order_id}",
                'OrderId': order_id,
                'Amount': total_amount,
                'Status': 'PENDING',
                'Version': 1
            },
            # Prevent overwriting an existing order with the exact same keys
            ConditionExpression='attribute_not_exists(CustomerID) AND attribute_not_exists(OrderTimestamp)',
            ReturnConsumedCapacity='TOTAL'
        )
        print(f"Order created successfully. Consumed: {response.get('ConsumedCapacity')}")
        return True
    except ClientError as e:
        if e.response['Error']['Code'] == 'ConditionalCheckFailedException':
            print("Order already exists. Skipping duplicate insert.")
        else:
            raise e
        return False
```

### Example 2: Declarative Terraform Table with Provisioned Auto Scaling
Configure a production table with Application Auto Scaling tracking 70% target utilization.

```hcl
resource "aws_dynamodb_table" "users" {
  name           = "ProductionUsers"
  billing_mode   = "PROVISIONED"
  read_capacity  = 10
  write_capacity = 10
  hash_key       = "UserId"

  attribute {
    name = "UserId"
    type = "S"
  }

  point_in_time_recovery {
    enabled = true
  }

  server_side_encryption {
    enabled = true
  }

  lifecycle {
    ignore_changes = [read_capacity, write_capacity]
  }
}

resource "aws_appautoscaling_target" "dynamodb_table_read_target" {
  max_capacity       = 100
  min_capacity       = 10
  resource_id        = "table/${aws_dynamodb_table.users.name}"
  scalable_dimension = "dynamodb:table:ReadCapacityUnits"
  service_namespace  = "dynamodb"
}

resource "aws_appautoscaling_policy" "dynamodb_table_read_policy" {
  name               = "DynamoDBReadCapacityUtilization:${aws_appautoscaling_target.dynamodb_table_read_target.resource_id}"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.dynamodb_table_read_target.resource_id
  scalable_dimension = aws_appautoscaling_target.dynamodb_table_read_target.scalable_dimension
  service_namespace  = aws_appautoscaling_target.dynamodb_table_read_target.service_namespace

  target_tracking_scaling_policy_configuration {
    predefined_metric_specification {
      predefined_metric_type = "DynamoDBReadCapacityUtilization"
    }
    target_value = 70.0
  }
}
```

### Example 3: TransactWriteItems for Atomic Multi-Item Transactions
Execute an atomic debit-and-credit transaction across two user accounts.

```python
import boto3

client = boto3.client('dynamodb')

def transfer_funds(sender_id, recipient_id, amount):
    try:
        response = client.transact_write_items(
            TransactItems=[
                {
                    # 1. Debit sender if balance is sufficient
                    'Update': {
                        'TableName': 'Accounts',
                        'Key': {'AccountId': {'S': sender_id}},
                        'UpdateExpression': 'SET Balance = Balance - :amt',
                        'ConditionExpression': 'Balance >= :amt',
                        'ExpressionAttributeValues': {':amt': {'N': str(amount)}}
                    }
                },
                {
                    # 2. Credit recipient
                    'Update': {
                        'TableName': 'Accounts',
                        'Key': {'AccountId': {'S': recipient_id}},
                        'UpdateExpression': 'SET Balance = Balance + :amt',
                        'ExpressionAttributeValues': {':amt': {'N': str(amount)}}
                    }
                }
            ]
        )
        print("Transaction committed atomically.")
        return True
    except client.exceptions.TransactionCanceledException as e:
        print(f"Transaction failed / rolled back: {e.response['CancellationReasons']}")
        return False
```

### Example 4: Calculating Precise WCU Consumption
Understanding rounding calculations for write operations:

```
Scenario A: PutItem with 800 bytes payload
Calculation: 800 bytes <= 1 KB -> Consumes 1 WCU

Scenario B: PutItem with 2,400 bytes payload
Calculation: ceil(2,400 / 1,024) = 3 KB -> Consumes 3 WCU

Scenario C: TransactWriteItems with 2,400 bytes payload
Calculation: 3 KB * 2 (Transactional multiplier) -> Consumes 6 WCU
```

### Example 5: Implementing Optimistic Locking via Version Numbers
Prevent lost updates in concurrent environments using version checks.

```python
def update_user_status(user_id, new_status, current_version):
    table = boto3.resource('dynamodb').Table('Users')
    try:
        table.update_item(
            Key={'UserId': user_id},
            UpdateExpression='SET #st = :status, Version = Version + :inc',
            ConditionExpression='Version = :expectedVersion',
            ExpressionAttributeNames={'#st': 'Status'},
            ExpressionAttributeValues={
                ':status': new_status,
                ':inc': 1,
                ':expectedVersion': current_version
            }
        )
        return True
    except ClientError as e:
        if e.response['Error']['Code'] == 'ConditionalCheckFailedException':
            print("Concurrent update conflict! Refresh data and retry.")
        return False
```

---

## 6. Best Practices

1. **Avoid Hot Partition Keys**: Never use low-cardinality values (e.g., status flags `Active/Inactive`, gender, or the current calendar year) as the Partition Key. Choose high-cardinality keys (e.g., `UUID`, `CustomerID`, `DeviceID`) to ensure traffic distributes evenly across all underlying physical partitions.
2. **Favor Query Over Scan**: Never use `Scan` operations in production API request paths. `Scan` reads every single item in the entire table sequentially, consuming massive RCUs and degrading response times. Design tables so queries read via the Partition Key (`Query`).
3. **Use Eventually Consistent Reads by Default**: Eventually consistent reads consume **50% fewer RCUs** than strongly consistent reads. Use strongly consistent reads only when business logic strictly mandates reading mutations made milliseconds prior (e.g., financial ledgering).
4. **Enable Point-In-Time Recovery (PITR)**: Enable PITR across all production tables. Provides continuous automatic backups with 35-day retention, allowing you to restore the table to any second in time to recover from accidental deletions or corrupted scripts.
5. **Implement Exponential Backoff with Jitter**: When making high-volume requests that risk `ProvisionedThroughputExceededException`, configure SDK HTTP retries with truncated exponential backoff and decorrelated jitter to avoid stampeding thundering herds.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Using `Scan` with Filters in APIs** | High query latency and massive RCU consumption; filters only apply *after* reading entire partitions. | Design composite primary keys and Secondary Indexes (GSIs) to use `Query` instead. |
| **Monolithic Partition Keys (Hot Keys)** | 100,000 requests/sec hitting a single partition key exceed the 1,000 WCU partition limit, causing throttling. | Distribute keys using **partition salt suffixes** (e.g., `SENSOR_102#0` through `SENSOR_102#9`). |
| **Storing Large Payloads (>400 KB)** | DynamoDB returns a `ValidationException: Item size has exceeded the acceptable limit`. | Store large files/blobs in **Amazon S3** and store the S3 object URL in DynamoDB (Claim Check). |
| **Ignoring Eventually Consistent Latency** | Query immediately following a write returns outdated data in rare race conditions. | Use `ConsistentRead=true` when reading newly written data if immediate coherence is required. |
| **Provisioning Static Capacity for Burst Apps** | Massive billing waste during low hours and throttling during traffic spikes. | Adopt **On-Demand Capacity Mode** (`PAY_PER_REQUEST`) for spiky or unpredictable applications. |
| **Modifying Primary Keys After Table Creation** | Primary key schema attributes cannot be changed once a table is provisioned. | Plan keys carefully; altering schema requires migrating data to a new table. |

---

## 8. Performance, Security & Cost Notes

### Throughput & Partition Capacity
- **Hard Partition Limits**:
  - Max storage per physical partition: **10 GB**
  - Max throughput per physical partition: **1,000 WCU** and **3,000 RCU**
- When a table is queried, DynamoDB buffers throughput using **burst capacity** (up to 300 seconds of unused capacity), smoothing out temporary micro-spikes.

### Security Enforcements
- **Encryption at Rest**: Enabled by default using AWS KMS (SSE-KMS or AWS owned key).
- **Fine-Grained Access Control (FGAC)**: Use IAM policy condition keys (`dynamodb:LeadingKeys`, `dynamodb:Attributes`) to allow users or mobile apps to query only their own items based on their Cognito identity.

### Cost Breakdown
- **On-Demand Mode**:
  - $1.25 per million write request units
  - $0.25 per million read request units
- **Provisioned Mode**:
  - $0.00065 per WCU-hour (~$0.47 per WCU/month)
  - $0.00013 per RCU-hour (~$0.09 per RCU/month)
- **Storage**: First 25 GB free; $0.25 per GB/month thereafter.

---

## 9. Comparison Table

| Attribute | Amazon DynamoDB | Amazon RDS PostgreSQL | Amazon DocumentDB |
|:---|:---|:---|:---|
| **Database Paradigm** | NoSQL Key-Value & Document | Relational SQL (ACID) | NoSQL Document (MongoDB API) |
| **Scaling Architecture** | Horizontal (Serverless Auto-Partition)| Vertical (Instance Sizing) + Read Rep | Horizontal storage + Vertical compute|
| **Schema Flexibility** | Schema-less (except PK) | Rigid relational schema | Schema-less JSON BSON |
| **Max Item / Row Size** | 400 KB | Multiple gigabytes | 16 MB |
| **Read Latency** | Single-digit ms (1–5 ms) | Single-digit ms (indexed) | Single-digit ms |
| **Query Mechanism** | Key Query / Filter Expressions | Complex SQL joins & aggregates | MongoDB query syntax |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How does Amazon DynamoDB determine which physical partition stores an item?</strong></summary>

**Answer:**
DynamoDB uses an internal cryptographic hash function (MD5-based):
1. The client SDK submits a request containing the **Partition Key** value.
2. DynamoDB's request router hashes the partition key value into a 128-bit integer.
3. The keyspace is divided into ranges mapped across active physical partitions. The calculated hash integer determines the exact storage partition leader node responsible for that range.
4. If a Sort Key is present, items sharing the same partition key hash are stored in sorted order on that partition's local B-tree index.
</details>

<details>
<summary><strong>Q2: What is the difference between an Eventually Consistent Read and a Strongly Consistent Read in DynamoDB?</strong></summary>

**Answer:**
- **Eventually Consistent Read (Default)**: When data is written, the leader replica updates its local log and acknowledges the write to the client once a quorum of 2 out of 3 replicas agree. An eventually consistent read may read from a follower replica that has not yet completed replication, potentially returning slightly stale data (typically within seconds). Consumes **0.5 RCU per 4 KB**.
- **Strongly Consistent Read**: Guarantees returning a response that reflects all successful write operations that occurred prior to the read. DynamoDB routes the read directly to the partition leader or verifies consensus, guaranteeing fresh data. Consumes **1.0 RCU per 4 KB** (2x the cost).
</details>

<details>
<summary><strong>Q3: How do you calculate required WCUs for writing 200 items per second with an average size of 2.8 KB?</strong></summary>

**Answer:**
1. Round item size up to the next 1 KB increment:
   $$2.8\text{ KB} \rightarrow 3\text{ KB}$$
2. Each write of a 3 KB item requires **3 WCUs**.
3. Multiply by items written per second:
   $$\text{Total WCU} = 200\text{ items/sec} \times 3\text{ WCU/item} = 600\text{ WCUs}$$
If writing via transactions (`TransactWriteItems`), capacity doubles to **1,200 WCUs**.
</details>

<details>
<summary><strong>Q4: What causes a "Hot Partition" in DynamoDB and how can it be mitigated?</strong></summary>

**Answer:**
A hot partition occurs when access patterns disproportionately target a single partition key value (e.g., 5,000 writes/sec directed to `PK: "2026-11-25"`). Because an individual physical partition is capped at **1,000 WCUs and 3,000 RCUs**, requests exceeding this threshold are throttled with `ProvisionedThroughputExceededException`, even if the table has thousands of provisioned capacity units available across other partitions.

**Mitigation Strategies**:
1. **Partition Salting**: Append a pseudo-random integer suffix (e.g., `Order#2026-11-25#0` through `Order#2026-11-25#9`) to distribute the writes across 10 distinct physical partitions.
2. **DynamoDB Accelerator (DAX)**: Implement in-memory microsecond caching for read-heavy hot keys.
</details>

<details>
<summary><strong>Q5: What is the maximum size of a single DynamoDB item, and how should larger objects be stored?</strong></summary>

**Answer:**
The hard limit for a single DynamoDB item is **400 KB** (including both attribute names and attribute values).
To handle larger files (e.g., PDF documents, high-res images, large JSON payloads):
Implement the **Claim Check Pattern**:
1. Upload the raw payload directly to an **Amazon S3** bucket.
2. Store the resulting S3 object URI (e.g., `s3://bucket/docs/report.pdf`) and document metadata in the DynamoDB item.
3. Applications query DynamoDB to retrieve the pointer and download the payload from S3.
</details>

<details>
<summary><strong>Q6: Why is `Scan` considered an anti-pattern in DynamoDB production applications?</strong></summary>

**Answer:**
A `Scan` operation reads every single item in the entire table sequentially from beginning to end.
- It consumes immense amounts of Read Capacity Units (RCUs), directly competing with customer traffic and driving up monthly billing costs.
- As the table grows from 1,000 items to 10 million items, the execution duration scales linearly, resulting in multi-second timeouts.
- Even if a `FilterExpression` is provided, the filter is applied *after* the entire dataset is read from disk into memory, meaning you are billed for reading the un-filtered data.
Production APIs should strictly use `Query` on Primary Keys or Global Secondary Indexes.
</details>

<details>
<summary><strong>Q7: How do DynamoDB conditional writes enable optimistic concurrency control?</strong></summary>

**Answer:**
Conditional writes allow an update or delete to proceed only if the item satisfies specific attribute criteria:
- An item includes a `Version` attribute (integer).
- When a client reads the item, it notes `Version = 1`.
- When updating, the client passes `ConditionExpression: "Version = :v"` with `:v = 1`, and sets `Version = Version + 1`.
- If another concurrent thread updated the item in the interim, the version in the database is already `2`, causing the conditional check to fail (`ConditionalCheckFailedException`). The client catches the exception, re-reads the fresh data, and retries.
</details>

<details>
<summary><strong>Q8: What is Point-In-Time Recovery (PITR) in DynamoDB and how does it function?</strong></summary>

**Answer:**
Point-In-Time Recovery provides continuous, automatic, microsecond-level backups for DynamoDB tables:
- Retains incremental change logs for the preceding **35 days**.
- Allows administrators to restore the table to any single second within that 35-day retention window.
- Restoration creates an entirely new table with the exact state of the data, secondary indexes, and settings at that timestamp, protecting organizations against human error, malicious code drops, or accidental `DeleteTable` disasters.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an On-Demand Table via CLI</strong></summary>

**Objective**: Provision an on-demand billing table with composite keys.

**Command**:
```bash
aws dynamodb create-table \
  --table-name LabProducts \
  --attribute-definitions \
    AttributeName=Category,AttributeType=S \
    AttributeName=ProductId,AttributeType=S \
  --key-schema \
    AttributeName=Category,KeyType=HASH \
    AttributeName=ProductId,KeyType=RANGE \
  --billing-mode PAY_PER_REQUEST
```
</details>

<details>
<summary><strong>Exercise 2: Insert Items with Condition Expressions</strong></summary>

**Objective**: Write an item ensuring it does not overwrite existing data.

**Command**:
```bash
aws dynamodb put-item \
  --table-name LabProducts \
  --item '{
    "Category": {"S": "Electronics"},
    "ProductId": {"S": "PROD-100"},
    "Price": {"N": "299.99"},
    "Title": {"S": "Wireless Headphones"}
  }' \
  --condition-expression "attribute_not_exists(ProductId)"
```
</details>

<details>
<summary><strong>Exercise 3: Execute a High-Performance Query</strong></summary>

**Objective**: Retrieve all products in the `Electronics` category.

**Command**:
```bash
aws dynamodb query \
  --table-name LabProducts \
  --key-condition-expression "Category = :cat" \
  --expression-attribute-values '{":cat": {"S": "Electronics"}}' \
  --return-consumed-capacity TOTAL
```
</details>

<details>
<summary><strong>Exercise 4: Enable Point-In-Time Recovery (PITR)</strong></summary>

**Objective**: Activate continuous backup protection.

**Command**:
```bash
aws dynamodb update-continuous-backups \
  --table-name LabProducts \
  --point-in-time-recovery-specification PointInTimeRecoveryEnabled=true
```
</details>

<details>
<summary><strong>Exercise 5: Calculate Consumed Capacity Units</strong></summary>

**Objective**: Observe exact capacity consumption using CLI flags.

**Command**:
```bash
aws dynamodb get-item \
  --table-name LabProducts \
  --key '{"Category": {"S": "Electronics"}, "ProductId": {"S": "PROD-100"}}' \
  --consistent-read \
  --return-consumed-capacity TOTAL
```
*Note the consumed capacity value returned in the response JSON.*
</details>

<details>
<summary><strong>Exercise 6: Test Optimistic Locking Failure</strong></summary>

**Objective**: Trigger a `ConditionalCheckFailedException` by passing an outdated version number.

**Command**:
```bash
# This should fail if Price is not 100.00
aws dynamodb update-item \
  --table-name LabProducts \
  --key '{"Category": {"S": "Electronics"}, "ProductId": {"S": "PROD-100"}}' \
  --update-expression "SET Price = :newPrice" \
  --condition-expression "Price = :oldPrice" \
  --expression-attribute-values '{":newPrice": {"N": "199.99"}, ":oldPrice": {"N": "100.00"}}'
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                             DynamoDB Architecture Cheat Sheet                         |
+---------------------------------------------------------------------------------------+
|  Dimension             | Limit / Calculation                                          |
|------------------------+--------------------------------------------------------------|
|  Max Item Size         | 400 KB (Strict hard limit)                                   |
|  1 WCU                 | 1 write / sec up to 1 KB (Transactional = 2 WCU)             |
|  1 RCU                 | 1 strong read / sec up to 4 KB (Eventually = 0.5 RCU)        |
|  Partition Limits      | 10 GB storage / 1,000 WCU / 3,000 RCU per physical partition |
|  Access Pattern        | Query on Primary Key (Avoid Scan in APIs)                    |
|  Consistency           | Eventually Consistent (default) vs Strongly Consistent       |
|  Disaster Recovery     | Point-In-Time Recovery (PITR: 35 days continuous)            |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon DynamoDB Developer Guide](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/Introduction.html)
- [How DynamoDB Partitions Data](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/HowItWorks.Partitions.html)
- [Understanding DynamoDB Read and Write Capacity Units](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/HowItWorks.ReadWriteCapacityMode.html)
- [Amazon DynamoDB Transactions Documentation](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/transaction-apis.html)
- [Optimistic Locking with Version Numbers in DynamoDB](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/DynamoDBMapper.OptimisticLocking.html)
