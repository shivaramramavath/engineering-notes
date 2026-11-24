---
topic: "Data Lake and Modern Data Architecture Pattern"
module: "07-architecture-patterns"
track: "aws"
last-tested: "2026-11"
prerequisites:
  - "03-storage-and-databases/01-s3.md"
  - "03-storage-and-databases/03-rds-and-aurora.md"
  - "06-operations/07-security-services.md"
complexity: "advanced"
tags:
  - "data-lake"
  - "s3"
  - "glue"
  - "athena"
  - "lake-formation"
  - "redshift"
  - "analytics"
---

# Data Lake and Modern Data Architecture Pattern

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

A modern Data Lake architecture on AWS decouples storage from compute, allowing organizations to ingest, store, govern, and analyze massive volumes of structured, semi-structured, and unstructured data at petabyte-to-exabyte scale. By establishing **Amazon S3** as the central storage repository, data is stored once in open-standard columnar formats (such as Apache Parquet or Apache Iceberg) and made available simultaneously to diverse analytical engines—including **Amazon Athena** (serverless interactive SQL), **Amazon EMR** (Apache Spark/Flink), **Amazon Redshift** (data warehousing and Redshift Spectrum), and **Amazon SageMaker** (machine learning).

### Why the AWS Lakehouse Pattern?
- **Decoupled Storage & Compute**: Scale storage independently on durable, inexpensive S3 tiers (\$0.023/GB down to \$0.00099/GB in Glacier Deep Archive) while spinning compute up and down on-demand.
- **Open Standards**: Avoid vendor lock-in by maintaining data in Apache Parquet, ORC, Avro, or transactional table formats (Iceberg, Delta Lake, Hudi) rather than proprietary database storage engines.
- **Unified Governance (AWS Lake Formation)**: Enforce centralized, fine-grained access control (cell, row, and column-level security) across multiple analytical engines through a single control plane.
- **Multi-Engine Flexibility**: Run fast SQL queries with Athena, heavy batch transformations with AWS Glue/EMR, and business intelligence dashboards with QuickSight on the identical underlying dataset.

---

## 2. Core Concepts

### The 3 Medallion Storage Layers
Modern AWS data lakes organize data across three architectural tiers within Amazon S3:

1. **Bronze Layer (Raw Ingestion Zone)**:
   - Preserves source data in its native format (raw JSON, CSV, API payloads, Kinesis streams) without transformation.
   - Immutable write-once audit history partitioned by ingestion date (`year=YYYY/month=MM/day=DD`).
2. **Silver Layer (Cleansed / Enriched Zone)**:
   - Data is cleansed, deduplicated, schema-validated, and converted into compressed columnar format (Apache Parquet with Snappy or ZSTD compression).
   - Serves as the primary operational querying zone for data engineers and ad-hoc Athena queries.
3. **Gold Layer (Curated / Business Zone)**:
   - Aggregated, domain-modeled, and business-ready datasets (e.g., star/snowflake schemas).
   - Optimized for executive reporting, BI dashboards, high-speed Redshift queries, and feature stores for machine learning models.

### Key AWS Data Services
- **AWS Glue Data Catalog**: A managed, Hive-compatible metadata repository storing table schemas, partition locations, and data statistics.
- **AWS Glue Crawlers**: Automated processes that inspect S3 object structures, infer schemas, and populate catalog tables.
- **Amazon Athena**: Serverless interactive query service powered by Presto/Trino, charging \$5.00 per TB of data scanned.
- **AWS Lake Formation**: Centralized data governance layer managing table registration, Tag-Based Access Control (LF-TBAC), and cross-account data sharing via AWS Resource Access Manager (RAM).
- **Amazon Redshift Spectrum**: Query open Parquet data directly in S3 from an existing Redshift data warehouse cluster without loading data into cluster storage.

---

## 3. How It Works Internally

```
+----------------------------------------------------------------------------------------------------+
|                                MODERN DATA LAKE ARCHITECTURE ON AWS                                |
+----------------------------------------------------------------------------------------------------+

  [ Batch / Streaming Sources ]
  (RDS, App Logs, IoT Kinesis)
              |
              v
  +--------------------------------------------------------------------------------------------------+
  | INGESTION LAYER                                                                                  |
  |  - Amazon Kinesis Firehose / MSK / AppFlow / AWS Glue ETL                                        |
  +--------------------------------------------------------------------------------------------------+
              |
              v
  +--------------------------------------------------------------------------------------------------+
  | STORAGE LAYER (Amazon S3 Lakehouse)                                                              |
  |                                                                                                  |
  |  +-----------------------+     +-----------------------+     +-----------------------+           |
  |  | BRONZE (Raw Zone)     | --> | SILVER (Cleansed)     | --> | GOLD (Curated Zone)   |           |
  |  | raw JSON / CSV dumps  |     | Parquet + Snappy      |     | Aggregated Iceberg    |           |
  |  | partitioned by date   |     | Schema-validated      |     | Business Star-Schema  |           |
  |  +-----------------------+     +-----------------------+     +-----------------------+           |
  +--------------------------------------------------------------------------------------------------+
              ^                                ^                                ^
              |                                |                                |
  +--------------------------------------------------------------------------------------------------+
  | GOVERNANCE & CATALOG (AWS Lake Formation & AWS Glue Data Catalog)                                 |
  |  - Centralized Schema Catalog & Partition Metadata                                              |
  |  - Column-level & Row-level Security / Tag-Based Access Control (LF-TBAC)                       |
  +--------------------------------------------------------------------------------------------------+
              |                                |                                |
              v                                v                                v
  +-----------------------+        +-----------------------+        +-----------------------+
  | Amazon Athena         |        | Amazon EMR Serverless |        | Amazon Redshift       |
  | (Interactive SQL)     |        | (Spark Data Science)  |        | (Spectrum Analytics)  |
  +-----------------------+        +-----------------------+        +-----------------------+
```

### Query Execution Flow with Lake Formation & Athena
1. An analyst issues an SQL query via Amazon Athena: `SELECT user_id, email, transaction_amount FROM gold_orders WHERE country = 'US'`.
2. Athena queries the **AWS Lake Formation** endpoint to verify the user's IAM and LF-TBAC permissions.
3. Lake Formation evaluates fine-grained policies: if the analyst is restricted from viewing Personally Identifiable Information (PII), Lake Formation strips the `email` column or masks it.
4. Lake Formation generates temporary, credential-scoped S3 data access tokens for Athena.
5. Athena directly reads the underlying Parquet splits from Amazon S3, executes the distributed query across its fleet, and returns the result to the user.

---

## 4. Syntax & Usage

### 4.1 Terraform: S3 Lakehouse Bucket, Glue Catalog Database, and Table

```hcl
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.40"
    }
  }
}

provider "aws" {
  region = "us-east-1"
}

# S3 Lakehouse Storage Bucket
resource "aws_s3_bucket" "lakehouse" {
  bucket = "enterprise-modern-lakehouse-data"
}

resource "aws_s3_bucket_server_side_encryption_configuration" "lakehouse_encryption" {
  bucket = aws_s3_bucket.lakehouse.id

  rule {
    apply_server_side_encryption_by_default {
      sse_algorithm = "AES256"
    }
  }
}

# Glue Data Catalog Database
resource "aws_glue_catalog_database" "analytics_db" {
  name        = "ecommerce_analytics"
  description = "Central metadata database for e-commerce data lake"
}

# Glue Data Catalog Table (Parquet Format)
resource "aws_glue_catalog_table" "silver_orders" {
  name          = "orders"
  database_name = aws_glue_catalog_database.analytics_db.name
  table_type    = "EXTERNAL_TABLE"

  parameters = {
    EXTERNAL              = "TRUE"
    "parquet.compression" = "SNAPPY"
  }

  storage_descriptor {
    location      = "s3://${aws_s3_bucket.lakehouse.bucket}/silver/orders/"
    input_format  = "org.apache.hadoop.hive.ql.io.parquet.MapredParquetInputFormat"
    output_format = "org.apache.hadoop.hive.ql.io.parquet.MapredParquetOutputFormat"

    ser_de_info {
      name                  = "ParquetSerDe"
      serialization_library = "org.apache.hadoop.hive.ql.io.parquet.serde.ParquetHiveSerDe"
      parameters = {
        "serialization.format" = "1"
      }
    }

    columns {
      name = "order_id"
      type = "string"
    }
    columns {
      name = "customer_id"
      type = "string"
    }
    columns {
      name = "amount"
      type = "double"
    }
    columns {
      name = "status"
      type = "string"
    }
  }

  # Partitioning columns
  partition_keys {
    name = "year"
    type = "string"
  }
  partition_keys {
    name = "month"
    type = "string"
  }
}
```

---

## 5. Practical Examples

### Example 1: AWS Glue PySpark ETL Script Converting Raw JSON to Parquet

```python
import sys
from awsglue.transforms import *
from awsglue.utils import getResolvedOptions
from pyspark.context import SparkContext
from awsglue.context import GlueContext
from awsglue.job import Job
from pyspark.sql.functions import year, month, to_date

# Initialize Glue Context
args = getResolvedOptions(sys.argv, ["JOB_NAME", "SOURCE_PATH", "TARGET_PATH"])
sc = SparkContext()
glueContext = GlueContext(sc)
spark = glueContext.spark_session
job = Job(glueContext)
job.init(args["JOB_NAME"], args)

source_path = args["SOURCE_PATH"]
target_path = args["TARGET_PATH"]

# Read Raw JSON data from Bronze S3 zone
df_raw = spark.read.json(source_path)

# Data transformation, cleansing, and date extraction
df_cleansed = df_raw.filter(df_raw["amount"].isNotNull()) \
                    .filter(df_raw["amount"] > 0) \
                    .withColumn("order_date", to_date(df_raw["timestamp"])) \
                    .withColumn("year", year("order_date")) \
                    .withColumn("month", month("order_date"))

# Write to Silver Zone in Apache Parquet format partitioned by Year and Month
df_cleansed.write \
    .mode("append") \
    .partitionBy("year", "month") \
    .parquet(target_path, compression="snappy")

job.commit()
```

### Example 2: Querying the Data Lake via Amazon Athena (Python SDK Boto3)

```python
import time
import boto3

athena = boto3.client("athena", region_name="us-east-1")

def run_athena_query(query: str, database: str, output_s3: str):
    response = athena.start_query_execution(
        QueryString=query,
        QueryExecutionContext={"Database": database},
        ResultConfiguration={"OutputLocation": output_s3}
    )
    query_id = response["QueryExecutionId"]
    print(f"Started query ID: {query_id}")

    # Poll execution status
    while True:
        status_resp = athena.get_query_execution(QueryExecutionId=query_id)
        state = status_resp["QueryExecution"]["Status"]["State"]
        if state in ["SUCCEEDED", "FAILED", "CANCELLED"]:
            break
        time.sleep(1)

    if state == "SUCCEEDED":
        scanned_bytes = status_resp["QueryExecution"]["Statistics"]["DataScannedInBytes"]
        print(f"Query Succeeded! Data Scanned: {scanned_bytes / (1024 * 1024):.2f} MB")
        return athena.get_query_results(QueryExecutionId=query_id)
    else:
        reason = status_resp["QueryExecution"]["Status"].get("StateChangeReason", "Unknown")
        raise RuntimeError(f"Athena query failed: {reason}")

if __name__ == "__main__":
    SQL = "SELECT year, count(*) as total_orders, sum(amount) as revenue FROM orders GROUP BY year"
    run_athena_query(SQL, "ecommerce_analytics", "s3://enterprise-modern-lakehouse-data/athena-results/")
```

### Example 3: Defining Column-Level Security in AWS Lake Formation via CLI

```bash
# Grant access to Data Analyst role, excluding sensitive PII columns (e.g. credit_card, ssn)
aws lakeformation grant-permissions \
    --principal DataLakePrincipalIdentifier="arn:aws:iam::123456789012:role/DataAnalystRole" \
    --resource '{
        "TableWithColumns": {
            "DatabaseName": "ecommerce_analytics",
            "Name": "orders",
            "ColumnWildcard": {
                "ExcludedColumnNames": ["customer_credit_card", "ssn"]
            }
        }
    }' \
    --permissions "SELECT" \
    --permissions-with-grant-option "SELECT"
```

### Example 4: Creating an Apache Iceberg Table in Athena

```sql
-- Creating a transactional Iceberg table in Amazon Athena
CREATE TABLE ecommerce_analytics.iceberg_orders (
    order_id string,
    customer_id string,
    order_amount double,
    order_ts timestamp
)
PARTITIONED BY (days(order_ts))
LOCATION 's3://enterprise-modern-lakehouse-data/gold/iceberg_orders/'
TBLPROPERTIES (
    'table_type'='ICEBERG',
    'format'='parquet',
    'write_compression'='snappy'
);

-- Performing ACID MERGE / UPSERT operation natively in Athena
MERGE INTO ecommerce_analytics.iceberg_orders target
USING staging_orders source
ON target.order_id = source.order_id
WHEN MATCHED THEN
    UPDATE SET order_amount = source.order_amount, order_ts = source.order_ts
WHEN NOT MATCHED THEN
    INSERT (order_id, customer_id, order_amount, order_ts)
    VALUES (source.order_id, source.customer_id, source.order_amount, source.order_ts);
```

### Example 5: Redshift Spectrum External Schema Definition

```sql
-- Connect Amazon Redshift directly to the Glue Data Catalog
CREATE EXTERNAL SCHEMA spectrum_lake
FROM DATA CATALOG
DATABASE 'ecommerce_analytics'
IAM_ROLE 'arn:aws:iam::123456789012:role/RedshiftSpectrumRole'
CREATE EXTERNAL DATABASE IF NOT EXISTS;

-- Query S3 data lake files directly joined with internal Redshift warehouse tables
SELECT 
    w.warehouse_name,
    count(s.order_id) as s3_orders_count,
    sum(s.amount) as total_revenue
FROM spectrum_lake.orders s
JOIN internal_warehouses w ON s.warehouse_id = w.id
WHERE s.year = '2026'
GROUP BY w.warehouse_name;
```

---

## 6. Best Practices

1. **Always Use Columnar Formats (Apache Parquet or ORC)**: Columnar compression reduces storage by 75-90% and accelerates Athena/Redshift Spectrum queries by reading only requested columns.
2. **Partition Data by Query Access Patterns**: Partition on high-cardinality, frequently filtered fields (e.g., `year`, `month`, `region`). Avoid over-partitioning into tiny files (< 128 MB).
3. **Target Optimal S3 File Sizes (128 MB to 512 MB)**: Query engines incur high HTTP overhead opening millions of tiny files. Compact small files using AWS Glue or Apache Iceberg compaction.
4. **Enforce Centralized Governance with AWS Lake Formation**: Replace fragmented S3 bucket policies with Lake Formation's Tag-Based Access Control (LF-TBAC) to govern row/column permissions across multiple teams.
5. **Implement S3 Lifecycle Policies on Raw Ingestion Zones**: Transition Bronze raw JSON/CSV files to S3 Standard-IA after 30 days and Glacier Deep Archive after 90 days.
6. **Use Partition Projection in Athena**: For highly partitioned datasets, enable Athena Partition Projection to avoid expensive `GetPartitions` calls to the Glue Data Catalog.

---

## 7. Common Mistakes

| Mistake | Why It Hurts | Correct Approach |
|:---|:---|:---|
| **Querying raw JSON/CSV files with Athena** | Athena charges \$5.00/TB scanned; scanning uncompressed raw text scans 100% of the dataset, resulting in massive bills. | Convert raw files to compressed Parquet; Athena scans only queried columns. |
| **The "Small Files Problem" (millions of 10KB files)** | Opening millions of S3 files causes massive metadata overhead and causes Athena/EMR queries to time out. | Compact files into 128MB-512MB chunks using Glue ETL or Iceberg maintenance jobs. |
| **Over-partitioning (e.g., partitioning by minute)** | Generates millions of empty directories; Glue Crawlers and Athena catalog lookups slow down drastically. | Limit partitioning to 1-3 coarse dimensions (`year/month/day`). |
| **Managing S3 permissions purely via S3 Bucket Policies** | Complex bucket policies cannot enforce column masking, row filtering, or fine-grained table access for analytical users. | Implement **AWS Lake Formation** permissions. |
| **Running Glue Crawlers on production schedule before every query** | Crawlers take minutes to hours to scan millions of S3 objects, delaying analytical pipelines. | Use Partition Projection or update the Glue Catalog programmatically via SDK in ETL jobs. |
| **Storing unencrypted PII in Data Lake buckets** | Breaches compliance mandates (GDPR, HIPAA, SOC 2) and exposes sensitive data to unauthorized internal analysts. | Enable AWS KMS SSE-KMS, apply Lake Formation column exclusion/masking, and mask sensitive fields in Bronze-to-Silver ETL. |

---

## 8. Performance, Security & Cost Notes

### Performance Optimization
- **Partition Pruning**: Querying `WHERE year = '2026'` allows Athena to skip all S3 prefixes for preceding years, reducing scanned bytes by up to 95%.
- **Pushdown Predicates**: Analytical engines push SQL filtering down to the Parquet reader level, discarding non-matching row groups before loading data into memory.

### Security Controls
- **Lake Formation Tag-Based Access Control (LF-TBAC)**: Assign tags (e.g., `Confidentiality=Restricted`, `DataCategory=PII`) to databases, tables, and columns, granting IAM principals access based on matching tags.
- **S3 Block Public Access**: Enable at the AWS Account level across all Data Lake storage buckets.

### Cost Analysis
- **Amazon S3 Standard**: \$0.023 per GB/month.
- **Amazon Athena**: \$5.00 per TB of data scanned (rounded up to nearest 10 MB per query).
- **AWS Glue ETL**: \$0.44 per DPU-hour (billed per second).
- **AWS Lake Formation**: Free service tier (charges apply only for underlying Glue Catalog, Athena, and S3 usage).

---

## 9. Comparison Table

| Analytical Service | Query Model | Latency Profile | Maintenance | Primary Strengths |
|:---|:---|:---|:---|:---|
| **Amazon Athena** | Serverless Presto/Trino SQL | Seconds to Minutes | Zero (Pay per query scan) | Ad-hoc data discovery, serverless analytics, S3 lake queries |
| **Amazon Redshift** | Massively Parallel Processing (MPP) | Sub-second to Seconds | Medium (Cluster or Serverless) | High-concurrency enterprise BI, complex joins, data warehousing |
| **Amazon EMR Serverless** | Distributed Spark / Flink | Minutes to Hours | Low (Managed Spark runtime) | Heavy batch ETL, machine learning feature engineering, graph processing |
| **AWS Glue ETL** | Serverless Spark / Python | Minutes to Hours | Low (Jobs scheduled / triggered) | Data cleansing, schema cataloging, automated event pipelines |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does columnar storage (e.g., Apache Parquet) reduce Athena query costs and improve performance?</strong></summary>

Amazon Athena charges \$5.00 per terabyte of data scanned. In row-based formats (CSV or JSON), a query selecting only two columns (`SELECT customer_id, amount FROM orders`) must read every single byte of every row from disk. In columnar storage (Apache Parquet):
1. Data is organized vertically by columns into row groups.
2. Athena reads only the specific byte offsets corresponding to the requested columns (`customer_id` and `amount`), completely skipping unneeded columns.
3. Because data within each column shares the same datatype, compression algorithms (Snappy, ZSTD) achieve compression ratios of 75-90%, further slashing bytes transferred over the network and directly reducing cost by up to 90%.
</details>

<details>
<summary><strong>2. What is the "Small Files Problem" in an S3 Data Lake, and how is it resolved?</strong></summary>

When data streams into S3 in real-time (e.g., Kinesis Firehose buffering every 60 seconds), millions of tiny files (10 KB to 500 KB) accumulate across prefixes. Analytical engines (Athena, Spark, Presto) must make separate HTTP `GET` requests to S3 to open each file, read metadata footers, and decompress blocks. This causes high network I/O latency, connection pool exhaustion, and query timeouts. It is resolved by:
1. Increasing buffering thresholds on Kinesis Firehose (e.g., 64 MB to 128 MB).
2. Running compaction jobs using AWS Glue or Spark to coalesce small files into target sizes of 128 MB to 512 MB.
3. Adopting Apache Iceberg or Delta Lake, which provide automated compaction and snapshot management.
</details>

<details>
<summary><strong>3. How does AWS Lake Formation implement Column-Level and Row-Level security?</strong></summary>

Traditional S3 IAM policies grant or deny access at the S3 bucket or prefix level. AWS Lake Formation acts as a centralized authorization layer. When an analytical engine (such as Athena or Redshift Spectrum) executes a query:
1. It requests access tokens from Lake Formation on behalf of the querying IAM user.
2. Lake Formation evaluates the user's permissions in the Glue Data Catalog.
3. For **Column-Level Security**, Lake Formation excludes ungranted column names from the schema view and data splits.
4. For **Row-Level Security**, Lake Formation evaluates row-filter expressions (e.g., `country = 'US'`) and filters out unauthorized data records before returning storage descriptors, guaranteeing zero leakage of restricted rows.
</details>

<details>
<summary><strong>4. What is Athena Partition Projection and when should you use it?</strong></summary>

By default, Athena queries metadata from the AWS Glue Data Catalog to locate partitions. For tables with millions of partitions (e.g., partitioned by year, month, day, hour, and device ID), catalog lookups take several seconds or time out. **Partition Projection** allows you to configure partition rules (e.g., numeric ranges, date patterns) directly in table properties. Athena calculates partition S3 paths mathematically in-memory rather than querying the Glue Data Catalog, speeding up query initiation by orders of magnitude.
</details>

<details>
<summary><strong>5. Explain the differences between Amazon Athena and Amazon Redshift Spectrum.</strong></summary>

- **Amazon Athena**: Fully serverless query engine powered by Presto/Trino. There is no cluster to provision, size, or maintain. Queries are billed entirely on-demand (\$5.00/TB scanned). Ideal for ad-hoc queries, data discovery, and serverless data lake access.
- **Amazon Redshift Spectrum**: A feature of Amazon Redshift that allows a provisioned or serverless Redshift data warehouse cluster to query external tables directly in S3. It is ideal when you need to join petabytes of historical S3 data lake files directly with hot, indexed internal relational data stored in Redshift local storage.
</details>

<details>
<summary><strong>6. Why are transactional table formats like Apache Iceberg replacing plain Parquet in modern AWS data lakes?</strong></summary>

Plain Parquet on S3 lacks ACID transactions; concurrent writes risk dirty reads, and updating or deleting rows requires rewriting entire files or prefixes. **Apache Iceberg** introduces a transactional metadata layer on top of Parquet files in S3, providing:
1. Full ACID compliance (atomic commits, concurrent writers).
2. Native SQL `UPDATE`, `DELETE`, and `MERGE INTO` operations directly in Athena.
3. Time Travel (querying historical table states as of a specific timestamp or snapshot ID).
4. Schema evolution and partition evolution without rewriting existing datasets.
</details>

<details>
<summary><strong>7. How do you implement automated data deduplication in an S3 Data Lake?</strong></summary>

Data deduplication is commonly implemented in the transition from the Bronze (Raw) to Silver (Cleansed) layer. In an AWS Glue PySpark job, data engineers window records by primary business key and order by timestamp descending:
```python
window = Window.partitionBy("order_id").orderBy(col("updated_at").desc())
df_deduped = df.withColumn("rank", row_number().over(window)).filter(col("rank") == 1).drop("rank")
```
Alternatively, using Apache Iceberg, upserts are handled natively via the `MERGE INTO` statement.
</details>

<details>
<summary><strong>8. What is the role of AWS Glue Crawlers and why should they be used selectively in production?</strong></summary>

AWS Glue Crawlers connect to source data stores (S3, RDS, DynamoDB), infer data schemas from sample files, detect partitions, and register tables in the Glue Data Catalog. While excellent for initial data onboarding and prototyping, running crawlers on production schedules across petabyte-scale lakes is inefficient and expensive because crawlers must list millions of S3 objects. In production, schemas and partitions should be created and updated deterministically using IaC (Terraform) or directly by ETL pipeline jobs via Glue APIs.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Challenge 1: Deploy a Data Lake Bucket and Glue Database with Terraform</strong></summary>

**Goal**: Provision an S3 bucket with SSE-S3 encryption and create a Glue Catalog Database named `sales_lake`.
**Verification**: Verify in the AWS console that the database exists and can be seen inside the Amazon Athena query editor.
</details>

<details>
<summary><strong>Challenge 2: Query Sample Parquet Data using Amazon Athena</strong></summary>

**Goal**: Upload an Apache Parquet file to S3, create an external table definition in Athena, and execute a `SELECT COUNT(*)` query.
**Verification**: Confirm that the query succeeds and observe the `Data scanned` metric in the Athena console.
</details>

<details>
<summary><strong>Challenge 3: Configure AWS Lake Formation Column Masking</strong></summary>

**Goal**: Register an S3 path in Lake Formation, grant `SELECT` permissions to an IAM role on a Glue table, and restrict access to a sensitive column.
**Verification**: Assume the restricted IAM role and execute an Athena query attempting to select the restricted column; verify that access is denied or the column is excluded.
</details>

<details>
<summary><strong>Challenge 4: Deploy an Apache Iceberg Table in Athena</strong></summary>

**Goal**: Create an Apache Iceberg table in Athena, insert sample rows, and execute an `UPDATE` statement changing an attribute value.
**Verification**: Verify that the record updates successfully without recreating the table or rewriting unaffected files.
</details>

<details>
<summary><strong>Challenge 5: Configure Athena Partition Projection</strong></summary>

**Goal**: Define a table partitioned by date (`year/month/day`) using Partition Projection properties instead of manual `ALTER TABLE ADD PARTITION`.
**Verification**: Query a specific date prefix without running Glue crawlers or partition repair, confirming immediate query execution.
</details>

<details>
<summary><strong>Challenge 6: Set Up S3 Lifecycle Transitions for Cold Lakehouse Data</strong></summary>

**Goal**: Configure an S3 lifecycle rule on the Bronze raw bucket to transition objects to S3 Glacier Instant Retrieval after 60 days.
**Verification**: Verify using `aws s3api get-bucket-lifecycle-configuration` that the rule is active and targets the correct prefix.
</details>

---

## 12. Summary / Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
|                                MODERN DATA LAKE DESIGN CHEAT SHEET                                 |
+----------------------------------------------------------------------------------------------------+
| Architecture Tier | Storage Format         | Purpose                                               |
+-------------------+------------------------+-------------------------------------------------------+
| Bronze Zone       | Raw JSON / CSV / Avro  | Immutable audit capture, partitioned by ingestion date|
| Silver Zone       | Apache Parquet + Snappy| Cleansed, validated, typed, partitioned by business keys|
| Gold Zone         | Parquet / Iceberg      | Aggregated metrics, dimensional models, BI ready      |
+-------------------+------------------------+-------------------------------------------------------+
| Service           | Role                                                                           |
| S3 Storage        | Central decoupled lakehouse storage ($0.023/GB down to $0.00099/GB)            |
| Glue Data Catalog | Central Hive-compatible metadata schema repository                             |
| AWS Lake Formation| Centralized governance, column/row-level security, LF-TBAC                     |
| Amazon Athena     | Serverless interactive SQL query engine ($5.00/TB scanned)                     |
| File Optimization | Target file sizes between 128 MB and 512 MB to prevent Small Files Problem    |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Prescriptive Guidance: Building a Modern Data Architecture on AWS](https://docs.aws.amazon.com/prescriptive-guidance/latest/modern-data-architecture/welcome.html)
- [AWS Lake Formation Developer Guide](https://docs.aws.amazon.com/lake-formation/latest/dg/what-is-lake-formation.html)
- [Amazon Athena User Guide: Using Apache Iceberg Tables](https://docs.aws.amazon.com/athena/latest/ug/querying-iceberg.html)
- [Top 10 Performance Tuning Tips for Amazon Athena](https://aws.amazon.com/blogs/big-data/top-10-performance-tuning-tips-for-amazon-athena/)
- [Amazon Redshift Spectrum Documentation](https://docs.aws.amazon.com/redshift/latest/dg/c-getting-started-using-spectrum.html)
