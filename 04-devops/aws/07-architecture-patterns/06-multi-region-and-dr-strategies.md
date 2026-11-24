---
topic: "Multi-Region Architecture and Disaster Recovery Strategies"
module: "07-architecture-patterns"
track: "aws"
last-tested: "2026-11"
prerequisites:
  - "03-storage-and-databases/03-rds-and-aurora.md"
  - "03-storage-and-databases/02-dynamodb.md"
  - "04-networking/05-route53-and-dns-patterns.md"
  - "06-operations/09-reliability-and-cost.md"
complexity: "advanced"
tags:
  - "multi-region"
  - "disaster-recovery"
  - "rto"
  - "rpo"
  - "aurora-global-db"
  - "dynamodb-global-tables"
  - "route53"
---

# Multi-Region Architecture and Disaster Recovery Strategies

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

Disaster Recovery (DR) and Multi-Region architectures ensure that mission-critical applications withstand catastrophic regional outages, natural disasters, fiber cuts, or systemic service degradations. While designing across multiple Availability Zones (Multi-AZ) protects against data center-level hardware failures, true enterprise business continuity requires strategies that span physically independent AWS geographical regions.

### Key Metrics: RTO and RPO
Every DR strategy is defined and measured by two foundational metrics:
- **Recovery Point Objective (RPO)**: The maximum acceptable amount of data loss measured in time. (e.g., An RPO of 1 hour means the business can tolerate losing up to 1 hour of newly written data during a disaster).
- **Recovery Time Objective (RTO)**: The maximum acceptable duration of downtime before the application is fully restored and operational. (e.g., An RTO of 15 minutes means services must be restored within 15 minutes of an outage).

Balancing RTO, RPO, and cost determines which of the **4 Industry-Standard DR Strategies** an organization should adopt: **Backup and Restore**, **Pilot Light**, **Warm Standby**, or **Multi-Region Active-Active**.

---

## 2. Core Concepts

### The 4 Disaster Recovery Strategies

```
+----------------------------------------------------------------------------------------------------+
|                               THE 4 DISASTER RECOVERY TIERS ON AWS                                 |
+----------------------------------------------------------------------------------------------------+

  Higher RTO / RPO (Hours / Days) -------------------------------> Lower RTO / RPO (Seconds / Zero)
  Lowest Cost ----------------------------------------------------> Highest Cost & Complexity

   +-------------------+    +-------------------+    +-------------------+    +--------------------+
   | 1. Backup/Restore |    |  2. Pilot Light   |    |  3. Warm Standby  |    | 4. Multi-Region    |
   |                   |    |                   |    |                   |    |    Active-Active   |
   +-------------------+    +-------------------+    +-------------------+    +--------------------+
   | - S3 Cross-Region |    | - Core DB replica |    | - Scaled-down     |    | - Both regions     |
   | - AWS Backup copy |    |   continuously on |    |   fleet running   |    |   serve live users |
   | - Provision infra |    | - App compute is  |    | - Instant scale   |    | - Global Tables /  |
   |   from scratch    |    |   provisioned on  |    |   up on failover  |    |   Aurora Global DB |
   |   during disaster |    |   failover        |    |                   |    | - Zero downtime    |
   +-------------------+    +-------------------+    +-------------------+    +--------------------+
     RTO: Hours to Days       RTO: 10 - 30 Mins        RTO: Minutes             RTO: Sub-Second
     RPO: Hours               RPO: Seconds/Minutes     RPO: Seconds             RPO: Near Zero
```

1. **Backup and Restore (Cold Standby)**:
   - Data is backed up periodically and replicated to a secondary AWS region using AWS Backup, EBS snapshot copies, or S3 Cross-Region Replication (CRR).
   - Compute infrastructure in the secondary region is non-existent until disaster strikes; systems are rebuilt from IaC (Terraform/CloudFormation) during failover.
   - Lowest cost; highest RTO (hours/days).

2. **Pilot Light**:
   - The critical core data layer (e.g., Aurora Global Database, DynamoDB Global Tables) is continuously running and replicating data in real-time in the DR region.
   - Compute servers, containers, and web servers are maintained as dormant AMIs/container images with Auto Scaling Groups set to `desired_capacity = 0`.
   - On failover, ASG desired capacity is scaled up via automated scripts. RTO: 10–30 minutes; RPO: seconds.

3. **Warm Standby**:
   - A fully functional, scaled-down replica of the entire production environment runs 24/7 in the secondary region.
   - Handles low-level synthetic traffic or internal testing continuously.
   - On disaster, Route 53 shifts DNS traffic to the secondary region, and the fleet immediately scales out horizontally to handle 100% production load. RTO: Minutes; RPO: Seconds.

4. **Multi-Region Active-Active**:
   - Both regions simultaneously serve live production traffic routed by Amazon Route 53 (Latency-based, Geolocation, or Geoproximity routing) or AWS Global Accelerator.
   - Databases utilize bi-directional multi-region replication (DynamoDB Global Tables) or active read/write splits (Aurora Global Database).
   - True zero-downtime failover with RTO $\approx 0$ and RPO $\approx 0$.

---

## 3. How It Works Internally

```
+----------------------------------------------------------------------------------------------------+
|                         MULTI-REGION ACTIVE-PASSIVE FAILOVER ARCHITECTURE                          |
+----------------------------------------------------------------------------------------------------+

                                      [ Global End Users ]
                                                |
                                                v
                      +----------------------------------------------------+
                      | Amazon Route 53 (Failover Routing Policy)          |
                      |  - Primary Record: us-east-1 (Health Check A)      |
                      |  - Secondary Record: us-west-2 (Standby)           |
                      +----------------------------------------------------+
                                    |                        :
                        Active Flow |                        : Failover Flow
                                    v                        v
             +------------------------------+        +------------------------------+
             | PRIMARY REGION (us-east-1)   |        | SECONDARY REGION (us-west-2) |
             |                              |        |                              |
             | [ CloudFront / ALB Ingress ] |        | [ CloudFront / ALB Ingress ] |
             |              |               |        |              :               |
             |              v               |        |              v (Scaled Up)   |
             | [ ECS / EC2 Compute Fleet ]  |        | [ ECS / EC2 Compute Fleet ]  |
             |              |               |        |              : (Desired = 0) |
             |              v (Writes)      |        |              v               |
             | +--------------------------+ |        | +--------------------------+ |
             | | Amazon Aurora Primary    | |        | | Amazon Aurora Secondary  | |
             | | (Read / Write Cluster)   | |        | | (Read-Only Replica)      | |
             | +--------------------------+ |        | +--------------------------+ |
             +------------------------------+        +------------------------------+
                            |                                       ^
                            +---------------------------------------+
                              Storage-Level Cross-Region Replication
                              (Asynchronous, Typical Latency < 1 sec)
```

### Storage-Level Replication Mechanics
- **Amazon Aurora Global Database**: Uses dedicated storage infrastructure engines to replicate WAL changes across regions without database compute overhead. Storage replication latency is typically under 1 second. Promoting the secondary cluster to primary takes under 1 minute via the `FailoverGlobalCluster` API call.
- **Amazon DynamoDB Global Tables**: Uses fully managed multi-master replication across chosen regions. Every write in any region is replicated to all other regions within ~1 second. Conflicts are resolved automatically using "Last Write Wins" (LWW) based on internal AWS timestamps.
- **Amazon S3 Cross-Region Replication (CRR)**: Asynchronously replicates newly uploaded objects across regions. Optional **S3 Replication Time Control (S3 RTC)** provides an SLA guaranteeing 99.99% of objects replicate within 15 minutes.

---

## 4. Syntax & Usage

### 4.1 Terraform: Route 53 DNS Failover with Health Checks

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
  alias  = "primary"
  region = "us-east-1"
}

provider "aws" {
  alias  = "secondary"
  region = "us-west-2"
}

# Route 53 Public Hosted Zone
resource "aws_route53_zone" "primary" {
  name = "example.com"
}

# Health Check for Primary Region Ingress
resource "aws_route53_health_check" "primary_health" {
  fqdn              = "api-us-east-1.example.com"
  port              = 443
  type              = "HTTPS"
  resource_path     = "/healthz"
  failure_threshold = 3
  request_interval  = 10 # Fast 10-second health check interval

  tags = {
    Name = "primary-region-health-check"
  }
}

# Primary Failover DNS Record
resource "aws_route53_record" "api_primary" {
  zone_id = aws_route53_zone.primary.zone_id
  name    = "api.example.com"
  type    = "A"

  failover_routing_policy {
    type = "PRIMARY"
  }

  set_identifier  = "primary-us-east-1"
  health_check_id = aws_route53_health_check.primary_health.id

  alias {
    name                   = "d111111abcdef8.cloudfront.net"
    zone_id                = "Z2FDTNDATAQYW2" # CloudFront Hosted Zone ID
    evaluate_target_health = true
  }
}

# Secondary Failover DNS Record (Active on primary failure)
resource "aws_route53_record" "api_secondary" {
  zone_id = aws_route53_zone.primary.zone_id
  name    = "api.example.com"
  type    = "A"

  failover_routing_policy {
    type = "SECONDARY"
  }

  set_identifier = "secondary-us-west-2"

  alias {
    name                   = "d222222abcdef9.cloudfront.net"
    zone_id                = "Z2FDTNDATAQYW2"
    evaluate_target_health = true
  }
}
```

---

## 5. Practical Examples

### Example 1: Amazon Aurora Global Database Provisioning via Terraform

```hcl
# Aurora Global Cluster Backbone
resource "aws_rds_global_cluster" "global_db" {
  global_cluster_identifier = "production-aurora-global-db"
  engine                    = "aurora-postgresql"
  engine_version            = "15.4"
  database_name             = "enterprise_db"
  storage_encrypted         = true
}

# Primary Regional Cluster (us-east-1)
resource "aws_rds_cluster" "primary_cluster" {
  provider                  = aws.primary
  cluster_identifier        = "aurora-cluster-us-east-1"
  engine                    = aws_rds_global_cluster.global_db.engine
  engine_version            = aws_rds_global_cluster.global_db.engine_version
  global_cluster_identifier = aws_rds_global_cluster.global_db.id
  master_username           = "dbadmin"
  manage_master_user_password = true

  db_subnet_group_name   = "primary-db-subnet-group"
  vpc_security_group_ids = ["sg-primary-db"]
  skip_final_snapshot    = true
}

# Secondary Regional Cluster (us-west-2, Storage Replica)
resource "aws_rds_cluster" "secondary_cluster" {
  provider                  = aws.secondary
  cluster_identifier        = "aurora-cluster-us-west-2"
  engine                    = aws_rds_global_cluster.global_db.engine
  engine_version            = aws_rds_global_cluster.global_db.engine_version
  global_cluster_identifier = aws_rds_global_cluster.global_db.id

  db_subnet_group_name   = "secondary-db-subnet-group"
  vpc_security_group_ids = ["sg-secondary-db"]
  skip_final_snapshot    = true

  depends_on = [aws_rds_cluster.primary_cluster]
}
```

### Example 2: Python Script for Planned Aurora Global Database Switchover

```python
import boto3
import sys

rds_client = boto3.client("rds", region_name="us-west-2")

def execute_planned_switchover(global_cluster_id: str, target_cluster_arn: str):
    """
    Executes a zero-data-loss planned switchover of an Aurora Global Database.
    Synchronizes storage before promoting the target secondary cluster.
    """
    print(f"Initiating planned switchover for global cluster: {global_cluster_id}")
    try:
        response = rds_client.switchover_global_cluster(
            GlobalClusterIdentifier=global_cluster_id,
            TargetDbClusterIdentifier=target_cluster_arn
        )
        print("Switchover command submitted successfully.")
        print(f"Global Cluster Status: {response['GlobalCluster']['Status']}")
    except Exception as e:
        print(f"Error during switchover: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    GLOBAL_ID = "production-aurora-global-db"
    TARGET_ARN = "arn:aws:rds:us-west-2:123456789012:cluster:aurora-cluster-us-west-2"
    execute_planned_switchover(GLOBAL_ID, TARGET_ARN)
```

### Example 3: DynamoDB Global Tables v2 (Multi-Region Active-Active)

```hcl
resource "aws_dynamodb_table" "global_users" {
  provider     = aws.primary
  name         = "global-users-table"
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "UserId"

  attribute {
    name = "UserId"
    type = "S"
  }

  stream_enabled   = true
  stream_view_type = "NEW_AND_OLD_IMAGES"

  # Replica in us-west-2
  replica {
    region_name            = "us-west-2"
    point_in_time_recovery = true
    propagate_tags         = true
  }

  point_in_time_recovery {
    enabled = true
  }
}
```

### Example 4: S3 Cross-Region Replication (CRR) with Replication Time Control (RTC)

```hcl
# S3 Destination Bucket in us-west-2
resource "aws_s3_bucket" "dr_bucket" {
  provider = aws.secondary
  bucket   = "enterprise-data-backup-us-west-2"
}

resource "aws_s3_bucket_versioning" "dr_versioning" {
  provider = aws.secondary
  bucket   = aws_s3_bucket.dr_bucket.id
  versioning_configuration {
    status = "Enabled"
  }
}

# Replication Configuration on Primary Bucket
resource "aws_s3_bucket_replication_configuration" "crr" {
  provider = aws.primary
  role     = aws_iam_role.replication_role.arn
  bucket   = aws_s3_bucket.primary_bucket.id

  rule {
    id     = "ReplicateAllWithRTC"
    status = "Enabled"

    destination {
      bucket        = aws_s3_bucket.dr_bucket.arn
      storage_class = "STANDARD_IA"

      replication_time {
        status = "Enabled"
        time {
          minutes = 15
        }
      }

      metrics {
        status = "Enabled"
        event_threshold {
          minutes = 15
        }
      }
    }
  }
}
```

### Example 5: CloudFront Origin Groups for Automatic Multi-Region Ingress Failover

```hcl
resource "aws_cloudfront_distribution" "multi_region_cdn" {
  enabled = true

  origin {
    domain_name = "api-us-east-1.example.com"
    origin_id   = "primary-alb"
    custom_origin_config {
      http_port              = 80
      https_port             = 443
      origin_protocol_policy = "https-only"
      origin_ssl_protocols   = ["TLSv1.2"]
    }
  }

  origin {
    domain_name = "api-us-west-2.example.com"
    origin_id   = "secondary-alb"
    custom_origin_config {
      http_port              = 80
      https_port             = 443
      origin_protocol_policy = "https-only"
      origin_ssl_protocols   = ["TLSv1.2"]
    }
  }

  origin_group {
    origin_id = "failover-origin-group"

    failover_criteria {
      status_codes = [500, 502, 503, 504]
    }

    member {
      origin_id = "primary-alb"
    }

    member {
      origin_id = "secondary-alb"
    }
  }

  default_cache_behavior {
    target_origin_id       = "failover-origin-group"
    viewer_protocol_policy = "redirect-to-https"
    allowed_methods        = ["GET", "HEAD", "OPTIONS", "PUT", "POST", "PATCH", "DELETE"]
    cached_methods         = ["GET", "HEAD"]
    forwarded_values {
      query_string = true
      cookies { forward = "all" }
    }
  }
}
```

---

## 6. Best Practices

1. **Conduct Regular GameDay Drills**: A disaster recovery plan that has never been tested in production will fail during a real emergency. Schedule quarterly automated failover simulations.
2. **Automate Infrastructure Deployment via Continuous IaC**: Keep secondary region IaC (Terraform modules) strictly in sync with production via automated CI/CD pipeline deployments.
3. **Use Aurora Global Database Switchover for Planned Maintenance**: Use `switchover_global_cluster` (which synchronizes storage replicas before promotion) rather than unplanned failover to ensure zero data loss.
4. **Implement Client-Side Routing Retries and AWS Global Accelerator**: Global Accelerator provides static Anycast IPs and performs sub-minute edge-level regional health checks, bypassing DNS TTL propagation delays.
5. **Monitor Replication Lag Continuously**: Create CloudWatch alarms on Aurora `AuroraGlobalDBReplicationLag` (> 5000 ms) and DynamoDB `ReplicationLatency`.
6. **Beware of Split-Brain Scenarios in Active-Active Systems**: Ensure all stateful entities write with deterministic UUIDs or partition keys and leverage conflict resolution algorithms.

---

## 7. Common Mistakes

| Mistake | Why It Hurts | Correct Approach |
|:---|:---|:---|
| **Relying solely on DNS failover with long TTLs** | Client resolvers and ISPs cache DNS records for hours, preventing users from receiving updated failover IPs. | Set Route 53 TTL to 10-60 seconds, or use AWS Global Accelerator. |
| **Failing to test DR failover regularly** | Hidden configuration drifts (e.g., outdated AMIs, expired certificates, missing IAM roles) cause failover to crash. | Run scheduled automated "Chaos GameDays" in production or staging. |
| **Assuming Backup and Restore meets a 1-hour RTO** | Restoring multi-terabyte RDS snapshots and launching dozens of EC2 instances takes 6+ hours under real disaster conditions. | Choose Pilot Light or Warm Standby if RTO must be under 30 minutes. |
| **Ignoring cross-region data transfer costs** | Streaming hundreds of gigabytes per hour across AWS regions incurs \$0.02/GB data egress fees, driving unexpected monthly bills. | Replicate only essential mission-critical state tables; compress data payloads. |
| **Performing unplanned Aurora failover during transient network blip** | Unplanned failover breaks storage replication links and can cause split-brain data divergence. | Verify sustained regional failure before executing manual promotion. |
| **Hardcoding region-specific ARNs in application configs** | The secondary application fails to start because it cannot locate IAM roles, KMS keys, or S3 buckets tied to the primary region. | Use dynamic environment parameters and region-agnostic naming conventions. |

---

## 8. Performance, Security & Cost Notes

### Performance
- **Replication Latency**:
  - Aurora Global Database: Typically < 1 second.
  - DynamoDB Global Tables: Typically 500ms - 1500ms.
  - S3 CRR: Asynchronous (99.99% under 15 minutes with S3 RTC enabled).

### Security
- **Multi-Region KMS Keys**: Use AWS KMS Multi-Region Keys (`aws_kms_replica_key`) so data encrypted in the primary region can be decrypted seamlessly in the secondary region without re-encryption.
- **Cross-Region IAM Roles**: Ensure IAM roles and Service-Linked Roles exist independently in both regions.

### Cost Breakdown
- **Pilot Light / Warm Standby**: Compute costs increase by 10% to 50% depending on standby capacity.
- **Active-Active**: Effectively doubles compute and load balancing costs (100% compute overhead + cross-region data transfer at \$0.02/GB).

---

## 9. Comparison Table

| Metric / Dimension | Backup & Restore | Pilot Light | Warm Standby | Multi-Region Active-Active |
|:---|:---|:---|:---|:---|
| **RTO** | Hours to Days | 10 - 30 Minutes | Under 5 Minutes | Sub-Second / Zero |
| **RPO** | Hours | Seconds to Minutes | Seconds | Near Zero (< 1 sec) |
| **Cost Overhead** | Low (< 5%) | Low-Medium (15-25%) | Medium-High (40-60%) | High (100%+ & Egress) |
| **Complexity** | Low | Medium | High | Very High |
| **Active Compute in DR** | None | None (ASG = 0) | Minimal (e.g., 20% size) | 100% Live Traffic |
| **Typical Use Case** | Non-critical internal tools | Enterprise line-of-business | High-value SaaS platforms | Tier-0 financial / health systems |

---

## 10. Interview Questions

<details>
<summary><strong>1. Explain the technical differences between RTO and RPO with concrete enterprise examples.</strong></summary>

- **RTO (Recovery Time Objective)**: The duration of time permitted to elapse before business systems are restored after a disaster. For example, if an e-commerce platform suffers a primary region outage at 10:00 AM and has an RTO of 30 minutes, all customer transactions and APIs must be operational in the secondary region by 10:30 AM.
- **RPO (Recovery Point Objective)**: The maximum acceptable amount of data loss measured in time. If the same platform has an RPO of 5 minutes, backups and database replication must be synchronized such that no more than 5 minutes of transaction data is permanently lost during the failover event.
</details>

<details>
<summary><strong>2. How does Amazon Aurora Global Database achieve sub-second cross-region replication?</strong></summary>

Aurora Global Database bypasses the traditional database compute layer entirely for replication. Instead of the primary instance writing to a binary log (WAL) and transmitting it to a secondary database engine to re-execute SQL statements, the underlying **Aurora distributed storage tier** replicates storage volume changes directly across regions using dedicated AWS backbone networks. The secondary region storage nodes receive raw storage log records and apply them directly to local storage disks, achieving typical replication latencies under 1 second without impacting primary database CPU performance.
</details>

<details>
<summary><strong>3. What is the difference between a planned switchover and an unplanned failover in an Aurora Global Database?</strong></summary>

- **Planned Switchover (`switchover_global_cluster`)**: Used for routine maintenance or scheduled DR testing. Aurora synchronizes all pending storage transactions from the primary to the secondary region, ensuring **zero data loss**. The secondary region is promoted to read-write while the former primary becomes a secondary replica.
- **Unplanned Failover (`failover_global_cluster`)**: Used during catastrophic, unrecoverable regional disasters. Aurora immediately breaks the global cluster replication relationship and promotes the secondary cluster to a standalone read-write database. Any un-replicated data in-flight during the outage is lost (bounded by RPO).
</details>

<details>
<summary><strong>4. Why is AWS Global Accelerator often preferred over Route 53 DNS failover for mission-critical disaster recovery?</strong></summary>

Route 53 DNS failover relies on updating DNS records. Even with a low TTL (e.g., 30 seconds), downstream client applications, mobile operating systems, corporate DNS forwarders, and public ISPs frequently cache DNS entries for minutes or hours, ignoring the TTL and continuing to send traffic to the failed region. **AWS Global Accelerator** provides static Anycast IP addresses routed over the AWS global edge network. When a regional health check fails, Global Accelerator automatically shifts ingress traffic to healthy endpoints in the alternate region in **under 30 seconds**, completely bypassing DNS resolver caching limitations.
</details>

<details>
<summary><strong>5. How does DynamoDB Global Tables resolve write conflicts across multiple active regions?</strong></summary>

DynamoDB Global Tables uses a **Last-Write-Wins (LWW)** conflict resolution algorithm. When concurrent writes update the same item in two different regions nearly simultaneously, DynamoDB compares the internal timestamp of the writes. The write with the most recent timestamp is persisted across all regions, overwriting the earlier write. Applications requiring stricter concurrency control must implement logical version numbers or write through a single active designated leader region.
</details>

<details>
<summary><strong>6. What is a "Split-Brain" disaster scenario and how do you protect against it?</strong></summary>

Split-brain occurs when a secondary region is promoted to primary because it incorrectly believes the primary region has failed (e.g., due to a temporary cross-region network partition), while the original primary is still healthy and accepting writes. This causes both regions to diverge independently, creating conflicting, unreconcilable database state. To protect against split-brain:
1. Use quorum-based external health checkers located in a 3rd independent region.
2. Require multi-factor confirmation or automated fencing (e.g., revoking IAM execution credentials on the primary cluster) before promoting the standby.
</details>

<details>
<summary><strong>7. How do AWS KMS Multi-Region Keys simplify multi-region disaster recovery?</strong></summary>

Standard AWS KMS keys are strictly regional and cannot decrypt ciphertexts generated in other regions. **Multi-Region Keys** consist of a primary key in one region and synchronized replica keys in other regions sharing the exact same key ID and key material. This allows data encrypted at rest (e.g., S3 objects or Aurora database tables) in `us-east-1` to be replicated and decrypted in `us-west-2` using the local replica key without re-encrypting the data over the wire.
</details>

<details>
<summary><strong>8. What are the key cost drivers of a Multi-Region Active-Active architecture?</strong></summary>

1. **Compute Duplication**: Running full-capacity web and application fleets simultaneously in both regions.
2. **Database Overhead**: Provisioning active database clusters and instances in both regions.
3. **Cross-Region Data Egress**: AWS charges standard data transfer fees (typically \$0.02 per GB) for all replication traffic across region boundaries (Aurora, DynamoDB, S3, SQS).
4. **Operational Tooling**: Duplicated observability stacks, log ingestion pipelines, and multi-region testing infrastructure.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Challenge 1: Configure Route 53 DNS Failover with Health Checks</strong></summary>

**Goal**: Provision two S3 static websites (or ALBs) in `us-east-1` and `us-west-2`. Create a Route 53 primary failover record pointing to region 1 and a secondary record pointing to region 2.
**Verification**: Terminate or disable the health check endpoint in region 1 and verify with `dig` or `curl` that DNS resolves to region 2 within 60 seconds.
</details>

<details>
<summary><strong>Challenge 2: Deploy DynamoDB Global Table Across Two Regions</strong></summary>

**Goal**: Provision a DynamoDB table in `us-east-1` with a replica in `eu-west-1` using Terraform.
**Verification**: Put an item into the `us-east-1` table using the AWS CLI and verify that the item appears in the `eu-west-1` table within 2 seconds.
</details>

<details>
<summary><strong>Challenge 3: Configure S3 Cross-Region Replication with S3 RTC</strong></summary>

**Goal**: Create two versioned S3 buckets in different regions. Configure CRR with Replication Time Control (RTC) and an IAM replication role.
**Verification**: Upload a file to the primary bucket; verify that the object appears in the DR bucket and check the `ReplicationStatus` metadata attribute.
</details>

<details>
<summary><strong>Challenge 4: Deploy CloudFront Origin Groups for Ingress Redundancy</strong></summary>

**Goal**: Configure a CloudFront distribution with an Origin Group composed of two regional Application Load Balancers.
**Verification**: Return HTTP 502 from the primary ALB; confirm CloudFront transparently fails over and serves the response from the secondary ALB.
</details>

<details>
<summary><strong>Challenge 5: Simulate Pilot Light Scale-Up via EventBridge and Lambda</strong></summary>

**Goal**: Build a Lambda function that triggers an Auto Scaling Group in `us-west-2` to scale `desired_capacity` from 0 to 5 upon receiving a custom disaster trigger event.
**Verification**: Send a test event to EventBridge; confirm that EC2 instances launch and register as healthy in the secondary target group.
</details>

<details>
<summary><strong>Challenge 6: Set Up AWS Backup Cross-Region Copy Rule</strong></summary>

**Goal**: Configure an AWS Backup Vault and Plan in `us-east-1` that automatically copies daily EBS and RDS snapshots to a secondary vault in `us-west-2`.
**Verification**: Run an on-demand backup job; confirm the recovery point appears in the secondary region backup vault.
</details>

---

## 12. Summary / Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
|                               DISASTER RECOVERY ARCHITECTURE CHEAT SHEET                           |
+----------------------------------------------------------------------------------------------------+
| DR Pattern        | Target RTO   | Target RPO   | Compute State in DR       | Data Replication     |
+-------------------+--------------+--------------+---------------------------+----------------------+
| Backup & Restore  | 24+ hours    | 12-24 hours  | 0% (Rebuilt on demand)    | Snapshot / AWS Backup|
| Pilot Light       | 10-30 mins   | < 1 minute   | 0% (ASG desired = 0)      | Aurora / DynamoDB    |
| Warm Standby      | < 5 mins     | Seconds      | 20-30% (Scaled down live) | Aurora / DynamoDB    |
| Active-Active     | ~0 (Instant) | Real-time    | 100% (Both serve traffic) | Global Tables / DB   |
+-------------------+--------------+--------------+---------------------------+----------------------+
| Ingress Routing   | Global Accelerator (bypasses DNS TTL) or Route 53 Failover Policy (TTL=30s)    |
| Encryption        | Use AWS KMS Multi-Region Keys to decrypt cross-region payloads transparently   |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Reliability Pillar - AWS Well-Architected Framework](https://docs.aws.amazon.com/wellarchitected/latest/reliability-pillar/)
- [Disaster Recovery of Workloads on AWS: Recovery in the Cloud Whitepaper](https://docs.aws.amazon.com/whitepapers/latest/disaster-recovery-workloads-on-aws/)
- [Amazon Aurora Global Database User Guide](https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/aurora-global-database.html)
- [Amazon DynamoDB Global Tables Developer Guide](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/GlobalTables.html)
- [AWS Global Accelerator Architectural Overview](https://docs.aws.amazon.com/global-accelerator/latest/dg/what-is-global-accelerator.html)
