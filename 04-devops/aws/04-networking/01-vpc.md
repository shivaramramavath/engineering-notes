# VPC: Your Private Network in AWS

A VPC (Virtual Private Cloud) is a logically isolated network inside AWS where you place resources like EC2 instances, Fargate tasks, RDS databases and Lambda functions (when attached). You choose the IP range, carve it into subnets, and control exactly what can talk to what and to the internet.

Almost everything in AWS that has an IP address lives in a VPC, so networking problems ("connection timed out") are, more often than not, a VPC problem. Understanding five things (CIDR, subnets, route tables, gateways, security groups) solves most of them.

Prerequisites: [Accounts, Regions and Billing](../01-foundations/02-accounts-regions-and-billing.md) (regions and AZs).

---

## The model

```
VPC  10.0.0.0/16                                   (lives in ONE region)
 │
 ├── AZ a ── public subnet  10.0.0.0/24   ─┐  route: 0.0.0.0/0 → Internet Gateway
 │        └─ private subnet 10.0.10.0/24   │  route: 0.0.0.0/0 → NAT Gateway
 ├── AZ b ── public subnet  10.0.1.0/24    │
 │        └─ private subnet 10.0.11.0/24  ─┘
 │
 ├── Internet Gateway (IGW)   ←→ the internet
 └── NAT Gateway              private subnets → internet (outbound only)
```

| Piece | What it does |
|---|---|
| **CIDR block** | The VPC's private IP range, e.g. `10.0.0.0/16` (65,536 addresses). Subnets are slices of it. |
| **Subnet** | A range inside the VPC, living in **one AZ**. Instances get IPs from their subnet. |
| **Route table** | Rules mapping destination ranges to targets. Each subnet uses exactly one. |
| **Internet Gateway** | Lets resources with public IPs reach (and be reached from) the internet. |
| **NAT Gateway** | Lets private resources make **outbound** connections to the internet; nothing can initiate inbound through it. |
| **Security group** | Stateful firewall attached to a network interface. |
| **Network ACL** | Stateless firewall at the subnet boundary. |

### What makes a subnet "public" or "private"?

Not a setting, but **routing**:

- **Public subnet**: its route table has `0.0.0.0/0 → Internet Gateway`. A resource there *also* needs a public IP to be reachable from the internet.
- **Private subnet**: no route to an IGW. Outbound internet (if needed) goes via a NAT Gateway (or VPC endpoints for AWS services). Nothing from the internet can reach it directly.

Every route table also has an implicit **local route** for the VPC's own CIDR, so everything inside a VPC can reach everything else by default at the routing level. Security groups are what actually restrict it.

---

## Planning the CIDR

- Pick a **private range** (RFC 1918): `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`.
- A VPC's CIDR can be between `/16` (large) and `/28` (tiny).
- **Avoid overlapping ranges** with other VPCs, your office, or on-prem networks you may later connect to. Overlap blocks peering and VPNs, and it's painful to fix later.
- Size generously: `/16` for the VPC, `/24` (or larger) subnets. It's free to have unused addresses.
- AWS **reserves 5 addresses per subnet** (the first four and the last), so a `/28` leaves just 11 usable. Remember this when sizing subnets that hold many ENIs (Fargate tasks, Lambda-in-VPC, EKS pods each use addresses).
- Spread subnets across **at least two AZs** so a single AZ outage doesn't take you down.

The **default VPC** (one per region in older accounts) has public subnets and is fine for experiments. Real workloads should live in a VPC you design yourself.

---

## Security groups vs network ACLs

| | Security group | Network ACL |
|---|---|---|
| Attached to | Network interface (instance, task, DB, load balancer…) | Subnet |
| State | **Stateful**: reply traffic is automatically allowed | **Stateless**: you must allow return traffic (including ephemeral ports) explicitly |
| Rules | **Allow only** | Allow **and deny**, evaluated in numbered order |
| Default | All inbound blocked; all outbound allowed | Default NACL allows everything |

Use **security groups** as your main tool. Leave NACLs at default unless you need an explicit subnet-level deny (for example blocking a bad IP range).

The most useful idiom: **reference another security group as the source** instead of IP ranges:

```
alb-sg    : inbound 443 from 0.0.0.0/0
app-sg    : inbound 8080 from alb-sg          ← only the load balancer can reach the app
db-sg     : inbound 5432 from app-sg          ← only the app can reach the database
```

This keeps working as instances come and go, and expresses intent ("the app tier may talk to the DB tier") instead of brittle IPs.

---

## Internet access from private subnets

Resources in private subnets often need to reach out (pull container images, call APIs, install packages). Options:

1. **NAT Gateway**: managed, scales automatically. Sits in a public subnet (zonal mode) and private subnets route `0.0.0.0/0` to it. Billed **per hour plus per GB processed**, so it's a classic source of surprise cost.
   - AWS added a **regional NAT Gateway** mode in November 2025: one NAT Gateway that automatically spans the AZs where your workloads run, with no public-subnet-per-AZ hosting and no per-AZ route table juggling. Prefer it for new setups unless you need private NAT connectivity (which it doesn't support). Check the VPC docs for its constraints.
   - With the classic **zonal** mode, deploy one NAT Gateway **per AZ** and route each AZ's private subnet to its local one. A single NAT in one AZ is a single point of failure and adds cross-AZ data charges.
2. **VPC endpoints**: reach AWS services without the internet (below). This often removes most of your NAT traffic.
3. **IPv6 + egress-only internet gateway**: outbound-only IPv6 with no NAT cost. IPv6 is increasingly worth adopting, partly because **public IPv4 addresses are billed hourly** (since 2024), including those attached to running resources and idle Elastic IPs.

---

## VPC endpoints: private paths to AWS services

| Type | Services | How it works | Cost |
|---|---|---|---|
| **Gateway endpoint** | **S3** and **DynamoDB** | A route-table entry pointing to the service | **Free** |
| **Interface endpoint** (PrivateLink) | Most other services (SQS, Secrets Manager, ECR, CloudWatch Logs, STS, …) and third-party/your own services | An ENI with a private IP in your subnet | Hourly per endpoint per AZ + per GB |

```bash
# Free: let private subnets reach S3 without NAT
aws ec2 create-vpc-endpoint --vpc-id vpc-0abc --service-name com.amazonaws.ap-south-1.s3 \
  --route-table-ids rtb-0private1 rtb-0private2
```

Adding the S3 gateway endpoint is one of the cheapest wins in AWS networking: S3 traffic from private subnets stops flowing (and being billed) through the NAT Gateway. Fargate tasks pulling from ECR typically need **ECR (api + dkr) interface endpoints plus the S3 gateway endpoint** (image layers come from S3) if you don't use NAT. Endpoint policies can additionally restrict which buckets or actions are reachable through the endpoint.

---

## Connecting VPCs and networks

| Option | Use for | Notes |
|---|---|---|
| **VPC peering** | Connect two VPCs directly | **Non-transitive** (A↔B and B↔C does not give A↔C); CIDRs must not overlap; you still need routes + security group rules |
| **Transit Gateway** | Hub-and-spoke across many VPCs/on-prem | Scales better than a mesh of peerings; per-attachment and per-GB cost |
| **Site-to-Site VPN / Direct Connect** | Link on-prem to AWS | VPN over the internet; Direct Connect is a dedicated link |
| **PrivateLink** | Expose one service privately to other VPCs/accounts | One-way, no CIDR overlap concerns |

Multi-account setups with several VPCs usually end up on Transit Gateway or PrivateLink. A single VPC is plenty for most apps.

---

## Typical three-tier layout

```
                 Internet
                    │
              ┌─────▼─────┐
 public       │    ALB    │  (public subnets, 2+ AZs)          sg: alb-sg
              └─────┬─────┘
 private (app)  ┌───▼────────────┐
                │ ECS / EC2 / …  │  (private subnets, 2+ AZs)  sg: app-sg ← alb-sg
                └───┬────────┬───┘
 private (data)     │        │
          ┌─────────▼──┐   ┌─▼───────────────┐
          │ RDS/Aurora │   │ S3, DynamoDB    │  via gateway endpoints
          └────────────┘   └─────────────────┘
```

Only the load balancer is exposed. Apps and databases have no public IPs. See [Load Balancing and API Gateway](./03-load-balancing-and-api-gateway.md), [ECS/Fargate](../02-compute/03-ecs-fargate.md) and [RDS and Aurora](../03-storage-and-databases/03-rds-and-aurora.md).

---

## Related settings worth knowing

- **DNS support / DNS hostnames**: enable both on the VPC. Interface endpoints and private hosted zones rely on them.
- **VPC Flow Logs**: record accepted/rejected traffic metadata per interface, delivered to CloudWatch Logs or S3. Essential for answering "who is being blocked?" and for security forensics (they have a cost, so scope them).
- **Lambda in a VPC** loses default internet access, so it needs NAT or endpoints (see [Lambda](../02-compute/02-lambda.md)).
- **Elastic IPs** are static public IPv4 addresses you own. They are billed whether attached or idle.

---

## Creating a VPC

For anything beyond experiments, define it in code (see [Infrastructure as Code](../06-operations/01-infrastructure-as-code.md)). In CDK, one construct builds the whole layout:

```ts
import * as ec2 from "aws-cdk-lib/aws-ec2";

const vpc = new ec2.Vpc(this, "AppVpc", {
  ipAddresses: ec2.IpAddresses.cidr("10.0.0.0/16"),
  maxAzs: 2,
  natGateways: 1,   // cost vs resilience: 1 is cheaper, one per AZ is more resilient
  subnetConfiguration: [
    { name: "public",  subnetType: ec2.SubnetType.PUBLIC,              cidrMask: 24 },
    { name: "private", subnetType: ec2.SubnetType.PRIVATE_WITH_EGRESS, cidrMask: 24 },
  ],
  gatewayEndpoints: { S3: { service: ec2.GatewayVpcEndpointAwsService.S3 } },
});
```

Check that the NAT option you want (zonal vs regional) is supported by your CDK version before relying on it.

---

## Common mistakes

- **Overlapping CIDRs** with other networks, discovered only when you try to peer.
- **Subnets too small** (a `/28` full of ENIs).
- A **single AZ** (or a single NAT Gateway) for something production-critical.
- Putting databases in public subnets, or leaving security groups open to `0.0.0.0/0` on SSH/DB ports.
- Forgetting the **route table**: creating an IGW and a public subnet but not adding the `0.0.0.0/0 → igw` route (or associating the table with the subnet).
- Expecting a resource in a public subnet to be reachable without a **public IP**.
- Missing the **S3 gateway endpoint**, so large S3 transfers pay NAT per-GB fees.
- Assuming VPC peering is transitive.
- Forgetting that **NACLs are stateless** and blocking return traffic.
- Leaving NAT Gateways, interface endpoints and Elastic IPs running in forgotten dev VPCs.

---

## Debugging connectivity

Work from the outside in. For "A can't reach B":

1. **Is the target actually listening** on that port, on `0.0.0.0` (not just `127.0.0.1`)?
2. **Security group on B**: inbound rule for A's address or security group on that port?
3. **Security group on A**: outbound allows it (default allows all)?
4. **Route table of A's subnet**: is there a route to B's range (local, peering, TGW, NAT/IGW)?
5. **NACLs** on both subnets: do they allow both directions, including ephemeral ports?
6. **Public vs private**: does A need internet via NAT/IGW? Does the resource have a public IP?
7. **DNS**: does the name resolve to the private or public IP you expect (private hosted zones, endpoint DNS)?

| Symptom | Usually |
|---|---|
| **Connection timed out** | Network path blocked: security group, route, NACL, no NAT/IGW |
| **Connection refused** | Network path OK, but nothing listening on that port (or wrong port/interface) |
| Works by IP, fails by name | DNS settings/resolution |
| Private subnet can't reach the internet | No NAT route, NAT in wrong subnet, or the NAT lacks an IGW route |
| ECS/Lambda can't pull images or call AWS APIs | No NAT or missing interface/gateway endpoints |

Tools: **VPC Reachability Analyzer** (checks the configured path between two endpoints and tells you which component blocks it), **Flow Logs** (`REJECT` entries), and `nc -zv host port` / `curl -v` from the source machine.

---

## Quick Summary

- A VPC is your isolated network: **CIDR → subnets (one AZ each) → route tables → gateways**, guarded by **security groups**.
- **Public subnet = route to an IGW; private subnet = no such route.** It's all routing.
- Use **security groups** (stateful, allow-only, reference each other) as the primary control; NACLs are stateless extras.
- Private resources reach out via **NAT Gateway** (regional mode now available) or **VPC endpoints**. Add the **free S3/DynamoDB gateway endpoints**.
- NAT Gateways, interface endpoints and public IPv4 addresses cost money even when idle.
- Plan non-overlapping CIDRs, use 2+ AZs, keep databases private, and define it all in IaC.
- *Timed out* = path blocked; *refused* = nothing listening.

**Next:** [Route 53 and CloudFront](./02-route53-and-cloudfront.md)