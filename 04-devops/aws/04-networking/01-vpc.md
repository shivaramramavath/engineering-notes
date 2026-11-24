# Amazon Virtual Private Cloud (VPC) Architecture & Fundamentals

> **Module**: `04-networking`  
> **Topic**: Software-Defined Networking, Subnetting, Routing, and Internet Connectivity  
> **Target Audience**: Cloud Engineers, Network Architects, Systems Administrators  
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

In physical data centers, networking requires procurement of physical switches, routers, firewalls, and copper/fiber cabling to isolate sensitive database tiers from public-facing web servers. Re-architecting subnets or updating firewall boundaries typically required physical hardware recabling and days of change-management review.

**Amazon Virtual Private Cloud (Amazon VPC)** is the software-defined networking (SDN) foundation of AWS. It provisions a logically isolated virtual network dedicated to your AWS account, granting complete control over:
- IP address ranges (Classless Inter-Domain Routing - CIDR).
- Creation of public, private, and isolated subnets across multiple Availability Zones.
- Custom route tables and packet forwarding rules.
- Network gateways (Internet Gateways, NAT Gateways, Egress-Only Internet Gateways).
- Multi-tier security perimeters using Security Groups and Network ACLs.

Every enterprise production workload in AWS—whether running EC2 instances, ECS Fargate tasks, EKS Kubernetes worker nodes, or RDS Aurora databases—resides inside an Amazon VPC.

---

## 2. Core Concepts

### 2.1 CIDR Blocks and Sizing
- A VPC is defined by an IPv4 CIDR block with a netmask between **/16** (65,536 IP addresses) and **/28** (16 IP addresses).
- Standard RFC 1918 Private IPv4 address spaces are typically used:
  - `10.0.0.0/8` (e.g., `10.0.0.0/16`)
  - `172.16.0.0/12` (e.g., `172.16.0.0/16`)
  - `192.168.0.0/16` (e.g., `192.168.0.0/20`)

### 2.2 The 5 Reserved IP Addresses per Subnet
In every AWS subnet, **5 IP addresses are permanently reserved** by AWS and cannot be assigned to user resources:
- `x.x.x.0`: **Network address**.
- `x.x.x.1`: **VPC Router** address (default gateway).
- `x.x.x.2`: **Amazon-provided DNS** server (Route 53 Resolver / Amazon Provided DNS).
- `x.x.x.3`: **Future AWS use**.
- `x.x.x.255`: **Network broadcast** address (AWS does not support broadcast in VPC, but reserves the address).
*Example*: A `/24` subnet contains $256 - 5 = \mathbf{251}$ usable IP addresses.

### 2.3 Subnet Types
- **Public Subnet**: A subnet whose route table has a direct route to an **Internet Gateway (IGW)** (`0.0.0.0/0 -> igw-xxxx`). Resources can communicate bidirectionally with the internet if assigned a public IPv4 or Elastic IP.
- **Private Subnet**: A subnet whose route table directs outbound internet traffic through a **NAT Gateway** located in a public subnet (`0.0.0.0/0 -> nat-xxxx`). Backend instances can initiate outbound connections (e.g., package updates) while remaining completely protected from inbound internet scanning.
- **Isolated (Database) Subnet**: A subnet whose route table contains *no route to the internet* (neither IGW nor NAT Gateway). Can only communicate with other subnets in the VPC or via VPC Endpoints.

### 2.4 Gateways: IGW vs. NAT Gateway vs. NAT Instance
- **Internet Gateway (IGW)**: Horizontally scaled, redundant, highly available VPC edge component. Performs 1:1 Network Address Translation between private instance IPs and public/Elastic IPs. Zero bandwidth limits, zero maintenance, free of charge.
- **NAT Gateway**: Managed AWS network service deployed inside a public subnet. Enables instances in private subnets to reach the internet. Automatically scales up to **100 Gbps** per gateway. Redundant within an AZ. Incurs hourly and per-GB processing charges.
- **NAT Instance (Deprecated)**: Self-managed EC2 instance running Linux iptables. Legacy anti-pattern; single point of failure and bottlenecked by instance network bandwidth.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Amazon VPC Multi-Tier Network Topology                     |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Internet / Public Clients ]                                                    |
|             |                                                                     |
|             v (Public IPv4 / HTTPS: 443)                                          |
|  +-----------------------------------------------------------------------------+  |
|  | Internet Gateway (IGW: igw-0123456789abcdef0)                               |  |
|  +---------------------------------------|-------------------------------------+  |
|                                          |                                        |
|  [ Customer VPC: 10.0.0.0/16 ]           v                                        |
|  +-----------------------------------------------------------------------------+  |
|  | PUBLIC SUBNETS (AZ 1a & 1b)                                                 |  |
|  | Route Table: 0.0.0.0/0 -> igw-xxxx | 10.0.0.0/16 -> local                   |  |
|  |                                                                             |  |
|  |  +------------------------------+     +----------------------------------+  |  |
|  |  | Public Subnet 1a (10.0.1.0/24)|     | Public Subnet 1b (10.0.2.0/24)   |  |  |
|  |  | - Public Application Load Balancer (ALB Node A & B)                     |  |  |
|  |  | - NAT Gateway A (EIP Attached)     | - NAT Gateway B (EIP Attached)   |  |  |
|  |  +------------------------------+     +----------------------------------+  |  |
|  +------------------|-------------------------------------|--------------------+  |
|                     | Forward Traffic                     | Forward Traffic       |
|                     v                                     v                       |
|  +-----------------------------------------------------------------------------+  |
|  | PRIVATE APPLICATION SUBNETS (AZ 1a & 1b)                                    |  |
|  | Route Table 1a: 0.0.0.0/0 -> nat-gw-a | Route Table 1b: 0.0.0.0/0 -> nat-b |  |
|  |                                                                             |  |
|  |  +------------------------------+     +----------------------------------+  |  |
|  |  | Private App 1a (10.0.10.0/24) |     | Private App 1b (10.0.11.0/24)    |  |  |
|  |  | - EC2 Auto Scaling / ECS Tasks |     | - EC2 Auto Scaling / ECS Tasks   |  |  |
|  |  +------------------------------+     +----------------------------------+  |  |
|  +------------------|-------------------------------------|--------------------+  |
|                     | Database Queries (Port 5432)        |                       |
|                     v                                     v                       |
|  +-----------------------------------------------------------------------------+  |
|  | ISOLATED DATABASE SUBNETS (AZ 1a & 1b) - Zero Internet Egress               |  |
|  | Route Table: 10.0.0.0/16 -> local (NO 0.0.0.0/0 route)                      |  |
|  |                                                                             |  |
|  |  +------------------------------+     +----------------------------------+  |  |
|  |  | Private DB 1a (10.0.20.0/24)  |     | Private DB 1b (10.0.21.0/24)     |  |  |
|  |  | - Aurora Primary Writer       |<--->| - Aurora Read Replica / Standby  |  |  |
|  |  +------------------------------+     +----------------------------------+  |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Provisioning a Multi-Tier VPC via AWS CLI
```bash
# 1. Create VPC
VPC_ID=$(aws ec2 create-vpc \
  --cidr-block 10.0.0.0/16 \
  --tag-specifications 'ResourceType=vpc,Tags=[{Key=Name,Value=prod-vpc}]' \
  --query 'Vpc.VpcId' \
  --output text)

# Enable DNS hostnames inside the VPC
aws ec2 modify-vpc-attribute --vpc-id "$VPC_ID" --enable-dns-hostnames '{"Value": true}'
aws ec2 modify-vpc-attribute --vpc-id "$VPC_ID" --enable-dns-support '{"Value": true}'

# 2. Create Internet Gateway and attach to VPC
IGW_ID=$(aws ec2 create-internet-gateway \
  --tag-specifications 'ResourceType=internet-gateway,Tags=[{Key=Name,Value=prod-igw}]' \
  --query 'InternetGateway.InternetGatewayId' \
  --output text)

aws ec2 attach-internet-gateway --vpc-id "$VPC_ID" --internet-gateway-id "$IGW_ID"

# 3. Create Public Subnet
PUB_SUBNET=$(aws ec2 create-subnet \
  --vpc-id "$VPC_ID" \
  --cidr-block 10.0.1.0/24 \
  --availability-zone us-east-1a \
  --tag-specifications 'ResourceType=subnet,Tags=[{Key=Name,Value=public-1a}]' \
  --query 'Subnet.SubnetId' \
  --output text)

# 4. Create Public Route Table and add default 0.0.0.0/0 route to IGW
PUB_RT=$(aws ec2 create-route-table --vpc-id "$VPC_ID" --query 'RouteTable.RouteTableId' --output text)
aws ec2 create-route --route-table-id "$PUB_RT" --destination-cidr-block 0.0.0.0/0 --gateway-id "$IGW_ID"
aws ec2 associate-route-table --subnet-id "$PUB_SUBNET" --route-table-id "$PUB_RT"
```

---

## 5. Practical Examples

### Example 1: Production Multi-Tier VPC in Terraform (3 Tiers across 2 AZs)
Declarative Terraform module provisioning Public, Private App, and Isolated Database subnets with Multi-AZ NAT Gateways.

```hcl
variable "vpc_cidr" { default = "10.0.0.0/16" }

resource "aws_vpc" "main" {
  cidr_block           = var.vpc_cidr
  enable_dns_hostnames = true
  enable_dns_support   = true

  tags = { Name = "enterprise-production-vpc" }
}

resource "aws_internet_gateway" "igw" {
  vpc_id = aws_vpc.main.id
  tags   = { Name = "prod-igw" }
}

# --- PUBLIC SUBNETS ---
resource "aws_subnet" "public_1a" {
  vpc_id                  = aws_vpc.main.id
  cidr_block              = "10.0.1.0/24"
  availability_zone       = "us-east-1a"
  map_public_ip_on_launch = true
  tags                    = { Name = "public-subnet-1a" }
}

resource "aws_subnet" "public_1b" {
  vpc_id                  = aws_vpc.main.id
  cidr_block              = "10.0.2.0/24"
  availability_zone       = "us-east-1b"
  map_public_ip_on_launch = true
  tags                    = { Name = "public-subnet-1b" }
}

# --- NAT GATEWAYS (1 per AZ for High Availability) ---
resource "aws_eip" "nat_1a" { domain = "vpc" }
resource "aws_eip" "nat_1b" { domain = "vpc" }

resource "aws_nat_gateway" "nat_1a" {
  allocation_id = aws_eip.nat_1a.id
  subnet_id     = aws_subnet.public_1a.id
  tags          = { Name = "nat-gw-1a" }
}

resource "aws_nat_gateway" "nat_1b" {
  allocation_id = aws_eip.nat_1b.id
  subnet_id     = aws_subnet.public_1b.id
  tags          = { Name = "nat-gw-1b" }
}

# --- PRIVATE APPLICATION SUBNETS ---
resource "aws_subnet" "app_1a" {
  vpc_id            = aws_vpc.main.id
  cidr_block        = "10.0.10.0/24"
  availability_zone = "us-east-1a"
  tags              = { Name = "private-app-1a" }
}

resource "aws_subnet" "app_1b" {
  vpc_id            = aws_vpc.main.id
  cidr_block        = "10.0.11.0/24"
  availability_zone = "us-east-1b"
  tags              = { Name = "private-app-1b" }
}

# --- ISOLATED DATABASE SUBNETS ---
resource "aws_subnet" "db_1a" {
  vpc_id            = aws_vpc.main.id
  cidr_block        = "10.0.20.0/24"
  availability_zone = "us-east-1a"
  tags              = { Name = "isolated-db-1a" }
}

resource "aws_subnet" "db_1b" {
  vpc_id            = aws_vpc.main.id
  cidr_block        = "10.0.21.0/24"
  availability_zone = "us-east-1b"
  tags              = { Name = "isolated-db-1b" }
}

# --- ROUTE TABLES ---
resource "aws_route_table" "public" {
  vpc_id = aws_vpc.main.id
  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.igw.id
  }
  tags = { Name = "public-route-table" }
}

resource "aws_route_table_association" "pub_1a" {
  subnet_id      = aws_subnet.public_1a.id
  route_table_id = aws_route_table.public.id
}

resource "aws_route_table_association" "pub_1b" {
  subnet_id      = aws_subnet.public_1b.id
  route_table_id = aws_route_table.public.id
}

resource "aws_route_table" "private_1a" {
  vpc_id = aws_vpc.main.id
  route {
    cidr_block     = "0.0.0.0/0"
    nat_gateway_id = aws_nat_gateway.nat_1a.id
  }
  tags = { Name = "private-rt-1a" }
}

resource "aws_route_table_association" "priv_1a" {
  subnet_id      = aws_subnet.app_1a.id
  route_table_id = aws_route_table.private_1a.id
}

resource "aws_route_table" "private_1b" {
  vpc_id = aws_vpc.main.id
  route {
    cidr_block     = "0.0.0.0/0"
    nat_gateway_id = aws_nat_gateway.nat_1b.id
  }
  tags = { Name = "private-rt-1b" }
}

resource "aws_route_table_association" "priv_1b" {
  subnet_id      = aws_subnet.app_1b.id
  route_table_id = aws_route_table.private_1b.id
}

# Isolated database route table (only local routing)
resource "aws_route_table" "database" {
  vpc_id = aws_vpc.main.id
  tags   = { Name = "database-isolated-rt" }
}

resource "aws_route_table_association" "db_1a" {
  subnet_id      = aws_subnet.db_1a.id
  route_table_id = aws_route_table.database.id
}

resource "aws_route_table_association" "db_1b" {
  subnet_id      = aws_subnet.db_1b.id
  route_table_id = aws_route_table.database.id
}
```

### Example 2: Configuring an Egress-Only Internet Gateway for IPv6
Grant private IPv6 instances outbound internet access while completely blocking inbound traffic.

```bash
# 1. Create Egress-Only Internet Gateway
EIGW_ID=$(aws ec2 create-egress-only-internet-gateway \
  --vpc-id "$VPC_ID" \
  --query 'EgressOnlyInternetGateway.EgressOnlyInternetGatewayId' \
  --output text)

# 2. Add default IPv6 route to private route table
aws ec2 create-route \
  --route-table-id "$PRIV_RT_ID" \
  --destination-ipv6-cidr-block "::/0" \
  --egress-only-internet-gateway-id "$EIGW_ID"
```

### Example 3: S3 Gateway Endpoint to Eliminate NAT Gateway Data Charges
Route S3 traffic privately within the AWS network to avoid $0.045/GB NAT Gateway data processing fees.

```bash
aws ec2 create-vpc-endpoint \
  --vpc-id "$VPC_ID" \
  --service-name "com.amazonaws.us-east-1.s3" \
  --vpc-endpoint-type "Gateway" \
  --route-table-ids "$PRIV_RT_1A" "$PRIV_RT_1B"
```

### Example 4: Allocating and Attaching an Elastic IP (EIP)
```bash
# Allocate public IPv4 address from Amazon pool
ALLOC_ID=$(aws ec2 allocate-address --domain vpc --query 'AllocationId' --output text)

# Associate with an instance
aws ec2 associate-address \
  --instance-id i-0123456789abcdef0 \
  --allocation-id "$ALLOC_ID"
```

### Example 5: Troubleshooting Private Subnet Egress via Reachability Analyzer
Diagnose why an EC2 instance in a private subnet cannot reach an external IP.

```bash
aws ec2-network-insights create-network-insights-path \
  --source i-0123456789abcdef0 \
  --destination "52.0.1.1" \
  --destination-port 443 \
  --protocol TCP
```

---

## 6. Best Practices

1. **Deploy 1 NAT Gateway Per Availability Zone in Production**: While a single NAT Gateway can theoretically route traffic for subnets in other AZs, cross-AZ traffic incurs **$0.01/GB cross-AZ data fees** and creates a single point of failure. Deploying a NAT Gateway in each public AZ guarantees high availability and localized routing.
2. **Always Use S3 and DynamoDB Gateway Endpoints**: AWS Gateway Endpoints are **100% free of charge**. Attaching Gateway Endpoints to private route tables routes S3 and DynamoDB traffic directly across the AWS internal backbone, completely bypassing expensive NAT Gateway data processing charges ($0.045/GB).
3. **Plan CIDR Ranges to Prevent Overlaps**: When designing VPC CIDRs, coordinate across the enterprise to avoid IP overlaps with corporate on-premises networks or other AWS accounts. Overlapping CIDRs prevent VPC Peering and AWS Direct Connect / VPN routing.
4. **Isolate Database Tiers from NAT Gateways**: Place database clusters (RDS, Aurora, DocumentDB) in dedicated **Isolated Subnets** with no route to `0.0.0.0/0` (not even NAT Gateways). Databases should never have direct internet egress.
5. **Always Enable VPC Flow Logs**: Enable VPC Flow Logs from day one, publishing to Amazon CloudWatch Logs or an S3 security lake. Flow logs are critical for diagnosing dropped packets, auditing security group rules, and investigating network security incidents.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Deploying Backend Databases in Public Subnets** | Vulnerable to automated port scanning, brute-force attacks, and internet exploits. | Place databases in private or isolated subnets with only private IPs. |
| **Placing a NAT Gateway in a Private Subnet** | Private instances cannot reach the internet; a NAT Gateway requires an IGW route in its own subnet. | A NAT Gateway **must always reside in a public subnet** with a direct route to an Internet Gateway. |
| **Single NAT Gateway for Multi-AZ Fleets** | If AZ 1a crashes, private subnets in AZ 1b lose internet access completely; incurs cross-AZ fees. | Deploy 1 NAT Gateway per Availability Zone in production environments. |
| **Sizing VPCs Too Small (e.g. /24)** | Subnets run out of IP addresses when EKS pods, Lambda functions, and ALBs scale out. | Allocate a large base VPC CIDR (e.g., `/16`) and allocate `/20` or `/22` subnets. |
| **Forgetting the 5 Reserved IPs per Subnet** | Provisioning a `/28` subnet (16 IPs) and wondering why only 11 instances can launch. | Always account for the **5 AWS-reserved IPs** when planning subnet capacity. |
| **Routing Heavy S3 Data Through NAT Gateway** | Massive unexpected AWS bills due to NAT Gateway $0.045/GB data processing fees. | Provision a **Free S3 Gateway Endpoint** in all private route tables. |

---

## 8. Performance, Security & Cost Notes

### Throughput & Performance
- **Internet Gateway**: Horizontally scaled across thousands of AWS network devices; has no bandwidth caps and does not bottleneck throughput.
- **NAT Gateway**: Scales automatically from 5 Gbps up to **100 Gbps** throughput. Supports up to 55,000 simultaneous connections per destination IP.

### Security Enforcements
- **Stateless Edge Routing**: Route tables direct packets strictly based on destination CIDR blocks. They do not perform stateful packet inspection.
- **Network Isolation**: Instances in different VPCs cannot communicate unless explicitly joined via VPC Peering, Transit Gateway, or AWS PrivateLink.

### Cost Breakdown
- **VPC & Internet Gateway**: 100% Free of charge.
- **NAT Gateway**:
  - Hourly fee: ~$0.045 per hour per gateway (~$32.40/month per AZ).
  - Data processing fee: **$0.045 per GB** transferred through the gateway.
- **Public IPv4 Addresses**: AWS charges **$0.005 per hour per public IPv4 address** (~$3.60/month per active IP).

---

## 9. Comparison Table

| Feature | Internet Gateway (IGW) | NAT Gateway | S3 / DynamoDB Gateway Endpoint |
|:---|:---|:---|:---|
| **Placement** | VPC Edge (Global to VPC) | Public Subnet (AZ-specific) | Route Table Target |
| **Traffic Direction** | Inbound & Outbound (Public IPs)| Outbound Only (Private IPs) | Outbound to AWS Services |
| **High Availability** | Managed redundant fabric | Redundant within single AZ | Redundant managed service |
| **Hourly Cost** | Free ($0.00) | ~$0.045 / hour / gateway | Free ($0.00) |
| **Data Processing Fee** | None | $0.045 per GB processed | None ($0.00) |
| **Target Workload** | Public ALBs & Bastion hosts | Private EC2/ECS internet egress | S3 & DynamoDB internal routing |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: Why are there 5 reserved IP addresses in every AWS VPC subnet?</strong></summary>

**Answer:**
In any subnet (e.g., `10.0.1.0/24`), AWS permanently reserves the first four IP addresses and the last IP address for networking infrastructure:
1. `10.0.1.0`: **Network address** (identifies the subnet).
2. `10.0.1.1`: **VPC Router** (acts as the default gateway for instances in the subnet).
3. `10.0.1.2`: **Amazon-Provided DNS (AmazonProvidedDNS)** (the Route 53 Resolver IP address, calculated as base + 2).
4. `10.0.1.3`: **Reserved by AWS** for future functionality.
5. `10.0.1.255`: **Network Broadcast address** (AWS VPC does not support broadcast/multicast, but reserves the address for protocol compliance).
Thus, a `/24` subnet yields $256 - 5 = 251$ usable IP addresses.
</details>

<details>
<summary><strong>Q2: What is the architectural difference between a Public Subnet and a Private Subnet?</strong></summary>

**Answer:**
The sole technical difference is the subnet's **Route Table**:
- **Public Subnet**: Its associated route table has a default route targeting an **Internet Gateway**:
  `0.0.0.0/0 -> igw-xxxx`
  Instances inside can receive direct inbound traffic from the internet if assigned a public IPv4 address or Elastic IP.
- **Private Subnet**: Its associated route table does *not* route directly to an IGW. To reach the internet, its default route targets a **NAT Gateway** located in a public subnet:
  `0.0.0.0/0 -> nat-xxxx`
  Instances inside have only private IP addresses and can initiate outbound connections, but external internet clients cannot initiate inbound connections.
</details>

<details>
<summary><strong>Q3: Why must a NAT Gateway always be deployed in a Public Subnet?</strong></summary>

**Answer:**
A NAT Gateway functions as an intermediary proxy: it receives private packets from instances in private subnets, substitutes the private IP with its own Elastic IP, and forwards the packet out to the internet.
To forward packets to the internet, the NAT Gateway itself requires a direct route to an Internet Gateway (`0.0.0.0/0 -> igw-xxxx`). If a NAT Gateway is deployed in a private subnet, its own route table lacks an Internet Gateway route, meaning it cannot forward traffic externally, breaking egress for all dependent private instances.
</details>

<details>
<summary><strong>Q4: How does an S3 Gateway Endpoint eliminate NAT Gateway bandwidth costs?</strong></summary>

**Answer:**
Without a VPC Endpoint, when an EC2 instance in a private subnet downloads objects from Amazon S3, packets route through the NAT Gateway across the public internet path to S3's public IP range. AWS bills $0.045 per GB for all data processed by the NAT Gateway. If an application transfers 20 TB of data from S3, the NAT Gateway bill exceeds $900.

An **S3 Gateway Endpoint** creates an internal routing entry directly in the private subnet's route table (e.g., `pl-63a5400a (com.amazonaws.us-east-1.s3) -> vpce-xxxx`). Traffic destined for S3 stays entirely within the private AWS network fabric. Gateway Endpoints are **completely free** with zero data processing fees, completely eliminating the $900 NAT charge.
</details>

<details>
<summary><strong>Q5: What is an Egress-Only Internet Gateway and when is it used?</strong></summary>

**Answer:**
An Egress-Only Internet Gateway (EIGW) is used exclusively for **IPv6 traffic**:
- In IPv6, every instance receives a globally unique, publicly routable IPv6 address (there is no private RFC 1918 NAT in IPv6).
- Without an EIGW, assigning an IPv6 address to an instance in a private subnet would allow arbitrary internet clients to initiate inbound connections to it.
- An **Egress-Only Internet Gateway** allows private IPv6 instances to initiate outbound connections to the internet (stateful connection tracking) while strictly dropping any inbound connection initiated from the public internet. It functions as a NAT Gateway equivalent for IPv6, but is completely free.
</details>

<details>
<summary><strong>Q6: Why is deploying a single NAT Gateway across multiple Availability Zones an anti-pattern?</strong></summary>

**Answer:**
Deploying 1 NAT Gateway to serve private subnets across multiple AZs introduces two severe operational penalties:
1. **Single Point of Failure**: If the Availability Zone hosting the single NAT Gateway experiences a network outage, private subnets in *all other AZs* lose internet access simultaneously.
2. **Cross-AZ Data Charges**: Outbound traffic originating from AZ 1b routed to the NAT Gateway in AZ 1a incurs AWS inter-AZ data transfer charges ($0.01 per GB in each direction), inflating monthly networking bills. In production, always deploy 1 NAT Gateway per AZ.
</details>

<details>
<summary><strong>Q7: What is the Amazon Provided DNS server IP address in a VPC with CIDR `172.16.0.0/16`?</strong></summary>

**Answer:**
In any AWS VPC, the Amazon-Provided DNS (Route 53 Resolver) is located at the base network address **plus 2**:
$$\text{VPC Base} + 2 = 172.16.0.0 + 2 = \mathbf{172.16.0.2}$$
Additionally, the Route 53 Resolver is always accessible via the universal link-local IP address: **`169.254.169.253`**.
</details>

<details>
<summary><strong>Q8: What is an Elastic IP (EIP) and what happens when an EIP is unattached?</strong></summary>

**Answer:**
An Elastic IP is a static, public IPv4 address allocated to your AWS account that remains reserved until explicitly released:
- Unlike dynamic public IPs (which change whenever an EC2 instance stops or restarts), an Elastic IP remains constant and can be rapidly remapped to a replacement instance during failovers.
- **Cost Rule**: To encourage efficient IPv4 utilization, AWS charges **$0.005 per hour** for every allocated public IPv4 address, regardless of whether it is attached to a running instance or sits unattached.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Custom VPC with CLI and Enable DNS Attributes</strong></summary>

**Objective**: Provision a `/16` VPC and verify DNS hostname resolution.

**Commands**:
```bash
VPC_ID=$(aws ec2 create-vpc --cidr-block 10.100.0.0/16 --query 'Vpc.VpcId' --output text)
aws ec2 modify-vpc-attribute --vpc-id "$VPC_ID" --enable-dns-hostnames '{"Value": true}'
aws ec2 describe-vpc-attribute --vpc-id "$VPC_ID" --attribute enableDnsHostnames
```
</details>

<details>
<summary><strong>Exercise 2: Create a Public Subnet and Attach an Internet Gateway</strong></summary>

**Objective**: Wire up public internet routing.

**Commands**:
```bash
IGW_ID=$(aws ec2 create-internet-gateway --query 'InternetGateway.InternetGatewayId' --output text)
aws ec2 attach-internet-gateway --vpc-id "$VPC_ID" --internet-gateway-id "$IGW_ID"

SUBNET_ID=$(aws ec2 create-subnet --vpc-id "$VPC_ID" --cidr-block 10.100.1.0/24 --availability-zone us-east-1a --query 'Subnet.SubnetId' --output text)

RT_ID=$(aws ec2 create-route-table --vpc-id "$VPC_ID" --query 'RouteTable.RouteTableId' --output text)
aws ec2 create-route --route-table-id "$RT_ID" --destination-cidr-block 0.0.0.0/0 --gateway-id "$IGW_ID"
aws ec2 associate-route-table --subnet-id "$SUBNET_ID" --route-table-id "$RT_ID"
```
</details>

<details>
<summary><strong>Exercise 3: Attach an S3 Gateway Endpoint to a Route Table</strong></summary>

**Objective**: Route S3 traffic internally.

**Command**:
```bash
aws ec2 create-vpc-endpoint \
  --vpc-id "$VPC_ID" \
  --service-name com.amazonaws.us-east-1.s3 \
  --route-table-ids "$RT_ID"
```
</details>

<details>
<summary><strong>Exercise 4: Verify Subnet Available IP Count</strong></summary>

**Objective**: Verify the 5 reserved IP calculation.

**Command**:
```bash
aws ec2 describe-subnets --subnet-ids "$SUBNET_ID" --query 'Subnets[0].AvailableIpAddressCount'
# For /24 (256 total), output should be 251.
```
</details>

<details>
<summary><strong>Exercise 5: Allocate and Release an Elastic IP</strong></summary>

**Objective**: Manage public IP lifecycle.

**Commands**:
```bash
ALLOC_ID=$(aws ec2 allocate-address --domain vpc --query 'AllocationId' --output text)
aws ec2 release-address --allocation-id "$ALLOC_ID"
```
</details>

<details>
<summary><strong>Exercise 6: Test VPC Reachability via VPC Reachability Analyzer</strong></summary>

**Objective**: Use AWS network reasoning tools to test routing paths between subnets.

**Command**:
```bash
aws ec2-network-insights create-network-insights-path \
  --source "$SUBNET_ID" \
  --destination "$IGW_ID" \
  --protocol TCP \
  --destination-port 443
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                                Amazon VPC Architecture Matrix                         |
+---------------------------------------------------------------------------------------+
|  Component             | Subnet Association                | Routing Rule Target      |
|------------------------+-----------------------------------+--------------------------|
|  Public Subnet         | Public Route Table                | 0.0.0.0/0 -> igw-xxxx    |
|  Private App Subnet    | Private Route Table (Per AZ)      | 0.0.0.0/0 -> nat-xxxx    |
|  Isolated DB Subnet    | Isolated Route Table              | 10.0.0.0/16 -> local only|
|  S3 Gateway Endpoint   | Private Route Tables              | pl-xxxx -> vpce-xxxx     |
|  Reserved IPs          | 5 IPs per subnet (.0, .1, .2, .3, .255)                      |
|  NAT Gateway HA        | 1 NAT Gateway per AZ in production (Avoid cross-AZ fees)     |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon VPC User Guide](https://docs.aws.amazon.com/vpc/latest/userguide/what-is-amazon-vpc.html)
- [VPC Subnet Sizing and CIDR Allocation Best Practices](https://docs.aws.amazon.com/vpc/latest/userguide/VPC_Subnets.html)
- [Amazon VPC NAT Gateway User Guide](https://docs.aws.amazon.com/vpc/latest/userguide/vpc-nat-gateway.html)
- [Gateway Endpoints for Amazon S3 and DynamoDB](https://docs.aws.amazon.com/vpc/latest/privatelink/gateway-endpoints.html)
- [AWS VPC Reachability Analyzer](https://docs.aws.amazon.com/vpc/latest/reachability/what-is-reachability-analyzer.html)
