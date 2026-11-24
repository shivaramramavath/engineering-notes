# Elastic Load Balancing (ELB): ALB, NLB, and Gateway Load Balancers

> **Module**: `04-networking`  
> **Topic**: Layer 7 vs Layer 4 vs Layer 3 Routing, Target Groups, Health Checks & High Availability  
> **Target Audience**: Cloud Architects, SREs, DevOps Engineers, Backend Developers  
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

Distributing network traffic across multiple application servers is the foundational requirement for building fault-tolerant, horizontally scalable systems. In traditional data centers, load balancing required expensive proprietary hardware appliances (e.g., F5 BIG-IP, Citrix NetScaler) that introduced single points of failure, complex maintenance contracts, and rigid throughput limits.

**AWS Elastic Load Balancing (ELB)** automatically distributes incoming application traffic across multiple targets—such as Amazon EC2 instances, containers (ECS/EKS), IP addresses, and AWS Lambda functions—across multiple Availability Zones. ELB provides:
1. **High Availability**: Automatic traffic shifting away from unhealthy targets to healthy instances in real time.
2. **Elastic Scalability**: Seamless scaling from dozens of requests to millions of requests per second without manual intervention.
3. **Layer-Specific Specialization**: Tailored architectures spanning OSI Layer 7 (Application Load Balancer), Layer 4 (Network Load Balancer), and Layer 3 (Gateway Load Balancer for inline virtual network appliances).

---

## 2. Core Concepts

### 2.1 The ELB Product Family
1. **Application Load Balancer (ALB - OSI Layer 7)**:
   - Inspects HTTP/HTTPS headers, hostnames, URL paths, query parameters, and HTTP methods.
   - Ideal for microservices, containerized workloads, WebSocket applications, and modern web architectures.
   - Supports advanced features like redirects, fixed responses, HTTP/2, gRPC, and mutual TLS (mTLS).
2. **Network Load Balancer (NLB - OSI Layer 4)**:
   - Operates at the transport layer (TCP, UDP, TLS).
   - Engineered for extreme performance, capable of handling millions of requests per second with ultra-low sub-millisecond latencies.
   - Provides a static IP address per Availability Zone and supports Elastic IP allocation.
   - Preserves client source IP addresses natively without requiring proxy headers.
3. **Gateway Load Balancer (GWLB - OSI Layer 3)**:
   - Deploys, scales, and manages third-party virtual security appliances (firewalls, deep packet inspection, IDS/IPS).
   - Uses the GENEVE protocol (port 6081) to transparently intercept and steer entire IP packet flows through firewall appliances without altering source/destination headers.
4. **Classic Load Balancer (CLB - Legacy)**:
   - Previous-generation load balancer operating at Layer 4/7. Deprecated for modern architectures.

### 2.2 Target Groups & Routing
- **Target Types**:
  - `instance`: Targets identified by EC2 Instance ID.
  - `ip`: Targets identified by private IPv4/IPv6 addresses (required for ECS Fargate, Kubernetes pods with AWS VPC CNI, and hybrid on-premises endpoints).
  - `lambda`: Direct execution of an AWS Lambda function from an ALB listener rule.
  - `alb`: Allows an NLB to route traffic directly to an internal ALB (combining static IPs with Layer 7 routing).
- **Routing Rules**: Listener conditions evaluate Host header (`api.domain.com`), Path pattern (`/orders/*`), Query string, HTTP header, or Source CIDR.

### 2.3 Health Checks, Draining & Session Stickiness
- **Health Checks**: The load balancer periodically sends synthetic HTTP/TCP probes to registered targets. Targets failing sequential checks are taken out of service.
- **Deregistration Delay (Connection Draining)**: When a target is deregistered (e.g., during Auto Scaling scale-in or deployment), ELB stops sending new requests and gives in-flight requests a grace period (default 300 seconds) to complete before severing the socket.
- **Sticky Sessions (Session Affinity)**: Binds a user's session to a specific backend target using an ELB-generated cookie or custom application cookie.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                           APPLICATION LOAD BALANCER INTERNAL TOPOLOGY                             |
|                                                                                                   |
|  [Internet Client]                                                                                |
|         |                                                                                         |
|         v (DNS Query: app.example.com -> Resolves to multiple ALB Node IPs across AZs)            |
|  +---------------------------------------------------------------------------------------------+  |
|  | PUBLIC SUBNETS (VPC)                                                                        |  |
|  |                                                                                             |  |
|  |   Availability Zone A                        Availability Zone B                            |  |
|  |   +-----------------------+                  +-----------------------+                      |  |
|  |   | ALB Node (ENI)        |                  | ALB Node (ENI)        |                      |  |
|  |   | 10.0.1.45             |                  | 10.0.2.88             |                      |  |
|  |   | Public IP: 52.1.2.3   |                  | Public IP: 54.4.5.6   |                      |  |
|  |   +-----------+-----------+                  +-----------+-----------+                      |  |
|  +---------------|------------------------------------------|----------------------------------+  |
|                  |     \                              /     |                                     |
|                  |       \ (Cross-Zone LB Enabled)  /       |                                     |
|                  |         \                      /         |                                     |
|  +---------------v-----------v------------------v-----------v----------------------------------+  |
|  | PRIVATE APP SUBNETS (VPC)                                                                   |  |
|  |                                                                                             |  |
|  |   [Target Group: Web Microservice]            [Target Group: Order API]                     |  |
|  |   Path: /web/*                                Path: /api/orders/*                           |  |
|  |   +-----------------------+                  +-----------------------+                      |  |
|  |   | ECS Task (10.0.10.12) |                  | EC2 Instance (10.0.20)|                      |  |
|  |   +-----------------------+                  +-----------------------+                      |  |
|  |   +-----------------------+                  +-----------------------+                      |  |
|  |   | ECS Task (10.0.11.89) |                  | EC2 Instance (10.0.21)|                      |  |
|  |   +-----------------------+                  +-----------------------+                      |  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

1. **DNS-Based Scaling**: ALBs scale horizontally by provisioning internal software nodes with dedicated ENIs across your selected AZs. Route 53 dynamically updates the ALB's DNS record with multiple A/AAAA records.
2. **TLS Termination**: The load balancer terminates HTTPS at the edge node using certificates managed in AWS Certificate Manager (ACM) via Server Name Indication (SNI).
3. **Reverse Proxying**: For ALBs, the load balancer initiates a separate TCP connection to the backend target. The original client IP is appended into the `X-Forwarded-For` HTTP header.
4. **Cross-Zone Load Balancing**: Distributes incoming traffic evenly across all registered targets in all enabled Availability Zones, preventing hot spots if client connections land unevenly across ALB front-end nodes.

---

## 4. Syntax & Usage

### 4.1 Creating an Application Load Balancer via AWS CLI

```bash
# 1. Create the ALB across public subnets
aws elbv2 create-load-balancer \
    --name "prod-web-alb" \
    --subnets "subnet-0123456789abcdef0" "subnet-0987654321fedcba0" \
    --security-groups "sg-0123456789abcdef0" \
    --scheme "internet-facing" \
    --type "application" \
    --ip-address-type "dualstack"

# 2. Create Target Group for IP-based container targets
aws elbv2 create-target-group \
    --name "web-microservice-tg" \
    --protocol "HTTP" \
    --port 80 \
    --vpc-id "vpc-0123456789abcdef0" \
    --target-type "ip" \
    --health-check-protocol "HTTP" \
    --health-check-path "/healthz" \
    --health-check-interval-seconds 15 \
    --healthy-threshold-count 2 \
    --unhealthy-threshold-count 3

# 3. Create an HTTPS Listener with ACM Certificate
aws elbv2 create-listener \
    --load-balancer-arn "arn:aws:elasticloadbalancing:us-east-1:123456789012:loadbalancer/app/prod-web-alb/50dc6c495c0c9188" \
    --protocol "HTTPS" \
    --port 443 \
    --certificates CertificateArn="arn:aws:acm:us-east-1:123456789012:certificate/12345678-1234-1234-1234-123456789012" \
    --default-actions Type=forward,TargetGroupArn="arn:aws:elasticloadbalancing:us-east-1:123456789012:targetgroup/web-microservice-tg/73e2d6bc24d8a067"
```

---

## 5. Practical Examples

### 5.1 Complete Production ALB with Path-Based Routing & HTTP-to-HTTPS Redirect (Terraform)

```hcl
# Application Load Balancer
resource "aws_lb" "main" {
  name               = "prod-enterprise-alb"
  internal           = false
  load_balancer_type = "application"
  security_groups    = [aws_security_group.alb_sg.id]
  subnets            = var.public_subnet_ids

  enable_deletion_protection = true
  drop_invalid_header_fields = true

  tags = {
    Environment = "production"
  }
}

# HTTP:80 Listener -> Redirect to HTTPS:443
resource "aws_lb_listener" "http_redirect" {
  load_balancer_arn = aws_lb.main.arn
  port              = 80
  protocol          = "HTTP"

  default_action {
    type = "redirect"

    redirect {
      port        = "443"
      protocol    = "HTTPS"
      status_code = "HTTP_301"
    }
  }
}

# HTTPS:443 Listener
resource "aws_lb_listener" "https" {
  load_balancer_arn = aws_lb.main.arn
  port              = 443
  protocol          = "HTTPS"
  ssl_policy        = "ELBSecurityPolicy-TLS13-1-2-2021-06"
  certificate_arn   = var.acm_certificate_arn

  default_action {
    type = "fixed-response"

    fixed_response {
      content_type = "text/plain"
      message_body = "Not Found"
      status_code  = "404"
    }
  }
}

# Target Group: Frontend Web App
resource "aws_lb_target_group" "web" {
  name        = "tg-prod-frontend"
  port        = 80
  protocol    = "HTTP"
  vpc_id      = var.vpc_id
  target_type = "ip"

  deregistration_delay = 30

  health_check {
    enabled             = true
    path                = "/healthz"
    protocol            = "HTTP"
    matcher             = "200"
    interval            = 10
    timeout             = 5
    healthy_threshold   = 2
    unhealthy_threshold = 2
  }
}

# Listener Rule: Forward /web/* to frontend target group
resource "aws_lb_listener_rule" "web_rule" {
  listener_arn = aws_lb_listener.https.arn
  priority     = 10

  action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.web.arn
  }

  condition {
    path_pattern {
      values = ["/web/*", "/"]
    }
  }
}
```

### 5.2 Network Load Balancer (NLB) with Static Elastic IPs for Ultra-Low Latency

```hcl
resource "aws_eip" "nlb_eip_a" {
  domain = "vpc"
}

resource "aws_eip" "nlb_eip_b" {
  domain = "vpc"
}

resource "aws_lb" "network" {
  name               = "prod-gaming-nlb"
  internal           = false
  load_balancer_type = "network"

  subnet_mapping {
    subnet_id     = var.public_subnet_a_id
    allocation_id = aws_eip.nlb_eip_a.id
  }

  subnet_mapping {
    subnet_id     = var.public_subnet_b_id
    allocation_id = aws_eip.nlb_eip_b.id
  }

  enable_cross_zone_load_balancing = true
}

resource "aws_lb_target_group" "tcp_targets" {
  name        = "tg-gaming-tcp"
  port        = 9000
  protocol    = "TCP"
  vpc_id      = var.vpc_id
  target_type = "instance"

  health_check {
    protocol = "TCP"
    port     = "9000"
    interval = 10
  }
}

resource "aws_lb_listener" "tcp_listener" {
  load_balancer_arn = aws_lb.network.arn
  port              = 9000
  protocol          = "TCP"

  default_action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.tcp_targets.arn
  }
}
```

### 5.3 Mutual TLS (mTLS) Authentication on ALB

```hcl
# Trust Store containing Certificate Authority (CA) bundle in S3
resource "aws_lb_trust_store" "client_ca" {
  name                         = "enterprise-client-ca-store"
  ca_certificates_bundle_s3_bucket = var.s3_cert_bucket
  ca_certificates_bundle_s3_key    = "ca-bundle.pem"
}

# ALB Listener enforcing mTLS verification
resource "aws_lb_listener" "mtls_listener" {
  load_balancer_arn = aws_lb.main.arn
  port              = 8443
  protocol          = "HTTPS"
  ssl_policy        = "ELBSecurityPolicy-TLS13-1-2-2021-06"
  certificate_arn   = var.server_acm_arn

  mutual_authentication {
    mode            = "verify"
    trust_store_arn = aws_lb_trust_store.client_ca.arn
  }

  default_action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.web.arn
  }
}
```

### 5.4 Sticky Sessions Configuration on Target Group

```hcl
resource "aws_lb_target_group" "sticky_web" {
  name        = "tg-sticky-session-web"
  port        = 80
  protocol    = "HTTP"
  vpc_id      = var.vpc_id
  target_type = "instance"

  stickiness {
    type            = "lb_cookie"
    cookie_duration = 86400 # 24 hours
    enabled         = true
  }

  health_check {
    path = "/status"
  }
}
```

### 5.5 NLB Fronting an Internal ALB (Static IP to L7 Routing Architecture)

```hcl
# Internal ALB Target Group inside NLB
resource "aws_lb_target_group" "nlb_to_alb" {
  name        = "tg-nlb-fronting-alb"
  port        = 443
  protocol    = "TCP"
  vpc_id      = var.vpc_id
  target_type = "alb"

  health_check {
    protocol = "HTTPS"
    path     = "/healthz"
    port     = "443"
  }
}

# Register the Internal ALB as the target
resource "aws_lb_target_group_attachment" "attach_alb" {
  target_group_arn = aws_lb_target_group.nlb_to_alb.arn
  target_id        = aws_lb.internal_alb.arn
  port             = 443
}
```

---

## 6. Best Practices

1. **Enable Deletion Protection**: Set `enable_deletion_protection = true` on production load balancers to prevent accidental destruction during infrastructure changes.
2. **Drop Invalid HTTP Headers**: Enable `drop_invalid_header_fields = true` on ALBs to mitigate HTTP request smuggling attacks by rejecting headers containing non-conforming characters.
3. **Tune Deregistration Delay for Containers**: Lower container deregistration delay from the 300s default to 15s - 30s in ECS/EKS to dramatically speed up deployment rolling updates.
4. **Use TLS 1.3 Security Policies**: Select `ELBSecurityPolicy-TLS13-1-2-2021-06` to enforce TLS 1.2+ and TLS 1.3 while disabling vulnerable legacy ciphers (CBC, RC4, 3DES).
5. **Always Enable Cross-Zone Load Balancing**: Balances load evenly across AZs. Note: Cross-Zone is enabled by default on ALB, but must be explicitly enabled on NLB (cross-zone data transfer is free on NLBs within the same region).

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Backend Security Group Blocks ALB | EC2/ECS SG restricts port 80 to `0.0.0.0/0` or forgets ALB SG. | Targets show `unhealthy`; ALB returns HTTP 502 Bad Gateway. | Set target SG ingress to allow traffic strictly from ALB's Security Group ID. |
| Hardcoding ALB IP Addresses | ALBs use dynamic IP addresses that scale up and down. | Connection drops when ALB scales or updates node IPs. | Point traffic to ALB DNS name or use an NLB if static IPs are mandatory. |
| Health Check Path Returns Non-200 | App responds with 301/302 redirect on health check endpoint. | Target marked unhealthy because default matcher expects 200. | Update health check path or update matcher to `200,301,302`. |
| Deploying ALB in a Single AZ | ALB created in only one subnet. | Fails creation (ALB requires at least two Availability Zones). | Provide at least two public subnets in distinct AZs. |
| Neglecting `X-Forwarded-Proto` | App checks protocol and loops redirects (`http -> https`). | Infinite redirect loop in browser (`ERR_TOO_MANY_REDIRECTS`). | Inspect `X-Forwarded-Proto` in application to detect HTTPS offloaded by ALB. |
| Forgetting Cross-Zone on NLB | Default NLB configuration has cross-zone disabled. | Server utilization severely imbalanced across AZs. | Set `enable_cross_zone_load_balancing = true`. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Pre-Warming
- **ALB Pre-Warming**: ALBs scale automatically via DNS changes, but sudden 10x traffic surges within seconds can cause 504 errors before new nodes launch. Contact AWS Support to pre-warm the ALB prior to major flash sales or television broadcasts.
- **NLB Zero Pre-Warming**: NLBs handle instant surges of millions of RPS without pre-warming thanks to the Hyperplane network fabric.

### 8.2 Security
- **Target Security Group Isolation**: Backend application servers should never have public IPs or allow public ingress. Lock target group security groups so they accept connections exclusively from the ALB's security group.
- **WAF Direct Association**: AWS WAF Web ACLs attach directly to ALBs, blocking malicious traffic before it reaches backend instances.

### 8.3 Cost & Capacity Units (LCU / NCU)
- **ALB Pricing**: \$0.0225 per ALB hour + \$0.008 per Load Balancer Capacity Unit (LCU-hour).
  - An LCU measures: 25 new connections/sec, 3,000 active connections/min, 1 GB processed bytes, or 1,000 rule evaluations. You are billed based on the highest dimension.
- **NLB Pricing**: \$0.0225 per NLB hour + \$0.006 per Network Capacity Unit (NCU-hour).

---

## 9. Comparison Table

| Feature / Dimension | Application Load Balancer (ALB) | Network Load Balancer (NLB) | Gateway Load Balancer (GWLB) |
|---|---|---|---|
| **OSI Layer** | Layer 7 (Application) | Layer 4 (Transport) | Layer 3 (Network Gateway) |
| **Protocols** | HTTP, HTTPS, gRPC, WebSocket | TCP, UDP, TLS | IP (GENEVE tunneling) |
| **Static / Elastic IP**| No (Dynamic DNS name) | Yes (1 Static IP per AZ) | No |
| **Latency** | Low (~2ms - 10ms) | Ultra-low (sub-millisecond) | Ultra-low transparent |
| **Routing Intelligence**| Path, Host, Query, Headers, Methods| Port and Protocol only | Transparent bump-in-the-wire |
| **Source IP Preservation**| Via `X-Forwarded-For` header | Native (packet IP unaltered) | Preserved via GENEVE header |
| **WAF Integration** | Direct (Native AWS WAF) | Via AWS WAF on ALB (or VPC) | External virtual appliances |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What causes an Application Load Balancer to return an HTTP 502 Bad Gateway error vs an HTTP 504 Gateway Timeout?</strong></summary>

- **HTTP 502 Bad Gateway**: The ALB was able to connect to the backend target, but the target sent an invalid, malformed response (e.g., TCP reset `RST`, incomplete HTTP response headers, or SSL handshake failure between ALB and target).
- **HTTP 504 Gateway Timeout**: The ALB forwarded the request to the target, but the target failed to respond before the configured idle timeout period expired (default 60 seconds), typically caused by slow database queries or target thread pool exhaustion.
</details>

<details>
<summary><strong>Q2: How does an NLB preserve the client source IP, and why can this cause security group issues on the backend?</strong></summary>

NLBs operate at Layer 4 using the AWS Hyperplane packet-processing fabric. When routing packets to targets registered by instance ID, the NLB does not rewrite the IP packet header; the source IP remains the actual client's public IP address. 
**Security Group Issue**: Because the packet arrives with the client's public IP, the backend target's Security Group cannot simply allow the NLB's security group. It must allow traffic from client IPs (`0.0.0.0/0`) or from the client VPC CIDR. (Note: AWS introduced Security Groups for NLBs in 2023, allowing centralized ingress filtering directly at the NLB level).
</details>

<details>
<summary><strong>Q3: How do you design an architecture that requires both static whitelisted IP addresses and Layer 7 path-based routing?</strong></summary>

Deploy an Internet-Facing **Network Load Balancer (NLB)** with Elastic IPs assigned to its subnets. Configure an NLB TCP listener on port 443 with a target group of type `alb`, pointing directly to an internal **Application Load Balancer (ALB)**. The NLB provides the static, immutable public IP addresses required by external enterprise firewalls, while the internal ALB handles TLS offloading, path-based routing (`/api` vs `/auth`), and microservice dispatching.
</details>

<details>
<summary><strong>Q4: What is the purpose of Gateway Load Balancer (GWLB) and how does it utilize GENEVE encapsulation?</strong></summary>

GWLB simplifies deploying a fleet of third-party virtual network appliances (like Palo Alto Networks, Fortinet, or Check Point firewalls) in an auto-scaled, highly available cluster. GWLB intercepts raw IP packets, encapsulates them in a GENEVE (Generic Network Virtualization Encapsulation) packet on UDP port 6081 with metadata (such as original ENI and flow ID), and forwards them to the firewall appliances. The appliance inspects or scrubs the traffic and returns the packet to GWLB, which routes it cleanly to its destination.
</details>

<details>
<summary><strong>Q5: What happens when an EC2 instance in a target group fails its health checks?</strong></summary>

1. Once the instance fails consecutive checks matching the `UnhealthyThresholdCount`, the load balancer marks the target state as `unhealthy`.
2. The load balancer immediately stops dispatching new connections to that instance.
3. Existing in-flight connections may complete depending on the protocol.
4. The load balancer continues probing the unhealthy target; if it passes consecutive checks matching `HealthyThresholdCount`, it is automatically returned to healthy service.
</details>

<details>
<summary><strong>Q6: How does ALB handle Mutual TLS (mTLS) and when would you use it?</strong></summary>

ALB supports mTLS by validating X.509 client certificates directly at the load balancer listener. The administrator configures an ALB Trust Store containing the Root and Intermediate CA certificates in S3. When a client connects, the ALB performs two-way TLS authentication. If valid, the ALB can pass the client certificate metadata (Subject DN, Issuer, Serial Number) in HTTP headers (`X-Amzn-Mtls-Clientcert-*`) to backend applications. This is heavily used in Open Banking, B2B APIs, and Zero-Trust architectures.
</details>

<details>
<summary><strong>Q7: Why is Cross-Zone Load Balancing critical, and what are its cost implications?</strong></summary>

Without cross-zone balancing, if AZ-A receives 50% of traffic and has 2 instances, each instance handles 25% load. If AZ-B has 8 instances, each instance handles only 6.25% load, causing resource starvation and crashes in AZ-A. Cross-zone balancing distributes requests across all 10 instances equally (10% load each). 
**Cost**: ALB cross-zone balancing is always enabled and incurs zero data transfer charges. NLB cross-zone balancing is disabled by default; enabling it inside the same AWS region is free.
</details>

<details>
<summary><strong>Q8: What is connection draining (deregistration delay) and why must it be configured during rolling deployments?</strong></summary>

When an Auto Scaling group or ECS deployment scales down or replaces an instance, the target is deregistered from the Target Group. Deregistration delay tells ELB to stop sending new requests to that target while allowing in-flight requests to finish over a specified duration (default 300s). If deregistration delay is set to 0, active database transactions or file uploads are abruptly terminated with TCP resets, causing `502` or connection errors for active users.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Path-Based Routing on ALB</strong></summary>

**Goal**: Route `/api/*` to an API target group and all other traffic to a Static target group.
1. Deploy an ALB with two distinct target groups: `tg-api` and `tg-static`.
2. Register two test EC2 instances or Docker containers into each target group.
3. In the ALB HTTPS listener, create a rule: IF `Path is /api/*` THEN `Forward to tg-api` with priority 10.
4. Set default action: `Forward to tg-static`.
5. Curl `/api/users` and `/home` to verify correct target group dispatching.
</details>

<details>
<summary><strong>Exercise 2: Setup HTTP to HTTPS 301 Redirect</strong></summary>

**Goal**: Force all unencrypted HTTP port 80 traffic to HTTPS port 443 with zero target overhead.
1. On the ALB, create a listener on port 80.
2. Select action: `Redirect`.
3. Configure Protocol: `HTTPS`, Port: `443`, Status code: `301 - Permanently moved`.
4. Test with `curl -IL http://<alb-dns-name>` and observe the immediate 301 redirect.
</details>

<details>
<summary><strong>Exercise 3: Deploy an NLB with Static Elastic IPs</strong></summary>

**Goal**: Provide static public IP endpoints for an application.
1. Allocate two Elastic IPs in the EC2 Console.
2. Create an Internet-facing NLB across two public subnets, mapping the allocated EIPs.
3. Configure a TCP listener on port 80 forwarding to a target group.
4. Verify DNS resolution returns exactly those two static Elastic IP addresses.
</details>

<details>
<summary><strong>Exercise 4: Test Connection Draining Behavior</strong></summary>

**Goal**: Observe target state transitions during deregistration.
1. Set `deregistration_delay.timeout_seconds` to 60 on a target group.
2. Run a long-running HTTP request: `curl "http://<alb>/sleep?seconds=45"`.
3. Simultaneously deregister the target instance via AWS CLI.
4. Observe the target status move to `draining`, verify the curl request completes successfully, and observe the target become `unused` only after 60 seconds.
</details>

<details>
<summary><strong>Exercise 5: Simulate Unhealthy Host Failover</strong></summary>

**Goal**: Test automatic target removal on health check failure.
1. Configure health check path to `/health` with unhealthy threshold of 2.
2. Deploy two instances behind the ALB.
3. On instance 1, simulate failure: `chmod 000 /var/www/html/health`.
4. Monitor ALB Target Health in AWS Console; observe target 1 become `unhealthy` while traffic shifts 100% to instance 2 with zero request errors.
</details>

<details>
<summary><strong>Exercise 6: Mutual TLS (mTLS) Authentication Verification</strong></summary>

**Goal**: Reject client requests that lack an approved client certificate.
1. Generate a self-signed Root CA and issue a client certificate using OpenSSL.
2. Upload the Root CA certificate to an S3 bucket and create an ALB Trust Store.
3. Configure the ALB HTTPS listener with `mutual_authentication.mode = "verify"`.
4. Attempt connection with `curl https://<alb>` (fails with TLS alert).
5. Attempt connection with `curl --cert client.crt --key client.key https://<alb>` (succeeds).
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| ELASTIC LOAD BALANCING ARCHITECTURAL SELECTION MATRIX                                             |
+----------------------+---------------------------+------------------------------------------------+
| Feature              | Application Load Balancer | Network Load Balancer                          |
+----------------------+---------------------------+------------------------------------------------+
| Primary Protocol     | HTTP / HTTPS / gRPC       | TCP / UDP / TLS                                |
| Routing Decision     | Layer 7 (URLs, Headers)   | Layer 4 (IP, Port)                             |
| Throughput / Scale   | Millions of RPS (scales)  | Millions of RPS (instant, no pre-warming)      |
| IP Addressing        | Dynamic DNS hostnames     | Static IPv4 per AZ / Elastic IPs               |
| WAF Protection       | Native direct attachment  | Indirect (via ALB target or VPC firewall)      |
| Client IP Header     | X-Forwarded-For           | Native IP preserved / Proxy Protocol v2        |
+----------------------+---------------------------+------------------------------------------------+
```

- **ALB for Web & APIs**: Advanced Layer 7 routing, microservices, containerized workloads.
- **NLB for Extreme Performance & Static IPs**: Sub-millisecond latency, gaming, financial feeds, non-HTTP sockets.
- **GWLB for Security Appliances**: Transparent Layer 3 GENEVE steering for inline firewall clusters.

---

## 13. Further Reading

- [AWS Elastic Load Balancing User Guide](https://docs.aws.amazon.com/elasticloadbalancing/latest/userguide/)
- [Application Load Balancer Routing Rules & Conditions](https://docs.aws.amazon.com/elasticloadbalancing/latest/application/load-balancer-listeners.html)
- [Network Load Balancer Target Groups & Health Probes](https://docs.aws.amazon.com/elasticloadbalancing/latest/network/load-balancer-target-groups.html)
- [Mutual Authentication with ALB Trust Stores](https://aws.amazon.com/blogs/aws/mutual-authentication-for-application-load-balancer-to-verify-client-identities/)
- [04-networking/06-cloudfront.md](file:///04-devops/aws/04-networking/06-cloudfront.md)
