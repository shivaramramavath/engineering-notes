# Module 04: Networking & Content Delivery

Welcome to the **AWS Networking & Content Delivery** module. This curriculum provides comprehensive, production-grade coverage of software-defined networking (VPC), routing and subnetting, network security perimeters, global content delivery (CloudFront), intelligent DNS management (Route 53), high-performance load balancing (ALB/NLB), API management, hybrid cloud connectivity (Direct Connect & VPN), and perimeter threat mitigation.

---

## Module Overview

Networking is the backbone of every enterprise cloud architecture. This module equips network engineers, cloud architects, and systems administrators with the operational and design expertise required to construct secure, scalable, highly available, and compliant multi-tier cloud network topologies.

---

## Detailed Curriculum Roadmap

| # | Note | Description | Key Focus Areas |
|:---|:---|:---|:---|
| **01** | [Amazon VPC Fundamentals](01-vpc.md) | Software-defined networking, CIDR calculations, subnets, and internet gateways | RFC 1918 CIDR sizing, 5 AWS reserved IPs, Public vs Private subnets, IGW vs NAT Gateway |
| **02** | [Amazon VPC Deep Dive & Routing](02-vpc-deep-dive.md) | Advanced route tables, multi-AZ high availability, and egress architecture | Longest prefix match, NAT Gateway multi-AZ HA, Egress-Only Internet Gateway, VPC Flow Logs |
| **03** | [Security Groups & Network ACLs](03-security-groups-and-nacls.md) | Multi-tier network defense, stateful vs stateless packet inspection, and troubleshooting | Stateful SG vs Stateless NACL, Ephemeral port ranges, Security Group references, VPC Reachability Analyzer |
| **04** | [VPC Interconnectivity: Peering, TGW & PrivateLink](04-vpc-peering-transit-gateway-and-privatelink.md) | Connecting VPCs, hub-and-spoke transit fabrics, and private SaaS services | VPC Peering non-transitive limits, Transit Gateway route domains, AWS PrivateLink Interface Endpoints |
| **05** | [Amazon Route 53 & DNS Patterns](05-route53-and-dns-patterns.md) | Global Anycast DNS, public/private hosted zones, and intelligent routing | Alias records at zone apex, Latency/Geolocation/Weighted routing, Route 53 Resolver hybrid DNS |
| **06** | [Amazon CloudFront & Global CDN](06-cloudfront.md) | Edge delivery, cache keys, Origin Access Control, and edge compute | Edge PoPs, Regional Edge Caches, Origin Shield, Origin Access Control (OAC), CloudFront Functions vs Lambda@Edge |
| **07** | [Elastic Load Balancing: ALB, NLB & GWLB](07-load-balancing.md) | Layer 7, Layer 4, and Layer 3 load balancing, target groups, and high availability | ALB L7 vs NLB L4 vs GWLB L3, target groups, connection draining, cross-zone balancing, mutual TLS |
| **08** | [Amazon API Gateway Architecture](08-api-gateway.md) | REST APIs, HTTP APIs, WebSocket APIs, authentication, and private integrations | HTTP API vs REST API, Token bucket rate limiting, Cognito JWT authorizers, VPC Links |
| **09** | [AWS Hybrid Connectivity: Direct Connect & VPN](09-vpn-and-direct-connect-overview.md) | Connecting on-premises datacenters to AWS via private fiber and IPsec tunnels | Site-to-Site VPN dual tunnels, Client VPN OpenVPN, Direct Connect dedicated vs hosted, Transit VIF, DXGW |
| **10** | [AWS Network Security: WAF, Shield & Network Firewall](10-waf-shield-and-network-security.md) | Multi-layer perimeter defense, DDoS mitigation, and deep packet inspection | AWS Shield Standard vs Advanced, AWS WAF managed rules and rate limiting, AWS Network Firewall Suricata IPS |

---

## Architectural Principles Enforced

1. **Zero Trust & Defense-in-Depth**:
   - Network boundaries are enforced across multiple layers: Edge DDoS mitigation (AWS Shield), Layer 7 inspection (AWS WAF), Layer 3 stateful filtering (AWS Network Firewall), subnet boundaries (Stateless NACLs), and host firewalls (Stateful Security Groups).
2. **Elimination of Public IP Exposure**:
   - Workload instances (EC2, ECS, EKS, RDS) reside exclusively in private subnets with no public IPv4 addresses. Inbound traffic enters via reverse-proxies (ALBs/CloudFront), and outbound API communication traverses AWS PrivateLink VPC Endpoints.
3. **Hub-and-Spoke Scalability**:
   - Replace complex $O(n^2)$ mesh VPC peering with **AWS Transit Gateway**, isolating environments using dedicated route tables and centralizing egress inspection through an Inspection VPC.
4. **Resilient Hybrid Failover**:
   - Design mission-critical hybrid networks with **AWS Direct Connect** backed by dynamic BGP failover to redundant **AWS Site-to-Site VPN** connections.

---

## Recommended Learning Flow

```
[01. VPC Fundamentals] --------> [02. VPC Deep Dive & Routing]
        |                                       |
        v                                       v
[03. SGs & NACLs] -------------> [04. Peering, Transit Gateway & PrivateLink]
                                                |
        +---------------------------------------+
        |
        v
[05. Route 53 & DNS Patterns] ----> [06. CloudFront CDN]
        |                                   |
        v                                   v
[07. Elastic Load Balancing] -----> [08. API Gateway]
        |                                   |
        +-----------------------------------+
        |
        v
[09. Hybrid Connectivity (DX/VPN)] -> [10. Network Security (WAF/Shield/Firewall)]
```

---

## Module Pre-requisites & Cross-References

- **Compute & Containers**: See [02-compute](../02-compute/) for container networking with ECS Fargate and EKS AWS VPC CNI.
- **Storage & Databases**: See [03-storage-and-databases](../03-storage-and-databases/) for private RDS database subnets and S3 VPC Gateway Endpoints.
- **Operations & Security**: See [06-operations](../06-operations/) for multi-account networking with AWS Control Tower and Centralized Network Landing Zones.
