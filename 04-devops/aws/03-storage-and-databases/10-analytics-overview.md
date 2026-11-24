# AWS Cloud Analytics: Athena, Glue, Redshift, and Data Lakehouse Architecture

> **Module**: `03-storage-and-databases`  
> **Topic**: Modern Data Architecture, Serverless SQL, ETL Catalogs, and Cloud Data Warehousing  
> **Target Audience**: Data Engineers, Analytics Architects, Solutions Engineers  
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

Enterprise organizations generate petabytes of raw data daily across transactional relational databases, NoSQL streams, IoT telemetry, clickstream events, and third-party SaaS applications. Operating traditional monolithic data warehouses that require rigid pre-loading ETL pipelines introduces severe latency bottlenecks, high operational costs, and data silos.

AWS pioneers the **Modern Data Lakehouse Architecture**, uniting the low-cost scalability of an object-based **Data Lake (Amazon S3)** with the high-performance analytical capabilities of a **Data Warehouse (Amazon Redshift)**:
- **Amazon Athena**: Interactive, serverless SQL query engine based on open-source Trino/Presto. Queries raw files directly in S3 with zero infrastructure provisioning, billing strictly per terabyte scanned.
- **AWS Glue**: Serverless data integration engine providing automated schema discovery (**Glue Crawlers**), a centralized metadata repository (**Glue Data Catalog**), and managed distributed data transformations (**Glue ETL Jobs on Apache Spark**).
- **Amazon Redshift**: Enterprise cloud data warehouse delivering petabyte-scale SQL analytics across massive datasets using columnar storage, massively parallel processing (MPP), and automated machine learning optimizations.
- **Amazon QuickSight**: Cloud-native, serverless Business Intelligence (BI) service delivering interactive dashboards powered by the in-memory **SPICE** calculation engine.

---

## 2. Core Concepts

### 2.1 Amazon Athena: Serverless S3 Querying
- Built on open-source Trino and Presto query engines.
- Queries structured, semi-structured, and unstructured data stored in Amazon S3 (Apache Parquet, ORC, JSON, CSV, Avro).
- **Pricing Model**: Billed strictly for the volume of data scanned by queries (**$5.00 per TB scanned**, minimum 10 MB per query).
- **Optimization Strategy**: Compressing data into columnar formats (Parquet/ORC) and partitioning directories by date or region reduces data scanned by **80% to 95%**, reducing query costs and execution times proportionally.

### 2.2 AWS Glue: Serverless Data Integration
- **Glue Data Catalog**: A centralized metadata metastore compliant with Apache Hive. Stores table definitions, schemas, and partition locations, shared seamlessly across Athena, Amazon EMR, and Redshift Spectrum.
- **Glue Crawlers**: Automated discovery jobs that scan raw S3 buckets, infer file schemas, detect file formats, and automatically create or update metadata tables in the Glue Data Catalog.
- **Glue ETL**: Serverless distributed data transformation engine running managed Apache Spark and Python scripts.

### 2.3 Amazon Redshift: Cloud Data Warehouse
- **Architecture**:
  - **Leader Node**: Receives client SQL connections, parses queries, coordinates execution plans, and aggregates results.
  - **Compute Nodes**: Massively Parallel Processing (MPP) nodes that execute queries in parallel over distributed columnar slices.
- **Node Types**:
  - **RA3 Nodes with Managed Storage**: Completely decouples compute from storage. Data is stored durably in high-speed Amazon S3-backed storage while hot blocks are cached locally in high-performance NVMe SSDs.
- **Redshift Spectrum**: Executes SQL queries directly against exabytes of data stored in Amazon S3 without loading data into Redshift local tables.
- **Redshift Serverless**: Automatically provisions and scales warehouse capacity in Redshift Processing Units (RPUs) based on workload concurrency, shutting down to zero when idle.

### 2.4 Data Distribution & Sorting in Redshift
- **Distribution Styles**:
  - `AUTO`: Redshift chooses the optimal distribution strategy automatically.
  - `KEY`: Rows are distributed across compute slices based on the hash of a specific column, co-locating joined tables to eliminate cross-node network shuffles.
  - `ALL`: A full copy of the table is duplicated across every node (ideal for small, slowly changing dimension tables < 2–3 million rows).
  - `EVEN`: Rows are distributed round-robin across slices (fallback when no clear join key exists).
- **Sort Keys**: Determines the physical ordering of data blocks on disk. Allows query execution engines to skip reading unneeded data blocks using min/max block metadata (zone maps).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        AWS Modern Data Lakehouse Architecture                     |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Data Producers ] (IoT Telemetry / RDS Relational DBs / Clickstreams)           |
|            |                                                                      |
|            v (Batch Upload / Kinesis Data Firehose Streaming)                     |
|  +-----------------------------------------------------------------------------+  |
|  | S3 Centralized Data Lake (Raw / Bronze Zone)                                |  |
|  | s3://data-lake/raw/events/2026/11/25/                                       |  |
|  +---------------------------------------|-------------------------------------+  |
|                                          |                                        |
|                     +--------------------+--------------------+                   |
|                     |                                         |                   |
|                     v                                         v                   |
|  +------------------------------------+   +------------------------------------+  |
|  | AWS Glue Crawler                   |   | AWS Glue ETL (Apache Spark)        |  |
|  | - Scans raw files & infers schema  |   | - Converts JSON/CSV -> Parquet     |  |
|  | - Updates Glue Data Catalog        |   | - Partitions by date & cleans data |  |
|  +------------------------------------+   +------------------------------------+  |
|                     |                                         |                   |
|                     v                                         v                   |
|  +-----------------------------------------------------------------------------+  |
|  | AWS Glue Data Catalog (Central Hive Metastore)                              |  |
|  | - Database: analytics_db | Table: clickstream (Schema: user_id, action, ts) |  |
|  +-----------------------------------------------------------------------------+  |
|            |                                                  |                   |
|            v                                                  v                   |
|  +-----------------------------------+    +------------------------------------+  |
|  | Amazon Athena (Serverless SQL)    |    | Amazon Redshift (Data Warehouse)   |  |
|  | - Ad-hoc queries directly on S3   |    | - RA3 Nodes + Redshift Spectrum    |  |
|  | - Billed per TB scanned ($5/TB)   |    | - Heavy aggregations & BI reports  |  |
|  +-----------------------------------+    +------------------------------------+  |
|                                 \              /                                  |
|                                  v            v                                   |
|  +-----------------------------------------------------------------------------+  |
|  | Amazon QuickSight (Cloud-Native Business Intelligence Dashboards)           |  |
|  | - In-Memory SPICE Calculation Engine & Generative BI Q                      |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating an Athena Table over Partitioned Parquet Data (DDL)
```sql
CREATE EXTERNAL TABLE IF NOT EXISTS analytics_db.orders (
  order_id string,
  customer_id string,
  amount double,
  status string
)
PARTITIONED BY (year string, month string, day string)
ROW FORMAT SERDE 'org.apache.hadoop.hive.ql.io.parquet.serde.ParquetHiveSerDe'
STORED AS INPUTFORMAT 'org.apache.hadoop.hive.ql.io.parquet.MapredParquetInputFormat'
OUTPUTFORMAT 'org.apache.hadoop.hive.ql.io.parquet.MapredParquetOutputFormat'
LOCATION 's3://production-data-lake-111122223333/curated/orders/'
TBLPROPERTIES ('parquet.compress'='SNAPPY');
```

Load newly added partitions automatically:
```sql
MSCK REPAIR TABLE analytics_db.orders;
```

### 4.2 Querying via Amazon Athena CLI
```bash
aws athena start-query-execution \
  --query-string "SELECT customer_id, SUM(amount) as total_spent FROM analytics_db.orders WHERE year='2026' AND month='11' GROUP BY customer_id ORDER BY total_spent DESC LIMIT 10;" \
  --query-execution-context Database=analytics_db \
  --result-configuration OutputLocation=s3://production-data-lake-111122223333/athena-query-results/
```

---

## 5. Practical Examples

### Example 1: Declarative Terraform Setup for Glue Catalog & Athena Database
Deploy a serverless data catalog environment with automated S3 query result storage.

```hcl
resource "aws_s3_bucket" "lake_storage" {
  bucket = "enterprise-datalake-111122223333"
}

resource "aws_s3_bucket" "athena_results" {
  bucket = "athena-results-vault-111122223333"
}

resource "aws_glue_catalog_database" "analytics" {
  name        = "corporate_analytics_db"
  description = "Central metadata database for enterprise lakehouse"
}

resource "aws_athena_workgroup" "analytics_wg" {
  name = "analytics-engineering"

  configuration {
    enforce_workgroup_configuration    = true
    publish_cloudwatch_metrics_enabled = true

    result_configuration {
      output_location = "s3://${aws_s3_bucket.athena_results.bucket}/workgroup-output/"
      encryption_configuration {
        encryption_option = "SSE_S3"
      }
    }
  }
}
```

### Example 2: AWS Glue Crawler Definition
Automate schema discovery across daily application logs in S3.

```hcl
resource "aws_iam_role" "glue_crawler_role" {
  name = "glue-crawler-service-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "glue.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy_attachment" "glue_service" {
  role       = aws_iam_role.glue_crawler_role.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSGlueServiceRole"
}

resource "aws_glue_crawler" "logs_crawler" {
  database_name = aws_glue_catalog_database.analytics.name
  name          = "s3-raw-logs-crawler"
  role          = aws_iam_role.glue_crawler_role.arn

  s3_target {
    path = "s3://${aws_s3_bucket.lake_storage.bucket}/raw/application-logs/"
  }

  schedule = "cron(0 0 * * ? *)" # Runs nightly at midnight
}
```

### Example 3: Redshift Serverless Data Warehouse Provisioning
Create a serverless Redshift namespace and workgroup scaling dynamically based on query volume.

```hcl
resource "aws_redshiftserverless_namespace" "analytics" {
  namespace_name      = "production-dw-namespace"
  admin_username      = "dwadmin"
  admin_user_password = "SecurePassword123!"
  db_name             = "enterprise_dw"
}

resource "aws_redshiftserverless_workgroup" "analytics" {
  workgroup_name = "production-dw-workgroup"
  namespace_name = aws_redshiftserverless_namespace.analytics.namespace_name
  base_capacity  = 32 # Base capacity in Redshift Processing Units (RPUs)

  subnet_ids         = var.private_subnet_ids
  security_group_ids = [var.redshift_security_group_id]
}
```

### Example 4: Redshift Table DDL with Optimal Distribution and Sort Keys
High-performance DDL for a fact table joined frequently with a customer dimension table.

```sql
CREATE TABLE sales_dw.fact_transactions (
  transaction_id VARCHAR(64) NOT NULL,
  customer_id VARCHAR(32) NOT NULL,
  store_id INT NOT NULL,
  transaction_timestamp TIMESTAMP NOT NULL,
  total_amount NUMERIC(12, 2) NOT NULL,
  tax_amount NUMERIC(8, 2)
)
DISTSTYLE KEY
DISTKEY(customer_id) -- Co-locates with dim_customers table on same compute slice
COMPOUND SORTKEY(transaction_timestamp, customer_id); -- Optimizes time-series filters
```

### Example 5: Querying S3 Data Lake via Redshift Spectrum
Execute an external join between a local Redshift table and an S3 Parquet dataset without loading S3 data.

```sql
-- 1. Create external schema pointing to Glue Data Catalog
CREATE EXTERNAL SCHEMA s3_lake
FROM DATA CATALOG
DATABASE 'corporate_analytics_db'
IAM_ROLE 'arn:aws:iam::111122223333:role/RedshiftSpectrumRole';

-- 2. Join local fast table with historical archive in S3
SELECT 
  t.customer_id,
  c.customer_name,
  SUM(t.total_amount) AS historical_lifetime_value
FROM sales_dw.fact_transactions t
JOIN s3_lake.customer_archive c ON t.customer_id = c.customer_id
WHERE t.transaction_timestamp >= '2026-01-01'
GROUP BY t.customer_id, c.customer_name
ORDER BY historical_lifetime_value DESC
LIMIT 100;
```

---

## 6. Best Practices

1. **Always Convert Raw Data to Apache Parquet**: Storing data in compressed columnar Parquet (with Snappy compression) reduces storage size by 80%, cuts Athena query costs by up to 90%, and accelerates scan speeds by 10x compared to raw JSON or CSV.
2. **Partition S3 Buckets by Query Filter Keys**: Partition data by frequently filtered dimensions (e.g., `year=YYYY/month=MM/region=XX`). When querying a single month, Athena skips scanning the rest of the bucket, minimizing data scanned.
3. **Set Data Scan Limits in Athena Workgroups**: Configure a **per-query data scan limit** (e.g., maximum 500 GB) in the Athena Workgroup settings. If an engineer accidentally runs an unpartitioned `SELECT *` across a multi-terabyte dataset, Athena cancels the query automatically, preventing multi-hundred dollar surprises.
4. **Choose Redshift Distribution Keys Carefully**: When joining large fact tables with large dimension tables, assign the identical column as the `DISTKEY` on both tables. This forces Redshift to store matching keys on the exact same physical compute slice, eliminating network redistribution shuffles.
5. **Use Redshift Serverless for Variable Analytics Workloads**: For business intelligence and reporting teams running queries primarily during working hours, deploy **Redshift Serverless**. It scales capacity dynamically during peak hours and pauses compute automatically when idle, cutting data warehouse bills by 40–60%.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Querying Raw Unpartitioned CSVs in Athena** | Queries scan terabytes of text, taking minutes to run and racking up massive per-TB scan bills. | Convert files to **Apache Parquet** partitioned by date using AWS Glue ETL. |
| **Running `SELECT *` without Partitions** | Scans the entire bucket history; Athena bills $5.00 per terabyte scanned across the entire dataset. | Select only required columns (`SELECT col1, col2`) and filter on partition keys (`WHERE year='2026'`). |
| **Using `DISTSTYLE EVEN` on Joined Fact Tables** | Compute nodes must continuously shuffle gigabytes of data across the network during SQL joins. | Set `DISTSTYLE KEY` on the shared foreign key column on both joined tables. |
| **Running Glue Crawlers on Every Ingestion** | High crawler costs and unnecessary schema recalculations. | Use Glue Crawlers for initial schema discovery; notify the catalog of new partitions via **S3 Event Notifications** or `MSCK REPAIR`. |
| **Using Redshift as an OLTP Database** | Extremely poor single-row insert/update performance and transaction lock contention. | Redshift is an OLAP data warehouse. Use Amazon Aurora or DynamoDB for transactional OLTP workloads. |
| **Ignoring Athena Query Result S3 Bucket Cleanup** | S3 bucket accumulating millions of query output CSV files indefinitely, inflating storage costs. | Configure an S3 lifecycle policy to delete Athena query results after 7 to 14 days. |

---

## 8. Performance, Security & Cost Notes

### Query Performance Dynamics
- **Columnar Storage Advantage**: In columnar formats (Parquet, ORC, Redshift), the database reads only the specific columns referenced in the `SELECT` clause from disk, skipping all other columns entirely.
- **Zone Maps (Block Metadata)**: Redshift stores 1 MB disk blocks accompanied by in-memory metadata recording the minimum and maximum values of each block. If a query filters for `date >= '2026-11-01'`, Redshift skips reading blocks where `max_date < '2026-11-01'`.

### Security Governance
- **AWS Lake Formation**: Centralized governance layer over S3 and Glue Data Catalog. Provides fine-grained **column-level, row-level, and cell-level security** filters, restricting sensitive columns (e.g., PII, credit cards) based on IAM roles without managing separate database views.

### Cost Breakdown
- **Amazon Athena**: $5.00 per TB of data scanned ($0.005/GB).
- **AWS Glue ETL**: $0.44 per Data Processing Unit (DPU) per hour (billed in 1-second increments).
- **Redshift Serverless**: $0.375 per RPU-hour.

---

## 9. Comparison Table

| Feature | Amazon Athena | Amazon Redshift | Amazon EMR |
|:---|:---|:---|:---|
| **Primary Paradigm** | Serverless Interactive SQL | Cloud Data Warehouse (MPP) | Managed Big Data Framework (Hadoop/Spark)|
| **Data Storage Location**| Amazon S3 (In-situ) | Managed Redshift Storage / S3 | Amazon S3 / HDFS |
| **Underlying Engine** | Presto / Trino | PostgreSQL-derived MPP Engine | Apache Spark, Flink, Hive, Presto |
| **Query Latency** | Seconds to minutes | Sub-second to seconds (Complex joins)| Batch jobs (Minutes to hours) |
| **Pricing Model** | $5.00 per TB scanned | Billed per RPU-hour or instance| Billed per instance-hour + EMR fee |
| **Maintenance Burden** | Zero (100% Serverless) | Minimal (Serverless) / Medium | High (Cluster tuning, JVM config) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How does partitioning and columnar formatting optimize query performance and cost in Amazon Athena?</strong></summary>

**Answer:**
Amazon Athena pricing is strictly based on the volume of data scanned by queries ($5.00 per TB scanned):
1. **Partitioning**: Organizes data into an S3 directory hierarchy based on distinct column keys (e.g., `/year=2026/month=11/`). When a query specifies `WHERE year='2026' AND month='11'`, Athena uses metadata pruning to read only that specific folder, skipping 95%+ of the bucket.
2. **Columnar Formats (Apache Parquet / ORC)**: Organizes data by column rather than row. When a query requests `SELECT customer_id, SUM(amount)`, Athena reads only the bytes for the `customer_id` and `amount` columns from disk, skipping all other fields.
3. **Compression**: Parquet applies dictionary encoding and Snappy compression to column blocks, shrinking file sizes by up to 80%.
Combined, these optimizations reduce data scanned by up to 90–99%, reducing a $50 query to $0.50 while accelerating execution speeds from minutes to seconds.
</details>

<details>
<summary><strong>Q2: What is the role of the AWS Glue Data Catalog in a Modern Data Lakehouse?</strong></summary>

**Answer:**
The AWS Glue Data Catalog serves as the **centralized, unified metadata metastore** for the entire AWS data and analytics ecosystem:
- It maintains schema definitions, data types, physical S3 locations, and partition keys for structured and semi-structured datasets stored in Amazon S3.
- It is fully Apache Hive Metastore-compliant.
- A single table definition registered in the Glue Data Catalog can be queried concurrently by **Amazon Athena** (serverless SQL), **Amazon Redshift Spectrum** (data warehouse federation), **Amazon EMR** (Apache Spark processing), and **AWS Glue ETL jobs**, eliminating duplicate schema definitions and data silos.
</details>

<details>
<summary><strong>Q3: How does Amazon Redshift Spectrum query data stored in Amazon S3?</strong></summary>

**Answer:**
Redshift Spectrum allows Amazon Redshift clusters to execute SQL queries directly against exabytes of data stored in Amazon S3 without loading or transforming the data into local Redshift tables:
1. The user connects to Redshift and executes a query referencing an external schema registered in the AWS Glue Data Catalog.
2. The Redshift Leader Node parses the query and pushes the S3 scan and aggregation operations down to a massive, serverless fleet of **thousands of Redshift Spectrum processing nodes**.
3. Spectrum nodes read, filter, and project the S3 data in parallel, streaming only the aggregated results back to the local Redshift cluster.
4. Redshift joins the external S3 data with local relational tables, providing seamless data lakehouse federation.
</details>

<details>
<summary><strong>Q4: What is the difference between Redshift Distribution Styles: KEY, ALL, and EVEN?</strong></summary>

**Answer:**
Distribution styles determine how table rows are physically allocated across Redshift compute node slices:
- **`KEY`**: Rows are distributed based on the hash of a chosen column. If two large tables joined frequently (e.g., `Orders` and `OrderItems`) use the same distribution key (`order_id`), Redshift stores matching rows on the exact same physical compute slice, eliminating network data shuffles during joins.
- **`ALL`**: A full, identical copy of the table is replicated across every single compute node. Best for small, slowly changing dimension tables (< 2–3 million rows) so joins can execute locally on every slice.
- **`EVEN`**: Rows are distributed round-robin evenly across all slices. Best when a table does not participate in joins or lacks a clear, high-cardinality distribution key.
</details>

<details>
<summary><strong>Q5: What is an AWS Glue Crawler and when should it be used?</strong></summary>

**Answer:**
An AWS Glue Crawler is an automated metadata discovery service:
- It connects to source data repositories (Amazon S3, Amazon RDS, DynamoDB).
- It reads data samples, infers the schema (data types, column headers), detects file formats (CSV, JSON, Parquet), and calculates partition keys from S3 directory paths.
- It automatically writes or updates table metadata in the AWS Glue Data Catalog.
It is best used for **initial dataset onboarding** or when raw upstream datasets have dynamic, evolving schemas.
</details>

<details>
<summary><strong>Q6: How does AWS Lake Formation simplify data governance?</strong></summary>

**Answer:**
In a standard S3 data lake, access control is governed via S3 Bucket Policies and IAM policies, which only enforce coarse permissions at the bucket or prefix level. If a table contains sensitive data (e.g., credit card numbers or salaries), administrators had to create separate S3 buckets with sanitized data.

**AWS Lake Formation**:
- Sits on top of S3 and the Glue Data Catalog.
- Enforces fine-grained **table-level, column-level, row-level, and cell-level security**.
- Administrators can grant an analyst role access to `orders_table` while explicitly masking or revoking access to the `credit_card_number` column. When the analyst queries via Athena or Redshift, Lake Formation filters the data on the fly.
</details>

<details>
<summary><strong>Q7: What is the purpose of Redshift Sort Keys?</strong></summary>

**Answer:**
Redshift stores data in 1 MB physical disk blocks. For each block, Redshift maintains **Zone Maps** in memory recording the minimum and maximum values of every column in that block.
A **Sort Key** dictates the physical order in which rows are written to disk:
- When a query filters by a sort key column (e.g., `WHERE transaction_date >= '2026-11-01'`), the execution engine inspects the Zone Maps.
- It completely skips reading disk blocks where `max_date < '2026-11-01'`, eliminating disk I/O and accelerating query execution speeds by orders of magnitude.
</details>

<details>
<summary><strong>Q8: When should an organization choose Amazon Redshift Serverless over Amazon Athena?</strong></summary>

**Answer:**
- Choose **Amazon Athena** for ad-hoc querying, exploratory data analysis, log inspection, and infrequent queries directly against S3 data lakes where running a 24/7 database cluster is cost-prohibitive.
- Choose **Amazon Redshift Serverless** for high-concurrency, sub-second business intelligence dashboards, complex multi-table analytical joins, predictive machine learning models, and heavy data warehouse workloads requiring enterprise query optimization, result caching, and dedicated compute power.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an Athena Database and Workgroup via CLI</strong></summary>

**Objective**: Configure a query workgroup enforcing query result encryption.

**Commands**:
```bash
aws athena create-work-group \
  --name "bi-analysts" \
  --configuration '{
    "ResultConfiguration": {
      "OutputLocation": "s3://production-data-lake-111122223333/athena-results/",
      "EncryptionConfiguration": {"EncryptionOption": "SSE_S3"}
    },
    "EnforceWorkGroupConfiguration": true,
    "PublishCloudWatchMetricsEnabled": true
  }'
```
</details>

<details>
<summary><strong>Exercise 2: Create a Glue Database via AWS CLI</strong></summary>

**Objective**: Provision a metadata catalog database.

**Command**:
```bash
aws glue create-database \
  --database-input '{"Name": "ecommerce_analytics", "Description": "E-commerce data catalog"}'
```
</details>

<details>
<summary><strong>Exercise 3: Execute an Athena Query with Partition Pruning</strong></summary>

**Objective**: Query partitioned data and inspect data scanned metrics.

**Command**:
```bash
EXEC_ID=$(aws athena start-query-execution \
  --query-string "SELECT COUNT(*) FROM ecommerce_analytics.orders WHERE year='2026' AND month='11'" \
  --work-group "bi-analysts" \
  --query 'QueryExecutionId' --output text)

aws athena get-query-execution --query-execution-id "$EXEC_ID" \
  --query 'QueryExecution.Statistics.DataScannedInBytes'
```
</details>

<details>
<summary><strong>Exercise 4: Provision a Redshift Serverless Workgroup</strong></summary>

**Objective**: Deploy an auto-scaling data warehouse workgroup.

**Command**:
```bash
aws redshift-serverless create-workgroup \
  --workgroup-name "analytics-workgroup" \
  --namespace-name "analytics-namespace" \
  --base-capacity 32 \
  --subnet-ids "subnet-01a" "subnet-01b" "subnet-01c"
```
</details>

<details>
<summary><strong>Exercise 5: Configure Per-Query Data Limits in Athena</strong></summary>

**Objective**: Protect against runaway query costs by capping data scanned at 100 GB.

**Command**:
```bash
aws athena update-work-group \
  --work-group "bi-analysts" \
  --configuration-updates '{
    "BytesScannedCutoffPerQuery": 107374182400,
    "EnforceWorkGroupConfiguration": true
  }'
```
</details>

<details>
<summary><strong>Exercise 6: Test Redshift Spectrum External Schema Integration</strong></summary>

**Objective**: Connect Redshift to the Glue Data Catalog.

**SQL Command**:
```sql
CREATE EXTERNAL SCHEMA IF NOT EXISTS spectrum_schema
FROM DATA CATALOG
DATABASE 'ecommerce_analytics'
IAM_ROLE 'arn:aws:iam::111122223333:role/RedshiftSpectrumRole';
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                             AWS Analytics Ecosystem Cheat Sheet                       |
+---------------------------------------------------------------------------------------+
|  Service               | Primary Role                      | Pricing Metric           |
|------------------------+-----------------------------------+--------------------------|
|  Amazon Athena         | Serverless SQL directly on S3     | $5.00 per TB scanned     |
|  AWS Glue Data Catalog | Central Hive-compliant metastore  | First 1M objects free    |
|  AWS Glue ETL          | Serverless Apache Spark jobs      | $0.44 per DPU-hour       |
|  Amazon Redshift       | High-performance MPP warehouse    | RPU-hours (Serverless)   |
|  Redshift Spectrum     | Query S3 from Redshift cluster    | $5.00 per TB scanned     |
|  Amazon QuickSight     | Serverless BI & SPICE dashboards  | User subscription / mo   |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon Athena User Guide](https://docs.aws.amazon.com/athena/latest/ug/what-is.html)
- [AWS Glue Developer Guide](https://docs.aws.amazon.com/glue/latest/dg/what-is-glue.html)
- [Amazon Redshift Database Developer Guide](https://docs.aws.amazon.com/redshift/latest/dg/welcome.html)
- [Building Big Data Lakehouses on AWS](https://aws.amazon.com/big-data/datalakes-and-analytics/)
- [Top 10 Performance Tuning Tips for Amazon Athena](https://aws.amazon.com/blogs/big-data/top-10-performance-tuning-tips-for-amazon-athena/)
