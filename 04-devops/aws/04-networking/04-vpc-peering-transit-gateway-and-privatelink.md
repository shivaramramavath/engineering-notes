# AWS VPC Peering, Transit Gateway, and PrivateLink Architecture

> **Module**: `04-networking`  
> **Topic**: Cross-VPC Connectivity, Hub-and-Spoke Topologies, and Private Service Publishing  
> **Target Audience**: Cloud Network Architects, Principal Infrastructure Engineers, Security Leads  
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

As enterprises expand their cloud footprints, infrastructure naturally fragments into dozens or hundreds of dedicated AWS accounts and Amazon VPCs to isolate environments (production, staging, sandbox), business units, and compliance boundaries.

Connecting these distributed VPCs securely and efficiently introduces significant architectural choices:
- Establishing direct 1-to-1 connections between pairs of VPCs (**VPC Peering**) creates an unmanageable full-mesh topology ($N(N-1)/2$) as the number of VPCs grows, while strictly forbidding transitive routing.
- Managing centralized routing between hundreds of VPCs, corporate on-premises data centers, and third-party partner networks requires an enterprise cloud router: **AWS Transit Gateway (TGW)**.
- Consuming software services, APIs, or SaaS applications hosted by external accounts or partners without exposing services to the public internet and without risking CIDR IP overlap conflicts requires **AWS PrivateLink**.

This guide covers the design patterns, routing mechanics, and operational trade-offs across **VPC Peering**, **AWS Transit Gateway**, and **AWS PrivateLink**.

---

## 2. Core Concepts

### 2.1 Amazon VPC Peering
- **1-to-1 Direct Network Connection**: Connects two VPCs across the AWS private network backbone.
- **Non-Transitive Routing**: Traffic **cannot** traverse an intermediate VPC:
  - If VPC A is peered with VPC B, and VPC B is peered with VPC C, **VPC A cannot communicate with VPC C** through VPC B.
  - Direct peering between A and C must be established.
- **No Bandwidth Bottlenecks**: Peering does not pass through a physical appliance or gateway; traffic traverses existing AWS software-defined network fabrics with zero bandwidth throttling or single-point-of-failure risks.
- **Strict CIDR Constraint**: Peered VPCs **cannot have overlapping CIDR blocks**.

### 2.2 AWS Transit Gateway (TGW)
A regional network transit hub acting as a high-performance **cloud router**:
- **Hub-and-Spoke Topology**: Replaces complex full-mesh peering with a centralized router connecting thousands of VPCs, AWS Site-to-Site VPN connections, and AWS Direct Connect gateways.
- **Transitive Routing Supported**: Enables traffic to flow freely between spoke VPCs (e.g., Spoke A -> TGW -> Spoke B).
- **Transit Gateway Route Tables**: Supports multiple independent route tables, enabling network segmentation:
  - *Production Route Table*: Connects production VPCs to shared services and on-premises, completely isolated from development VPCs.
  - *Inspection Route Table*: Directs all inter-VPC traffic through an egress/inspection VPC running firewall appliances (e.g., AWS Network Firewall).
- **Scale**: Up to **50 Gbps** of burst bandwidth per VPC attachment. Shared across accounts via **AWS Resource Access Manager (RAM)**.

### 2.3 AWS PrivateLink & VPC Endpoints
Exposes services privately over the AWS network without internet gateways or VPC peering:
- **Interface VPC Endpoints**:
  - Powered by AWS PrivateLink.
  - Provisions an Elastic Network Interface (ENI) with a private IP directly inside your subnet.
  - Routes traffic privately to supported AWS services (Secrets Manager, ECR, CloudWatch, SSM) or custom partner services.
- **Gateway Endpoints**:
  - Targets for route tables (supports **Amazon S3** and **Amazon DynamoDB** only).
  - Free of charge with zero data processing fees.
- **VPC Endpoint Services**: Allows an application behind a **Network Load Balancer (NLB)** in a provider account to be published privately to thousands of consumer VPCs, completely eliminating CIDR IP overlap issues.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Cross-VPC Interconnect Topologies Compared                 |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ TOPOLOGY 1: VPC PEERING (Direct 1-to-1 / Non-Transitive) ]                    |
|                                                                                   |
|        +---------------------+          +---------------------+                   |
|        | VPC A (10.1.0.0/16) | <======> | VPC B (10.2.0.0/16) |                   |
|        +---------------------+          +---------------------+                   |
|                   \\                               //                             |
|                    \\  (NO TRANSITIVE ROUTE!)     //                              |
|                     v                            v                                |
|        +---------------------+          +---------------------+                   |
|        | VPC C (10.3.0.0/16) | <======> | VPC D (10.4.0.0/16) |                   |
|        +---------------------+          +---------------------+                   |
|        * N*(N-1)/2 connections -> Becomes unmanageable at scale!                  |
|                                                                                   |
|  -------------------------------------------------------------------------------  |
|  [ TOPOLOGY 2: AWS TRANSIT GATEWAY (Hub-and-Spoke / Transitive Routing) ]         |
|                                                                                   |
|      [ VPC Spoke 1 ]        [ VPC Spoke 2 ]        [ On-Premises Data Center ]    |
|      (10.10.0.0/16)         (10.20.0.0/16)         (Direct Connect / VPN)         |
|             \                      |                      /                       |
|              v                     v                     v                        |
|  +-----------------------------------------------------------------------------+  |
|  | AWS Transit Gateway (TGW) - Central Regional Cloud Router                   |  |
|  | - Transit Gateway Route Table A (Prod Segments)                             |  |
|  | - Transit Gateway Route Table B (Dev Segments)                              |  |
|  | - Central Inspection Routing (Sends east-west traffic to Network Firewall)  |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
|  -------------------------------------------------------------------------------  |
|  [ TOPOLOGY 3: AWS PRIVATELINK (Service Publishing / Overlapping CIDRs Allowed) ]  |
|                                                                                   |
|  [ Consumer VPC (10.0.0.0/16) ]              [ SaaS Provider VPC (10.0.0.0/16) ]  |
|  +-----------------------------+             +---------------------------------+  |
|  | Interface Endpoint (ENI)    | ==Tunnel==> | Network Load Balancer (NLB)     |  |
|  | Private IP: 10.0.1.50       | (AWS Fabric)| Target: Backend Microservices   |  |
|  +-----------------------------+             +---------------------------------+  |
|  * Unidirectional access to specific port only; NO network-level routing peering! |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating and Accepting a VPC Peering Connection via CLI
```bash
# 1. Request Peering Connection from VPC-A to VPC-B
PEER_ID=$(aws ec2 create-vpc-peering-connection \
  --vpc-id "vpc-01111111111111111" \
  --peer-vpc-id "vpc-02222222222222222" \
  --tag-specifications 'ResourceType=vpc-peering-connection,Tags=[{Key=Name,Value=vpa-to-vpb}]' \
  --query 'VpcPeeringConnection.VpcPeeringConnectionId' \
  --output text)

# 2. Accept Peering Connection (Executed by owner of VPC-B)
aws ec2 accept-vpc-peering-connection \
  --vpc-peering-connection-id "$PEER_ID"

# 3. Add Route in VPC-A Route Table pointing to VPC-B CIDR
aws ec2 create-route \
  --route-table-id "rtb-01a1a1a1a1a1a1a1a" \
  --destination-cidr-block "10.2.0.0/16" \
  --vpc-peering-connection-id "$PEER_ID"

# 4. Add Route in VPC-B Route Table pointing to VPC-A CIDR
aws ec2 create-route \
  --route-table-id "rtb-02b2b2b2b2b2b2b2b" \
  --destination-cidr-block "10.1.0.0/16" \
  --vpc-peering-connection-id "$PEER_ID"
```

### 4.2 Creating an AWS Transit Gateway and Attachment via CLI
```bash
# 1. Create Transit Gateway
TGW_ID=$(aws ec2 create-transit-gateway \
  --description "Central enterprise router" \
  --options "AmazonSideAsn=64512,AutoAcceptSharedAttachments=enable" \
  --query 'TransitGateway.TransitGatewayId' \
  --output text)

# 2. Attach VPC to Transit Gateway
aws ec2 create-transit-gateway-vpc-attachment \
  --transit-gateway-id "$TGW_ID" \
  --vpc-id "vpc-01111111111111111" \
  --subnet-ids "subnet-01a" "subnet-01b"
```

---

## 5. Practical Examples

### Example 1: Declarative Terraform Transit Gateway with Network Segmentation
Deploy an AWS Transit Gateway with two isolated route tables (Production vs Development).

```hcl
resource "aws_ec2_transit_gateway" "tgw" {
  description                     = "Enterprise Transit Gateway"
  default_route_table_association = "disable"
  default_route_table_propagation = "disable"

  tags = { Name = "central-tgw" }
}

# --- TRANSIT GATEWAY ROUTE TABLES ---
resource "aws_ec2_transit_gateway_route_table" "production" {
  transit_gateway_id = aws_ec2_transit_gateway.tgw.id
  tags               = { Name = "tgw-production-rt" }
}

resource "aws_ec2_transit_gateway_route_table" "development" {
  transit_gateway_id = aws_ec2_transit_gateway.tgw.id
  tags               = { Name = "tgw-development-rt" }
}

# --- ATTACHMENTS ---
resource "aws_ec2_transit_gateway_vpc_attachment" "prod_vpc" {
  transit_gateway_id = aws_ec2_transit_gateway.tgw.id
  vpc_id             = var.prod_vpc_id
  subnet_ids         = var.prod_tgw_subnet_ids
  tags               = { Name = "tgw-prod-attachment" }
}

resource "aws_ec2_transit_gateway_vpc_attachment" "dev_vpc" {
  transit_gateway_id = aws_ec2_transit_gateway.tgw.id
  vpc_id             = var.dev_vpc_id
  subnet_ids         = var.dev_tgw_subnet_ids
  tags               = { Name = "tgw-dev-attachment" }
}

# Associate attachments with their respective isolated route tables
resource "aws_ec2_transit_gateway_route_table_association" "prod_assoc" {
  transit_gateway_attachment_id  = aws_ec2_transit_gateway_vpc_attachment.prod_vpc.id
  transit_gateway_route_table_id = aws_ec2_transit_gateway_route_table.production.id
}

resource "aws_ec2_transit_gateway_route_table_association" "dev_assoc" {
  transit_gateway_attachment_id  = aws_ec2_transit_gateway_vpc_attachment.dev_vpc.id
  transit_gateway_route_table_id = aws_ec2_transit_gateway_route_table.development.id
}
```

### Example 2: Publishing a Microservice Privately via AWS PrivateLink
Configure an endpoint service behind an NLB that allows external consumer accounts to consume the API privately.

```hcl
# Provider Account: Network Load Balancer in front of backend instances
resource "aws_lb" "api_nlb" {
  name               = "internal-api-nlb"
  internal           = true
  load_balancer_type = "network"
  subnets            = var.private_subnet_ids
}

# Provider Account: Create VPC Endpoint Service
resource "aws_vpc_endpoint_service" "api_service" {
  acceptance_required        = true
  network_load_balancer_arns = [aws_lb.api_nlb.arn]
  allowed_principals         = ["arn:aws:iam::222233334444:root"] # Whitelist consumer account

  tags = { Name = "payments-api-privatelink" }
}

# Consumer Account (Account 222233334444): Create Interface Endpoint
resource "aws_vpc_endpoint" "consumer_endpoint" {
  vpc_id              = var.consumer_vpc_id
  service_name        = aws_vpc_endpoint_service.api_service.service_name
  vpc_endpoint_type   = "Interface"
  subnet_ids          = var.consumer_private_subnets
  security_group_ids  = [var.consumer_endpoint_sg_id]
  private_dns_enabled = false
}
```

### Example 3: Routing Transit Gateway Traffic to Centralized Egress Inspection VPC
Route all internet-bound traffic from multiple spoke VPCs through a central inspection firewall VPC.

```
Spoke VPC Route Table:
0.0.0.0/0 -> tgw-0123456789abcdef0

Transit Gateway Spoke Route Table:
0.0.0.0/0 -> tgw-attach-inspection-vpc

Inspection VPC Appliance:
Evaluates traffic with AWS Network Firewall -> Outbound to NAT Gateway / IGW
```

### Example 4: Testing PrivateLink Connection and Latency
Verify private endpoint connectivity inside a consumer instance using `curl`.

```bash
# Query the Interface Endpoint private DNS name
curl -I -k "https://vpce-0123456789abcdef0-abcdef12.vpce-svc-0feeedcba98765432.us-east-1.vpce.amazonaws.com:8443/healthz"
```

### Example 5: Resolving Transitive Routing Failures in VPC Peering
Why VPC A cannot reach VPC C through VPC B:

```
[VPC A: 10.1.0.0/16] <=== Peering 1 ===> [VPC B: 10.2.0.0/16] <=== Peering 2 ===> [VPC C: 10.3.0.0/16]

If VPC A adds route: 10.3.0.0/16 -> pcx-peering-1 (pointing to VPC B)
AWS drops the packet at VPC B's edge!
VPC Peering does not support router packet re-forwarding (non-transitive).
Solution: Create a direct Peering 3 between A and C, or migrate to AWS Transit Gateway!
```

---

## 6. Best Practices

1. **Adopt AWS Transit Gateway for 5+ VPCs**: When managing more than 4–5 VPCs across multiple accounts, abandon VPC Peering. Peering complexity scales as $O(N^2)$, making security updates and route audits fragile. Transit Gateway provides a scalable, auditable hub-and-spoke architecture.
2. **Use AWS PrivateLink for Cross-Account Service Consumption**: If Team A needs to expose an API to Team B, avoid VPC Peering or Transit Gateway if all they need is API access. PrivateLink exposes only the specific TCP port/service, eliminates route table changes, and permits identical, overlapping CIDRs.
3. **Dedicated Subnets for Transit Gateway Attachments**: In each VPC, create a small, dedicated `/28` subnet in each AZ exclusively for the Transit Gateway ENI attachment. Do not place application EC2 instances or databases in the TGW subnet to prevent routing conflicts.
4. **Isolate Environments via TGW Route Tables**: Never use a single flat default route table in Transit Gateway. Create separate route tables for **Production**, **Development**, and **Shared Services** to enforce strict segmentation at the cloud routing layer.
5. **Enable Appliance Mode on Inspection TGW Attachments**: When routing traffic through a centralized inspection VPC with stateful firewalls (Palo Alto, Fortinet, AWS Network Firewall), enable **Appliance Mode** (`appliance-mode-support = enable`). This forces bidirectional traffic to traverse the exact same AZ, preventing asymmetric routing firewall drops.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Expecting VPC Peering to be Transitive** | Traffic from Spoke A dropped when attempting to reach Spoke C through Central Hub B. | Peer A and C directly, or adopt **AWS Transit Gateway** which natively supports transitive routing. |
| **Peering VPCs with Overlapping CIDRs** | AWS CLI / console rejects peering creation with an overlapping CIDR error. | Allocate unique, non-overlapping CIDR blocks, or connect via **AWS PrivateLink**. |
| **Missing Return Routes in Peered VPCs** | Outbound packet reaches target instance, but response packet is dropped because target lacks a return route. | Always configure matching reciprocal routes in **both** VPC route tables. |
| **Asymmetric Routing with Centralized Firewalls** | Stateful firewalls drop packets because the outbound flow traversed AZ 1a while the inbound response traversed AZ 1b. | Enable **Transit Gateway Appliance Mode** on the inspection VPC attachment. |
| **Using Transit Gateway for Simple 2-VPC Connect** | Paying ~$73/month in fixed TGW hourly fees plus data processing fees for a basic 2-VPC link. | Use **VPC Peering** for simple, low-complexity 2-VPC connections (peering is 100% free of hourly fees). |
| **Opening Entire Subnets in PrivateLink SGs** | Security groups on Interface Endpoints overly permissive. | Restrict Interface Endpoint security groups to specific consumer security groups or explicit IPs. |

---

## 8. Performance, Security & Cost Notes

### Performance & Throughput Comparison
- **VPC Peering**: No aggregate throughput limit. Bounded only by the maximum network performance of the communicating EC2 instances (up to 100/200 Gbps ENA). Zero added latency.
- **Transit Gateway**: Bounded at **50 Gbps per VPC attachment** (supports horizontal scaling via ECMP across multiple VPNs). Adds ~1 millisecond routing latency.
- **PrivateLink**: Scales automatically up to **100 Gbps** per endpoint.

### Security Isolation
- **PrivateLink (Zero Network Exposure)**: Consumers connect exclusively to an NLB port. Consumers cannot ping, port-scan, or access any other host, subnet, or service in the provider's VPC.
- **TGW Segmentation**: Enforces strict network-level micro-segmentation using independent routing tables.

### Cost Breakdown
- **VPC Peering**:
  - Hourly fee: **$0.00 (Completely Free)**.
  - Data transfer: Standard inter-AZ ($0.01/GB) or inter-region ($0.02/GB) data transfer rates.
- **AWS Transit Gateway**:
  - Hourly fee: **$0.05 per hour per attachment** (~$36.50/month per VPC attached).
  - Data processing: **$0.02 per GB** processed through the gateway.
- **AWS PrivateLink (Interface Endpoints)**:
  - Hourly fee: **$0.01 per hour per AZ endpoint** (~$7.30/month per AZ).
  - Data processing: **$0.01 per GB** processed.

---

## 9. Comparison Table

| Attribute | Amazon VPC Peering | AWS Transit Gateway | AWS PrivateLink |
|:---|:---|:---|:---|
| **Topology** | 1-to-1 Point-to-Point Mesh | Hub-and-Spoke Central Router | Unidirectional Private Service Endpoint |
| **Transitive Routing** | **No** (Strictly non-transitive)| **Yes** (Full transit support) | N/A (Service-level proxy) |
| **Overlapping CIDRs** | **Forbidden** (Must be unique) | **Forbidden** across routed spokes| **Allowed** (Eliminates CIDR conflicts) |
| **Max Scale** | Tens of VPCs (Mesh complexity)| Thousands of VPCs & VPNs | Thousands of consumer VPCs |
| **Security Scope** | Full network-level access | Full network-level access | Specific TCP port & service only |
| **Hourly Fee** | **$0.00 (Free)** | ~$36.50 / month / VPC | ~$7.30 / month / AZ |
| **Data Fee** | Standard inter-AZ rates | $0.02 / GB processed | $0.01 / GB processed |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: Why is VPC Peering non-transitive and how does this affect enterprise architecture?</strong></summary>

**Answer:**
Non-transitive routing means that traffic entering a VPC via a peering connection cannot be forwarded out through another peering connection, VPN, Direct Connect, or Internet Gateway. If VPC A is peered with VPC B, and VPC B is peered with VPC C:
- VPC A cannot route packets to VPC C through VPC B.
- VPC B cannot act as a router or NAT proxy between A and C.

In enterprise architecture, connecting 10 VPCs in a full mesh requires 45 individual peering connections; connecting 100 VPCs requires **4,950 peering connections** ($N(N-1)/2$). This administrative burden makes pure VPC peering unviable for large organizations, mandating the adoption of **AWS Transit Gateway**.
</details>

<details>
<summary><strong>Q2: When should an architect choose AWS PrivateLink instead of VPC Peering or Transit Gateway?</strong></summary>

**Answer:**
Choose **AWS PrivateLink** when:
1. **CIDR Blocks Overlap**: Both VPCs use `10.0.0.0/16`. VPC Peering and Transit Gateway strictly reject overlapping address spaces. PrivateLink works seamlessly because it operates via proxy endpoints (ENIs) without network routing.
2. **Third-Party / SaaS Consumption**: Consuming services hosted by an external vendor, partner, or separate business unit where you want to grant access exclusively to a single application port (e.g., port 443) without exposing your internal network to their routing table.
3. **Least Privilege / Zero Trust**: Restricting access strictly to a specific microservice rather than establishing bidirectional network-level routing.
</details>

<details>
<summary><strong>Q3: What is Transit Gateway Appliance Mode and why is it mandatory for centralized firewalls?</strong></summary>

**Answer:**
When an organization routes east-west traffic between VPCs through a centralized inspection VPC containing stateful firewalls (such as Palo Alto, Fortinet, or AWS Network Firewall):
- Outbound packets from Spoke A (in AZ 1a) route through the firewall instance in AZ 1a.
- When the response packet returns from Spoke B, Transit Gateway's default hashing algorithm might forward the packet to the firewall instance in AZ 1b.
- Because stateful firewalls drop packets if they have not seen the initial TCP `SYN` handshake, the return packet is dropped (**asymmetric routing failure**).
- Enabling **Appliance Mode** on the inspection VPC attachment forces Transit Gateway to route both the forward and return flows of a connection through the exact same physical Availability Zone, maintaining stateful firewall synchronization.
</details>

<details>
<summary><strong>Q4: How does AWS Resource Access Manager (RAM) facilitate Transit Gateway deployments in multi-account organizations?</strong></summary>

**Answer:**
In enterprise multi-account environments, the Transit Gateway is typically created and managed in a central **Network / Hub AWS Account**:
- The network team shares the Transit Gateway with the entire AWS Organization (or specific Organizational Units) using **AWS RAM**.
- Member accounts (Production, Staging, Shared Services) automatically see the shared Transit Gateway in their local VPC console.
- Application teams create VPC attachments linking their local VPCs to the shared Transit Gateway, while central network administrators retain exclusive control over routing tables, policies, and inspection routing.
</details>

<details>
<summary><strong>Q5: What are the two types of VPC Endpoints and which services do they support?</strong></summary>

**Answer:**
1. **Gateway Endpoints**:
   - Implemented as target entries in subnet route tables (`pl-xxxx -> vpce-xxxx`).
   - Supports **Amazon S3** and **Amazon DynamoDB** only.
   - **100% Free** of charge (no hourly fees, no data processing charges).
2. **Interface Endpoints (AWS PrivateLink)**:
   - Implemented as Elastic Network Interfaces (ENIs) assigned a private IP within your subnet.
   - Supports over 150+ AWS services (Secrets Manager, ECR, CloudWatch, Systems Manager, SQS, SNS, STS, Athena), AWS Marketplace partners, and custom endpoint services.
   - Incurs an hourly fee per AZ (~$0.01/hr) plus data processing charges ($0.01/GB).
</details>

<details>
<summary><strong>Q6: Can an instance in a private subnet access an S3 Gateway Endpoint if the subnet has no route to an Internet Gateway?</strong></summary>

**Answer:**
**Yes**. Gateway Endpoints operate entirely within the private software-defined AWS network fabric.
The route table directs traffic targeting the S3 Prefix List (`pl-63a5400a`) directly to the Gateway Endpoint (`vpce-xxxx`). Packets never touch an Internet Gateway, NAT Gateway, or the public internet, allowing instances in completely isolated subnets with zero internet access to read and write to S3 securely.
</details>

<details>
<summary><strong>Q7: How do you achieve high availability with AWS PrivateLink Interface Endpoints?</strong></summary>

**Answer:**
To achieve high availability:
- Deploy an Interface Endpoint across **multiple Availability Zones** (creating an endpoint ENI in at least 2 or 3 distinct subnets, e.g., AZ 1a and AZ 1b).
- Enable **Private DNS** on the endpoint (`privateDnsEnabled = true`).
- AWS creates an internal Route 53 record resolving the standard public service DNS name (e.g., `secretsmanager.us-east-1.amazonaws.com`) to the private IP addresses of the endpoint ENIs in each AZ.
- Client applications automatically load balance requests across healthy AZ interfaces using round-robin DNS.
</details>

<details>
<summary><strong>Q8: What is the maximum bandwidth of a single AWS Transit Gateway VPC attachment?</strong></summary>

**Answer:**
A single Transit Gateway VPC attachment supports up to **50 Gbps of burst throughput** per Availability Zone.
For hybrid connections (Site-to-Site VPN), each standard IPsec tunnel is capped at 1.25 Gbps, but Transit Gateway supports **Equal-Cost Multi-Path (ECMP)** routing across up to 50 parallel VPN tunnels, enabling aggregate hybrid bandwidth exceeding 50+ Gbps.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a VPC Peering Connection via AWS CLI</strong></summary>

**Objective**: Connect two sandbox VPCs.

**Command**:
```bash
aws ec2 create-vpc-peering-connection \
  --vpc-id "$VPC_A_ID" \
  --peer-vpc-id "$VPC_B_ID" \
  --tag-specifications 'ResourceType=vpc-peering-connection,Tags=[{Key=Name,Value=lab-peer}]'
```
</details>

<details>
<summary><strong>Exercise 2: Create an Interface Endpoint for AWS Secrets Manager</strong></summary>

**Objective**: Access Secrets Manager privately without NAT Gateway.

**Command**:
```bash
aws ec2 create-vpc-endpoint \
  --vpc-id "$VPC_ID" \
  --service-name com.amazonaws.us-east-1.secretsmanager \
  --vpc-endpoint-type Interface \
  --subnet-ids "$SUBNET_1A" "$SUBNET_1B" \
  --security-group-ids "$ENDPOINT_SG" \
  --private-dns-enabled
```
</details>

<details>
<summary><strong>Exercise 3: Inspect Transit Gateway Route Table Routes</strong></summary>

**Objective**: Query routes in a specific TGW route table.

**Command**:
```bash
aws ec2 search-transit-gateway-routes \
  --transit-gateway-route-table-id "tgw-rtb-0123456789abcdef0" \
  --filters "Name=state,Values=active"
```
</details>

<details>
<summary><strong>Exercise 4: Verify Private DNS Resolution on Interface Endpoint</strong></summary>

**Objective**: Test that public AWS domain resolves to private ENI IP.

**Command**:
```bash
dig secretsmanager.us-east-1.amazonaws.com +short
# Output should return private IP (e.g. 10.0.1.x), confirming Private DNS resolution!
```
</details>

<details>
<summary><strong>Exercise 5: Create a DynamoDB Gateway Endpoint</strong></summary>

**Objective**: Attach a free DynamoDB endpoint to private route tables.

**Command**:
```bash
aws ec2 create-vpc-endpoint \
  --vpc-id "$VPC_ID" \
  --service-name com.amazonaws.us-east-1.dynamodb \
  --route-table-ids "$PRIV_RT_ID"
```
</details>

<details>
<summary><strong>Exercise 6: Test Appliance Mode Setting on Transit Gateway Attachment</strong></summary>

**Objective**: Enable appliance mode on an inspection VPC attachment.

**Command**:
```bash
aws ec2 modify-transit-gateway-vpc-attachment \
  --transit-gateway-attachment-id "tgw-attach-0123456789abcdef0" \
  --options "ApplianceModeSupport=enable"
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                         AWS Interconnect Decision Framework                           |
+---------------------------------------------------------------------------------------+
|  Use Case                              | Recommended Technology                       |
|----------------------------------------+----------------------------------------------|
|  Simple 1-to-1 link between 2 VPCs    | VPC Peering (Free, zero latency)             |
|  Centralized routing for 5+ VPCs       | AWS Transit Gateway (Hub-and-spoke)          |
|  Centralized Firewall / Inspection     | Transit Gateway with Appliance Mode          |
|  Overlapping CIDRs between accounts    | AWS PrivateLink (VPC Endpoint Services)      |
|  Access S3 or DynamoDB from VPC        | Gateway Endpoints (Free, no NAT fees)        |
|  Access AWS APIs (SSM/ECR/Secrets)     | Interface Endpoints (PrivateLink)            |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon VPC Peering Guide](https://docs.aws.amazon.com/vpc/latest/peering/what-is-vpc-peering.html)
- [AWS Transit Gateway User Guide](https://docs.aws.amazon.com/vpc/latest/tgw/what-is-tgw.html)
- [AWS PrivateLink and VPC Endpoints Documentation](https://docs.aws.amazon.com/vpc/latest/privatelink/what-is-privatelink.html)
- [Centralized Inspection Architecture with AWS Transit Gateway](https://aws.amazon.com/blogs/networking-and-content-delivery/deployment-models-for-aws-network-firewall-with-vpc-routing-enhancements/)
- [Sharing AWS Transit Gateway via AWS RAM](https://docs.aws.amazon.com/ram/latest/userguide/share-resources.html)
