# Amazon VPC Security Groups and Network ACLs: Multi-Tier Defense

> **Module**: `04-networking`  
> **Topic**: Stateful Instance Firewalls vs. Stateless Subnet Firewalls  
> **Target Audience**: Cloud Security Engineers, Network Architects, Systems Administrators  
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

In cloud environments, relying on a single network perimeter is an architectural anti-pattern: if an attacker breaches an edge firewall, an unsegmented internal network allows unrestricted lateral movement across application servers, caches, and databases.

Amazon VPC provides a robust, defense-in-depth security model built on two complementary filtering layers:
1. **Security Groups (Instance Level / Stateful)**: Virtual firewalls attached directly to Elastic Network Interfaces (ENIs). They evaluate traffic on a per-instance basis, enforce stateful connection tracking, and allow fine-grained zero-trust segmentation by referencing other Security Groups.
2. **Network Access Control Lists (NACLs) (Subnet Level / Stateless)**: Perimeter firewalls governing traffic entering and exiting an entire subnet. They evaluate rules in numerical order, support explicit **Deny** rules, and act as an isolated guardrail protecting all resources within a subnet.

Understanding the interaction between stateful Security Groups and stateless NACLs—particularly connection tracking, ephemeral return ports, and rule precedence—is critical for designing resilient, secure cloud perimeters.

---

## 2. Core Concepts

### 2.1 Security Groups: Stateful Virtual Firewalls
- **Attachment**: Attached directly to **Elastic Network Interfaces (ENIs)** (e.g., EC2 instances, Lambda VPC interfaces, RDS database endpoints, ALB nodes).
- **Stateful Connection Tracking**:
  - If an inbound packet is permitted, the outbound response traffic is **automatically allowed**, regardless of outbound rules.
  - If an outbound connection is initiated, the inbound response traffic is automatically allowed.
- **Rule Structure**:
  - **Allow rules only**: You cannot create explicit "Deny" rules. Everything is denied by default unless explicitly allowed.
  - Can reference IP CIDR blocks (e.g., `10.0.1.0/24`) or **Source Security Group IDs** (e.g., `sg-web-servers`).
- **Default Behavior**:
  - Default Security Group: Denies all inbound traffic; allows all outbound traffic (`0.0.0.0/0`).
  - Newly created custom Security Group: Denies all inbound traffic; allows all outbound traffic.

### 2.2 Security Group Chaining (Source SG References)
The most powerful security pattern in AWS:
- Instead of whitelisting static IP addresses (which break when Auto Scaling launches new instances), Security Group rules can reference another Security Group ID as the source:
  ```
  App-SG Inbound: Allow Port 5432 FROM sg-web-servers
  ```
- Any instance possessing the `sg-web-servers` security group is automatically authorized to communicate with the application database, regardless of its private IP address or Availability Zone.

### 2.3 Network Access Control Lists (NACLs): Stateless Subnet Boundaries
- **Attachment**: Associated with an entire **Subnet**. Every subnet must be associated with exactly one NACL.
- **Stateless Nature**:
  - NACLs do not track connection states.
  - A permitted inbound packet has **no bearing on the return traffic**; an explicit outbound rule must permit the response packet, and vice versa.
- **Rule Structure**:
  - Uses **numbered rules** (e.g., `100`, `200`, `300`).
  - Evaluated in **ascending numerical order**; the first matching rule determines whether the packet is accepted or dropped.
  - Contains an unchangeable default rule (`*`) that denies all unmatched traffic.
  - Supports both **Allow** and **Deny** rules.
- **Ephemeral Return Ports**:
  - When a client connects to a server on port 443, the client's OS allocates a temporary high-order port for the response: **TCP 1024–65535**.
  - A stateless NACL must explicitly permit outbound traffic to ephemeral ports `1024–65535` for clients to receive server responses.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Multi-Tier VPC Packet Evaluation Flow                      |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Inbound Internet Client: 198.51.100.50:52412 ]                                 |
|             |                                                                     |
|             v (Destination: 10.0.1.25:443)                                        |
|  ===============================================================================  |
|  [ LAYER 1: SUBNET BOUNDARY - Network ACL (Stateless) ]                           |
|  Rule 100: Allow TCP Port 443 from 0.0.0.0/0 ---------> MATCH: ALLOW!             |
|  Rule *: Deny All Traffic                                                         |
|  ===============================================================================  |
|             |                                                                     |
|             v                                                                     |
|  ===============================================================================  |
|  [ LAYER 2: INSTANCE BOUNDARY - Security Group (Stateful) ]                       |
|  Rule: Allow TCP Port 443 from 0.0.0.0/0 --------------> MATCH: ALLOW!            |
|  * Nitro Connection Tracker creates entry:                                        |
|    [ 198.51.100.50:52412 <-> 10.0.1.25:443 | State: ESTABLISHED ]                 |
|  ===============================================================================  |
|             |                                                                     |
|             v                                                                     |
|  [ Web Server Process Inside EC2 (Nginx / Express / Gunicorn) ]                   |
|             |                                                                     |
|             v (Server Generates HTTP 200 Response Payload)                        |
|  ===============================================================================  |
|  [ RETURN TRAFFIC: Security Group Evaluation ]                                    |
|  * Match found in Nitro Connection Tracker!                                       |
|  * Packet allowed OUTBOUND automatically (Stateful bypass)                        |
|  ===============================================================================  |
|             |                                                                     |
|             v                                                                     |
|  ===============================================================================  |
|  [ RETURN TRAFFIC: Network ACL Evaluation (Stateless) ]                           |
|  * No state tracking! Evaluates outbound NACL rules sequentially:                 |
|  Rule 100: Allow TCP Ephemeral Ports 1024-65535 to 0.0.0.0/0 ---> MATCH: ALLOW!   |
|  (If Rule 100 is missing, packet is dropped by Rule * Deny!)                     |
|  ===============================================================================  |
|             |                                                                     |
|             v                                                                     |
|  [ Packet arrives safely at client browser ]                                      |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating Chained Security Groups via AWS CLI
```bash
# 1. Create ALB Security Group (Allows Inbound HTTPS 443 from Internet)
ALB_SG=$(aws ec2 create-security-group \
  --group-name "alb-public-sg" \
  --description "Security group for public ALB" \
  --vpc-id "vpc-0123456789abcdef0" \
  --query 'GroupId' --output text)

aws ec2 authorize-security-group-ingress \
  --group-id "$ALB_SG" \
  --protocol tcp \
  --port 443 \
  --cidr 0.0.0.0/0

# 2. Create Application Security Group (Allows Inbound Port 8080 ONLY from ALB SG)
APP_SG=$(aws ec2 create-security-group \
  --group-name "app-backend-sg" \
  --description "Security group for backend app tier" \
  --vpc-id "vpc-0123456789abcdef0" \
  --query 'GroupId' --output text)

aws ec2 authorize-security-group-ingress \
  --group-id "$APP_SG" \
  --protocol tcp \
  --port 8080 \
  --source-group "$ALB_SG"

# 3. Create Database Security Group (Allows Inbound PostgreSQL 5432 ONLY from App SG)
DB_SG=$(aws ec2 create-security-group \
  --group-name "database-sg" \
  --description "Security group for Aurora PostgreSQL" \
  --vpc-id "vpc-0123456789abcdef0" \
  --query 'GroupId' --output text)

aws ec2 authorize-security-group-ingress \
  --group-id "$DB_SG" \
  --protocol tcp \
  --port 5432 \
  --source-group "$APP_SG"
```

### 4.2 Blocking an Attacker IP Address via Network ACL Rule
```bash
# Add Rule 50 to drop all traffic from malicious IP 203.0.113.45 before evaluation reaches Rule 100
aws ec2 create-network-acl-entry \
  --network-acl-id "acl-0123456789abcdef0" \
  --rule-number 50 \
  --protocol -1 \
  --rule-action deny \
  --cidr-block "203.0.113.45/32" \
  --ingress
```

---

## 5. Practical Examples

### Example 1: Declarative Terraform 3-Tier Chained Security Group Architecture
Clean implementation of zero-trust security group chaining across Web, App, and Database tiers.

```hcl
# --- TIER 1: PUBLIC LOAD BALANCER ---
resource "aws_security_group" "alb" {
  name        = "alb-security-group"
  description = "Ingress from public internet"
  vpc_id      = var.vpc_id

  ingress {
    description = "Public HTTPS"
    from_port   = 443
    to_port     = 443
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  egress {
    description = "Egress to application tier"
    from_port   = 8080
    to_port     = 8080
    protocol    = "tcp"
    cidr_blocks = [var.vpc_cidr]
  }
}

# --- TIER 2: APPLICATION INSTANCES ---
resource "aws_security_group" "app" {
  name        = "app-backend-security-group"
  description = "Ingress restricted to ALB"
  vpc_id      = var.vpc_id

  ingress {
    description     = "HTTP from ALB SG only"
    from_port       = 8080
    to_port         = 8080
    protocol        = "tcp"
    security_groups = [aws_security_group.alb.id] # Sourced directly from ALB SG
  }

  egress {
    description = "Outbound to database"
    from_port   = 5432
    to_port     = 5432
    protocol    = "tcp"
    cidr_blocks = [var.vpc_cidr]
  }
}

# --- TIER 3: DATABASE CLUSTER ---
resource "aws_security_group" "database" {
  name        = "database-security-group"
  description = "Ingress restricted to App instances"
  vpc_id      = var.vpc_id

  ingress {
    description     = "PostgreSQL from App SG only"
    from_port       = 5432
    to_port         = 5432
    protocol        = "tcp"
    security_groups = [aws_security_group.app.id] # Sourced directly from App SG
  }

  egress {
    description = "No outbound traffic permitted"
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = []
  }
}
```

### Example 2: Hardened Custom Network ACL Configuration in Terraform
Subnet-level perimeter allowing web traffic and ephemeral return ports while explicitly blocking known abusive ranges.

```hcl
resource "aws_network_acl" "public_subnet_acl" {
  vpc_id     = var.vpc_id
  subnet_ids = var.public_subnet_ids

  # 1. Block malicious subnet explicitly
  ingress {
    rule_no    = 50
    action     = "deny"
    cidr_block = "198.51.100.0/24"
    protocol   = "-1"
    from_port  = 0
    to_port    = 0
  }

  # 2. Allow inbound HTTPS (Port 443)
  ingress {
    rule_no    = 100
    action     = "allow"
    cidr_block = "0.0.0.0/0"
    protocol   = "tcp"
    from_port  = 443
    to_port    = 443
  }

  # 3. Allow inbound ephemeral ports (for outbound connections initiated by the subnet)
  ingress {
    rule_no    = 110
    action     = "allow"
    cidr_block = "0.0.0.0/0"
    protocol   = "tcp"
    from_port  = 1024
    to_port    = 65535
  }

  # 4. Outbound HTTPS (to call external APIs)
  egress {
    rule_no    = 100
    action     = "allow"
    cidr_block = "0.0.0.0/0"
    protocol   = "tcp"
    from_port  = 443
    to_port    = 443
  }

  # 5. Outbound ephemeral return ports (MANDATORY for returning web responses)
  egress {
    rule_no    = 110
    action     = "allow"
    cidr_block = "0.0.0.0/0"
    protocol   = "tcp"
    from_port  = 1024
    to_port    = 65535
  }

  tags = { Name = "hardened-public-nacl" }
}
```

### Example 3: Self-Referencing Security Group for Internal Cluster Nodes
Allow nodes in an Elasticsearch, Kafka, or Cassandra cluster to communicate among themselves on internal cluster gossip ports.

```bash
# Create Cluster Security Group
CLUSTER_SG=$(aws ec2 create-security-group \
  --group-name "kafka-cluster-sg" \
  --description "Self-referencing security group for Kafka cluster" \
  --vpc-id "$VPC_ID" \
  --query 'GroupId' --output text)

# Authorize ingress from ITSELF
aws ec2 authorize-security-group-ingress \
  --group-id "$CLUSTER_SG" \
  --protocol tcp \
  --port 9092 \
  --source-group "$CLUSTER_SG"
```

### Example 4: Diagnosing Dropped Packets via Reachability Analyzer
Test whether a security group or NACL is responsible for dropping traffic between two instances.

```bash
aws ec2-network-insights create-network-insights-path \
  --source "i-0123456789abcdef0" \
  --destination "i-0feeedcba98765432" \
  --destination-port 5432 \
  --protocol TCP
```

### Example 5: Resolving Ephemeral Port Starvation in NACLs
Why omitting outbound ephemeral ports breaks `curl` from an EC2 instance:

```bash
# INSIDE EC2 INSTANCE:
curl -I https://aws.amazon.com
# HANGS indefinitely...

# ROOT CAUSE:
# Inbound NACL allowed response packets, but the Outbound NACL had only port 443 allowed.
# The client OS selected local source port 51240. The return packet from AWS was blocked 
# by the Outbound NACL because port 51240 was not covered by the 1024-65535 ephemeral rule!
```

---

## 6. Best Practices

1. **Chain Security Groups via Source IDs, Never Static IPs**: Never hardcode private instance IP addresses in security group rules. Always reference the source security group ID (`security_groups = [aws_security_group.alb.id]`). This ensures auto-scaling fleets immediately communicate without firewall updates.
2. **Leave Network ACLs at Default (Allow All) Unless Explicitly Blocking IPs**: For standard multi-tier architectures, rely on **Security Groups** as the primary security control. Customize NACLs primarily when you need explicit **Deny rules** to block malicious IP ranges or geographic blocks at the subnet edge.
3. **Always Include Ephemeral Ports (1024–65535) in NACLs**: If you configure custom stateless NACLs, always add outbound and inbound rules permitting TCP ports `1024–65535`. Otherwise, response traffic will be dropped and client connections will hang.
4. **Never Open Management Ports (22/3389) to `0.0.0.0/0`**: Eliminate port 22 and 3389 rules completely. Use **AWS Systems Manager Session Manager** for keyless, encrypted shell access over outbound HTTPS (port 443).
5. **Group Rules by Environment and Role**: Create purpose-specific security groups (e.g., `sg-web-alb`, `sg-api-service`, `sg-aurora-postgres`) rather than mega-security groups containing hundreds of mixed rules.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Omitting Ephemeral Ports in Outbound NACLs** | Web servers receive HTTP requests, but responses cannot exit the subnet, causing client connection timeouts. | Add Outbound NACL rule permitting TCP ports **1024–65535** to `0.0.0.0/0`. |
| **Using Security Groups to Block Specific IPs** | Impossible; Security Groups do not support "Deny" rules. | Use a **Network ACL** (NACL) with an explicit lower-numbered rule (e.g. Rule 50) set to `DENY`. |
| **Hardcoding Instance Private IPs in Security Groups** | Auto Scaling launches replacement instances with new IPs, breaking application connectivity. | Reference the **Source Security Group ID** instead of private IP CIDRs. |
| **Confusing Rule Numbers in NACLs** | Placing an `ALLOW 0.0.0.0/0` rule at #100 and a `DENY 1.2.3.4` at #200; the Deny rule is never reached! | Place specific `DENY` rules at **lower numbers** (e.g. #50) before general `ALLOW` rules (e.g. #100). |
| **Over-Permissive Outbound Security Groups** | Compromised instances have unrestricted egress to download malware or exfiltrate data. | Restrict outbound security group rules to explicit destination ports (e.g., port 443 to VPC endpoints). |
| **Expecting Return Traffic in Security Groups to Need Outbound Rules** | Unnecessary rules; Security Groups are stateful. | Outbound return traffic for established inbound connections is allowed automatically. |

---

## 8. Performance, Security & Cost Notes

### Connection Tracking Limits
- **Stateful Table Capacity**: The AWS Nitro System maintains a hardware connection tracking table.
- An individual EC2 instance can track hundreds of thousands of concurrent connections.
- If an instance exceeds its connection tracking limit (due to massive UDP floods or short-lived TCP bursts), additional connections are dropped.
- **Untracked Connections**: Flow rules configured to allow `0.0.0.0/0` on all protocols without restrictions can be marked as "untracked" by Nitro, bypassing connection tracking limits.

### Security Boundaries
- **Layer 3/4 Filtering**: Security Groups and NACLs operate exclusively at OSI Layer 3 (IP) and Layer 4 (TCP/UDP/ICMP). They cannot inspect HTTP request headers, cookies, or SQL injection payloads. For Layer 7 inspection, use **AWS WAF** or an Application Load Balancer.

### Cost Dynamics
- Security Groups and Network ACLs are built into the AWS software-defined network fabric and are **100% free of charge**.

---

## 9. Comparison Table

| Feature | Security Group (SG) | Network ACL (NACL) |
|:---|:---|:---|
| **Operating Level** | Instance / ENI Level | Subnet Boundary Level |
| **State Tracking** | **Stateful** (Return traffic auto-allowed)| **Stateless** (Return traffic must be explicitly allowed)|
| **Rule Types** | **Allow rules only** (Default Deny) | **Allow and Deny rules** |
| **Rule Evaluation** | All rules evaluated simultaneously | Evaluated in **numerical order** (First match wins)|
| **Ephemeral Ports** | Handled automatically by state tracker| Must explicitly allow ports **1024–65535** |
| **Source Targets** | IP CIDR blocks or **Security Group IDs** | IP CIDR blocks only (Cannot reference SG IDs) |
| **Modification Impact**| Applies immediately to existing sessions| Applies immediately to new packets |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What does it mean that Security Groups are "stateful" while Network ACLs are "stateless"?</strong></summary>

**Answer:**
- **Stateful (Security Groups)**: When an inbound packet is allowed by an ingress rule, the underlying Nitro hypervisor creates an entry in its connection tracking table. The outbound response packet from the server is **automatically allowed to exit**, completely bypassing outbound security group rules. The reverse is also true for outbound-initiated connections.
- **Stateless (Network ACLs)**: NACLs do not track connection state. Every packet entering or leaving a subnet is evaluated independently from scratch. If an inbound packet is permitted into the subnet on port 443, the response packet leaving the subnet will be **dropped** unless an explicit outbound NACL rule permits traffic to the client's temporary ephemeral port (1024–65535).
</details>

<details>
<summary><strong>Q2: Why must Network ACLs permit outbound traffic on ports 1024–65535 for web servers?</strong></summary>

**Answer:**
When a web browser connects to an HTTPS web server on port 443, the client operating system randomly selects a high-order port from its **ephemeral port range** (typically TCP 1024 to 65535) as the source port for the connection.
The server processes the request and sends the HTTP response back directed to that ephemeral port. Because Network ACLs are stateless, they have no memory of the initial inbound port 443 request. If the outbound NACL does not have an explicit rule allowing traffic to ports 1024–65535, the response packet is dropped by the default `* Deny` rule, and the client browser hangs until timing out.
</details>

<details>
<summary><strong>Q3: How do you block a single malicious IP address in an Amazon VPC?</strong></summary>

**Answer:**
You **cannot** block an IP address using a Security Group because Security Groups support only "Allow" rules.
To block an IP address:
1. Identify the **Network ACL (NACL)** associated with the target subnet.
2. Add an inbound **DENY** rule specifying the attacker's IP address (e.g., `203.0.113.45/32`).
3. Assign this rule a **lower rule number** (e.g., Rule 50) than the general allow rules (e.g., Rule 100).
4. Because NACLs evaluate rules in numerical order and stop at the first match, packets from `203.0.113.45` match Rule 50 and are dropped immediately.
</details>

<details>
<summary><strong>Q4: What is Security Group Chaining and why is it preferred over CIDR rules?</strong></summary>

**Answer:**
Security Group Chaining refers to configuring a security group rule that specifies **another Security Group ID** (e.g., `sg-web-alb`) as the traffic source or destination, rather than an IP CIDR block.
- **Dynamic Adaptability**: In an Auto Scaling Group, instances are continuously launched and terminated with dynamic private IPs. Hardcoding IP ranges requires constant manual maintenance. With SG chaining, any instance assigned `sg-web-alb` is instantly authorized.
- **Zero-Trust Isolation**: Even if a rogue host in the same private subnet attempts to connect to the database on port 5432, traffic is dropped unless that host possesses the authorized application security group.
</details>

<details>
<summary><strong>Q5: What happens if an inbound packet matches multiple rules in a Network ACL?</strong></summary>

**Answer:**
Network ACL rules are evaluated strictly in **ascending numerical order** (e.g., 10, 20, 50, 100):
- As soon as a packet matches a rule's protocol, port, and CIDR criteria, the action (`ALLOW` or `DENY`) is executed **immediately**.
- Rule evaluation halts; subsequent higher-numbered rules are completely ignored.
- For example, if Rule 50 allows port 80 and Rule 60 denies port 80, traffic is **allowed** because Rule 50 matched first.
</details>

<details>
<summary><strong>Q6: What is a Self-Referencing Security Group?</strong></summary>

**Answer:**
A Self-Referencing Security Group is a security group that specifies **its own Group ID** as the source in an inbound rule (e.g., Inbound TCP 9092 from `sg-kafka-cluster`).
This pattern is widely used in distributed clustered systems (Apache Kafka, Elasticsearch, Cassandra, ZooKeeper). It allows all EC2 instances or containers assigned that security group to communicate freely among themselves on internal cluster communication ports, while preventing external instances outside the cluster from accessing internal ports.
</details>

<details>
<summary><strong>Q7: What is the maximum number of Security Groups that can be attached to an Elastic Network Interface (ENI)?</strong></summary>

**Answer:**
By default, up to **5 Security Groups** can be attached to a single Elastic Network Interface (can be adjusted via service quotas). When multiple security groups are attached, AWS evaluates the rules of all attached security groups collectively; if **any** security group permits the traffic, the packet is allowed.
</details>

<details>
<summary><strong>Q8: What does the asterisk (`*`) rule signify at the end of every Network ACL?</strong></summary>

**Answer:**
The asterisk (`*`) rule is the mandatory, unchangeable default rule present at the end of every Network ACL:
- It acts as an unconditional **catch-all DENY rule** for both inbound and outbound traffic.
- If a packet does not match any of the numbered rules evaluated above it, it matches the `*` rule and is dropped.
- The `*` rule cannot be modified, moved, or deleted.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create Chained Security Groups via AWS CLI</strong></summary>

**Objective**: Configure zero-trust access between Web and DB tiers.

**Commands**:
```bash
# Create Web SG
WEB_SG=$(aws ec2 create-security-group --group-name "web-sg" --description "Web tier" --vpc-id "$VPC_ID" --query 'GroupId' --output text)
aws ec2 authorize-security-group-ingress --group-id "$WEB_SG" --protocol tcp --port 443 --cidr 0.0.0.0/0

# Create DB SG referencing Web SG
DB_SG=$(aws ec2 create-security-group --group-name "db-sg" --description "DB tier" --vpc-id "$VPC_ID" --query 'GroupId' --output text)
aws ec2 authorize-security-group-ingress --group-id "$DB_SG" --protocol tcp --port 5432 --source-group "$WEB_SG"
```
</details>

<details>
<summary><strong>Exercise 2: Add an Explicit Deny Rule to a Network ACL</strong></summary>

**Objective**: Block a specific IP from entering a subnet.

**Command**:
```bash
aws ec2 create-network-acl-entry \
  --network-acl-id "acl-0123456789abcdef0" \
  --rule-number 40 \
  --protocol -1 \
  --rule-action deny \
  --cidr-block "198.51.100.50/32" \
  --ingress
```
</details>

<details>
<summary><strong>Exercise 3: Test Security Group Stateful Return Behavior</strong></summary>

**Objective**: Verify that inbound connections succeed even when outbound security group rules are completely removed.

**Steps**:
1. Remove all outbound rules from an EC2 instance's security group.
2. Ensure inbound port 443 is allowed.
3. Send an HTTPS request to the server from a client browser.
4. Verify that the response returns successfully, proving that stateful connection tracking bypasses outbound rules.
</details>

<details>
<summary><strong>Exercise 4: Create a Self-Referencing Security Group</strong></summary>

**Objective**: Configure internal cluster gossip permissions.

**Commands**:
```bash
CLUSTER_SG=$(aws ec2 create-security-group --group-name "cluster-sg" --description "Internal cluster" --vpc-id "$VPC_ID" --query 'GroupId' --output text)
aws ec2 authorize-security-group-ingress --group-id "$CLUSTER_SG" --protocol tcp --port 9000-9100 --source-group "$CLUSTER_SG"
```
</details>

<details>
<summary><strong>Exercise 5: Simulate Ephemeral Port Block in NACL</strong></summary>

**Objective**: Observe connection hang when ephemeral ports are omitted in a custom NACL.

**Steps**:
1. Create a custom NACL with Inbound Rule 100: Allow TCP 443.
2. Set Outbound Rule 100: Allow TCP 443 (omitting 1024-65535).
3. Associate with public subnet.
4. Attempt `curl https://<INSTANCE_IP>`.
5. Observe connection timing out due to missing outbound ephemeral rules.
</details>

<details>
<summary><strong>Exercise 6: Diagnose Security Group Connectivity via Reachability Analyzer</strong></summary>

**Objective**: Verify firewall paths using AWS Network Access Analyzer.

**Command**:
```bash
aws ec2-network-insights start-network-insights-analysis \
  --network-insights-path-id "nip-0123456789abcdef0"
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                         Security Groups vs. Network ACLs Cheat Sheet                  |
+---------------------------------------------------------------------------------------+
|  Dimension             | Security Group (SG)               | Network ACL (NACL)       |
|------------------------+-----------------------------------+--------------------------|
|  Scope                 | Instance / ENI Level              | Subnet Level Boundary    |
|  Statefulness          | Stateful (Auto return traffic)    | Stateless (Explicit ret) |
|  Rule Actions          | Allow only (Default Deny)         | Allow AND Deny           |
|  Rule Order            | All evaluated simultaneously      | Numerical (First match)  |
|  Ephemeral Ports       | Handled automatically             | Must explicitly allow    |
|  Source Referencing    | SGs can reference other SG IDs    | CIDR blocks only         |
|  Default Inbound       | Deny All                          | Allow All (Default NACL) |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon EC2 Security Groups for Linux Instances](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ec2-security-groups.html)
- [Control VPC Traffic with Network ACLs](https://docs.aws.amazon.com/vpc/latest/userguide/vpc-network-acls.html)
- [Security Group Connection Tracking Documentation](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/security-group-connection-tracking.html)
- [AWS VPC Reachability Analyzer](https://docs.aws.amazon.com/vpc/latest/reachability/what-is-reachability-analyzer.html)
- [AWS Well-Architected Security Pillar: Network Protection](https://docs.aws.amazon.com/wellarchitected/latest/security-pillar/protecting-networks.html)
