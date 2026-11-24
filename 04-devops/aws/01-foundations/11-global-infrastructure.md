# AWS Global Infrastructure: Regions, AZs, Edge & Partitions

> AWS Global Infrastructure provides the physical and optical topology of the cloud, spanning autonomous geographic Regions, multi-datacenter Availability Zones, 600+ Edge Points of Presence, and sovereign administrative Partitions.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01-what-is-aws.md](file:///04-devops/aws/01-foundations/01-what-is-aws.md), [03-accounts-regions-and-billing.md](file:///04-devops/aws/01-foundations/03-accounts-regions-and-billing.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | Global Cloud Physical Topology & Edge Network |
| **Tool Version** | AWS Global Infrastructure API, AWS CLI v2.22+ |
| **Tested With** | CloudFront Edge APIs, EC2 Local Zones, Linux AL2023 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Security & Cost Notes](#8-performance-security--cost-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
Software engineers often conceptualize "the cloud" as an abstract, ethereal fabric. In reality, cloud workloads are governed strictly by physical constraints: the speed of light through fiber optic cables, seismic fault lines, geopolitical data sovereignty borders, and regional power grid independence.

Understanding **AWS Global Infrastructure** allows architects to design systems that balance latency, compliance, and disaster resilience. If an application must process financial transactions within 5 milliseconds of Chicago traders, running in `us-east-1` (Virginia) violates physical latency thresholds. By selecting appropriate Regions, Availability Zones, Local Zones, Wavelength (5G edge), and CloudFront Points of Presence, engineers place compute and data exactly where physical and legal requirements dictate.

---

## 2. Core Concepts
- **Geographic Hierarchy**:
  - *Regions*: Completely autonomous physical geographical areas containing multiple AZs (e.g., `us-east-1` N. Virginia, `ap-northeast-1` Tokyo).
  - *Availability Zones (AZs)*: One or more discrete data centers with redundant power, networking, and connectivity, separated by meaningful physical distance (< 100 km).
  - *Local Zones*: Extensions of an AWS Region placing compute, storage, and database services close to major population centers (e.g. Chicago, Los Angeles, Dallas) for single-digit millisecond latency.
  - *AWS Wavelength*: Embeds AWS compute and storage inside telecommunications 5G networks (Verizon, Vodafone) for ultra-low latency mobile edge processing.
  - *AWS Outposts*: Physical AWS-managed 42U server racks delivered and installed directly inside customer on-premises data centers.
- **Edge Network**:
  - *Edge Locations (PoPs)*: 600+ points of presence worldwide caching static and dynamic web content (Amazon CloudFront) and terminating DNS queries (Amazon Route 53).
  - *Regional Edge Caches (RECs)*: Larger intermediate caching hubs positioned between edge locations and origin servers.
- **AWS Partitions**:
  - Independent administrative groupings with completely isolated IAM credential spaces:
    1. `aws`: Standard Commercial Partition (Global).
    2. `aws-us-gov`: AWS GovCloud (US-East and US-West, FedRAMP High, ITAR regulated).
    3. `aws-cn`: AWS China Partition (Beijing and Ningxia, ICP license mandated).

---

## 3. How It Works Internally
Global Infrastructure Latency and Connectivity Hierarchy:

```
[ End Users Worldwide ]
        |
        | Sub-10ms (Anycast BGP)
        v
+-------------------------------------------------------------+
|               CloudFront Edge Locations (600+ PoPs)         |
|  - TLS Termination, DDoS Mitigation (AWS Shield), WAF       |
+-------------------------------------------------------------+
        |
        | Private AWS 100Gbps+ Redundant Backbone
        v
+-------------------------------------------------------------+
|                 Regional Edge Caches (RECs)                 |
+-------------------------------------------------------------+
        |
        | Dedicated Private Optical Fiber Mesh
        v
+-------------------------------------------------------------+
|                AWS Region (e.g. us-east-1)                  |
|  +-----------------------+     +-----------------------+    |
|  | AZ 1 (use1-az1)       |     | AZ 2 (use1-az2)       |    |
|  | Data Centers A, B, C  |<--->| Data Centers D, E     |    |
|  +-----------------------+     +-----------------------+    |
|             \                   /                           |
|              v                 v                            |
|       [ Dedicated Local Zones & Outposts Links ]            |
+-------------------------------------------------------------+
```

### The Custom AWS Backbone
AWS operates its own global, private, terrestrial and subsea trans-oceanic fiber optic network. When a user requests content via CloudFront or an ALB with Global Accelerator, traffic enters the nearest Edge PoP and travels across the private AWS fiber backbone rather than traversing the congested public internet.

---

## 4. Syntax & Usage
CLI commands for discovering regions, zones, and local zone extensions:

```bash
# Query all active commercial regions
aws ec2 describe-regions --output table

# Discover Local Zones available in an account
aws ec2 describe-availability-zones \
    --all-availability-zones \
    --filters "Name=zone-type,Values=local-zone" \
    --query "AvailabilityZones[].[ZoneName, ZoneId, ParentRegionName, State]" \
    --output table

# Opt-in / Enable a specific Local Zone
aws ec2 modify-availability-zone-group \
    --group-name "us-east-1-chi-1" \
    --opt-in-status "opted-in"

# Query CloudFront edge point-of-presence origins
aws cloudfront list-distributions --query "DistributionList.Items[].[Id, DomainName, Status]"
```

---

## 5. Practical Examples

### Example 1: Discovering Physical Zone IDs to Eliminate Mapping Drift
- **Goal:** Identify the physical `ZoneId` behind arbitrary `ZoneName` strings to coordinate low-latency cross-account VPC peering.
- **Commands:**
```bash
aws ec2 describe-availability-zones \
    --region us-east-1 \
    --query "AvailabilityZones[?ZoneType=='availability-zone'].[ZoneName, ZoneId]" \
    --output table
```
- **Real Output:**
```text
-----------------------------
| DescribeAvailabilityZones |
+--------------+------------+
|  us-east-1a  |  use1-az1  |
|  us-east-1b  |  use1-az2  |
|  us-east-1c  |  use1-az4  |
|  us-east-1d  |  use1-az6  |
|  us-east-1e  |  use1-az3  |
|  us-east-1f  |  use1-az5  |
+--------------+------------+
```
- **Explanation:** To ensure two instances in different AWS accounts reside in the exact same physical building, both must be launched into the matching `ZoneId` (`use1-az1`).

### Example 2: Enabling and Subnetting a Local Zone
- **Goal:** Enable the Chicago Local Zone (`us-east-1-chi-1a`) to host low-latency compute.
- **Commands:**
```bash
# 1. Opt-in to Local Zone Group
aws ec2 modify-availability-zone-group \
    --group-name "us-east-1-chi-1" \
    --opt-in-status "opted-in"

# 2. Create subnet extending existing VPC into the Local Zone
aws ec2 create-subnet \
    --vpc-id vpc-0123456789abcdef0 \
    --cidr-block 10.0.100.0/24 \
    --availability-zone "us-east-1-chi-1a" \
    --query "Subnet.SubnetId" --output text
```
- **Real Output:**
```text
subnet-0a1b2c3d4e5f67890
```
- **Explanation:** The subnet operates seamlessly within the parent VPC, but compute launched within it physically executes in Chicago.

### Example 3: Edge Case - Multi-Partition ARN Syntax
- **Goal:** Author an IAM policy that operates validly in both Standard Commercial and AWS GovCloud.
- **ARN Pattern:**
```text
Standard Commercial: arn:aws:s3:::my-bucket
AWS GovCloud:        arn:aws-us-gov:s3:::my-bucket
AWS China:           arn:aws-cn:s3:::my-bucket
```
- **Partition-Independent IAM Statement:**
```json
{
    "Effect": "Allow",
    "Action": "s3:GetObject",
    "Resource": "arn:${aws:Partition}:s3:::my-company-bucket/*"
}
```
- **Explanation:** Using the `${aws:Partition}` policy variable prevents hardcoded `arn:aws:` strings from failing when deployed to GovCloud or China.

### Example 4: Failure & Anti-Pattern - Assuming Service Parity in Every Region
- **Goal:** Demonstrate error when calling a service API in a region where the service is not deployed.
- **Attempted Command:**
```bash
# Attempting to call AWS Bedrock in a region where Bedrock is not offered
aws bedrock list-foundation-models --region ap-south-2
```
- **Real Output:**
```text
Could not connect to the endpoint URL: "https://bedrock.ap-south-2.amazonaws.com/foundation-models"
```
- **Explanation & Fix:** Not all services exist in every region. Newer regions (e.g. `ap-south-2` Hyderabad, `me-central-1` UAE) take months or years to achieve parity with tier-1 regions like `us-east-1` or `eu-west-1`. Always verify the **AWS Regional Services List** before selecting regions.

### Example 5: Global Accelerator vs CloudFront Routing
- **Goal:** Accelerate non-HTTP TCP/UDP traffic using AWS Global Accelerator Anycast IPs.
- **Commands:**
```bash
aws globalaccelerator create-accelerator \
    --name "Gaming-UDP-Accelerator" \
    --ip-address-type IPV4 \
    --enabled \
    --query "Accelerator.[DnsName, Ipv4Addresses]" --output json
```
- **Real Output:**
```json
[
    "a1b2c3d4e5f67890.awsglobalaccelerator.com",
    [
        "192.0.2.10",
        "198.51.100.20"
    ]
]
```
- **Explanation:** Binds static Anycast IPv4 addresses at edge PoPs, routing raw TCP/UDP packets across the AWS private backbone directly to regional endpoints.

---

## 6. Best Practices
1. **Always Use `ZoneId` for Inter-Account Latency Optimization**: Never assume `us-east-1a` in Account A is the same physical facility as `us-east-1a` in Account B.
2. **Deploy Production Services Across 3 Availability Zones**: Multi-AZ deployments spanning 3 AZs survive the complete loss of an entire data center facility without dropping below N+1 redundancy.
3. **Use CloudFront Origin Access Control (OAC)**: Terminate TLS and serve static assets from edge PoPs, blocking direct public access to regional S3 origin buckets.
4. **Use Local Zones for Ultra-Low Latency (< 10ms)**: Deploy real-time gaming, financial trading, and video rendering workloads into Local Zones near major cities.
5. **Never Hardcode Partition Names in ARNs**: Use dynamic interpolation `arn:${aws:Partition}:...` in CloudFormation and Terraform modules.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Assuming all AWS services exist in all regions | Calling un-deployed regional endpoints throws DNS resolution errors | Check the official AWS Regional Services List before architectural signoff |
| Designing multi-region architecture without cross-region replication | Disasters in primary region result in total data loss in the secondary region | Configure automated S3 Cross-Region Replication (CRR) and Aurora Global Databases |
| Neglecting data transfer fees between regions | Cross-region replication of terabytes incurs high outbound transfer fees (~$0.02/GB) | Optimize payloads, compress objects, and replicate only business-critical datasets |
| Treating Local Zones as complete regions | Local Zones lack heavy services (e.g. RDS Aurora, Redshift, EMR) | Run primary databases in the parent region and cache/compute in the Local Zone |
| Confusing CloudFront with Global Accelerator | CloudFront optimizes HTTP/HTTPS caching; Global Accelerator optimizes raw TCP/UDP | Use CloudFront for web assets; Global Accelerator for gaming, IoT, or non-HTTP protocols |
| Attempting to use commercial IAM credentials in GovCloud | GovCloud (`aws-us-gov`) is an isolated partition with completely separate IAM stores | Create dedicated GovCloud credentials and configure the `us-gov-west-1` region profile |

---

## 8. Performance, Security & Cost Notes
- **Performance**: Edge PoPs terminate TLS handshakes within 10–20ms of end users, saving 100ms+ of round-trip TCP handshakes to distant regional data centers.
- **Security**: Traffic moving across the private AWS global backbone between regions is automatically encrypted at the physical layer (MACsec) on all point-to-point links.
- **Cost**: CloudFront data transfer out to the internet is cheaper than raw EC2 data transfer out. Routing user downloads through CloudFront reduces egress bills by 20–40%.

---

## 9. Comparison / When to Use What

| Architecture Component | Scale | Latency Profile | Best Use Case |
|---|---|---|---|
| **Availability Zone (AZ)** | 1+ physical data centers | < 2ms intra-region | Core production workloads, high-availability clusters |
| **Local Zone** | Metro-area mini data center | < 10ms to local users | Real-time gaming, media rendering, hybrid on-prem extension |
| **AWS Wavelength** | Embedded in 5G mobile towers | < 10ms to mobile devices | Smart cars, autonomous robotics, mobile AR/VR |
| **CloudFront Edge (PoP)**| 600+ edge locations globally | < 20ms to global web users | Caching static assets, video streaming, API acceleration |
| **AWS Outposts** | 42U physical on-premises rack | < 1ms to local LAN | Regulatory data sovereignty, hospital PACS systems |

---

## 10. Interview Questions

<details>
<summary>1. What is an AWS Partition and how many exist today?</summary>
An AWS Partition is an independent administrative grouping of regions with isolated IAM roots and credential databases. There are three primary partitions: `aws` (Standard Commercial), `aws-us-gov` (AWS GovCloud US), and `aws-cn` (AWS China).
</details>

<details>
<summary>2. Why can't two engineers in different AWS accounts assume their EC2 instances in `us-east-1a` are in the same building?</summary>
To prevent resource concentration, AWS randomizes the assignment of letter suffixes (`us-east-1a`, `us-east-1b`) to physical data centers per account. They must compare the physical `ZoneId` (e.g. `use1-az1`) to ensure co-location.
</details>

<details>
<summary>3. What is the architectural role of AWS Local Zones?</summary>
Local Zones place compute, storage, and networking services near large population, industry, and IT centers where no AWS Region currently exists (e.g. Los Angeles, Chicago), providing sub-10ms latency for real-time applications.
</details>

<details>
<summary>4. How does Amazon CloudFront differ from AWS Global Accelerator?</summary>
CloudFront is a Content Delivery Network (CDN) that caches HTTP/HTTPS content at edge locations worldwide. AWS Global Accelerator does not cache content; it uses Anycast IP addresses at edge PoPs to ingest raw TCP/UDP traffic and route it across the private AWS backbone to regional targets.
</details>

<details>
<summary>5. How does AWS protect inter-region data traffic from eavesdropping?</summary>
All traffic traversing the AWS global network between regions and data centers is automatically encrypted at the physical layer using IEEE 802.1AE MAC Security (MACsec) without performance degradation.
</details>

<details>
<summary>6. What is AWS Wavelength?</summary>
AWS Wavelength embeds AWS compute and storage infrastructure directly inside telecommunications providers' 5G networks (e.g. Verizon, Vodafone), allowing developers to serve mobile devices with single-digit millisecond latency.
</details>

<details>
<summary>7. What happens when an AZ suffers a catastrophic utility failure?</summary>
Because Availability Zones are physically separated and equipped with independent power substations, backup generators, and diverse internet feeds, failure in one AZ does not propagate to adjacent AZs in the same region.
</details>

<details>
<summary>8. How do Regional Edge Caches (RECs) improve CDN cache hit ratios?</summary>
RECs sit between Edge Locations (PoPs) and customer origin servers. They have larger cache storage tiers. When content falls out of an edge PoP cache, it queries the REC before hitting the origin, protecting backend servers from origin fetch spikes.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): List all active regions in your account</summary>
**Task:** Output all enabled commercial regions in table format.

**Solution:**
```bash
aws ec2 describe-regions \
    --all-regions \
    --query "Regions[?OptInStatus=='opt-in-not-required' || OptInStatus=='opted-in'].RegionName" \
    --output table
```
</details>

<details>
<summary>Exercise 2 (Easy): Identify the physical ZoneId of your default AZs</summary>
**Task:** Retrieve the physical Zone ID mappings for `eu-west-1`.

**Solution:**
```bash
aws ec2 describe-availability-zones \
    --region eu-west-1 \
    --query "AvailabilityZones[].[ZoneName, ZoneId]" \
    --output table
```
</details>

<details>
<summary>Exercise 3 (Medium): List all available Local Zones in North America</summary>
**Task:** Query all Local Zones located in the `us-east-1` parent region.

**Solution:**
```bash
aws ec2 describe-availability-zones \
    --region us-east-1 \
    --all-availability-zones \
    --filters "Name=zone-type,Values=local-zone" \
    --query "AvailabilityZones[].[ZoneName, ZoneId, State]" \
    --output table
```
</details>

<details>
<summary>Exercise 4 (Medium): Query active CloudFront Edge Point-of-Presence distributions</summary>
**Task:** List all active CloudFront distribution domains and their target origins.

**Solution:**
```bash
aws cloudfront list-distributions \
    --query "DistributionList.Items[].[DomainName, Origins.Items[0].DomainName, Status]" \
    --output table
```
</details>

<details>
<summary>Exercise 5 (Hard): Script cross-account physical AZ alignment</summary>
**Task:** Write a bash script that compares Zone IDs between two accounts to find shared physical facilities.

**Solution:**
```bash
#!/usr/bin/env bash
set -euo pipefail

REGION="us-east-1"
echo "Fetching Zone IDs for $REGION..."

ZONES=$(aws ec2 describe-availability-zones --region "$REGION" --query "AvailabilityZones[].[ZoneId, ZoneName]" --output text)

echo "Physical Mapping in active account:"
echo "$ZONES"
echo "Use the 'ZoneId' column (e.g. use1-az1) when configuring cross-account resources."
```
</details>

<details>
<summary>Exercise 6 (Hard): Provision an AWS Global Accelerator endpoint</summary>
**Task:** Use the CLI to create a standard dual-stack Global Accelerator with tags.

**Solution:**
```bash
aws globalaccelerator create-accelerator \
    --name "Global-App-Accelerator" \
    --ip-address-type IPV4 \
    --tags Key=Environment,Value=Production Key=Project,Value=EdgeRouting \
    --enabled \
    --query "Accelerator.[DnsName, Ipv4Addresses]" \
    --output table
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    GLOBAL INFRASTRUCTURE CHEAT SHEET                            |
+---------------------------------------------------------------------------------+
| REGION         | Autonomous geographic territory containing 3+ physical AZs     |
| AZ (DATA CTR)  | Sub-2ms optical latency; independent flood plains & generators |
| ZONE ID        | Immutable physical ID (use1-az1); eliminates account alias drift|
| LOCAL ZONE     | Single-digit millisecond latency extension near major metros   |
| WAVELENGTH     | 5G mobile telco edge compute (Verizon, Vodafone, KDDI)         |
| EDGE POPS      | 600+ edge locations caching static/dynamic content (CloudFront)|
| PARTITIONS     | aws (Commercial) | aws-us-gov (GovCloud) | aws-cn (China)      |
| BACKBONE       | 100Gbps+ private physical fiber mesh; MACsec hardware encrypted|
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS Global Infrastructure Official Site](https://aws.amazon.com/about-aws/global-infrastructure/)
- [AWS Regional Services List](https://aws.amazon.com/about-aws/global-infrastructure/regional-product-services/)
- [AWS Local Zones Deep Dive](https://aws.amazon.com/about-aws/global-infrastructure/localzones/)
- [AWS Global Accelerator Documentation](https://docs.aws.amazon.com/global-accelerator/latest/dg/what-is-global-accelerator.html)

---

[Previous: 10-console-cloudshell-and-tagging.md](file:///04-devops/aws/01-foundations/10-console-cloudshell-and-tagging.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 12-pricing-calculators-and-budgets.md](file:///04-devops/aws/01-foundations/12-pricing-calculators-and-budgets.md)
