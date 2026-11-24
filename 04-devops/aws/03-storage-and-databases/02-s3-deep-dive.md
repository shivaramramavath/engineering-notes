# Amazon S3 Advanced Architecture, Replication & Express One Zone

> **Module**: `03-storage-and-databases`  
> **Topic**: S3 Replication, WORM Object Lock, Express One Zone, and Multi-Region Access Points  
> **Target Audience**: Senior DevOps Engineers, Cloud Security Architects, Data Platform Engineers  
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

While basic Amazon S3 operations handle standard file uploads and downloads, enterprise environments require advanced data governance, regulatory compliance, multi-region disaster recovery, and extreme low-latency processing:
- Financial and healthcare institutions must adhere to strict **WORM (Write Once, Read Many)** regulatory mandates (e.g., SEC Rule 17a-4, HIPAA) preventing anyone—including root account administrators—from deleting or altering records for a mandatory retention period.
- Globally distributed platforms require asynchronous **Cross-Region Replication (CRR)** with predictable SLA guarantees to satisfy business continuity requirements.
- Modern high-performance compute workloads (e.g., training large language models with PyTorch, low-latency financial order matching, real-time analytics) find standard S3's multi-millisecond request latencies a bottleneck.

This deep dive covers the advanced capabilities of Amazon S3: **Cross-Region (CRR) & Same-Region Replication (SRR)**, **S3 Object Lock**, **S3 Express One Zone (High-Performance Directory Buckets)**, and **S3 Multi-Region Access Points (MRAP)**.

---

## 2. Core Concepts

### 2.1 S3 Cross-Region (CRR) & Same-Region Replication (SRR)
- **CRR**: Replicates objects asynchronously from a source bucket to a destination bucket in a different AWS Region. Used for geographic redundancy, compliance, and minimizing latency for multi-region applications.
- **SRR**: Replicates objects to a destination bucket within the *same* AWS Region. Used to aggregate logs across multiple accounts or isolate production and analytics environments.
- **Key Features**:
  - **S3 Replication Time Control (S3 RTC)**: Backed by a 99.9% SLA guaranteeing that 99.9% of newly uploaded objects are replicated within **15 minutes**.
  - **Bi-directional Replication**: Syncs mutations bidirectionally across active-active multi-region clusters.
  - **Ownership Overwrite**: Transmutes object ownership in the destination bucket to the destination account owner, preventing cross-account access lockouts.

### 2.2 S3 Object Lock & Compliance (WORM)
S3 Object Lock enforces Write Once, Read Many (WORM) storage, preventing objects from being deleted or overwritten:
- **Retention Modes**:
  - **Governance Mode**: Protects objects from deletion by standard users. Users with specific IAM permissions (`s3:BypassGovernanceRetention`) can override or delete the retention lock.
  - **Compliance Mode**: Strict, unbreakable protection. **No one**—not even the AWS root account or AWS Support—can delete or overwrite the object until the retention period expires.
- **Legal Hold**: A boolean lock (ON/OFF) applied independently of retention periods. Remains active until explicitly removed by an authorized principal.

### 2.3 S3 Express One Zone (High-Performance Directory Buckets)
Introduced at AWS re:Invent 2023, **S3 Express One Zone** is a purpose-built high-performance storage class:
- **Single-Digit Millisecond Latency**: Delivers up to **10x lower latency** than S3 Standard, responding in single-digit milliseconds consistently.
- **50% Lower Request Costs**: Request pricing is 50% cheaper than S3 Standard, making it ideal for query-intensive analytics (Amazon Athena, Apache Spark) and machine learning training loops.
- **Directory Buckets**: Objects are organized inside high-performance directory partitions rather than flat key namespaces. Pinned to a specific Availability Zone.

### 2.4 S3 Multi-Region Access Points (MRAP)
Provides a single global endpoint (`*.mrap.accesspoint.s3-global.amazonaws.com`) that routes client requests across multiple S3 buckets in different regions:
- Backed by **AWS Global Accelerator**, automatically routing traffic over the AWS global fiber backbone to the closest bucket with the lowest network latency.
- Features dynamic multi-region active-active or active-passive failover routing controls.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Advanced S3 Architecture & Ecosystem                       |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Global Multi-Region Client ]                                                   |
|             |                                                                     |
|             v (Anycast Routing via AWS Global Accelerator)                        |
|  +-----------------------------------------------------------------------------+  |
|  | S3 Multi-Region Access Point (MRAP)                                         |  |
|  | - Evaluates routing controls & regional latency                             |  |
|  +-----------------------------------------------------------------------------+  |
|             |                                                  |                  |
|             v (US East Traffic)                                v (EU West Traffic)|
|  +-------------------------------------+    +----------------------------------+  |
|  | Source Bucket: us-east-1            |    | Destination Bucket: eu-west-1    |  |
|  | - S3 Versioning: ENABLED            |    | - S3 Versioning: ENABLED         |  |
|  | - Object Lock: COMPLIANCE (7 Years) |    | - Object Ownership Overwritten   |  |
|  +-------------------------------------+    +----------------------------------+  |
|             |                                                  ^                  |
|             +--- S3 Replication Time Control (RTC: <15 min) ---+                  |
|                                                                                   |
|  -------------------------------------------------------------------------------  |
|  [ Ultra-Low-Latency Analytics & AI Compute Tier ]                                |
|                                                                                   |
|  [ EC2 Graviton / EKS Cluster (AZ: us-east-1a) ]                                  |
|             |                                                                     |
|             v (Sub-millisecond Direct Highway)                                    |
|  +-----------------------------------------------------------------------------+  |
|  | S3 Express One Zone Directory Bucket (Pinned to AZ: us-east-1a)             |  |
|  | - Storage Engine: Custom append-only high-IOPS hardware architecture       |  |
|  | - Latency: Consistent 1-3 milliseconds                                      |  |
|  | - Request Costs: 50% lower than S3 Standard                                 |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating an S3 Bucket with Object Lock Enabled
Object Lock can only be enabled during initial bucket creation:
```bash
aws s3api create-bucket \
  --bucket legal-compliance-vault-111122223333 \
  --region us-east-1 \
  --object-lock-enabled-for-bucket

# Configure default retention period: 5 years in Compliance Mode
aws s3api put-object-lock-configuration \
  --bucket legal-compliance-vault-111122223333 \
  --object-lock-configuration '{
    "ObjectLockRule": {
      "DefaultRetention": {
        "Mode": "COMPLIANCE",
        "Days": 1825
      }
    }
  }'
```

### 4.2 Creating an S3 Express One Zone Directory Bucket
Directory buckets follow the strict naming convention: `[bucket-base-name]--[az-id]--x-s3`.
```bash
aws s3api create-bucket \
  --bucket ai-training-cache--use1-az4--x-s3 \
  --create-bucket-configuration '{
    "Location": {
      "Type": "AvailabilityZone",
      "Name": "use1-az4"
    },
    "Bucket": {
      "DataRedundancy": "SingleAvailabilityZone",
      "Type": "Directory"
    }
  }'
```

---

## 5. Practical Examples

### Example 1: Cross-Region Replication (CRR) with S3 Replication Time Control (RTC)
Declarative Terraform configuration replicating from `us-east-1` to `us-west-2` with a 15-minute SLA guarantee.

```hcl
resource "aws_s3_bucket" "source" {
  bucket   = "source-data-lake-111122223333"
  provider = aws.us_east_1
}

resource "aws_s3_bucket_versioning" "source" {
  bucket   = aws_s3_bucket.source.id
  provider = aws.us_east_1
  versioning_configuration { status = "Enabled" }
}

resource "aws_s3_bucket" "destination" {
  bucket   = "dr-replica-lake-111122223333"
  provider = aws.us_west_2
}

resource "aws_s3_bucket_versioning" "destination" {
  bucket   = aws_s3_bucket.destination.id
  provider = aws.us_west_2
  versioning_configuration { status = "Enabled" }
}

resource "aws_iam_role" "replication" {
  name = "s3-replication-role"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "s3.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy" "replication" {
  name = "s3-replication-permissions"
  role = aws_iam_role.replication.id
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect   = "Allow"
        Action   = ["s3:GetReplicationConfiguration", "s3:ListBucket"]
        Resource = aws_s3_bucket.source.arn
      },
      {
        Effect   = "Allow"
        Action   = ["s3:GetObjectVersionForReplication", "s3:GetObjectVersionAcl", "s3:GetObjectVersionTagging"]
        Resource = "${aws_s3_bucket.source.arn}/*"
      },
      {
        Effect   = "Allow"
        Action   = ["s3:ReplicateObject", "s3:ReplicateDelete", "s3:ReplicateTags"]
        Resource = "${aws_s3_bucket.destination.arn}/*"
      }
    ]
  })
}

resource "aws_s3_bucket_replication_configuration" "replication" {
  bucket   = aws_s3_bucket.source.id
  role     = aws_iam_role.replication.arn
  provider = aws.us_east_1

  rule {
    id     = "crr-with-rtc"
    status = "Enabled"

    destination {
      bucket        = aws_s3_bucket.destination.arn
      storage_class = "STANDARD"

      replication_time {
        status = "Enabled"
        time { minutes = 15 }
      }

      metrics {
        status = "Enabled"
        event_threshold { minutes = 15 }
      }
    }
  }
}
```

### Example 2: Applying a Legal Hold to an S3 Object
Apply an indefinite legal hold to prevent deletion during ongoing litigation.

```bash
aws s3api put-object-legal-hold \
  --bucket legal-compliance-vault-111122223333 \
  --key "contracts/2026/vendor-agreement.pdf" \
  --legal-hold Status=ON

# Verify legal hold status
aws s3api get-object-legal-hold \
  --bucket legal-compliance-vault-111122223333 \
  --key "contracts/2026/vendor-agreement.pdf"
```

### Example 3: Benchmarking S3 Express One Zone vs S3 Standard Latency
Python script demonstrating single-digit millisecond read performance.

```python
import boto3
import time

s3 = boto3.client('s3')

EXPRESS_BUCKET = "ai-training-cache--use1-az4--x-s3"
STANDARD_BUCKET = "standard-ai-cache-111122223333"
OBJECT_KEY = "embeddings/vector-index.bin"

def benchmark_read(bucket, label):
    latencies = []
    for _ in range(50):
        start = time.perf_counter()
        s3.get_object(Bucket=bucket, Key=OBJECT_KEY)
        duration = (time.perf_counter() - start) * 1000 # convert to ms
        latencies.append(duration)
        
    avg = sum(latencies) / len(latencies)
    p99 = sorted(latencies)[int(len(latencies) * 0.99)]
    print(f"[{label}] Average: {avg:.2f}ms | P99: {p99:.2f}ms")

if __name__ == "__main__":
    benchmark_read(EXPRESS_BUCKET, "S3 Express One Zone")
    benchmark_read(STANDARD_BUCKET, "S3 Standard")
```

### Example 4: Creating an S3 Multi-Region Access Point (MRAP)
Configure global client routing across two regional buckets.

```bash
aws s3control create-multi-region-access-point \
  --account-id 111122223333 \
  --details '{
    "Name": "global-data-portal",
    "Regions": [
      {"Bucket": "source-data-lake-111122223333"},
      {"Bucket": "dr-replica-lake-111122223333"}
    ]
  }'
```

### Example 5: High-Performance Serverless SQL with Amazon Athena on S3
Query millions of compressed Parquet records stored in S3 directly without running database instances.

```sql
-- Create external table pointing to S3 Parquet dataset
CREATE EXTERNAL TABLE IF NOT EXISTS analytics_db.user_clicks (
  user_id string,
  event_time timestamp,
  action string,
  device_type string
)
STORED AS PARQUET
LOCATION 's3://source-data-lake-111122223333/clickstream/'
TBLPROPERTIES ("parquet.compress"="SNAPPY");

-- Query aggregated results with sub-second execution
SELECT 
  action, 
  COUNT(*) AS total_clicks 
FROM analytics_db.user_clicks 
WHERE event_time >= timestamp '2026-11-01 00:00:00'
GROUP BY action 
ORDER BY total_clicks DESC;
```

---

## 6. Best Practices

1. **Test Object Lock Retention in Governance Mode First**: Once an object is locked in **Compliance Mode**, it cannot be removed, reduced, or modified by anyone under any circumstances. Always pilot WORM policies in Governance Mode before committing to Compliance Mode.
2. **Deploy S3 Express One Zone in the Same AZ as Compute**: To achieve single-digit millisecond latency with S3 Express One Zone, your EC2 instances or EKS pods must reside in the exact same Availability Zone ID (e.g., `use1-az4`) as the directory bucket.
3. **Enable S3 RTC for Strict DR Compliance**: If your business disaster recovery plan specifies an RPO (Recovery Point Objective) under 30 minutes, always enable **S3 Replication Time Control (RTC)**. Standard CRR operates on a best-effort basis without timing guarantees.
4. **Use S3 Inventory Over HeadObject Crawlers**: For large buckets containing millions of objects, never run scripts that execute recursive `ListObjectsV2` or `HeadObject` API calls (which incurs high API fees and rate limiting). Enable **S3 Inventory** to generate daily or weekly CSV/Parquet reports of all bucket objects.
5. **Use Multi-Region Access Points for Global Applications**: Route client uploads and downloads through an MRAP endpoint. AWS Global Accelerator automatically circumvents public internet congestion by ingesting traffic at the nearest AWS edge PoP and routing over dedicated private fiber.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Locking Objects in Compliance Mode Accidentally** | Test buckets cannot be deleted, incurring storage bills for the entire retention term. | Test with short retention windows in **Governance Mode** with `s3:BypassGovernanceRetention` bypass rights. |
| **Assuming CRR Replicates Pre-Existing Objects** | Enabling CRR only replicates *new* objects uploaded *after* the rule is created. Old objects are not replicated. | Run an **S3 Batch Operations** replication job to replicate historical objects. |
| **Deploying S3 Express One Zone Cross-AZ** | Traffic incurs cross-AZ data transfer fees ($0.01/GB) and adds 1–2ms network latency, negating performance benefits. | Pin compute workloads and directory buckets to the exact same physical Availability Zone ID. |
| **Omitting Versioning on Replication Source/Dest** | S3 rejects the replication configuration; S3 Replication strictly requires Versioning on both buckets. | Enable `versioning_configuration { status = "Enabled" }` on both source and destination buckets. |
| **Missing Object Ownership Overwrite in Cross-Account CRR** | Destination account cannot access replicated objects because the source account still owns them. | Configure `Account` ownership transfer in the replication rule destination block. |
| **Using Standard S3 for High-Frequency Read/Write Loops** | Latency bottlenecks in ML training and high request costs on heavy GET/PUT cycles. | Migrate high-throughput transient datasets to **S3 Express One Zone**. |

---

## 8. Performance, Security & Cost Notes

### Express One Zone Performance Matrix
- **Request Latency**: Consistent 1–3 ms single-digit latency.
- **Request Throughput**: Capable of hundreds of thousands of transactions per second without request partitioning.
- **Request Pricing**: 50% cheaper request fees ($0.0025 per 1,000 PUT/POST vs $0.005 on S3 Standard).
- **Storage Pricing**: ~$0.16 / GB / month (higher storage fee reflecting specialized high-IOPS NVMe hardware).

### Security & WORM Compliance
- **SEC Rule 17a-4 / CFTC 1.31 Certification**: S3 Object Lock in Compliance Mode has been formally assessed by Cohasset Associates to satisfy strict financial regulatory WORM standards.
- **Delete Markers vs Permanent Deletion**: In Versioning-enabled buckets, a standard `DELETE` creates a Delete Marker. Under Object Lock, an explicit `DELETE` targeting a specific `VersionId` is denied by the S3 engine.

### Data Transfer Costs
- **CRR Data Transfer**: Cross-region replication incurs standard AWS inter-region data transfer egress fees ($0.02/GB) plus destination PUT request fees.
- **S3 RTC Surcharge**: Adds a small management charge (~$0.015 per GB replicated) to cover the 15-minute SLA guarantee.

---

## 9. Comparison Table

| Dimension | S3 Standard | S3 Express One Zone | S3 Object Lock (Compliance) | S3 Multi-Region Access Point |
|:---|:---|:---|:---|:---|
| **Primary Advantage** | 11 9's Multi-AZ durability | Single-digit millisecond latency | Unbreakable WORM compliance | Global low-latency routing |
| **AZ Redundancy** | Minimum 3 AZs | 1 Single AZ (Directory bucket) | Minimum 3 AZs | Global (Multi-Region) |
| **Request Cost** | Standard rate | 50% discount | Standard rate | Standard + MRAP routing fee |
| **Immutability** | Mutable | Mutable | Non-deletable until retention ends| N/A (Routing layer) |
| **Ideal Workload** | General object storage / Data lakes| AI/ML training, Spark, Athena cache| Regulatory records / Financial audit | Global distributed client APIs |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the operational difference between S3 Object Lock Governance Mode and Compliance Mode?</strong></summary>

**Answer:**
- **Governance Mode**: Prevents an object version from being overwritten or deleted by normal users, but allows authorized IAM users with the `s3:BypassGovernanceRetention` permission (and passing the `x-amz-bypass-governance-retention:true` header) to delete the object or alter the retention period. This allows administrators to clean up data if retention policies change.
- **Compliance Mode**: Completely enforces strict WORM guarantees. **No IAM user, no administrator, and not even the AWS root account** can delete the object version or decrease the retention duration until the configured period expires. AWS Support cannot bypass or delete objects in Compliance Mode under any circumstances.
</details>

<details>
<summary><strong>Q2: Why was S3 Express One Zone introduced, and how does its architecture differ from S3 Standard?</strong></summary>

**Answer:**
S3 Standard is optimized for 11 9's durability across 3+ Availability Zones, delivering 10–30 millisecond latency. For compute-intensive workloads like training machine learning models or distributed query engines (Spark, Presto, Athena), latency and high request costs are primary bottlenecks.

S3 Express One Zone introduces **Directory Buckets** pinned to a single Availability Zone:
- It uses specialized, ultra-high-speed solid-state hardware colocated alongside EC2 compute clusters.
- It delivers consistent **single-digit millisecond (1–3 ms) latency**, up to 10x faster than S3 Standard.
- It lowers request costs by **50%**, drastically reducing expenses for workloads performing millions of read/write operations per hour.
</details>

<details>
<summary><strong>Q3: Does enabling Cross-Region Replication (CRR) automatically replicate existing objects in the bucket?</strong></summary>

**Answer:**
No. By default, enabling CRR only replicates **new objects** uploaded *after* the replication configuration is activated. Pre-existing objects remain exclusively in the source bucket.

To replicate historical objects:
1. Enable CRR on the bucket.
2. Create and run an **S3 Batch Operations** replication job. S3 Batch Operations scans the source bucket, generates an inventory, and submits replication requests for all pre-existing object versions to the destination bucket.
</details>

<details>
<summary><strong>Q4: How does S3 Multi-Region Access Points (MRAP) route traffic globally?</strong></summary>

**Answer:**
MRAP provides a single global hostname backed by **AWS Global Accelerator**:
- When a client resolves the MRAP domain, anycast IP routing directs the connection to the closest AWS edge Point of Presence (PoP).
- Traffic enters the private AWS global fiber network immediately.
- Global Accelerator continuously measures network latency and health across the underlying regional buckets and routes the request to the regional bucket with the lowest latency.
- If a region experiences a severe outage, traffic automatically shifts to the healthy secondary region.
</details>

<details>
<summary><strong>Q5: What is S3 Replication Time Control (RTC) and what does its SLA guarantee?</strong></summary>

**Answer:**
S3 Replication Time Control (S3 RTC) is a feature of S3 Replication that guarantees:
- **99.9% of all new objects are replicated within 15 minutes** of upload.
- Backed by an official financial service level agreement (SLA) with service credits if AWS fails to meet the replication window.
- Provides real-time Amazon CloudWatch metrics tracking replication latency, pending bytes, and pending operations.
</details>

<details>
<summary><strong>Q6: How does an S3 Legal Hold differ from an S3 Retention Period?</strong></summary>

**Answer:**
- **Retention Period**: Specifies a fixed duration (e.g., 365 days) during which an object cannot be deleted. Once the retention timer expires, the object can be deleted normally.
- **Legal Hold**: A boolean flag (`ON` or `OFF`) with no expiration date. It remains active indefinitely until an authorized principal explicitly removes it. Legal holds can be placed on an object regardless of whether a retention period is configured, and both protections can operate concurrently (the object remains locked until the retention expires AND the legal hold is removed).
</details>

<details>
<summary><strong>Q7: How do you prevent ownership lockouts when replicating objects across different AWS accounts?</strong></summary>

**Answer:**
In a cross-account replication scenario, by default, replicated objects in the destination bucket remain owned by the source account, meaning the destination account owner cannot read or manage them without explicit permissions.

To prevent this:
In the replication configuration rule on the source bucket, configure the `Account` block with the destination AWS account ID and enable **Object Ownership Transfer** (`AccessControlTranslation: Owner=Destination`). S3 automatically changes the owner of the replica to the destination account upon replication.
</details>

<details>
<summary><strong>Q8: What is S3 Inventory and why should it be used instead of the `ListObjects` API?</strong></summary>

**Answer:**
Calling `ListObjectsV2` against a bucket with hundreds of millions of objects requires thousands of consecutive API calls, taking hours to execute, consuming compute resources, and risking S3 rate limiting and API charges.

**S3 Inventory** is an automated auditing feature that S3 executes internally:
- It generates a comprehensive daily or weekly manifest of all objects, metadata, encryption status, and version IDs.
- Outputs the inventory directly as an Apache Parquet, ORC, or CSV file into an S3 bucket.
- Query engines like Amazon Athena can query the Parquet manifest using SQL in seconds.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an S3 Bucket with Object Lock Enabled via CLI</strong></summary>

**Objective**: Provision a compliance bucket using the AWS CLI.

**Command**:
```bash
BUCKET_NAME="compliance-vault-$(date +%s)"
aws s3api create-bucket \
  --bucket "$BUCKET_NAME" \
  --region us-east-1 \
  --object-lock-enabled-for-bucket

aws s3api get-object-lock-configuration --bucket "$BUCKET_NAME"
```
</details>

<details>
<summary><strong>Exercise 2: Put an Object into Governance Mode Retention</strong></summary>

**Objective**: Lock a specific object version for 30 days.

**Commands**:
```bash
echo "Important document" > doc.txt
aws s3 cp doc.txt "s3://$BUCKET_NAME/doc.txt"

# Apply governance lock until 30 days from now
RETENTION_DATE=$(date -u -d "+30 days" +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date -u -v+30d +%Y-%m-%dT%H:%M:%SZ)

aws s3api put-object-retention \
  --bucket "$BUCKET_NAME" \
  --key "doc.txt" \
  --retention "Mode=GOVERNANCE,RetainUntilDate=$RETENTION_DATE"
```
</details>

<details>
<summary><strong>Exercise 3: Attempt Unauthorized Deletion of a Locked Object</strong></summary>

**Objective**: Observe S3 AccessDenied response when attempting to delete a locked object.

**Command**:
```bash
VERSION_ID=$(aws s3api list-object-versions --bucket "$BUCKET_NAME" --prefix "doc.txt" --query 'Versions[0].VersionId' --output text)

# This command should fail with AccessDenied
aws s3api delete-object --bucket "$BUCKET_NAME" --key "doc.txt" --version-id "$VERSION_ID"
```
</details>

<details>
<summary><strong>Exercise 4: Create an S3 Express One Zone Directory Bucket</strong></summary>

**Objective**: Provision a directory bucket pinned to a local Availability Zone ID.

**Command**:
```bash
# S3 Express requires strict name format: [name]--[az-id]--x-s3
aws s3api create-bucket \
  --bucket "fast-cache--use1-az4--x-s3" \
  --create-bucket-configuration '{"Location": {"Type": "AvailabilityZone", "Name": "use1-az4"}, "Bucket": {"DataRedundancy": "SingleAvailabilityZone", "Type": "Directory"}}'
```
</details>

<details>
<summary><strong>Exercise 5: Configure S3 Inventory for Daily Parquet Generation</strong></summary>

**Objective**: Set up an automated daily inventory schedule.

**Command**:
```bash
aws s3api put-bucket-inventory-configuration \
  --bucket "$BUCKET_NAME" \
  --id "DailyInventory" \
  --inventory-configuration '{
    "Id": "DailyInventory",
    "IsEnabled": true,
    "Destination": {
      "S3BucketDestination": {
        "AccountId": "111122223333",
        "Bucket": "arn:aws:s3:::audit-reports-bucket",
        "Format": "Parquet",
        "Prefix": "inventory"
      }
    },
    "Schedule": {"Frequency": "Daily"},
    "IncludedObjectVersions": "All",
    "OptionalFields": ["Size", "LastModifiedDate", "StorageClass", "ETag", "ReplicationStatus", "EncryptionStatus"]
  }'
```
</details>

<details>
<summary><strong>Exercise 6: Query S3 Data Lake via Amazon Athena</strong></summary>

**Objective**: Execute serverless SQL against Parquet data in S3 using the AWS CLI.

**Command**:
```bash
aws athena start-query-execution \
  --query-string "SELECT COUNT(*) FROM analytics_db.user_clicks" \
  --result-configuration "OutputLocation=s3://$BUCKET_NAME/athena-results/"
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                              Advanced S3 Architecture Matrix                          |
+---------------------------------------------------------------------------------------+
|  Feature               | Target Purpose                    | Key Constraint           |
|------------------------+-----------------------------------+--------------------------|
|  Replication (CRR/SRR) | Cross-region DR & geo-redundancy  | Requires Versioning ON   |
|  Replication Time Ctrl | 15-minute 99.9% SLA guarantee     | Hourly surcharge per GB  |
|  Object Lock (Compl)   | Unbreakable WORM legal compliance | Non-reversible even root |
|  Express One Zone      | 1-3ms latency for AI/Analytics    | Single AZ (Directory)    |
|  Multi-Region Access Pt| Global Anycast routing via GA     | Requires regional buckets|
|  S3 Inventory          | Daily/Weekly Parquet manifest     | Replaces ListObjects     |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon S3 Replication User Guide](https://docs.aws.amazon.com/AmazonS3/latest/userguide/replication.html)
- [Amazon S3 Object Lock & WORM Compliance](https://docs.aws.amazon.com/AmazonS3/latest/userguide/object-lock.html)
- [What is Amazon S3 Express One Zone?](https://docs.aws.amazon.com/AmazonS3/latest/userguide/s3-express-one-zone.html)
- [Configuring S3 Multi-Region Access Points](https://docs.aws.amazon.com/AmazonS3/latest/userguide/MultiRegionAccessPoints.html)
- [Querying AWS S3 Data Lakes with Amazon Athena](https://docs.aws.amazon.com/athena/latest/ug/what-is.html)
