# Amazon VPC Deep Dive: Flow Logs, DNS & Advanced Routing

> **Module**: `04-networking`  
> **Topic**: Network Observability, Route 53 Resolver, DHCP Option Sets, and Secondary CIDRs  
> **Target Audience**: Senior Network Engineers, Cloud Security Architects, SREs  
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

Provisioning basic subnets and route tables establishes network connectivity, but managing enterprise-grade infrastructure requires deep network visibility, internal DNS governance, security forensics, and elastic address space expansion:
- Security teams must detect unauthorized port scans, data exfiltration attempts, and compromised compute instances communicating with external command-and-control (C2) servers.
- Hybrid networks connecting AWS VPCs to on-premises enterprise data centers need private DNS resolution for both cloud (`*.aws.internal`) and corporate (`*.corp.com`) domains.
- Fast-growing Kubernetes clusters (Amazon EKS) frequently face IPv4 exhaustion when allocating pod IPs from standard private subnets.

This deep dive covers advanced VPC capabilities: **VPC Flow Logs & CloudWatch Insights analysis**, **Internal DNS resolution with Route 53 Resolver**, **Custom DHCP Option Sets**, **Secondary VPC CIDR blocks (including CGNAT for EKS)**, and **IPv6 Egress-Only Internet Gateways**.

---

## 2. Core Concepts

### 2.1 VPC Flow Logs
VPC Flow Logs capture metadata about IP traffic going to and from network interfaces in your VPC:
- Can be published at three levels: **Entire VPC**, **Subnet**, or **Individual Elastic Network Interface (ENI)**.
- Can be published to **Amazon CloudWatch Logs**, **Amazon S3**, or **Kinesis Data Firehose**.
- Captures accepted traffic (`ACCEPT`) and dropped/rejected traffic (`REJECT`).
- Does **not** capture raw packet payloads (deep packet inspection); it records Layer 3/4 flow metadata.

### 2.2 Standard Flow Log Record Format
```
${version} ${account-id} ${interface-id} ${srcaddr} ${dstaddr} ${srcport} ${dstport} ${protocol} ${packets} ${bytes} ${start} ${end} ${action} ${log-status}
```
- **`protocol`**: IANA protocol number (`6` = TCP, `17` = UDP, `1` = ICMP).
- **`action`**: `ACCEPT` (permitted by both Security Groups and NACLs) or `REJECT` (blocked by Security Groups or NACLs).
- **`log-status`**: `OK` (normal log), `NODATA` (no traffic during window), or `SKIPDATA` (traffic skipped due to internal capacity constraints).

### 2.3 VPC DNS Resolution & Route 53 Resolver
VPC DNS behavior is controlled by two boolean VPC attributes:
- **`enableDnsSupport`**: Enables the internal **Amazon-Provided DNS** server (`Route 53 Resolver`) located at the VPC base address + 2 (and link-local `169.254.169.253`). Resolves public DNS records and private hosted zone domains.
- **`enableDnsHostnames`**: Assigns public DNS hostnames (e.g., `ec2-52-1-2-3.compute-1.amazonaws.com`) to instances launched with public IP addresses.

### 2.4 Secondary CIDR Blocks & CGNAT
If an existing VPC runs out of available private IP addresses, AWS allows adding up to **4 secondary IPv4 CIDR blocks**:
- Additional CIDRs can be from RFC 1918 ranges or Carrier-Grade NAT (CGNAT) space: **`100.64.0.0/10`**.
- Extremely popular in Amazon EKS: secondary CGNAT CIDRs are dedicated to Kubernetes pods, preserving the primary RFC 1918 CIDR for routable databases and corporate peering.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        VPC Flow Log Capture & Telemetry Pipeline                  |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Network Packet Flow at Nitro Hypervisor Layer ]                                |
|  Client IP: 198.51.100.45:49152 ---> EC2 Instance: 10.0.1.25:443 (TCP SYN)      |
|             |                                                                     |
|             v (Evaluated at Hyperplane / Nitro Card)                              |
|  +-----------------------------------------------------------------------------+  |
|  | Nitro Security Engine: Evaluates Security Groups & Network ACLs             |  |
|  | - If Allowed: Action = ACCEPT                                               |  |
|  | - If Blocked: Action = REJECT                                               |  |
|  +-----------------------------------------------------------------------------+  |
|             |                                                                     |
|             v (Asynchronous Flow Log Collector - Zero Compute Impact)            |
|  +-----------------------------------------------------------------------------+  |
|  | VPC Flow Log Aggregation Service                                            |  |
|  | - Aggregates flow records over 1-minute or 10-minute intervals              |  |
|  +-----------------------------------------------------------------------------+  |
|             |                                                  |                  |
|             v (Streaming)                                      v (Batch Parquet)  |
|  +----------------------------------+    +-------------------------------------+  |
|  | Amazon CloudWatch Logs           |    | Amazon S3 Security Data Lake        |  |
|  | - CloudWatch Logs Insights       |    | - Athena SQL Queries                |  |
|  | - Real-time metric filter alarms |    | - Long-term compliance retention    |  |
|  +----------------------------------+    +-------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Enabling VPC Flow Logs via AWS CLI
Publish flow logs to CloudWatch Logs with 1-minute aggregation:
```bash
# 1. Create CloudWatch Log Group
aws logs create-log-group --log-group-name "/aws/vpc/flow-logs/production"

# 2. Create IAM Role for Flow Logs
aws iam create-role \
  --role-name "VPCFlowLogsServiceRole" \
  --assume-role-policy-document '{
    "Version": "2012-10-17",
    "Statement": [{
      "Effect": "Allow",
      "Principal": {"Service": "vpc-flow-logs.amazonaws.com"},
      "Action": "sts:AssumeRole"
    }]
  }'

# 3. Create Flow Log
aws ec2 create-flow-logs \
  --resource-type VPC \
  --resource-ids "vpc-0123456789abcdef0" \
  --traffic-type ALL \
  --log-destination-type cloud-watch-logs \
  --log-group-name "/aws/vpc/flow-logs/production" \
  --deliver-logs-permission-arn "arn:aws:iam::111122223333:role/VPCFlowLogsServiceRole" \
  --max-aggregation-interval 60
```

### 4.2 Adding a Secondary CIDR Block to an Existing VPC
```bash
aws ec2 associate-vpc-cidr-block \
  --vpc-id "vpc-0123456789abcdef0" \
  --cidr-block "100.64.0.0/16"
```

---

## 5. Practical Examples

### Example 1: Essential CloudWatch Insights Queries for VPC Flow Logs

#### Query A: Top 10 IP Addresses Blocked by Security Groups / NACLs
Identify potential port scanning or DDoS attackers:
```sql
fields @timestamp, srcAddr, dstPort, action
| filter action = 'REJECT'
| stats count(*) as rejectCount by srcAddr
| sort rejectCount desc
| limit 10
```

#### Query B: Top Internal Byte Consumers (Bandwidth Analysis)
Identify instances generating high data transfer egress:
```sql
fields @timestamp, srcAddr, dstAddr, bytes
| filter action = 'ACCEPT'
| stats sum(bytes) as totalBytes by srcAddr, dstAddr
| sort totalBytes desc
| limit 10
```

#### Query C: Detecting Inbound SSH (Port 22) Brute Force Attempts
```sql
fields @timestamp, srcAddr, dstAddr, dstPort, action
| filter dstPort = 22 and action = 'REJECT'
| stats count(*) as attempts by srcAddr
| sort attempts desc
```

### Example 2: Declarative Terraform Definition for S3-Backed Flow Logs with Custom Format
Publish custom flow log fields (including TCP flags, VPC ID, and subnet ID) directly to S3 as Parquet.

```hcl
resource "aws_s3_bucket" "flow_logs_bucket" {
  bucket = "enterprise-vpc-flow-logs-111122223333"
}

resource "aws_flow_log" "s3_flow_logs" {
  log_destination      = aws_s3_bucket.flow_logs_bucket.arn
  log_destination_type = "s3"
  traffic_type         = "ALL"
  vpc_id               = var.vpc_id

  # High-resolution 1-minute aggregation interval
  max_aggregation_interval = 60

  destination_options {
    file_format                = "parquet"
    per_hour_partition         = true
    hive_compatible_partitions = true
  }

  log_format = "$${version} $${account-id} $${vpc-id} $${subnet-id} $${instance-id} $${interface-id} $${srcaddr} $${dstaddr} $${srcport} $${dstport} $${protocol} $${tcp-flags} $${packets} $${bytes} $${action} $${log-status}"
}
```

### Example 3: Configuring a Custom DHCP Option Set for Hybrid Active Directory
Direct EC2 instances to resolve internal domains via on-premises Active Directory domain controllers.

```bash
# 1. Create DHCP Option Set
DHCP_OPT_ID=$(aws ec2 create-dhcp-options \
  --dhcp-configuration \
    "Key=domain-name-servers,Values=10.50.0.10,10.50.0.11,AmazonProvidedDNS" \
    "Key=domain-name,Values=corp.example.com" \
  --query 'DhcpOptions.DhcpOptionsId' \
  --output text)

# 2. Associate DHCP Option Set with VPC
aws ec2 associate-dhcp-options \
  --vpc-id "vpc-0123456789abcdef0" \
  --dhcp-options-id "$DHCP_OPT_ID"
```

### Example 4: Creating a Dedicated EKS Pod Subnet Using Secondary CGNAT CIDR
Provision subnets specifically dedicated to container pods without consuming corporate RFC 1918 space.

```bash
aws ec2 create-subnet \
  --vpc-id "vpc-0123456789abcdef0" \
  --cidr-block "100.64.1.0/24" \
  --availability-zone "us-east-1a" \
  --tag-specifications 'ResourceType=subnet,Tags=[{Key=Name,Value=eks-pod-subnet-1a}]'
```

### Example 5: Testing Route 53 Resolver Endpoint DNS Resolution
Resolve private domain names across a hybrid Direct Connect link using Route 53 Inbound and Outbound Endpoints.

```bash
# Perform DNS lookup against the VPC local Route 53 Resolver IP (base + 2)
dig @10.0.0.2 api.internal.corp.com +short
# Or using the universal link-local address:
dig @169.254.169.253 api.internal.corp.com +short
```

---

## 6. Best Practices

1. **Enable 1-Minute Flow Log Aggregation**: The default flow log aggregation interval is 10 minutes. For production security auditing and incident response, set `max-aggregation-interval = 60` (1 minute) to detect threats in near real time.
2. **Output S3 Flow Logs in Apache Parquet**: When streaming flow logs to Amazon S3 for long-term retention, enable `file_format = "parquet"` and `hive_compatible_partitions = true`. This allows Amazon Athena to query billions of records in seconds at a fraction of the cost.
3. **Use Secondary CGNAT (`100.64.0.0/10`) for Kubernetes Fleets**: To prevent VPC IP exhaustion in large EKS deployments, attach a secondary `100.64.0.0/16` CIDR block to the VPC and configure the AWS VPC CNI with custom networking to allocate pod IPs exclusively from this secondary pool.
4. **Never Remove `AmazonProvidedDNS` from DHCP Options**: When creating custom DHCP Option Sets pointing to corporate DNS servers, always append `AmazonProvidedDNS` to the list of nameservers. Otherwise, instances will lose access to internal AWS service endpoints, metadata resolution, and Route 53 Private Hosted Zones.
5. **Always Set `enableDnsHostnames = true` and `enableDnsSupport = true`**: Both attributes should remain enabled on every VPC to ensure AWS SDKs, public endpoints, and private VPC Endpoints resolve properly.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Expecting Flow Logs to Capture Packet Data** | Inability to inspect HTTP headers or payload data; Flow Logs only capture Layer 3/4 metadata. | Use AWS Network Firewall, Traffic Mirroring, or application-level proxies for Layer 7 inspection. |
| **Omitting `AmazonProvidedDNS` in Custom DHCP** | Instances cannot resolve AWS services, Private Hosted Zones, or IAM metadata endpoints. | Always include `AmazonProvidedDNS` alongside on-premises DNS servers in DHCP Option Sets. |
| **Using 10-Minute Aggregation for Security Audits** | 10-minute lag between a network intrusion or port scan and log visibility in CloudWatch. | Configure `max_aggregation_interval = 60` (1-minute intervals). |
| **Trying to Overlap Secondary CIDR with Existing Peering** | Secondary CIDR association fails if the chosen range overlaps with an existing peered VPC or VPN route. | Coordinate enterprise IP address management (IPAM) to ensure secondary CIDRs remain unique. |
| **Flooding CloudWatch Logs with ACCEPT Traffic** | Exorbitant CloudWatch log ingestion costs for busy web applications. | Export high-volume `ALL` traffic to **Amazon S3**; export only `REJECT` traffic to CloudWatch Logs for security alarms. |
| **Attempting to Shrink or Edit a Primary CIDR Block** | Primary VPC CIDR blocks cannot be resized or deleted once created. | Add **Secondary CIDR blocks** to expand capacity without recreating the VPC. |

---

## 8. Performance, Security & Cost Notes

### Zero Performance Overhead
- VPC Flow Logs are generated by the hypervisor and Nitro hardware components outside the path of network traffic.
- Enabling Flow Logs introduces **zero latency overhead** and consumes zero CPU or network bandwidth from customer EC2 instances.

### Security Capabilities
- **TCP Flag Tracking**: Extended flow log formats capture TCP flags (SYN, ACK, FIN, RST), allowing engineers to differentiate between attempted scans (SYN with REJECT) and established connections (SYN-ACK).
- **Security Compliance**: Meets PCI-DSS, SOC 2, and FedRAMP requirements for continuous network traffic monitoring and logging.

### Cost Breakdown
- **VPC Flow Logs Service**: 100% Free.
- **CloudWatch Logs Ingestion**: $0.50 per GB ingested (can be expensive for multi-gigabit traffic).
- **S3 Storage**: Billed at standard S3 storage rates (~$0.023/GB/month), significantly cheaper for high-throughput environments.

---

## 9. Comparison Table

| Dimension | VPC Flow Logs | AWS Traffic Mirroring | AWS Network Firewall |
|:---|:---|:---|:---|
| **Inspection Level** | Layer 3 & 4 (Metadata only) | Full Layer 2–7 (Raw packet copy) | Layer 3–7 (Inline deep packet inspection)|
| **Network Position** | Out-of-band telemetry | Out-of-band packet mirror | In-line active filtering |
| **Packet Payload?** | No (IP, Port, Protocol, Bytes) | Yes (Exact byte-for-byte copy) | Yes (Stateful inspection & Snort rules) |
| **Action Capability** | Passive monitoring (Log only) | Passive monitoring (Mirror only) | Active enforcement (Allow / Drop / Alert) |
| **Performance Impact**| Zero | Zero | Minimal (< 1 ms latency) |
| **Cost** | Low (Log storage fees only) | Moderate ($0.015/hr per ENI) | High ($0.395/hr + $0.065/GB processed) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What does a VPC Flow Log record with action `REJECT` indicate?</strong></summary>

**Answer:**
An `action = REJECT` record indicates that a network packet was evaluated and dropped before reaching its destination:
- It can be dropped by an inbound/outbound **Security Group rule** (stateful firewall drop).
- It can be dropped by an inbound/outbound **Network Access Control List (NACL)** rule (stateless subnet firewall drop).
- In security analysis, high volumes of `REJECT` records on high-order ports or ports like 22 (SSH) and 3389 (RDP) indicate port-scanning activity, automated vulnerability probing, or misconfigured security group egress rules.
</details>

<details>
<summary><strong>Q2: Why does an EC2 instance in a VPC fail to resolve local DNS if `enableDnsSupport` is disabled?</strong></summary>

**Answer:**
The `enableDnsSupport` attribute controls whether the VPC activates the **Amazon-Provided DNS** server (Route 53 Resolver at `169.254.169.253` and VPC CIDR + 2):
- If `enableDnsSupport = false`, the Amazon-Provided DNS server is completely disabled for that VPC.
- Instances cannot resolve public domain names (e.g., `amazon.com`), internal Route 53 Private Hosted Zones, or AWS service endpoints using the default resolver.
- Instances can only perform DNS lookups if an external, custom DNS server is explicitly configured via DHCP option sets or statically configured within the operating system.
</details>

<details>
<summary><strong>Q3: How does adding a Secondary CIDR block solve IP address exhaustion in Amazon EKS clusters?</strong></summary>

**Answer:**
In Amazon EKS, the AWS VPC CNI plugin allocates native private IP addresses directly from the host's VPC subnet to every Kubernetes pod. In large clusters running thousands of pods, primary RFC 1918 subnets (e.g., `10.0.0.0/16`) can rapidly run out of available IPs.

AWS allows associating a secondary **Carrier-Grade NAT (CGNAT) CIDR block** (`100.64.0.0/10`) to the VPC:
1. Subnets are created within the `100.64.0.0/16` secondary range.
2. The AWS VPC CNI is configured for **Custom Networking** (`AWS_VPC_K8S_CNI_CUSTOM_NETWORK_CFG = true`).
3. EC2 worker node primary ENIs remain on the primary corporate subnet (`10.0.x.x`), while all secondary pod ENIs draw IPs exclusively from the `100.64.x.x` pool, expanding pod capacity by 65,000+ IPs without consuming scarce corporate RFC 1918 addresses.
</details>

<details>
<summary><strong>Q4: What is the difference between the 1-minute and 10-minute flow log aggregation intervals?</strong></summary>

**Answer:**
- **10-minute Interval (Default)**: Aggregates network flow packets into consolidated summary records over a 10-minute window. Results in fewer log records, reducing CloudWatch ingestion costs. However, security incident response and troubleshooting are delayed by up to 10 minutes.
- **1-minute Interval**: Aggregates records over 60-second windows. Delivers near real-time visibility into traffic spikes, dropped packets, and active connection attempts. Essential for production security forensics and automated CloudWatch anomaly alarms.
</details>

<details>
<summary><strong>Q5: What happens if an organization associates a custom DHCP Option Set that points only to on-premises IP addresses?</strong></summary>

**Answer:**
If the DHCP Option Set defines only on-premises DNS servers (e.g., `domain-name-servers = 10.50.0.10, 10.50.0.11`) and omits `AmazonProvidedDNS`:
- Instances will direct all DNS queries across the VPN/Direct Connect link to the on-premises servers.
- The on-premises servers cannot resolve AWS Route 53 Private Hosted Zones or internal VPC endpoint domain names unless complex conditional forwarders are established.
- If the Direct Connect link drops, internal instances lose all DNS resolution entirely.
- Best practice: Always include `AmazonProvidedDNS` in the list of nameservers.
</details>

<details>
<summary><strong>Q6: Can VPC Flow Logs capture traffic to the AWS Instance Metadata Service (IMDS)?</strong></summary>

**Answer:**
No. VPC Flow Logs do **not** capture traffic to:
- The Instance Metadata Service (`169.254.169.254`).
- Amazon-Provided DNS (`169.254.169.253` or VPC CIDR + 2).
- Dynamic Host Configuration Protocol (DHCP) traffic.
- Traffic generated by a Windows instance for Amazon Windows license activation.
- Kubernetes pod-to-pod traffic traversing internal container bridge interfaces that do not cross the host's physical ENI.
</details>

<details>
<summary><strong>Q7: What is the benefit of publishing VPC Flow Logs to Amazon S3 in Apache Parquet format?</strong></summary>

**Answer:**
In high-throughput enterprise networks, VPC Flow Logs generate hundreds of gigabytes or terabytes of data daily.
- Publishing to S3 in **Apache Parquet format** with Hive-compatible partitioning compresses raw text logs by up to **80%**, saving significant storage costs.
- Parquet is a columnar format, allowing serverless query engines like **Amazon Athena** to read only the required columns (e.g., `srcaddr`, `bytes`, `action`) rather than scanning entire raw text lines, accelerating query speeds by 10x and cutting Athena query scan costs by up to 90%.
</details>

<details>
<summary><strong>Q8: What are TCP Flags in extended VPC Flow Logs and how are they used in forensics?</strong></summary>

**Answer:**
TCP flags record the bitmask of control flags transmitted during the flow aggregation window:
- `SYN` (bit 2 / value 2): Indicates connection initiation.
- `SYN-ACK` (bit 18 / value 18): Indicates a successful handshake response.
- `RST` (bit 4 / value 4): Indicates an immediate connection reset/rejection.
- `FIN` (bit 1 / value 1): Indicates graceful connection termination.
In security forensics, if a flow log shows `SYN` with `REJECT`, a client attempted to connect and was blocked by a firewall. If it shows `SYN` followed by `RST` from the server, the connection was permitted through firewalls but rejected by the host OS because no listening process existed on that port.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a CloudWatch VPC Flow Log via AWS CLI</strong></summary>

**Objective**: Enable flow logging on an existing subnet.

**Commands**:
```bash
LOG_GROUP="/aws/vpc/subnet-flow-logs"
aws logs create-log-group --log-group-name "$LOG_GROUP"

aws ec2 create-flow-logs \
  --resource-type Subnet \
  --resource-ids "subnet-0123456789abcdef0" \
  --traffic-type ALL \
  --log-destination-type cloud-watch-logs \
  --log-group-name "$LOG_GROUP" \
  --deliver-logs-permission-arn "arn:aws:iam::111122223333:role/VPCFlowLogsServiceRole"
```
</details>

<details>
<summary><strong>Exercise 2: Query CloudWatch Insights for Rejected Connections</strong></summary>

**Objective**: Identify blocked traffic in real time.

**Steps**:
1. Open CloudWatch Logs Insights.
2. Select log group `/aws/vpc/subnet-flow-logs`.
3. Run query:
   ```sql
   fields @timestamp, srcAddr, dstAddr, dstPort, protocol
   | filter action = 'REJECT'
   | sort @timestamp desc
   | limit 25
   ```
</details>

<details>
<summary><strong>Exercise 3: Associate a Secondary IPv4 CIDR Block</strong></summary>

**Objective**: Expand an existing VPC with CGNAT space.

**Command**:
```bash
aws ec2 associate-vpc-cidr-block \
  --vpc-id "vpc-0123456789abcdef0" \
  --cidr-block "100.64.0.0/16"

aws ec2 describe-vpcs --vpc-ids "vpc-0123456789abcdef0" \
  --query 'Vpcs[0].CidrBlockAssociationSet[*].[CidrBlock, CidrBlockState.State]'
```
</details>

<details>
<summary><strong>Exercise 4: Create a Custom DHCP Option Set</strong></summary>

**Objective**: Configure custom DNS domain search lists.

**Command**:
```bash
aws ec2 create-dhcp-options \
  --dhcp-configuration \
    "Key=domain-name-servers,Values=AmazonProvidedDNS" \
    "Key=domain-name,Values=internal.devops.lab"
```
</details>

<details>
<summary><strong>Exercise 5: Configure S3 Parquet Flow Log Delivery</strong></summary>

**Objective**: Configure columnar flow log streaming to S3.

**Command**:
```bash
aws ec2 create-flow-logs \
  --resource-type VPC \
  --resource-ids "vpc-0123456789abcdef0" \
  --traffic-type ALL \
  --log-destination-type s3 \
  --log-destination "arn:aws:s3:::my-flow-logs-bucket" \
  --destination-options '{"FileFormat": "parquet", "HiveCompatiblePartitions": true, "PerHourPartition": true}'
```
</details>

<details>
<summary><strong>Exercise 6: Test Local DNS Resolution Against `AmazonProvidedDNS`</strong></summary>

**Objective**: Verify that internal DNS queries resolve via `169.254.169.253`.

**Command**:
```bash
dig @169.254.169.253 checkip.amazonaws.com +short
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                            VPC Deep Dive Technical Summary                            |
+---------------------------------------------------------------------------------------+
|  Feature               | Best Practice                     | Key Rationale            |
|------------------------+-----------------------------------+--------------------------|
|  Flow Log Aggregation  | 1-minute (60 seconds)             | Near real-time forensics |
|  Flow Log Storage      | S3 in Apache Parquet format       | 80% compression, Athena  |
|  High-Volume Logs      | Send REJECT to CW, ALL to S3      | Prevents CloudWatch bills|
|  DNS Resolution        | Keep AmazonProvidedDNS in DHCP    | Mandatory for AWS service|
|  IP Exhaustion         | Secondary CGNAT (100.64.0.0/10)   | Dedicated EKS pod pools  |
|  Overhead              | Zero compute or latency overhead  | Captured outside the VM  |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon VPC Flow Logs User Guide](https://docs.aws.amazon.com/vpc/latest/userguide/flow-logs.html)
- [Querying VPC Flow Logs with CloudWatch Logs Insights](https://docs.aws.amazon.com/AmazonCloudWatch/latest/logs/CWL_AnalyzeLogData-discoverable-fields.html)
- [Working with DHCP Option Sets](https://docs.aws.amazon.com/vpc/latest/userguide/VPC_DHCP_Options.html)
- [Adding Secondary IPv4 CIDR Blocks to a VPC](https://docs.aws.amazon.com/vpc/latest/userguide/vpc-cidr-blocks.html)
- [Route 53 Resolver Architecture Guide](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/resolver.html)
