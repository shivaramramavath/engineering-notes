---
topic: "AWS Networking, VPC & Content Delivery Technical Interview Q&A"
module: "09-interview"
track: "aws"
difficulty: "advanced"
last-tested: "2026-11"
tags:
  - "interview"
  - "networking"
  - "vpc"
  - "security-groups"
  - "nacls"
  - "transit-gateway"
  - "privatelink"
  - "route53"
---

# AWS Networking, VPC & Content Delivery Technical Interview Q&A

## Table of Contents
- [1. VPC Architecture, CIDRs & Subnetting](#1-vpc-architecture-cidrs--subnetting)
- [2. Security Groups vs NACLs](#2-security-groups-vs-nacls)
- [3. Gateways & Hybrid Connectivity (NAT, Transit Gateway, Direct Connect)](#3-gateways--hybrid-connectivity-nat-transit-gateway-direct-connect)
- [4. AWS PrivateLink & VPC Endpoints](#4-aws-privatelink--vpc-endpoints)
- [5. Route 53 DNS & CloudFront Edge Delivery](#5-route-53-dns--cloudfront-edge-delivery)
- [6. Deep-Dive Networking Scenarios & Packet Flow Triage](#6-deep-dive-networking-scenarios--packet-flow-triage)

---

## 1. VPC Architecture, CIDRs & Subnetting

### Q1: In an AWS VPC subnet with CIDR `10.0.1.0/24`, how many usable IP addresses are available for EC2 instances, and why?
**Answer**:
A `/24` subnet contains $2^{(32 - 24)} = 256$ total IP addresses. However, on AWS, exactly **251 IP addresses** are usable.
AWS reserves exactly **5 IP addresses** in every subnet for internal infrastructure operations:
1. `10.0.1.0`: Network address (standard IP networking reservation).
2. `10.0.1.1`: Reserved by AWS for the **VPC Router / Default Gateway**.
3. `10.0.1.2`: Reserved by AWS for the **Amazon Provided DNS Server (AmazonRoute53Resolver)**.
4. `10.0.1.3`: Reserved by AWS for future operational use.
5. `10.0.1.255`: Network broadcast address (AWS does not support broadcast, but the address remains reserved).

### Q2: What distinguishes a Public Subnet, a Private Subnet, and an Isolated Subnet?
**Answer**:
Subnet classification is determined entirely by its **Route Table configuration**:
- **Public Subnet**: Its route table has an explicit route `0.0.0.0/0` targeting an **Internet Gateway (IGW)** (`target: igw-xxxx`). Instances with public IPv4 addresses can send and receive traffic directly to/from the internet.
- **Private Subnet**: Its route table has a default route `0.0.0.0/0` targeting a **NAT Gateway** (`target: nat-xxxx`) or NAT Instance located in a public subnet. Instances have only private IP addresses; they can initiate outbound internet connections (e.g., for software patches or external API calls), but external internet clients cannot initiate inbound connections.
- **Isolated Subnet**: Its route table contains **only the local VPC route** (`10.0.0.0/16 -> local`). There is zero route to an Internet Gateway, NAT Gateway, or VPN. These subnets are physically incapable of reaching or being reached by the internet. Used for sensitive data tiers (e.g., RDS databases, HSMs).

---

## 2. Security Groups vs NACLs

### Q3: Contrast Security Groups and Network Access Control Lists (NACLs) across statefulness, evaluation order, and operational layers.
**Answer**:

| Dimension | Security Groups | Network Access Control Lists (NACLs) |
|:---|:---|:---|
| **Layer of Operation** | **Virtual Network Interface (ENI)** level | **Subnet boundary** level |
| **Statefulness** | **Stateful**: If inbound traffic is allowed, return outbound traffic is automatically permitted regardless of outbound rules. | **Stateless**: Return traffic must be explicitly allowed by an outbound rule matching ephemeral ports (1024-65535). |
| **Rule Types** | **ALLOW rules only**. (Cannot define explicit Deny rules). | **ALLOW and DENY rules** supported. |
| **Evaluation Order** | **All rules evaluated** simultaneously before deciding to allow traffic. | **Numbered order** (evaluated lowest number to highest; first match wins). |
| **Scope** | Attached to individual instances/ENIs. | Attached to the entire subnet (all resources inherit). |
| **Primary Use Case** | Fine-grained application firewall; micro-segmentation. | Coarse subnet-level perimeter defense (e.g., blocking specific malicious CIDRs/IPs). |

### Q4: An engineer creates an inbound NACL rule allowing port 443, but clients still cannot establish an HTTPS connection. Why?
**Answer**:
Because **NACLs are completely stateless**.
When an external client connects to an HTTPS server:
1. Inbound packet arrives on destination port 443 from a client source port chosen from the client's **ephemeral port range** (typically ports `1024-65535`).
2. The inbound NACL allows port 443.
3. The server attempts to send the response packet back to the client's IP on destination port `1024-65535`.
4. If the subnet's **Outbound NACL** does not have a rule allowing outbound traffic on ephemeral ports (`tcp: 1024-65535`), the response packet is silently dropped at the subnet boundary.
*Fix*: Ensure the Outbound NACL contains a rule permitting `TCP 1024-65535` (or `0.0.0.0/0 all traffic`).

---

## 3. Gateways & Hybrid Connectivity (NAT, Transit Gateway, Direct Connect)

### Q5: How do AWS Transit Gateway and VPC Peering differ, and when should an enterprise transition from Peering to Transit Gateway?
**Answer**:
- **VPC Peering**:
  - Direct point-to-point network connection between two VPCs.
  - **No Transitive Routing**: If VPC A is peered with VPC B, and VPC B is peered with VPC C, VPC A *cannot* communicate with VPC C through VPC B.
  - Sizing limit: As the number of VPCs ($N$) grows, a full mesh requires $\frac{N(N-1)}{2}$ peering connections (e.g., 50 VPCs require 1,225 peering connections), creating immense routing table complexity.
  - Cost: Free to establish; you pay only standard cross-AZ/cross-region data transfer fees.
- **AWS Transit Gateway (TGW)**:
  - Centralized, highly scalable regional cloud router acting as a **Hub-and-Spoke** network architecture.
  - **Supports Transitive Routing**: Hundreds of VPCs, AWS Direct Connect gateways, and Site-to-Site VPNs attach to the central TGW hub and communicate through centralized route tables.
  - Cost: \$0.05 per attachment-hour + \$0.02 per GB processed.
- **Decision Rule**: Use VPC Peering for simple, low-volume setups with 2-5 VPCs. Transition to **AWS Transit Gateway** when managing complex multi-account environments (10+ VPCs) requiring centralized egress inspection firewalls, hybrid corporate data center links, and simplified routing governance.

### Q6: AWS Direct Connect vs AWS Site-to-Site VPN: Compare throughput, reliability, security, and setup lead time.
**Answer**:
- **AWS Site-to-Site VPN**:
  - Encrypted IPsec tunnels connecting an on-premises Customer Gateway to an AWS Virtual Private Gateway or Transit Gateway over the **public internet**.
  - *Bandwidth*: Capped at 1.25 Gbps per tunnel (can aggregate with ECMP).
  - *Latency / Reliability*: Variable; subject to public internet routing fluctuations and ISP congestion.
  - *Setup Lead Time*: Minutes to hours.
  - *Cost*: Low (\$0.05/hour + internet data transfer).
- **AWS Direct Connect (DX)**:
  - Dedicated, private physical telecommunications circuit (1 Gbps, 10 Gbps, or 100 Gbps) from the customer's data center to an AWS Direct Connect location.
  - *Bandwidth*: High, dedicated, unconstrained physical throughput.
  - *Latency / Reliability*: Extremely consistent, deterministic, ultra-low latency; completely bypasses the public internet.
  - *Encryption*: **Unencrypted by default** at Layer 3 (requires configuring MACsec at Layer 2 or running an IPsec VPN on top of DX for encryption).
  - *Setup Lead Time*: Weeks to months (requires telco cross-connect provisioning).
  - *Cost*: High (port hour fees + data transfer out).

---

## 4. AWS PrivateLink & VPC Endpoints

### Q7: Explain the difference between an S3 Gateway VPC Endpoint and an S3 Interface VPC Endpoint.
**Answer**:
- **Gateway Endpoint (S3 and DynamoDB only)**:
  - Operates at the **route table level**.
  - Adds a target prefix list (`pl-63a5400a -> vpce-xxxx`) to the VPC route table.
  - **Free of charge**: Zero hourly fee, zero per-GB data processing fee.
  - *Limitation*: Cannot be reached from on-premises over Direct Connect or VPN; cannot be accessed across peered VPCs.
- **Interface Endpoint (AWS PrivateLink)**:
  - Deploys dedicated **Elastic Network Interfaces (ENIs)** with private IP addresses directly inside your chosen VPC subnets.
  - Traffic routes via standard DNS resolution (Private DNS flips `s3.amazonaws.com` to resolve to the internal ENI IPs).
  - **Billable**: \$0.01 per AZ hour + \$0.01 per GB processed.
  - *Advantage*: Fully accessible from on-premises networks over AWS Direct Connect or Transit Gateway.
- *Best Practice*: For workloads inside the VPC, always use **Gateway Endpoints** to eliminate cost. Use **Interface Endpoints** when on-premises data center servers need private access to S3.

---

## 5. Route 53 DNS & CloudFront Edge Delivery

### Q8: Compare the primary Amazon Route 53 Routing Policies and identify when to use each.
**Answer**:
1. **Simple Routing**: Direct 1-to-1 mapping from a domain to a single resource IP or record. Does not support health checks.
2. **Weighted Routing**: Distributes traffic across multiple endpoints according to assigned percentage weights (e.g., 90% to v1, 10% to v2). Ideal for canary releases and blue/green testing.
3. **Latency-Based Routing (LBR)**: Directs client DNS queries to the AWS Region that provides the lowest network latency for that specific client.
4. **Failover Routing**: Active-Passive disaster recovery. Routes traffic to the primary endpoint as long as its health check passes; automatically fails over to the secondary standby endpoint upon failure.
5. **Geolocation Routing**: Routes traffic based on the geographic location of the DNS query origin (e.g., European users routed to `eu-west-1` for GDPR compliance).
6. **Geoproximity Routing**: Routes traffic based on geographic distance between user and AWS resources using a "bias" slider to shrink or expand regional routing boundaries.
7. **Multi-Value Answer**: Returns up to 8 healthy IP records chosen at random with Route 53 health checking, providing simple DNS-level load balancing.

### Q9: Why can't you create a standard DNS CNAME record for the zone apex (e.g., `example.com`), and how does Route 53 Alias solve this?
**Answer**:
Under the DNS specification (RFC 1034 / 2181), a **CNAME record cannot coexist with any other record type at the root/zone apex** of a domain (`example.com`), because the zone apex must contain SOA and NS records. Since AWS Application Load Balancers and CloudFront distributions only provide dynamic DNS hostnames (not static IP addresses), standard DNS cannot point `example.com` to an ALB.

**Route 53 Alias Records** resolve this:
- An Alias record is a Route 53-specific virtual pointer that behaves like a CNAME but responds to DNS queries as an **A Record** (returning actual IP addresses).
- It can be created at the zone apex (`example.com`).
- Route 53 recognizes AWS resource changes dynamically (e.g., if ALB IPs scale out or change, Route 53 automatically updates the returned IPs).
- **Queries to Alias records pointing to AWS resources are completely free of charge**.

---

## 6. Deep-Dive Networking Scenarios & Packet Flow Triage

### Scenario 1: Troubleshooting an Unreachable EC2 Instance in a Private Subnet
**Interviewer**: *"You deploy an EC2 instance in a private subnet. You need it to download dependencies from the public internet via `yum update`, but the command times out. Walk me through your troubleshooting checklist."*

**Structured Candidate Response**:
I would systematically verify the 5-point egress path:
1. **Route Table**: Inspect the private subnet's route table. Does it contain `0.0.0.0/0` pointing to an active **NAT Gateway ID** (`nat-xxxx`)?
2. **NAT Gateway Subnet & Status**: Verify that the NAT Gateway itself is deployed in a **Public Subnet** and is in the `Available` state.
3. **Public Route Table & IGW**: Verify that the public subnet housing the NAT Gateway has a route table with `0.0.0.0/0` pointing to an **Internet Gateway** (`igw-xxxx`).
4. **Security Group Outbound Rules**: Check the EC2 instance's Security Group. By default, SGs allow all outbound traffic (`0.0.0.0/0`). Ensure an engineer has not deleted the outbound rule or restricted outbound ports (HTTP/HTTPS ports 80/443 must be open).
5. **NACLs (Subnet Level)**: Check both the Private Subnet and Public Subnet Network ACLs. Verify that:
   - Inbound and Outbound rules allow outbound TCP 80/443.
   - Return inbound traffic is permitted on **ephemeral ports** (`TCP 1024-65535`).

### Scenario 2: Diagnosing Asymmetric Routing and Dropped Packets
**Interviewer**: *"An enterprise deploys a centralized firewall inspection appliance in a transit hub VPC. Outbound internet traffic leaves via the firewall, but return traffic times out. What is asymmetric routing, and how do you resolve it on AWS?"*

**Structured Candidate Response**:
- **The Issue**: Asymmetric routing occurs when an outbound network packet traverses one path (e.g., Spoke VPC $\rightarrow$ Transit Gateway $\rightarrow$ Firewall Appliance $\rightarrow$ NAT $\rightarrow$ Internet), but the return response packet bypasses the stateful firewall on the return trip (or hits a different firewall node in an active-active cluster). Because enterprise firewalls are stateful, if a node receives return SYN-ACK packets without having observed the initial SYN packet, it drops the connection.
- **Root Causes on AWS**:
  - Missing Transit Gateway Route Table separation.
  - Multi-AZ Gateway Load Balancer (GWLB) cross-zone routing mismatch.
- **The Solution**:
  1. Use **AWS Gateway Load Balancer (GWLB)**: GWLB uses the GENEVE protocol to preserve flow stickiness. All packets of a specific 5-tuple TCP flow are guaranteed to route to the exact same physical firewall instance for both forward and reverse paths.
  2. Implement strict transit routing isolation: Ensure the return route table in the public NAT subnet routes destination CIDRs back through the Transit Gateway rather than directly attempting local peering.
