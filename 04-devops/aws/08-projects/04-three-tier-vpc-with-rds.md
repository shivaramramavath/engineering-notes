---
project: "Enterprise Three-Tier VPC with Amazon Aurora PostgreSQL"
module: "08-projects"
track: "aws"
difficulty: "advanced"
architecture: "three-tier-enterprise-network"
last-tested: "2026-11"
tags:
  - "vpc"
  - "three-tier"
  - "rds"
  - "aurora"
  - "security-groups"
  - "terraform"
  - "project"
---

# Enterprise Three-Tier VPC with Amazon Aurora PostgreSQL

## Table of Contents
- [1. Project Topology & Specifications](#1-project-topology--specifications)
- [2. Declarative Code (Terraform)](#2-declarative-code-terraform)
- [3. Verification & Automated Test Harness](#3-verification--automated-test-harness)
- [4. Hardening, Security & Cost Analysis](#4-hardening-security--cost-analysis)
- [5. Operational Runbook & Disaster Recovery](#5-operational-runbook--disaster-recovery)

---

## 1. Project Topology & Specifications

### 1.1 Architecture Overview
This production project implements the foundational enterprise **Three-Tier VPC Architecture** across multiple Availability Zones on AWS:
1. **Tier 1: Public Subnets (Presentation/Ingress)**:
   - Houses internet-facing Application Load Balancers (ALB) and NAT Gateways.
   - Direct route to the Internet Gateway (`igw`).
2. **Tier 2: Private Application Subnets (Compute/Business Logic)**:
   - Houses application compute instances (EC2 Auto Scaling Groups or ECS tasks).
   - Outbound internet access is routed strictly through NAT Gateways for software patching and third-party APIs. Zero inbound internet access.
3. **Tier 3: Isolated Database Subnets (Data Persistence)**:
   - Houses **Amazon Aurora PostgreSQL** (Multi-AZ cluster).
   - **Zero Internet Routes**: These subnets have no route to an Internet Gateway or NAT Gateway. They are physically incapable of reaching or being reached by the public internet.
4. **Chained Security Groups**:
   - The Database SG permits TCP port 5432 ingress *only* from the Application SG.
   - The Application SG permits ingress *only* from the ALB SG.

```
+----------------------------------------------------------------------------------------------------+
|                         ENTERPRISE THREE-TIER VPC NETWORK TOPOLOGY                                 |
+----------------------------------------------------------------------------------------------------+

  [ Internet Traffic ]
           |
           v
  +--------------------------------------------------------------------------------------------------+
  | VPC: 10.0.0.0/16                                                                                 |
  |                                                                                                  |
  |  ============================= TIER 1: PUBLIC INGRESS SUBNETS ================================   |
  |  Subnet Public-1a (10.0.1.0/24)                    Subnet Public-1b (10.0.2.0/24)                |
  |  - Internet-Facing ALB                             - Internet-Facing ALB                         |
  |  - NAT Gateway 1a                                  - NAT Gateway 1b                              |
  |  Routes: 0.0.0.0/0 -> Internet Gateway (igw)                                                     |
  |  =============================================================================================   |
  |            | (Port 80/443)                                       | (Port 80/443)                 |
  |            v                                                     v                               |
  |  ============================= TIER 2: PRIVATE APPLICATION SUBNETS ===========================   |
  |  Subnet App-1a (10.0.11.0/24)                      Subnet App-1b (10.0.12.0/24)                  |
  |  - EC2 / ECS App Fleet                             - EC2 / ECS App Fleet                         |
  |  Routes: 0.0.0.0/0 -> NAT Gateway                                                                |
  |  Security Group: Ingress ONLY from Tier 1 ALB SG                                                 |
  |  =============================================================================================   |
  |            | (Port 5432)                                         | (Port 5432)                   |
  |            v                                                     v                               |
  |  ============================= TIER 3: ISOLATED DATABASE SUBNETS =============================   |
  |  Subnet DB-1a (10.0.21.0/24)                       Subnet DB-1b (10.0.22.0/24)                   |
  |  - Aurora Primary (Read/Write)                     - Aurora Replica (Multi-AZ Standby)           |
  |  Routes: Local VPC Only (NO Internet, NO NAT Gateway)                                            |
  |  Security Group: Ingress ONLY from Tier 2 App SG                                                 |
  |  =============================================================================================   |
  +--------------------------------------------------------------------------------------------------+
```

### 1.2 Bill of Materials
- `aws_vpc`: Dedicated enterprise CIDR `10.0.0.0/16`.
- `aws_subnet` (6 subnets): 2 public, 2 private application, 2 isolated database across `us-east-1a` and `us-east-1b`.
- `aws_internet_gateway` & `aws_nat_gateway`: Managed ingress and outbound translation.
- `aws_route_table` & associations: Strict route isolation (isolated DB tables contain only local VPC route).
- `aws_security_group`: Tiered chained firewall rules.
- `aws_db_subnet_group`: Attaches Aurora cluster strictly to Tier 3 isolated subnets.
- `aws_rds_cluster` & `aws_rds_cluster_instance`: Aurora PostgreSQL Multi-AZ cluster with storage encryption.

---

## 2. Declarative Code (Terraform)

Create `main.tf`:

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

variable "project_name" {
  type    = string
  default = "three-tier-platform"
}

# -----------------------------------------------------------------------------
# 1. VPC Backbone & Subnets
# -----------------------------------------------------------------------------
resource "aws_vpc" "vpc" {
  cidr_block           = "10.0.0.0/16"
  enable_dns_hostnames = true
  enable_dns_support   = true

  tags = { Name = "${var.project_name}-vpc" }
}

resource "aws_internet_gateway" "igw" {
  vpc_id = aws_vpc.vpc.id
  tags   = { Name = "${var.project_name}-igw" }
}

# Tier 1: Public Subnets
resource "aws_subnet" "public_a" {
  vpc_id            = aws_vpc.vpc.id
  cidr_block        = "10.0.1.0/24"
  availability_zone = "us-east-1a"
  tags              = { Name = "${var.project_name}-public-1a" }
}

resource "aws_subnet" "public_b" {
  vpc_id            = aws_vpc.vpc.id
  cidr_block        = "10.0.2.0/24"
  availability_zone = "us-east-1b"
  tags              = { Name = "${var.project_name}-public-1b" }
}

# Tier 2: Private App Subnets
resource "aws_subnet" "app_a" {
  vpc_id            = aws_vpc.vpc.id
  cidr_block        = "10.0.11.0/24"
  availability_zone = "us-east-1a"
  tags              = { Name = "${var.project_name}-app-1a" }
}

resource "aws_subnet" "app_b" {
  vpc_id            = aws_vpc.vpc.id
  cidr_block        = "10.0.12.0/24"
  availability_zone = "us-east-1b"
  tags              = { Name = "${var.project_name}-app-1b" }
}

# Tier 3: Isolated Database Subnets
resource "aws_subnet" "db_a" {
  vpc_id            = aws_vpc.vpc.id
  cidr_block        = "10.0.21.0/24"
  availability_zone = "us-east-1a"
  tags              = { Name = "${var.project_name}-db-1a" }
}

resource "aws_subnet" "db_b" {
  vpc_id            = aws_vpc.vpc.id
  cidr_block        = "10.0.22.0/24"
  availability_zone = "us-east-1b"
  tags              = { Name = "${var.project_name}-db-1b" }
}

# -----------------------------------------------------------------------------
# 2. Gateways & Routing Tables
# -----------------------------------------------------------------------------
resource "aws_eip" "nat_eip" {
  domain = "vpc"
}

resource "aws_nat_gateway" "nat" {
  allocation_id = aws_eip.nat_eip.id
  subnet_id     = aws_subnet.public_a.id
  tags          = { Name = "${var.project_name}-nat" }
}

# Public Route Table
resource "aws_route_table" "public_rt" {
  vpc_id = aws_vpc.vpc.id
  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.igw.id
  }
  tags = { Name = "${var.project_name}-public-rt" }
}

# App Route Table (via NAT)
resource "aws_route_table" "app_rt" {
  vpc_id = aws_vpc.vpc.id
  route {
    cidr_block     = "0.0.0.0/0"
    nat_gateway_id = aws_nat_gateway.nat.id
  }
  tags = { Name = "${var.project_name}-app-rt" }
}

# Database Route Table (ISOLATED - Local route only)
resource "aws_route_table" "db_rt" {
  vpc_id = aws_vpc.vpc.id
  tags   = { Name = "${var.project_name}-db-isolated-rt" }
}

# Route Associations
resource "aws_route_table_association" "pub_a" {
  subnet_id      = aws_subnet.public_a.id
  route_table_id = aws_route_table.public_rt.id
}

resource "aws_route_table_association" "pub_b" {
  subnet_id      = aws_subnet.public_b.id
  route_table_id = aws_route_table.public_rt.id
}

resource "aws_route_table_association" "app_a" {
  subnet_id      = aws_subnet.app_a.id
  route_table_id = aws_route_table.app_rt.id
}

resource "aws_route_table_association" "app_b" {
  subnet_id      = aws_subnet.app_b.id
  route_table_id = aws_route_table.app_rt.id
}

resource "aws_route_table_association" "db_a" {
  subnet_id      = aws_subnet.db_a.id
  route_table_id = aws_route_table.db_rt.id
}

resource "aws_route_table_association" "db_b" {
  subnet_id      = aws_subnet.db_b.id
  route_table_id = aws_route_table.db_rt.id
}

# -----------------------------------------------------------------------------
# 3. Chained Security Groups
# -----------------------------------------------------------------------------
# Tier 1 Security Group (ALB)
resource "aws_security_group" "alb_sg" {
  name        = "${var.project_name}-alb-sg"
  description = "Allows public HTTPS ingress"
  vpc_id      = aws_vpc.vpc.id

  ingress {
    from_port   = 443
    to_port     = 443
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

# Tier 2 Security Group (App Tier)
resource "aws_security_group" "app_sg" {
  name        = "${var.project_name}-app-sg"
  description = "Allows ingress ONLY from ALB SG"
  vpc_id      = aws_vpc.vpc.id

  ingress {
    from_port       = 8080
    to_port         = 8080
    protocol        = "tcp"
    security_groups = [aws_security_group.alb_sg.id]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

# Tier 3 Security Group (Database Tier)
resource "aws_security_group" "db_sg" {
  name        = "${var.project_name}-db-sg"
  description = "Allows PostgreSQL ingress ONLY from App SG"
  vpc_id      = aws_vpc.vpc.id

  ingress {
    from_port       = 5432
    to_port         = 5432
    protocol        = "tcp"
    security_groups = [aws_security_group.app_sg.id]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

# -----------------------------------------------------------------------------
# 4. Amazon Aurora PostgreSQL Database Cluster
# -----------------------------------------------------------------------------
resource "aws_db_subnet_group" "db_subnets" {
  name        = "${var.project_name}-db-subnet-group"
  description = "Isolated database subnet group"
  subnet_ids  = [aws_subnet.db_a.id, aws_subnet.db_b.id]
}

resource "aws_rds_cluster" "aurora" {
  cluster_identifier          = "${var.project_name}-aurora-cluster"
  engine                      = "aurora-postgresql"
  engine_version              = "15.4"
  database_name               = "enterprisedb"
  master_username             = "clusteradmin"
  manage_master_user_password = true # Auto-generated & managed in Secrets Manager
  db_subnet_group_name        = aws_db_subnet_group.db_subnets.name
  vpc_security_group_ids      = [aws_security_group.db_sg.id]
  storage_encrypted           = true
  deletion_protection         = false
  skip_final_snapshot         = true

  backup_retention_period   = 7
  preferred_backup_window   = "03:00-04:00"
  preferred_maintenance_window = "sun:04:30-sun:05:30"
}

# Primary DB Instance
resource "aws_rds_cluster_instance" "aurora_primary" {
  identifier         = "${var.project_name}-aurora-instance-1"
  cluster_identifier = aws_rds_cluster.aurora.id
  instance_class     = "db.t4g.medium"
  engine             = aws_rds_cluster.aurora.engine
  engine_version     = aws_rds_cluster.aurora.engine_version
}

# Multi-AZ Read Replica Instance
resource "aws_rds_cluster_instance" "aurora_replica" {
  identifier         = "${var.project_name}-aurora-instance-2"
  cluster_identifier = aws_rds_cluster.aurora.id
  instance_class     = "db.t4g.medium"
  engine             = aws_rds_cluster.aurora.engine
  engine_version     = aws_rds_cluster.aurora.engine_version
}

output "aurora_endpoint" {
  value = aws_rds_cluster.aurora.endpoint
}

output "aurora_reader_endpoint" {
  value = aws_rds_cluster.aurora.reader_endpoint
}

output "master_user_secret_arn" {
  value = aws_rds_cluster.aurora.master_user_secret[0].secret_arn
}
```

---

## 3. Verification & Automated Test Harness

### 3.1 Deployment Instructions

```bash
terraform init
terraform apply -auto-approve
DB_HOST=$(terraform output -raw aurora_endpoint)
SECRET_ARN=$(terraform output -raw master_user_secret_arn)
```

### 3.2 Network Isolation & Connectivity Verification

```bash
# 1. Verify Database Subnet Route Table has ZERO external gateways
echo "Auditing DB Route Table..."
aws ec2 describe-route-tables \
    --filters "Name=tag:Name,Values=three-tier-platform-db-isolated-rt" \
    --query "RouteTables[0].Routes"
# Expected output: Contains only 1 entry: { DestinationCidrBlock: "10.0.0.0/16", GatewayId: "local" }

# 2. Retrieve master database credentials from AWS Secrets Manager
echo "Retrieving credentials..."
CREDS=$(aws secretsmanager get-secret-value --secret-id "$SECRET_ARN" --query "SecretString" --output text)
echo "Retrieved database secret successfully."

# 3. Verify security group chaining
echo "Verifying Database SG allows ingress ONLY from App SG..."
aws ec2 describe-security-groups \
    --group-ids $(terraform output -raw db_sg_id 2>/dev/null || aws ec2 describe-security-groups --filters "Name=group-name,Values=three-tier-platform-db-sg" --query "SecurityGroups[0].GroupId" --output text) \
    --query "SecurityGroups[0].IpPermissions[0].UserIdGroupPairs"
```

### 3.3 Teardown Script

```bash
terraform destroy -auto-approve
```

---

## 4. Hardening, Security & Cost Analysis

### 4.1 Enterprise Security Hardening
- **Zero Inbound or Outbound Internet to DB Tier**: The database subnets (`10.0.21.0/24`, `10.0.22.0/24`) cannot route to the internet under any circumstances. Even if malicious code were executed on the database server, it could not phone home or download external payloads.
- **Automated Password Management via Secrets Manager**: Uses `manage_master_user_password = true`. AWS automatically generates a 32-character cryptographically random password, stores it in Secrets Manager, and rotates it without hardcoding credentials in Terraform state.
- **KMS Storage Encryption**: Aurora underlying cluster storage is fully encrypted at rest using AES-256 with automated daily snapshot encryption.

### 4.2 Cost Breakdown (Baseline Production Sizing)

| Component | Sizing / Configuration | Estimated Monthly Cost |
|:---|:---|:---|
| **Aurora PostgreSQL Cluster** | 2 $\times$ `db.t4g.medium` instances (Multi-AZ) | \$105.00 |
| **Aurora Storage** | 20 GB Storage + I/O operations | \$3.00 |
| **NAT Gateway** | 1 NAT Gateway (\$0.045/hr) | \$34.00 |
| **VPC Infrastructure** | Subnets, Route Tables, Internet Gateway | \$0.00 (Free) |
| **AWS Secrets Manager** | 1 Secret | \$0.40 |
| **Total Estimated Cost** | **Production Multi-AZ Enterprise Base** | **~\$142.40 / month** |

---

## 5. Operational Runbook & Disaster Recovery

### Common Operational Runbook Procedures

#### Alert 1: Aurora Primary Failover Event
- **Symptoms**: Applications experience transient 10-30 second disconnection errors as Aurora fails over to the standby replica.
- **Verification Runbook**:
  ```bash
  aws rds describe-events \
      --source-identifier "three-tier-platform-aurora-cluster" \
      --source-type "db-cluster" \
      --duration 60
  ```
- **Remediation**: Aurora automatically flips the cluster CNAME endpoint (`aurora_endpoint`) to point to the newly promoted primary instance. Application connection pools using retry logic will automatically reconnect without manual intervention.

#### Disaster Recovery & Point-in-Time Recovery
- **Continuous Backups**: Aurora continuously streams storage changes to Amazon S3. The database can be restored to any minute within the 7-day retention period:
  ```bash
  aws rds restore-db-cluster-to-point-in-time \
      --source-db-cluster-identifier "three-tier-platform-aurora-cluster" \
      --target-db-cluster-identifier "three-tier-platform-restored-cluster" \
      --restore-time $(date -u -d '2 hours ago' +%Y-%m-%dT%H:%M:%SZ) \
      --db-subnet-group-name "three-tier-platform-db-subnet-group"
  ```
