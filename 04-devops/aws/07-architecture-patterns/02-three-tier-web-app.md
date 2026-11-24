# Enterprise Three-Tier Web Application Architecture on AWS

> **Module**: `07-architecture-patterns`  
> **Topic**: Presentation, Application & Data Tiers, Multi-AZ High Availability & Security Group Chaining  
> **Target Audience**: Cloud Architects, Systems Engineers, Full-Stack Leads  
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

The **Three-Tier Web Application Architecture** is the foundational software design pattern of modern enterprise computing. By physically and logically separating a system into three decoupled tiers—**Presentation (Web)**, **Logic (Application)**, and **Data (Database)**—organizations achieve:
1. **Isolated Security Perimeters**: The database tier is never exposed to the public internet; it can only be reached by authorized application instances.
2. **Independent Scalability**: Sudden surges in web page views scale the web/application tier horizontally without requiring database re-provisioning.
3. **High Availability & Fault Tolerance**: Deploying each tier across multiple independent Availability Zones ensures zero downtime even during physical data center power cuts or hardware failures.

In AWS, the three-tier pattern is realized using cloud-native managed services: **Route 53 + CloudFront + ALB** (Presentation), **Auto-Scaled EC2 / ECS Fargate** (Logic), and **Amazon Aurora Multi-AZ + ElastiCache Redis** (Data).

---

## 2. Core Concepts

### 2.1 The Three Subnet Tiers
A production VPC is partitioned across at least two (preferably three) Availability Zones into three distinct subnet layers:
1. **Public Subnets (Web / Ingress Tier)**:
   - Houses the public **Application Load Balancers (ALB)** and **NAT Gateways**.
   - Route table routes `0.0.0.0/0` directly to an **Internet Gateway (IGW)**.
   - Workload instances (containers/EC2) are **never** placed here.
2. **Private Application Subnets (Logic Tier)**:
   - Houses the compute engines: **EC2 Auto Scaling Groups** or **ECS Fargate tasks**.
   - Route table routes `0.0.0.0/0` to the **NAT Gateways** in the public subnets for outbound internet egress (downloading security patches, calling third-party APIs).
   - Zero public IP addresses assigned.
3. **Isolated Database Subnets (Data Tier)**:
   - Houses **Amazon Aurora / RDS Multi-AZ** and **Amazon ElastiCache Redis**.
   - Route table has **NO internet route** (`0.0.0.0/0` is unrouted). Completely air-gapped from the internet. Accessible strictly within the VPC.

### 2.2 Security Group Chaining (Defense-in-Depth)
Instead of filtering traffic using static IP CIDR blocks (which change as servers scale), tiers authorize traffic using **Security Group References**:
- `ALB Security Group`: Ingress allows `0.0.0.0/0` on port 443 (HTTPS).
- `App Security Group`: Ingress allows port 80/8080 **ONLY from the ALB Security Group ID**.
- `Database Security Group`: Ingress allows port 5432 (Postgres) **ONLY from the App Security Group ID**.
- Result: Even if an attacker compromises a machine inside the VPC, they cannot query the database unless the packet originates from a valid application security group.

### 2.3 Caching Tier (In-Memory Acceleration)
- **Amazon ElastiCache (Redis)** sits in front of the database tier.
- Implements the **Cache-Aside Pattern**: Application checks Redis first; on a cache miss, it reads from Aurora and populates Redis, reducing relational database query load by up to 90%.

### 2.4 Secure Remote Administration
- **Eliminate SSH Bastion Hosts**: Bastion jump boxes with open port 22 create security liabilities.
- **Modern Standard**: Use **AWS Systems Manager Session Manager (SSM)**. Engineers open interactive terminal sessions over encrypted HTTPS tunnels authenticated via IAM Identity Center (SSO), with zero open inbound ports and full session recording in S3/CloudWatch.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        ENTERPRISE THREE-TIER MULTI-AZ ARCHITECTURE                                |
|                                                                                                   |
|  [Global Web Client]                                                                              |
|         |                                                                                         |
|         v (DNS Query: app.enterprise.com)                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | AMAZON ROUTE 53 (Anycast Global DNS)                                                        |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (HTTPS Request: Edge Caching & WAF Inspection)                                          |
|  +---------------------------------------------------------------------------------------------+  |
|  | AMAZON CLOUDFRONT CDN + AWS WAF (Web ACL: SQLi & Bot Protection)                            |  |
|  +---------------------------------------------------------------------------------------------+  |
|         | (AWS Global Fiber Backbone Transit)                                                     |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | VPC (10.0.0.0/16) - MULTI-AZ TOPOLOGY                                                        |  |
|  |                                                                                             |  |
|  | ========================= TIER 1: PUBLIC WEB / INGRESS TIER =============================== |  |
|  |   Availability Zone A (10.0.1.0/24)             Availability Zone B (10.0.2.0/24)           |  |
|  |   +-------------------------------+             +-------------------------------+           |  |
|  |   | Public ALB Node               |             | Public ALB Node               |           |  |
|  |   | NAT Gateway A                 |             | NAT Gateway B                 |           |  |
|  |   +---------------+---------------+             +---------------+---------------+           |  |
|  |                   |                                             |                           |  |
|  | ==================|======== TIER 2: PRIVATE APPLICATION TIER ===|========================== |  |
|  |                   v                                             v                           |  |
|  |   Availability Zone A (10.0.10.0/24)            Availability Zone B (10.0.20.0/24)          |  |
|  |   +-------------------------------+             +-------------------------------+           |  |
|  |   | EC2 / ECS App Worker (Node 1) | <---ASG---> | EC2 / ECS App Worker (Node 2) |           |  |
|  |   | (SG: Allow ALB SG on 8080)    |             | (SG: Allow ALB SG on 8080)    |           |  |
|  |   +---------------+---------------+             +---------------+---------------+           |  |
|  |                   |                                             |                           |  |
|  | ==================|======== TIER 3: ISOLATED DATABASE TIER =====|========================== |  |
|  |                   v                                             v                           |  |
|  |   Availability Zone A (10.0.100.0/24)           Availability Zone B (10.0.200.0/24)         |  |
|  |   +-------------------------------+             +-------------------------------+           |  |
|  |   | Aurora Primary (Writer)       | <--Sync---> | Aurora Read Replica (Reader)  |           |  |
|  |   | ElastiCache Redis Primary     |             | ElastiCache Redis Replica     |           |  |
|  |   | (SG: Allow App SG on 5432)    |             | (SG: Allow App SG on 5432)    |           |  |
|  |   +-------------------------------+             +-------------------------------+           |  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 CLI Commands for Deploying and Querying Three-Tier Components

```bash
# 1. Connect to an application instance securely without open port 22 or bastion hosts
aws ssm start-session \
    --target "i-0123456789abcdef0"

# 2. Check Aurora Database Cluster health and Multi-AZ endpoints
aws rds describe-db-clusters \
    --db-cluster-identifier "production-aurora-cluster" \
    --query "DBClusters[*].[Status, Endpoint, ReaderEndpoint, MultiAZ]"

# 3. Check Target Group health status for application instances
aws elbv2 describe-target-health \
    --target-group-arn "arn:aws:elasticloadbalancing:us-east-1:123456789012:targetgroup/app-tg/12345"
```

---

## 5. Practical Examples

### 5.1 Complete Security Group Chaining Implementation (Terraform)

```hcl
# 1. Application Load Balancer Security Group (Public Edge)
resource "aws_security_group" "alb_sg" {
  name        = "prod-alb-sg"
  description = "Accept public HTTPS traffic"
  vpc_id      = var.vpc_id

  ingress {
    description = "Public HTTPS"
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

# 2. Application Tier Security Group (Private)
resource "aws_security_group" "app_sg" {
  name        = "prod-app-sg"
  description = "Accept traffic ONLY from ALB"
  vpc_id      = var.vpc_id

  ingress {
    description     = "HTTP from ALB"
    from_port       = 8080
    to_port         = 8080
    protocol        = "tcp"
    security_groups = [aws_security_group.alb_sg.id] # Chained Reference!
  }

  egress {
    description = "Egress to internet via NAT Gateway"
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

# 3. Database Tier Security Group (Isolated)
resource "aws_security_group" "db_sg" {
  name        = "prod-database-sg"
  description = "Accept PostgreSQL traffic ONLY from App tier"
  vpc_id      = var.vpc_id

  ingress {
    description     = "PostgreSQL from App Tier"
    from_port       = 5432
    to_port         = 5432
    protocol        = "tcp"
    security_groups = [aws_security_group.app_sg.id] # Chained Reference!
  }

  egress {
    description = "No outbound egress required"
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["127.0.0.1/32"] # Block all egress
  }
}
```

### 5.2 Application Tier Auto Scaling Group with Health Check Draining (Terraform)

```hcl
resource "aws_launch_template" "app_template" {
  name_prefix   = "prod-app-lt-"
  image_id      = var.ami_id
  instance_type = "c6g.xlarge" # AWS Graviton3

  vpc_security_group_ids = [aws_security_group.app_sg.id]

  iam_instance_profile {
    name = aws_iam_instance_profile.ssm_profile.name
  }

  user_data = base64encode(<<-EOF
              #!/bin/bash
              systemctl start app.service
              EOF
  )

  metadata_options {
    http_endpoint               = "enabled"
    http_tokens                 = "required" # Enforce IMDSv2!
    http_put_response_hop_limit = 1
  }
}

resource "aws_autoscaling_group" "app_asg" {
  name_prefix         = "prod-app-asg-"
  vpc_zone_identifier = var.private_app_subnet_ids
  target_group_arns   = [aws_lb_target_group.app_tg.arn]

  min_size         = 2
  max_size         = 10
  desired_capacity = 4

  health_check_type         = "ELB" # ELB health check replaces EC2 status
  health_check_grace_period = 300

  launch_template {
    id      = aws_launch_template.app_template.id
    version = "$Latest"
  }

  instance_refresh {
    strategy = "Rolling"
    preferences {
      min_healthy_percentage = 50
    }
  }
}
```

### 5.3 Amazon Aurora PostgreSQL Multi-AZ Cluster (Terraform)

```hcl
resource "aws_db_subnet_group" "db_subnets" {
  name       = "prod-isolated-db-subnet-group"
  subnet_ids = var.isolated_db_subnet_ids
}

resource "aws_rds_cluster" "aurora" {
  cluster_identifier      = "prod-aurora-postgres-cluster"
  engine                  = "aurora-postgresql"
  engine_version          = "16.1"
  database_name           = "enterprisedb"
  master_username         = "dbadmin"
  manage_master_user_password = true # Auto-managed in Secrets Manager!
  vpc_security_group_ids  = [aws_security_group.db_sg.id]
  db_subnet_group_name    = aws_db_subnet_group.db_subnets.name
  deletion_protection     = true
  storage_encrypted       = true
}

# 1 Primary Writer
resource "aws_rds_cluster_instance" "writer" {
  identifier         = "prod-aurora-instance-1"
  cluster_identifier = aws_rds_cluster.aurora.id
  instance_class     = "db.r6g.xlarge"
  engine             = aws_rds_cluster.aurora.engine
}

# 1 Read Replica in secondary AZ for automated 30s failover
resource "aws_rds_cluster_instance" "reader" {
  identifier         = "prod-aurora-instance-2"
  cluster_identifier = aws_rds_cluster.aurora.id
  instance_class     = "db.r6g.xlarge"
  engine             = aws_rds_cluster.aurora.engine
}
```

---

## 6. Best Practices

1. **Enforce Security Group Chaining**: Never use CIDR ranges (like `10.0.0.0/16`) to authorize database traffic. Authorize database ingress strictly via the Application Security Group ID (`security_groups = [aws_security_group.app_sg.id]`).
2. **Deploy Redundant NAT Gateways Across AZs**: For production workloads, deploy one NAT Gateway per Availability Zone. Placing a single NAT Gateway in AZ-A creates a single point of failure: an outage in AZ-A severs internet egress for all applications in AZ-B.
3. **Use ELB Health Checks on Auto Scaling Groups**: Change the ASG `health_check_type` from `EC2` to `ELB`. An EC2 status check detects only hardware or hypervisor crashes; an ELB health check verifies that the web application process is actually responding with HTTP 200 to `/healthz`.
4. **Enforce IMDSv2 on All Compute Nodes**: Require `http_tokens = "required"` in your launch templates to neutralize SSRF (Server-Side Request Forgery) attacks targeting IAM instance credentials.
5. **Separate Reader and Writer Connections in Application Code**: Configure application connection pools to send `SELECT` queries to the Aurora **Reader Endpoint**, reserving the **Cluster (Writer) Endpoint** for `INSERT`/`UPDATE`/`DELETE`.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Placing Databases in Public Subnets | RDS deployed in public subnets with public IP enabled. | Database exposed to brute-force internet attacks. | Place RDS in **Isolated Subnets** with zero public routes. |
| Hardcoding CIDR Blocks Between Tiers | Using `10.0.10.0/24` in database security group rules. | New subnets cannot connect; IP expansion breaks rules. | Use **Security Group References** (`aws_security_group.app_sg.id`). |
| Single NAT Gateway Single Point of Failure | Provisioning only 1 NAT Gateway for cost savings in production. | Outage in AZ-A drops internet egress for all private subnets in AZ-B. | Deploy a dedicated NAT Gateway per Availability Zone. |
| Neglecting Application Graceful Draining | Terminating instances immediately on ASG scale-in. | Active user HTTP requests dropped with TCP resets. | Configure ALB `deregistration_delay` and instance termination lifecycle hooks. |
| Bastion Host SSH Key Leakage | Exposing port 22 on bastion hosts to the public internet. | Vulnerable to credential theft and brute force. | Eliminate bastions; use **AWS Systems Manager Session Manager**. |
| Database Connection Starvation | Scaling out 50 application nodes, each opening 100 connections. | 5,000 connections exhaust database memory and crash DB. | Deploy **Amazon RDS Proxy** to pool and multiplex database connections. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance Optimization
- **Amazon RDS Proxy**: Pools and shares thousands of database connections, preventing connection exhaustion during traffic surges and reducing failover times by up to 66%.
- **ElastiCache Read Offload**: Caches static catalog data and user session state in Redis, maintaining sub-millisecond p99 latency under heavy load.

### 8.2 Security
- **Air-Gapped Isolation**: The data tier has no route table entry for an Internet Gateway or NAT Gateway. Even if malicious malware infects an application server, data cannot be directly exfiltrated to the public internet from the database.

### 8.3 Cost Breakdown
- **Application Load Balancer**: \$0.0225/hour + \$0.008/LCU-hour (~$25/month).
- **NAT Gateways (2 AZs)**: \$0.045/hour $\times$ 2 = ~\$65/month + \$0.045/GB processed.
- **Aurora Cluster**: Two `db.r6g.xlarge` instances = ~\$380/month.
- **Compute Tier**: Auto-scaled `c6g.xlarge` On-Demand / Savings Plans.

---

## 9. Comparison Table

| Architecture Pattern | Scalability | Blast Radius / Isolation | Operational Overhead |
|---|---|---|---|
| **Monolith on Single EC2** | Low (Vertical only) | High (Single point of failure) | Low (Single box) |
| **Two-Tier (Web + DB)** | Moderate (Web scales, DB static)| Moderate | Moderate |
| **Three-Tier (Web/App/DB)** | **High (Each tier scales independently)**| **Low (Air-gapped security boundaries)**| **High (Standard enterprise baseline)**|
| **Serverless (Lambda/DynamoDB)**| Extreme (Instant zero-to-scale) | Extreme (Per-function isolation) | Lowest (Serverless managed) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: Why are database subnets isolated with no route to an Internet Gateway or NAT Gateway?</strong></summary>

Database tiers have no legitimate architectural need to communicate directly with the public Internet:
1. Databases should never be queried directly by public clients.
2. Database engines (like Aurora or RDS) receive software patches and updates managed directly by the AWS control plane through internal AWS hypervisor fabrics, meaning they do not need internet access to download updates.
By placing databases in **Isolated Subnets** with no route to `0.0.0.0/0`, the database is completely air-gapped from internet-based attacks, brute-force exploits, and unauthorized data exfiltration.
</details>

<details>
<summary><strong>Q2: How does Security Group Chaining eliminate the risks of IP-based firewall rules?</strong></summary>

In dynamic cloud environments, compute instances scale up and down, changing IP addresses continuously. If an administrator allows port 5432 from a subnet CIDR (e.g., `10.0.10.0/24`), any rogue instance launched into that subnet can connect to the database. 
With **Security Group Chaining**, the database security group permits ingress on port 5432 **only if the incoming packet carries the Application Security Group ID**. AWS virtual firewalls evaluate the ENI security group membership at the hypervisor layer. Even if an attacker launches a host in the same subnet, they cannot connect to the database unless their instance explicitly assumes the authorized application security group.
</details>

<details>
<summary><strong>Q3: What is the role of Amazon RDS Proxy in a high-concurrency three-tier architecture?</strong></summary>

Relational databases like PostgreSQL and MySQL allocate dedicated memory threads for every open connection (e.g., ~10 MB per connection). If an Auto Scaling group scales from 4 to 40 application instances, each opening a connection pool of 50 connections, the database faces 2,000 concurrent connections, leading to memory starvation and crashes.
**Amazon RDS Proxy**:
1. Pools and multiplexes database connections, allowing thousands of application requests to share a small pool of database connections.
2. Preserves database memory and CPU.
3. Automatically routes traffic to a newly promoted read replica during a Multi-AZ failover, cutting failover downtime by up to 66%.
</details>

<details>
<summary><strong>Q4: Why should an Auto Scaling Group use ELB health checks instead of standard EC2 health checks?</strong></summary>

- **EC2 Health Checks**: Only verify the physical hardware and OS hypervisor state (checking network reachability and host power). If the application server process (e.g., Nginx, Node.js, Spring Boot) crashes, encounters an unhandled exception, or locks up, the EC2 instance status remains `Healthy`.
- **ELB Health Checks**: The Application Load Balancer periodically issues HTTP synthetic requests (`GET /healthz`) directly to the application process. If the application process fails, times out, or returns a 500 status code, ELB flags the instance as `Unhealthy`. The Auto Scaling group automatically terminates the failed instance and launches a fresh replacement.
</details>

<details>
<summary><strong>Q5: How does AWS Systems Manager Session Manager eliminate the need for Bastion Hosts?</strong></summary>

Traditional architectures deployed an EC2 "bastion host" in a public subnet with port 22 open to the internet. This created severe security liabilities: managing SSH key pairs, monitoring open ports for brute-force attacks, and maintaining host patching.
**SSM Session Manager**:
1. Uses an outbound HTTPS connection (port 443) initiated by the SSM Agent inside the private instance to the AWS Systems Manager endpoint.
2. **No inbound ports are open** (port 22 is completely blocked).
3. Authentication is handled via corporate IAM Identity Center (SSO) with MFA.
4. Every keystroke and terminal session is logged to S3 and CloudWatch for compliance audits.
</details>

<details>
<summary><strong>Q6: How does Aurora Multi-AZ failover differ from standard RDS Multi-AZ?</strong></summary>

- **Standard RDS Multi-AZ**: Uses block-level synchronous replication to a standby instance in another AZ. During a failure, the standby is promoted, and DNS CNAME records flip, which typically takes **60 to 120 seconds**.
- **Amazon Aurora Multi-AZ**: Uses a shared, distributed storage volume replicated 6 ways across 3 AZs. When the primary writer crashes, an existing Read Replica is promoted to writer, which takes **under 30 seconds** because no storage replication synchronization or block recovery is required.
</details>

<details>
<summary><strong>Q7: What is the purpose of an Application Load Balancer Deregistration Delay?</strong></summary>

When an Auto Scaling group scales in or replaces an instance during a rolling deployment, the instance is deregistered from the ALB Target Group. 
The **Deregistration Delay** (Connection Draining) gives active, in-flight HTTP requests a grace period (e.g., 30 to 60 seconds) to complete processing. The ALB stops sending new requests to the instance while allowing existing connections to finish cleanly before terminating the instance, preventing dropped connections.
</details>

<details>
<summary><strong>Q8: Why is Multi-AZ NAT Gateway deployment critical for three-tier reliability?</strong></summary>

If an architecture deploys private subnets across AZ-A and AZ-B, but provisions only a single NAT Gateway in AZ-A:
If AZ-A suffers a network disruption, the instances in AZ-B lose outbound internet connectivity. If the application needs to call external APIs (like Stripe or Twilio), the entire application fails across all AZs. Deploying an independent NAT Gateway in each public subnet ensures that each AZ is completely self-contained.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Security Group Chaining in the Console</strong></summary>

**Goal**: Verify that security groups reference other security groups.
1. Create `alb-sg` allowing inbound HTTPS from `0.0.0.0/0`.
2. Create `app-sg` allowing inbound port 8080 from `alb-sg`.
3. Create `db-sg` allowing inbound port 5432 from `app-sg`.
4. Verify the source column displays the Security Group ID.
</details>

<details>
<summary><strong>Exercise 2: Connect to a Private EC2 Instance with SSM Session Manager</strong></summary>

**Goal**: Establish a terminal session with zero open inbound ports.
1. Launch an EC2 instance in a private subnet with no public IP and no open inbound rules.
2. Attach IAM role with policy `AmazonSSMManagedInstanceCore`.
3. Open the AWS Systems Manager console -> Fleet Manager -> Node Actions -> Start Session.
4. Confirm shell access.
</details>

<details>
<summary><strong>Exercise 3: Test Multi-AZ Aurora Failover</strong></summary>

**Goal**: Measure database failover downtime.
1. Deploy an Aurora PostgreSQL cluster with 1 writer and 1 reader.
2. In the RDS Console, select the writer instance and choose `Actions -> Failover`.
3. Monitor application connection logs and verify failover completes in under 30 seconds.
</details>

<details>
<summary><strong>Exercise 4: Configure Auto Scaling Group ELB Health Checks</strong></summary>

**Goal**: Trigger instance replacement on application process failure.
1. Create an ASG with `health_check_type: ELB`.
2. SSH/SSM into an instance and stop the web process (`systemctl stop nginx`).
3. Observe the ALB mark the target unhealthy, and observe the ASG terminate and replace the failed instance.
</details>

<details>
<summary><strong>Exercise 5: Implement S3 Gateway Endpoint to Eliminate NAT Charges</strong></summary>

**Goal**: Route S3 traffic over the private AWS backbone.
1. Create a VPC Gateway Endpoint for Amazon S3.
2. Associate the endpoint with private route tables.
3. Upload files to S3 from a private instance; verify packets bypass the NAT Gateway.
</details>

<details>
<summary><strong>Exercise 6: Test Rolling Deployment Instance Refresh</strong></summary>

**Goal**: Deploy a new AMI without downtime.
1. Update the Launch Template with a new AMI or user data script.
2. Initiate an **Instance Refresh** on the ASG with `min_healthy_percentage: 50`.
3. Continuously curl the ALB endpoint and verify zero HTTP 5xx errors occur during the rolling update.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| THREE-TIER SUBNET & SECURITY MATRIX                                                               |
+--------------------------+-----------------------+-----------------------+------------------------+
| Tier                     | Subnet Type           | Route Table Egress    | Inbound Security Rule  |
+--------------------------+-----------------------+-----------------------+------------------------+
| Tier 1: Web / Ingress    | Public Subnets        | 0.0.0.0/0 -> IGW      | Port 443 from 0.0.0.0/0|
| Tier 2: Logic / App      | Private Subnets       | 0.0.0.0/0 -> NAT GW   | Port 8080 from ALB-SG  |
| Tier 3: Database / Cache | Isolated Subnets      | No Internet Route     | Port 5432 from App-SG  |
+--------------------------+-----------------------+-----------------------+------------------------+
```

- **Security Group Chaining**: Authorizes traffic by security group ID, never static IPs.
- **Air-Gapped Database**: Isolated subnets prevent internet data exfiltration.
- **SSM Session Manager**: Zero open inbound ports; eliminates legacy SSH bastions.

---

## 13. Further Reading

- [AWS Architecture Center: Classic Three-Tier Web Architecture](https://aws.amazon.com/architecture/)
- [Amazon Aurora Under the Hood: Distributed Storage](https://aws.amazon.com/blogs/database/amazon-aurora-under-the-hood-quorum-and-correlated-failure/)
- [Amazon RDS Proxy Best Practices](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/rds-proxy.html)
- [AWS Systems Manager Session Manager Documentation](https://docs.aws.amazon.com/systems-manager/latest/userguide/session-manager.html)
- [04-networking/01-vpc.md](file:///04-devops/aws/04-networking/01-vpc.md)
