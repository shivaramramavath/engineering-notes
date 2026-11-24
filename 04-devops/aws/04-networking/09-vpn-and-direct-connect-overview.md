# AWS Hybrid Connectivity: Site-to-Site VPN, Client VPN & AWS Direct Connect

> **Module**: `04-networking`  
> **Topic**: Hybrid Cloud Networking, IPsec Tunnels, OpenVPN, Dedicated Fiber & Direct Connect Gateways  
> **Target Audience**: Network Engineers, Infrastructure Architects, Security Engineers  
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

Enterprise cloud migrations are rarely an all-or-nothing overnight switch. Large organizations operate on-premises data centers, colocation facilities, branch offices, and remote workforces that must reliably and securely interact with workloads hosted in AWS. Connecting these environments over the unencrypted public Internet exposes sensitive internal communications to eavesdropping, packet loss, and unpredictable jitter.

AWS provides a robust suite of hybrid networking solutions:
1. **AWS Site-to-Site VPN**: Connects remote networks and corporate offices to your Amazon VPCs or AWS Transit Gateways using encrypted IPsec VPN tunnels over the public Internet.
2. **AWS Client VPN**: A managed, OpenVPN-compatible remote access VPN service enabling mobile employees and developers to securely access resources in AWS and on-premises from anywhere.
3. **AWS Direct Connect (DX)**: Bypasses the public Internet entirely by establishing a dedicated, private physical fiber-optic connection between an enterprise data center and AWS, delivering ultra-consistent network performance, massive bandwidth (up to 100 Gbps), and reduced egress data transfer costs.

---

## 2. Core Concepts

### 2.1 AWS Site-to-Site VPN
- **Virtual Private Gateway (VGW)**: The Amazon VPC side VPN concentrator attached directly to a single VPC.
- **Customer Gateway (CGW)**: Represents your physical or software appliance on the corporate side (e.g., Cisco ASA, Juniper SRX, pfSense).
- **Dual-Tunnel Architecture**: Every AWS Site-to-Site VPN connection automatically provisions **two redundant IPsec tunnels** terminating at distinct AWS availability zones for high availability.
- **BGP Dynamic Routing vs Static Routing**: Border Gateway Protocol (BGP) dynamically advertises routes between on-premises and AWS, supporting automated failover and AS Path prepending.

### 2.2 AWS Client VPN
- Fully managed, elastic remote-access VPN based on the OpenVPN protocol.
- Integrates with Active Directory (AD), AWS IAM Identity Center (SAML 2.0), or mutual TLS (mTLS) certificate authentication.
- Associates with VPC subnets via managed Client VPN Endpoints, enforcing connection authorization rules per user group.

### 2.3 AWS Direct Connect (DX)
- **Physical Connection Types**:
  - *Dedicated Connection*: Physical 1 Gbps, 10 Gbps, or 100 Gbps Ethernet port dedicated exclusively to a single customer at an AWS Direct Connect location (meet-me room / colocation facility).
  - *Hosted Connection*: Sub-1 Gbps up to 10 Gbps connection provisioned through an AWS Direct Connect Delivery Partner over pre-existing partner infrastructure.
- **Virtual Interfaces (VIFs)**:
  - *Private VIF*: Connects directly to a specific VPC via a Virtual Private Gateway (VGW) using private RFC 1918 IP addresses.
  - *Public VIF*: Allows on-premises networks to access all public AWS service endpoints globally (S3, DynamoDB, CloudFront) without traversing the public Internet.
  - *Transit VIF*: Connects an AWS Direct Connect Gateway to an **AWS Transit Gateway (TGW)**, enabling a single Direct Connect circuit to route traffic across thousands of VPCs.
- **Direct Connect Gateway (DXGW)**: A global network aggregator that enables a single Direct Connect connection to communicate with multiple VPCs or Transit Gateways across any AWS region (except China).

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        HYBRID NETWORKING TOPOLOGY: DIRECT CONNECT & VPN                           |
|                                                                                                   |
|  [Enterprise On-Premises Datacenter]                                                              |
|  - Customer Router (BGP ASN: 65000)                                                               |
|  - IP Range: 192.168.0.0/16                                                                       |
|         |                                                      |                                  |
|         | (Option A: Primary Dedicated 10 Gbps Fiber)          | (Option B: Backup IPsec VPN)     |
|         v                                                      v                                  |
|  +-----------------------------+                       +-----------------------------+            |
|  | AWS DIRECT CONNECT LOCATION |                       | PUBLIC INTERNET             |            |
|  | - Direct Connect Meet-Me-Rm |                       | - Encrypted IPsec Tunnel 1  |            |
|  | - Dedicated Cross-Connect   |                       | - Encrypted IPsec Tunnel 2  |            |
|  +--------------+--------------+                       +--------------+--------------+            |
|                 | (Transit VIF)                                       | (IPsec over Internet)     |
|                 v                                                     v                           |
|  +-----------------------------+                       +-----------------------------+            |
|  | DIRECT CONNECT GATEWAY      |                       | CUSTOMER GATEWAY (CGW)      |            |
|  | (Global Multi-Region Hub)   |                       +--------------+--------------+            |
|  +--------------+--------------+                                      |                           |
|                 |                                                     |                           |
|                 +-----------------------+-----------------------------+                           |
|                                         |                                                         |
|                                         v                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS TRANSIT GATEWAY (TGW) - Global Routing Fabric                                           |  |
|  | - BGP Route Table: Evaluates AS Path (DX preferred over VPN)                                |  |
|  +--------------------------------------+------------------------------------------------------+  |
|                                         |                                                         |
|         +-------------------------------+-------------------------------+                         |
|         v                                                               v                         |
|  [VPC 1: Production (10.100.0.0/16)]                            [VPC 2: Data Lake (10.200.0.0/16)]|
|  - Private Subnets: ECS & RDS                                   - Private Subnets: EMR & S3 Ep    |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating Customer Gateway and Site-to-Site VPN via AWS CLI

```bash
# 1. Create Customer Gateway representing on-premises firewall
aws ec2 create-customer-gateway \
    --type "ipsec.1" \
    --public-ip "203.0.113.12" \
    --bgp-asn 65000 \
    --tag-specifications 'ResourceType=customer-gateway,Tags=[{Key=Name,Value=datacenter-router}]'

# 2. Create Virtual Private Gateway (VGW) and attach to VPC
aws ec2 create-vpn-gateway \
    --type "ipsec.1" \
    --tag-specifications 'ResourceType=vpn-gateway,Tags=[{Key=Name,Value=prod-vgw}]'

aws ec2 attach-vpn-gateway \
    --vpn-gateway-id "vgw-0123456789abcdef0" \
    --vpc-id "vpc-0123456789abcdef0"

# 3. Create Site-to-Site VPN Connection with BGP dynamic routing
aws ec2 create-vpn-connection \
    --type "ipsec.1" \
    --customer-gateway-id "cgw-0123456789abcdef0" \
    --vpn-gateway-id "vgw-0123456789abcdef0" \
    --options "StaticRoutesOnly=false"
```

---

## 5. Practical Examples

### 5.1 Redundant AWS Site-to-Site VPN with Route Propagation (Terraform)

```hcl
# Customer Gateway (On-Premises Endpoint)
resource "aws_customer_gateway" "onprem" {
  bgp_asn    = 65000
  ip_address = "198.51.100.25"
  type       = "ipsec.1"

  tags = {
    Name = "cgw-primary-datacenter"
  }
}

# Virtual Private Gateway attached to VPC
resource "aws_vpn_gateway" "vpn_gw" {
  vpc_id = var.vpc_id

  tags = {
    Name = "vgw-main"
  }
}

# Site-to-Site VPN Connection
resource "aws_vpn_connection" "main" {
  vpn_gateway_id      = aws_vpn_gateway.vpn_gw.id
  customer_gateway_id = aws_customer_gateway.onprem.id
  type                = "ipsec.1"
  static_routes_only  = false

  tunnel1_preshared_key = var.vpn_tunnel1_psk
  tunnel2_preshared_key = var.vpn_tunnel2_psk

  tags = {
    Name = "vpn-site-to-site-prod"
  }
}

# Enable automatic route propagation into private route table
resource "aws_vpn_gateway_route_propagation" "private_propagation" {
  vpn_gateway_id = aws_vpn_gateway.vpn_gw.id
  route_table_id = var.private_route_table_id
}
```

### 5.2 Direct Connect Gateway Attached to Multiple VPCs

```hcl
# Direct Connect Gateway
resource "aws_dx_gateway" "dxgw" {
  name            = "global-dx-gateway"
  amazon_side_asn = "64512"
}

# Associate DX Gateway with Production VPC VGW
resource "aws_dx_gateway_association" "prod_assoc" {
  dx_gateway_id         = aws_dx_gateway.dxgw.id
  associated_gateway_id = aws_vpn_gateway.prod_vgw.id
  allowed_prefixes      = ["10.100.0.0/16"]
}

# Associate DX Gateway with Staging VPC VGW
resource "aws_dx_gateway_association" "staging_assoc" {
  dx_gateway_id         = aws_dx_gateway.dxgw.id
  associated_gateway_id = aws_vpn_gateway.staging_vgw.id
  allowed_prefixes      = ["10.200.0.0/16"]
}
```

### 5.3 AWS Client VPN Endpoint with Mutual TLS Authentication

```hcl
resource "aws_ec2_client_vpn_endpoint" "dev_vpn" {
  description            = "Client VPN for engineering team"
  server_certificate_arn = var.server_acm_arn
  client_cidr_block      = "172.20.0.0/22"
  split_tunnel           = true

  authentication_options {
    type                       = "certificate-authentication"
    root_certificate_chain_arn = var.client_root_ca_arn
  }

  connection_log_options {
    enabled               = true
    cloudwatch_log_group  = var.cw_log_group_name
    cloudwatch_log_stream = var.cw_log_stream_name
  }
}

# Associate Client VPN with Private Subnet
resource "aws_ec2_client_vpn_network_association" "subnet_assoc" {
  client_vpn_endpoint_id = aws_ec2_client_vpn_endpoint.dev_vpn.id
  subnet_id              = var.private_subnet_id
}

# Authorize ingress traffic to VPC CIDR
resource "aws_ec2_client_vpn_authorization_rule" "allow_vpc" {
  client_vpn_endpoint_id = aws_ec2_client_vpn_endpoint.dev_vpn.id
  target_network_cidr    = var.vpc_cidr
  authorize_all_groups   = true
}
```

### 5.4 Hybrid Active/Passive Failover: Direct Connect Primary with VPN Backup

```hcl
# Attach both Direct Connect Transit VIF and Site-to-Site VPN to AWS Transit Gateway
# On-premises router prepends BGP AS Path on the VPN connection:
# - Advertised over Direct Connect: AS 65000
# - Advertised over Site-to-Site VPN: AS 65000 65000 65000 (Prepended twice)
# Result: AWS Transit Gateway automatically routes traffic over Direct Connect due to shorter AS Path.
# If Direct Connect physical port fails, BGP withdraws route, instantly shifting traffic to VPN.
```

### 5.5 MACsec Encryption on 100 Gbps Dedicated Direct Connect

```bash
# Enable MACsec (IEEE 802.1AE) for line-rate physical hardware layer encryption
aws directconnect associate-mac-sec-key \
    --connection-id "dxcon-01234567" \
    --secret-arn "arn:aws:secretsmanager:us-east-1:123456789012:secret:macsec-key-12345" \
    --ckn "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
```

---

## 6. Best Practices

1. **Always Configure Both VPN Tunnels**: AWS provides two tunnel endpoints located in distinct Availability Zones. Configure your customer gateway to maintain both tunnels active to ensure seamless failover during AWS maintenance.
2. **Use BGP Dynamic Routing Over Static Routes**: BGP dynamically detects dead link peers (via BFD - Bidirectional Forwarding Detection) within seconds and updates route paths automatically.
3. **Deploy Backup VPN for Direct Connect**: For non-critical workloads, use an AWS Site-to-Site VPN as a cost-effective backup to a single Direct Connect link. For mission-critical workloads, deploy dual Direct Connect links across diverse data centers.
4. **Enable Split Tunnel on Client VPN**: Prevents all employee Internet browsing from backhauling through your AWS VPC; only traffic destined for internal AWS CIDRs routes through the VPN tunnel.
5. **Standardize on AWS Transit Gateway for Scale**: Connecting more than 3 VPCs to on-premises via VGWs creates an unmanageable web of circuits. Use Transit Gateway paired with a Transit VIF.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Configuring Only One VPN Tunnel | Customer gateway administrator only configures Tunnel 1. | Downtime during AWS periodic maintenance on Tunnel 1. | Always configure both Tunnel 1 and Tunnel 2 in your firewall. |
| Forgetting Route Propagation | Private route table does not have VPN Route Propagation enabled. | Traffic reaches VPC from on-prem, but return packets drop. | Enable `Enable Route Propagation` on VPC Route Tables. |
| Asymmetric Routing Dropping Packets | Traffic enters via Direct Connect but leaves via VPN. | Stateful firewalls drop packets due to missing state. | Ensure BGP route metrics (AS Path, Local Preference) are symmetric. |
| Overlapping CIDR Blocks | On-prem network and AWS VPC both use `10.0.0.0/16`. | Routing collisions; packets cannot be dispatched. | Enforce strict non-overlapping IP address management (IPAM). |
| Expecting Public VIF to Access Private VPC | Customer provisions Public VIF and tries to ping EC2 private IP. | Cannot route (Public VIF is only for public AWS IPs). | Provision a Private VIF or Transit VIF for private VPC routing. |
| Missing MTU Adjustment (Packet Fragmentation) | MTU set to standard 1500 on IPsec tunnel instead of 1420. | Packet fragmentation causes severe throughput degradation. | Clamp MSS to 1379 and set tunnel MTU to 1420 bytes. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Throughput
- **Site-to-Site VPN**: Maximum throughput per tunnel is **1.25 Gbps**. To achieve higher speeds (up to ~5 Gbps), use **ECMP (Equal-Cost Multi-Path)** across multiple VPN connections on AWS Transit Gateway.
- **Direct Connect**: Zero jitter, dedicated unshared bandwidth at 1, 10, or 100 Gbps line rates.

### 8.2 Security
- **IPsec AES-256 Encryption**: Site-to-Site VPN encrypts data in transit using strong IKEv2 / IPsec ciphers.
- **MACsec (Layer 2 Encryption)**: Direct Connect does not encrypt traffic by default. For regulated data, enable MACsec (available on 10 Gbps and 100 Gbps dedicated connections) or run an IPsec VPN tunnel *over* the Direct Connect link.

### 8.3 Cost Considerations
- **Site-to-Site VPN**: \$0.05 per connection hour (~$36/month per VPN) + standard AWS data transfer out rates (\$0.09/GB).
- **Direct Connect Port Hours**: 1 Gbps dedicated port is ~\$0.30/hour (~$216/month); 10 Gbps is ~\$2.25/hour.
- **Direct Connect Data Transfer Discount**: Egress over Direct Connect is discounted to **\$0.02/GB** (vs \$0.09/GB over Internet), saving massive costs for data-intensive enterprises transferring hundreds of terabytes monthly.

---

## 9. Comparison Table

| Dimension | AWS Site-to-Site VPN | AWS Direct Connect (Dedicated) | AWS Client VPN |
|---|---|---|---|
| **Connection Type** | Encrypted tunnel over Public Internet | Dedicated private physical fiber | OpenVPN remote user client |
| **Throughput Limit**| 1.25 Gbps per tunnel | Up to 100 Gbps line rate | Scales dynamically per client |
| **Setup Time** | Minutes | Weeks to months (telecom cross-connect) | Minutes |
| **Reliability** | Internet weather dependent | 99.9% to 99.99% dedicated SLA | Cloud managed |
| **Encryption** | IPsec (Default enabled) | None (Requires MACsec or IPsec overlay) | TLS / OpenVPN |
| **Egress Data Cost** | Standard (\$0.09/GB) | Discounted (~$0.02/GB) | Standard internet egress |
| **Primary Target** | Branch offices & DR backup | Core Enterprise Datacenters | Remote developers & employees |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How do you design an active-passive hybrid architecture using Direct Connect and a Site-to-Site VPN backup?</strong></summary>

Both the Direct Connect (via Direct Connect Gateway) and the Site-to-Site VPN connect to an AWS Transit Gateway or Virtual Private Gateway. 
1. **Dynamic BGP**: Both links establish BGP peering with AWS using autonomous system numbers (ASNs).
2. **Path Selection**: In AWS route evaluation, Direct Connect routes are preferred over VPN routes by default if prefixes are identical. On the on-premises router, configure BGP AS-Path Prepending on the VPN connection (e.g., repeating your ASN 3 times) and assign a lower BGP Local Preference to the VPN link.
3. **Failover**: If the physical Direct Connect link is severed, the on-premise router withdraws the DX BGP announcement, causing AWS to automatically reroute hybrid traffic through the encrypted VPN tunnel within seconds.
</details>

<details>
<summary><strong>Q2: What is the purpose of an AWS Transit VIF versus a Private VIF?</strong></summary>

- **Private VIF**: Connects a Direct Connect circuit directly to a Virtual Private Gateway (VGW) attached to a single VPC. It cannot connect to an AWS Transit Gateway.
- **Transit VIF**: Connects a Direct Connect circuit to an **AWS Direct Connect Gateway**, which in turn attaches to an **AWS Transit Gateway (TGW)**. This allows a single Direct Connect physical connection to route traffic to thousands of VPCs, shared services, and multi-account landing zones across multiple regions.
</details>

<details>
<summary><strong>Q3: How does Equal-Cost Multi-Path (ECMP) routing work with AWS Site-to-Site VPN?</strong></summary>

A single VPN tunnel has a bandwidth cap of 1.25 Gbps. To scale beyond this limit, you configure multiple VPN connections terminating on an **AWS Transit Gateway** and enable ECMP on the Transit Gateway route table. When dynamic BGP advertises the identical CIDR with identical AS Path lengths across multiple tunnels, the Transit Gateway automatically load balances packets across up to 4 parallel tunnels, achieving aggregate bandwidth up to ~5 Gbps.
</details>

<details>
<summary><strong>Q4: Why does Direct Connect not provide encryption by default, and how can it be encrypted?</strong></summary>

Direct Connect is a Layer 2/Layer 3 private telecommunications circuit provided by telecom carriers. Because it is a physically dedicated circuit outside the public Internet, data is isolated from public traffic, but raw Ethernet frames remain unencrypted. 
To encrypt traffic over Direct Connect:
1. Enable **MACsec (IEEE 802.1AE)** on compatible 10 Gbps and 100 Gbps dedicated ports to provide line-rate Layer 2 hardware encryption.
2. Deploy a **Site-to-Site IPsec VPN overlay** on top of a Direct Connect Public VIF or Transit VIF, providing end-to-end Layer 3 encryption.
</details>

<details>
<summary><strong>Q5: What is Bidirectional Forwarding Detection (BFD) and why is it recommended for Direct Connect?</strong></summary>

Standard BGP keepalive timers check link status every 30 to 90 seconds. If an intermediate fiber cut occurs between your router and AWS, BGP might take up to 90 seconds to declare the link dead and fail over. BFD runs fast sub-second heartbeat probes (e.g., 300ms intervals). When an interface or transport issue occurs, BFD immediately notifies BGP to tear down the neighbor session and trigger failover in under 1 second.
</details>

<details>
<summary><strong>Q6: What is the role of a Customer Gateway (CGW) resource in AWS?</strong></summary>

A Customer Gateway in AWS is a metadata resource that provides AWS with information about your on-premises physical or software appliance. It records the appliance's public IPv4 address (or private IP if using Private IP VPN), its BGP Autonomous System Number (ASN), and optional certificate details for IKE authentication.
</details>

<details>
<summary><strong>Q7: How does Split Tunneling improve the user experience in AWS Client VPN?</strong></summary>

Without split tunneling, all client device network traffic—including streaming video, web searches, and personal traffic—is encapsulated and forced through the VPN tunnel into the AWS VPC, consuming company VPC bandwidth and degrading browsing speeds. 
With **Split Tunneling enabled**, the Client VPN desktop software only routes packets destined for the specific VPC CIDR ranges into the tunnel; all generic public Internet requests exit directly through the employee's local ISP.
</details>

<details>
<summary><strong>Q8: What is an AWS Direct Connect Public VIF used for?</strong></summary>

A Public VIF allows on-premises systems to communicate directly with AWS public IP endpoints—such as Amazon S3, DynamoDB, Amazon SQS, and AWS CloudFront—over the dedicated private Direct Connect link instead of routing across the public Internet. BGP advertises all global AWS public IP prefixes over the Public VIF.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Simulated Site-to-Site VPN Connection</strong></summary>

**Goal**: Provision Customer Gateway, Virtual Private Gateway, and inspect configuration parameters.
1. Create a Customer Gateway with public IP `198.51.100.1` and ASN `65000`.
2. Create and attach a VGW to your test VPC.
3. Create a Site-to-Site VPN connection.
4. Download the router configuration template for a Cisco or generic device and inspect Tunnel 1/2 PSK and BGP IP addresses.
</details>

<details>
<summary><strong>Exercise 2: Configure Route Propagation in VPC Route Table</strong></summary>

**Goal**: Automatically inject on-premises routes into AWS routing tables.
1. Navigate to VPC -> Route Tables.
2. Select your private subnet route table.
3. Enable Route Propagation for the attached Virtual Private Gateway.
4. Verify the route table displays propagated routes with the status `Yes`.
</details>

<details>
<summary><strong>Exercise 3: Deploy AWS Client VPN Endpoint</strong></summary>

**Goal**: Connect a developer laptop to private VPC resources using OpenVPN.
1. Generate server and client certificates using Easy-RSA.
2. Import certificates into AWS Certificate Manager (ACM).
3. Create a Client VPN endpoint with split-tunneling enabled.
4. Download the `.ovpn` configuration file, append client cert/key, and connect using the OpenVPN GUI client.
</details>

<details>
<summary><strong>Exercise 4: Analyze BGP AS Path Prepending for Failover</strong></summary>

**Goal**: Understand route selection mechanics between diverse links.
1. Map out an architecture with Direct Connect (ASN 65000) and VPN (ASN 65000 65000 65000).
2. Trace the BGP path evaluation inside AWS Transit Gateway.
3. Document why the shorter AS path wins and verify failover steps during simulated fiber cut.
</details>

<details>
<summary><strong>Exercise 5: Calculate Direct Connect Cost Break-Even Point</strong></summary>

**Goal**: Determine when Direct Connect becomes cheaper than Internet egress.
1. Compare standard egress at \$0.09/GB vs Direct Connect egress at \$0.02/GB (\$0.07/GB savings).
2. Factor in a 1 Gbps port cost of \$216/month.
3. Calculate the monthly data volume required to break even (\$216 / \$0.07 = ~3,085 GB / ~3.1 TB).
</details>

<details>
<summary><strong>Exercise 6: Set up MTU Clamping for IPsec Tunnels</strong></summary>

**Goal**: Prevent TCP packet drops due to IPsec encapsulation overhead.
1. Configure an on-premise firewall or Linux strongSwan router.
2. Set MTU to 1420 bytes on the `vti0` virtual tunnel interface.
3. Add iptables rule: `iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --clamp-mss-to-pmtu`.
4. Test with large `ping -s 1400 -M do <ip>` packets to confirm fragmentation avoidance.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| HYBRID AWS NETWORKING SELECTION MATRIX                                                            |
+--------------------------+-----------------------+-----------------------+------------------------+
| Requirement              | Site-to-Site VPN      | Direct Connect (DX)   | Client VPN             |
+--------------------------+-----------------------+-----------------------+------------------------+
| Dedicated Physical Link  | No (Public Internet)  | Yes (Private Fiber)   | No (Public Internet)   |
| Bandwidth Capacity       | 1.25 Gbps per tunnel  | 1 Gbps - 100 Gbps     | Elastic per client     |
| Provisioning Time        | 10 minutes            | 2 - 8 weeks           | 15 minutes             |
| End-User Target          | Branch offices        | Core Data Centers     | Mobile / Remote users  |
| Transit Gateway Support  | Yes (ECMP up to 5G)   | Yes (Transit VIF)     | No (Subnet target)     |
| BGP Routing Protocol     | Dynamic or Static     | Dynamic BGP required  | N/A                    |
+--------------------------+-----------------------+-----------------------+------------------------+
```

- **Direct Connect**: Unmatched consistency, high throughput, and reduced egress costs for primary data centers.
- **Site-to-Site VPN**: Quick, encrypted, resilient connectivity ideal for branch offices and Direct Connect failover.
- **Client VPN**: Secure remote developer and employee access to private cloud resources.

---

## 13. Further Reading

- [AWS Site-to-Site VPN User Guide](https://docs.aws.amazon.com/vpn/latest/s2svpn/VPC_VPN.html)
- [AWS Direct Connect User Guide](https://docs.aws.amazon.com/directconnect/latest/UserGuide/)
- [AWS Client VPN Administrator Guide](https://docs.aws.amazon.com/vpn/latest/clientvpn-admin/what-is.html)
- [Hybrid Cloud DNS with Route 53 Resolver and Direct Connect](https://aws.amazon.com/blogs/networking-and-content-delivery/hybrid-cloud-dns-options-for-amazon-vpc/)
- [04-networking/04-vpc-peering-transit-gateway-and-privatelink.md](file:///04-devops/aws/04-networking/04-vpc-peering-transit-gateway-and-privatelink.md)
