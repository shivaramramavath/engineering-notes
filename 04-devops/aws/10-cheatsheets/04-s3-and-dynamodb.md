---
topic: "Amazon S3 & DynamoDB Production Cheatsheet"
module: "10-cheatsheets"
track: "aws"
difficulty: "reference"
last-tested: "2026-11"
tags:
  - "cheatsheet"
  - "s3"
  - "dynamodb"
  - "storage"
  - "nosql"
  - "capacity-math"
---

# Amazon S3 & DynamoDB Production Cheatsheet

A high-density reference card for Amazon S3 operations, lifecycle rules, and DynamoDB capacity calculations and query patterns.

---

## 1. Amazon S3 Operations & Commands

```bash
# High-performance sync with delete and exclude patterns
aws s3 sync ./build s3://my-app-bucket/ --delete --exclude "*.git*" --cache-control "max-age=31536000"

# Generate temporary pre-signed download URL valid for 30 minutes
aws s3 presign s3://my-app-bucket/invoice.pdf --expires-in 1800

# Enable Default Encryption (KMS) across an existing bucket
aws s3api put-bucket-encryption --bucket my-app-bucket --server-side-encryption-configuration '{
  "Rules": [{ "ApplyServerSideEncryptionByDefault": { "SSEAlgorithm": "aws:kms", "KMSMasterKeyId": "alias/my-key" } }]
}'

# Enable S3 Block Public Access (All 4 settings True)
aws s3api put-public-access-block --bucket my-app-bucket --public-access-block-configuration \
    "BlockPublicAcls=true,IgnorePublicAcls=true,BlockPublicPolicy=true,RestrictPublicBuckets=true"
```

---

## 2. Amazon S3 Lifecycle Configuration Template

```json
{
  "Rules": [
    {
      "ID": "TransitionToTieredAndGlacier",
      "Status": "Enabled",
      "Filter": { "Prefix": "logs/" },
      "Transitions": [
        { "Days": 30, "StorageClass": "STANDARD_IA" },
        { "Days": 90, "StorageClass": "GLACIER_IR" },
        { "Days": 365, "StorageClass": "DEEP_ARCHIVE" }
      ],
      "Expiration": { "Days": 2555 },
      "NoncurrentVersionExpiration": { "NoncurrentDays": 30 }
    }
  ]
}
```

---

## 3. DynamoDB Capacity Unit Math & Limits

### 3.1 Read Capacity Unit (RCU) Math
- **1 RCU** = **One Strongly Consistent Read** per second for an item up to **4 KB**.
- **1 RCU** = **Two Eventually Consistent Reads** per second (up to 4 KB each).
- **Transactional Read**: Requires **2 RCUs** per 4 KB item.

$$\text{Strongly Consistent RCUs} = \text{Items/sec} \times \lceil \frac{\text{Item Size (KB)}}{4} \rceil$$
$$\text{Eventually Consistent RCUs} = \frac{\text{Items/sec} \times \lceil \frac{\text{Item Size (KB)}}{4} \rceil}{2}$$

*Example*: Reading 100 items/sec of size 10 KB with eventual consistency:
$\lceil 10 / 4 \rceil = 3$ blocks $\rightarrow (100 \times 3) / 2 = \mathbf{150\text{ RCUs}}$.

### 3.2 Write Capacity Unit (WCU) Math
- **1 WCU** = **One Standard Write** per second for an item up to **1 KB**.
- **Transactional Write**: Requires **2 WCUs** per 1 KB item.

$$\text{Standard WCUs} = \text{Items/sec} \times \lceil \frac{\text{Item Size (KB)}}{1} \rceil$$

*Example*: Writing 50 items/sec of size 3.5 KB:
$\lceil 3.5 / 1 \rceil = 4$ blocks $\rightarrow 50 \times 4 = \mathbf{200\text{ WCUs}}$.

### 3.3 Hard Limits to Remember
- **Maximum Item Size**: **400 KB** (including attribute names and binary values).
- **Partition Limit**: Max **10 GB data**, **1,000 WCU**, and **3,000 RCU** per physical partition.
- **Batch Operations**: `BatchGetItem` max **100 items / 16 MB**; `BatchWriteItem` max **25 items / 16 MB**.

---

## 4. DynamoDB Single-Table Key Naming Conventions

```text
Attribute Name | Example Value         | Purpose
---------------+-----------------------+-------------------------------------------------------------
PK             | USER#usr_1001         | Primary Partition Key (High cardinality entity hash)
SK             | METADATA#usr_1001     | Primary Sort Key (Entity type, relationship, or timestamp)
SK             | ORDER#ord_8820        | 1-to-Many Relationship (Customer orders partitioned by User)
GSI1PK         | STATUS#PENDING        | Inverted GSI Partition Key (Query across all pending items)
GSI1SK         | 2026-11-25T01:00:00Z  | Inverted GSI Sort Key (Temporal range query)
```

---

## 5. Python (Boto3) Query vs Scan Cheat Sheet

```python
import boto3
from boto3.dynamodb.conditions import Key, Attr

table = boto3.resource("dynamodb").Table("OrdersTable")

# 1. FAST QUERY: Targets specific Partition Key (O(1) partition lookup)
response = table.query(
    KeyConditionExpression=Key("PK").eq("CUSTOMER#44") & Key("SK").begins_with("ORDER#2026"),
    FilterExpression=Attr("status").eq("SHIPPED"), # Evaluated AFTER query read
    Limit=50
)
items = response.get("Items", [])

# 2. SLOW SCAN: Full Table Scan (O(N) reads across entire database - AVOID IN PRODUCTION)
scan_resp = table.scan(
    FilterExpression=Attr("status").eq("FAILED"),
    Limit=100
)
```
