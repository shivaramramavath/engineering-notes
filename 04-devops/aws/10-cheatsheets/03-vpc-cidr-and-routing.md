---
topic: "VPC CIDR Math, Subnetting & Routing Cheatsheet"
module: "10-cheatsheets"
track: "aws"
difficulty: "reference"
last-tested: "2026-11"
tags:
  - "cheatsheet"
  - "vpc"
  - "cidr"
  - "subnetting"
  - "routing"
  - "networking"
---

# VPC CIDR Math, Subnetting & Routing Cheatsheet

A high-density reference card for VPC CIDR allocation, subnetting math, reserved addresses, and route table patterns.

---

## 1. CIDR Sizing & Subnet Math Table

In every AWS VPC subnet, **5 IP addresses are always reserved by AWS**. Available IPs = $2^{(32 - \text{Prefix})} - 5$.

| CIDR Prefix | Subnet Mask | Total IPs ($2^{32-\text{Prefix}}$) | Usable IPs on AWS (Total - 5) | Best Suited For |
|:---|:---|:---|:---|:---|
| `/16` | `255.255.0.0` | 65,536 | **65,531** | Entire Production VPC Backbone |
| `/20` | `255.255.240.0` | 4,096 | **4,091** | Large EKS / ECS Container Subnets |
| `/22` | `255.255.252.0` | 1,024 | **1,019** | Standard Application Workload Subnets |
| `/24` | `255.255.255.0` | 256 | **251** | Standard Public Ingress / ALB Subnets |
| `/26` | `255.255.255.192`| 64 | **59** | Dedicated Database / RDS Subnets |
| `/28` | `255.255.255.240`| 16 | **11** | Small Gateway / Transit Subnets (Minimum allowed in AWS) |

---

## 2. AWS 5 Reserved IP Addresses per Subnet

For any subnet (e.g., `10.0.1.0/24`):

```
+---------------+--------------------------------------------------------------------+
| IP Address    | AWS Reservation Purpose                                            |
+---------------+--------------------------------------------------------------------+
| 10.0.1.0      | Network Address (Cannot be assigned to any ENI)                    |
| 10.0.1.1      | VPC Router / Default Gateway                                       |
| 10.0.1.2      | Amazon Provided DNS Resolver (AmazonRoute53Resolver)               |
| 10.0.1.3      | Reserved by AWS for future operational use                         |
| 10.0.1.255    | Network Broadcast Address (AWS does not support broadcast)        |
+---------------+--------------------------------------------------------------------+
```

---

## 3. Production Enterprise 3-Tier VPC Allocation Blueprint (`10.0.0.0/16`)

```
VPC CIDR: 10.0.0.0/16 (65,536 Total IPs)

├── Availability Zone A (us-east-1a)
│   ├── Tier 1 (Public Ingress):       10.0.1.0/24   (251 usable IPs) -> Internet Gateway
│   ├── Tier 2 (Private Application):  10.0.10.0/20  (4,091 usable IPs) -> NAT Gateway
│   └── Tier 3 (Isolated Database):    10.0.30.0/24  (251 usable IPs) -> Local Only
│
└── Availability Zone B (us-east-1b)
    ├── Tier 1 (Public Ingress):       10.0.2.0/24   (251 usable IPs) -> Internet Gateway
    ├── Tier 2 (Private Application):  10.0.20.0/20  (4,091 usable IPs) -> NAT Gateway
    └── Tier 3 (Isolated Database):    10.0.40.0/24  (251 usable IPs) -> Local Only
```

---

## 4. Route Table Reference Templates

### 4.1 Tier 1: Public Subnet Route Table
```
Destination        Target
10.0.0.0/16        local
0.0.0.0/0          igw-0123456789abcdef0 (Internet Gateway)
```

### 4.2 Tier 2: Private Application Subnet Route Table
```
Destination        Target
10.0.0.0/16        local
0.0.0.0/0          nat-0123456789abcdef0 (NAT Gateway in Public Subnet)
pl-63a5400a        vpce-0123456789abcdef0 (S3 Gateway Endpoint - Free traffic!)
```

### 4.3 Tier 3: Isolated Database Subnet Route Table
```
Destination        Target
10.0.0.0/16        local
(NO 0.0.0.0/0 route! Complete physical isolation from the public internet)
```

---

## 5. Security Groups vs NACLs Quick Diagnostic

| Symptom / Behavior | Check Security Group | Check Network ACL (NACL) |
|:---|:---|:---|
| **Outbound call works, return traffic blocked?** | Security Groups are **stateful** (Return traffic is never blocked). | Check **stateless** Outbound NACL for ephemeral ports (`1024-65535`). |
| **Need to explicitly block a single attacker IP?** | Cannot do (SGs only support Allow rules). | Add a numbered **DENY** rule at the top of the Inbound NACL. |
| **Instance cannot talk to another in the same SG?** | By default, SGs do not self-reference; add an ingress rule with source = `sg-xxxx`. | NACLs allow local subnet traffic by default. |
| **Rule evaluation order?** | **All rules evaluated** simultaneously. | Evaluated **numerically** (Rule 100 before 200; first match terminates). |
