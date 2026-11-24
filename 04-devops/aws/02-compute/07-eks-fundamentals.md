# Amazon Elastic Kubernetes Service (EKS) Fundamentals

> **Module**: `02-compute`  
> **Topic**: Managed Kubernetes Control Plane, Worker Fleets, and Pod Identity  
> **Target Audience**: Kubernetes Administrators, Cloud Architects, Platform Engineers  
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

Operating production Kubernetes clusters from scratch requires immense engineering effort: provisioning multi-master control planes, managing distributed `etcd` consensus quorums, executing complex zero-downtime control plane version upgrades, and securing control plane network communication across Availability Zones.

**Amazon Elastic Kubernetes Service (Amazon EKS)** is a certified Kubernetes-conformant managed service that automates the availability, scalability, and security of the Kubernetes control plane.

EKS manages the operational heavy lifting of running Kubernetes on AWS:
- **Managed Control Plane**: Provisions and runs highly available Kubernetes API servers and `etcd` persistence clusters distributed across a minimum of three Availability Zones.
- **Deep AWS Integration**: Integrates directly with AWS VPC networking (AWS VPC CNI), identity management (IAM Pod Identity / IRSA), load balancing (AWS Load Balancer Controller), and storage (EBS/EFS CSI drivers).
- **Hybrid Compute Backends**: Supports Managed Node Groups (EC2), serverless containers via AWS Fargate, and next-generation Kubernetes autoscaling with **Karpenter**.

---

## 2. Core Concepts

### 2.1 EKS Architecture: Control Plane vs. Data Plane
- **Managed Control Plane**: AWS-managed single-tenant VPC. Runs the Kubernetes API Server (`kube-apiserver`), Controller Manager (`kube-controller-manager`), Scheduler (`kube-scheduler`), and `etcd`. Scales automatically based on cluster load and is backed by a 99.95% SLA.
- **Data Plane (Worker Nodes)**: Customer-managed VPC. Runs customer pods, container runtimes (containerd), `kubelet`, and `kube-proxy`.

### 2.2 Worker Node Compute Strategies
1. **Managed Node Groups (MNG)**: EKS provisions and automates EC2 Auto Scaling Groups using AWS-curated Amazon Linux or Bottlerocket AMIs. Handles node draining and rolling updates during Kubernetes version upgrades.
2. **AWS Fargate for EKS**: Serverless container execution. Each pod runs in its own single-tenant microVM without managing EC2 worker nodes. Ideal for batch jobs or stateless microservices.
3. **Karpenter**: Open-source, high-performance node autoscaler built specifically for Kubernetes. Bypasses EC2 Auto Scaling Groups entirely; directly provisions optimal EC2 instances based on pending pod resource requests within seconds.

### 2.3 Networking: AWS VPC CNI Plugin
Unlike standard overlay networks (Flannel, Calico) that encapsulate pod traffic, the **AWS VPC CNI (`amazon-k8s-cni`)** allocates native private IP addresses directly from your VPC subnets to individual Kubernetes pods:
- Pods have direct routable access to VPC resources (RDS, EC2, on-premises networks via Direct Connect) with zero overlay network NAT overhead.
- Supports **Security Groups for Pods**, allowing engineers to apply standard AWS Security Groups directly to Kubernetes pods.

### 2.4 Pod Authentication: IRSA vs. EKS Pod Identity
Applications inside Kubernetes pods require AWS IAM permissions to access services like S3 or DynamoDB:
- **IAM Roles for Service Accounts (IRSA)**: Uses OIDC federation between EKS and AWS IAM. Pods project an OIDC web identity token, which AWS STS verifies to assume the IAM role.
- **EKS Pod Identity (Modern Standard)**: Simplifies IRSA. Directly maps Kubernetes Service Accounts to IAM Roles via an EKS cluster add-on agent without managing OIDC identity provider thumbprints or complex trust policy string conditions.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Amazon EKS Cluster Architecture                            |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ AWS Managed Control Plane VPC (99.95% SLA) ]                                   |
|  +-----------------------------------------------------------------------------+  |
|  | Multi-AZ Control Plane (us-east-1a, us-east-1b, us-east-1c)                 |  |
|  |                                                                             |  |
|  |  +-------------------+   +--------------------+   +----------------------+  |  |
|  |  |   kube-apiserver  |   |  etcd Cluster (3x) |   |  kube-controller-mgr |  |  |
|  |  +-------------------+   +--------------------+   +----------------------+  |  |
|  +-----------------------------------------------------------------------------+  |
|                                         | Cross-Account Elastic Network Interfaces|
+-----------------------------------------|-----------------------------------------+
                                          | (EKS Managed ENIs)
+-----------------------------------------|-----------------------------------------+
|  [ Customer VPC (Data Plane) ]          v                                         |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  | Worker Node: EC2 / Bottlerocket (Subnet 1a)                                 |  |
|  |                                                                             |  |
|  |  +-------------------+   +--------------------+   +----------------------+  |  |
|  |  | containerd / CNI  |   | kubelet            |   | kube-proxy           |  |  |
|  |  +-------------------+   +--------------------+   +----------------------+  |  |
|  |            |                                                                |  |
|  |            +---> Pod A (IP: 10.0.1.25) [Native VPC IP from Primary Subnet]  |  |
|  |            +---> Pod B (IP: 10.0.1.26) [Native VPC IP from Primary Subnet]  |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  | Karpenter Autoscaler Controller / NodePool                                  |  |
|  | - Continuously watches pending pods                                         |  |
|  | - Calls ec2:RunInstances directly to provision right-sized Graviton hosts    |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

### EKS Pod Identity Authorization Flow
```
[Pod Container] 
       | (Wants to read from Amazon S3)
       v
[EKS Pod Identity Agent DaemonSet] (Listens on 169.254.170.23 / link-local)
       |
       +---> Intercepts credential request
       +---> Contacts EKS Pod Identity Service with Kubernetes ServiceAccount token
       |
       v
[AWS STS (Security Token Service)]
       |
       +---> Validates pod identity association
       +---> Generates short-lived temporary IAM credentials
       |
       v
[Pod Container] ---> Calls s3:GetObject with STS temporary token!
```

---

## 4. Syntax & Usage

### 4.1 Cluster Provisioning with `eksctl`
```yaml
# cluster-config.yaml
apiVersion: eksctl.io/v1alpha5
kind: ClusterConfig

metadata:
  name: prod-eks-cluster
  region: us-east-1
  version: "1.30"

vpc:
  id: "vpc-0123456789abcdef0"
  subnets:
    private:
      us-east-1a: { id: "subnet-01a" }
      us-east-1b: { id: "subnet-01b" }
      us-east-1c: { id: "subnet-01c" }

managedNodeGroups:
  - name: general-purpose-nodes
    instanceType: c7g.xlarge
    desiredCapacity: 3
    minSize: 2
    maxSize: 10
    privateNetworking: true
    volumeSize: 50
    volumeType: gp3
    iam:
      withAddonPolicies:
        autoScaler: true
        ebs: true
```

Deploying via CLI:
```bash
eksctl create cluster -f cluster-config.yaml
```

### 4.2 Updating `kubeconfig` to Connect via `kubectl`
```bash
aws eks update-kubeconfig \
  --region us-east-1 \
  --name prod-eks-cluster

# Verify cluster connectivity
kubectl get nodes -o wide
```

---

## 5. Practical Examples

### Example 1: EKS Pod Identity IAM Role & Association
Modern authentication mapping an IAM role directly to a Kubernetes ServiceAccount without complex OIDC trust conditions.

```bash
# 1. Create IAM Role with trust policy for EKS Pod Identity
aws iam create-role \
  --role-name S3AccessPodRole \
  --assume-role-policy-document '{
    "Version": "2012-10-17",
    "Statement": [{
      "Effect": "Allow",
      "Principal": {"Service": "pods.eks.amazonaws.com"},
      "Action": ["sts:AssumeRole", "sts:TagSession"]
    }]
  }'

# 2. Attach S3 read policy
aws iam attach-role-policy \
  --role-name S3AccessPodRole \
  --policy-arn arn:aws:iam::aws:policy/AmazonS3ReadOnlyAccess

# 3. Create EKS Pod Identity Association
aws eks create-pod-identity-association \
  --cluster-name prod-eks-cluster \
  --namespace production \
  --service-account s3-reader-sa \
  --role-arn arn:aws:iam::111122223333:role/S3AccessPodRole
```

**Kubernetes Deployment YAML:**
```yaml
apiVersion: v1
kind: ServiceAccount
metadata:
  name: s3-reader-sa
  namespace: production
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: data-reader
  namespace: production
spec:
  replicas: 2
  selector:
    matchLabels:
      app: data-reader
  template:
    metadata:
      labels:
        app: data-reader
    spec:
      serviceAccountName: s3-reader-sa
      containers:
        - name: app
          image: public.ecr.aws/amazonlinux/amazonlinux:2023
          command: ["sleep", "3600"]
```

### Example 2: Declarative Terraform EKS Cluster with VPC CNI & AWS Auth
```hcl
module "eks" {
  source  = "terraform-aws-modules/eks/aws"
  version = "~> 20.0"

  cluster_name    = "production-eks"
  cluster_version = "1.30"

  cluster_endpoint_public_access  = true
  cluster_endpoint_private_access = true

  vpc_id                   = var.vpc_id
  subnet_ids               = var.private_subnet_ids
  control_plane_subnet_ids = var.private_subnet_ids

  eks_managed_node_groups = {
    core_nodes = {
      name           = "core-node-group"
      instance_types = ["c7g.xlarge", "m7g.xlarge"]
      capacity_type  = "ON_DEMAND"

      min_size     = 2
      max_size     = 8
      desired_size = 3

      block_device_mappings = {
        xvda = {
          device_name = "/dev/xvda"
          ebs = {
            volume_size = 50
            volume_type = "gp3"
            encrypted   = true
          }
        }
      }
    }
  }

  enable_cluster_creator_admin_permissions = true
}
```

### Example 3: Provisioning Compute Dynamically with Karpenter
Karpenter `NodePool` configuration dynamically provisioning Graviton and AMD Spot instances for pending workloads.

```yaml
apiVersion: karpenter.sh/v1beta1
kind: NodePool
metadata:
  name: default
spec:
  template:
    spec:
      requirements:
        - key: "karpenter.k8s.aws/instance-category"
          operator: In
          values: ["c", "m", "r"]
        - key: "karpenter.k8s.aws/instance-cpu"
          operator: In
          values: ["4", "8", "16"]
        - key: "karpenter.sh/capacity-type"
          operator: In
          values: ["spot", "on-demand"]
        - key: "kubernetes.io/arch"
          operator: In
          values: ["arm64"]
      nodeClassRef:
        apiVersion: karpenter.k8s.aws/v1beta1
        kind: EC2NodeClass
        name: default
  limits:
    cpu: 500
    memory: 1000Gi
  disruption:
    consolidationPolicy: WhenUnderutilized
    expireAfter: 720h # 30 days
---
apiVersion: karpenter.k8s.aws/v1beta1
kind: EC2NodeClass
metadata:
  name: default
spec:
  amiFamily: Bottlerocket
  role: "KarpenterNodeRole-production-eks"
  subnetSelectorTerms:
    - tags:
        karpenter.sh/discovery: "production-eks"
  securityGroupSelectorTerms:
    - tags:
        karpenter.sh/discovery: "production-eks"
```

### Example 4: AWS Load Balancer Controller Ingress Definition
Declarative ALB Ingress automatically managing public ALB provisioning, SSL certificates via ACM, and target registration.

```yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: web-ingress
  namespace: production
  annotations:
    kubernetes.io/ingress.class: alb
    alb.ingress.kubernetes.io/scheme: internet-facing
    alb.ingress.kubernetes.io/target-type: ip
    alb.ingress.kubernetes.io/listen-ports: '[{"HTTPS": 443}]'
    alb.ingress.kubernetes.io/certificate-arn: "arn:aws:acm:us-east-1:111122223333:certificate/abc-123"
spec:
  rules:
    - host: api.example.com
      http:
        paths:
          - path: /
            pathType: Prefix
            backend:
              service:
                name: web-service
                port:
                  number: 80
```

### Example 5: Enabling Prefix Delegation for VPC CNI to Solve IP Exhaustion
Expand the number of IP addresses each EC2 node can assign to pods without consuming large subnet CIDR blocks.

```bash
# Enable Prefix Delegation in AWS VPC CNI DaemonSet
kubectl set env daemonset aws-node \
  -n kube-system \
  ENABLE_PREFIX_DELEGATION=true

# Set warm prefix target to prevent over-allocating /28 prefixes
kubectl set env daemonset aws-node \
  -n kube-system \
  WARM_PREFIX_TARGET=1
```

---

## 6. Best Practices

1. **Adopt EKS Pod Identity over IRSA**: For new clusters on Kubernetes 1.28+, standardize on **EKS Pod Identity**. It eliminates OIDC provider setup, simplifies IAM trust policies, and provides centralized auditing.
2. **Use Karpenter for Node Autoscaling**: Replace the legacy Kubernetes Cluster Autoscaler with Karpenter. Karpenter provisions nodes in 30–45 seconds (vs. 3–5 minutes for ASG-based autoscaling) and continuously consolidates underutilized nodes to cut EC2 bills by 30–50%.
3. **Enable Prefix Delegation on VPC CNI**: Standard VPC CNI allocates single private IP addresses per pod, which is bounded by the EC2 instance's maximum secondary IP limit. Enabling `ENABLE_PREFIX_DELEGATION=true` assigns `/28` IPv4 prefixes (16 IPs each), allowing smaller instances (e.g., `t4g.medium`) to run up to 110 pods without running out of IPs.
4. **Enforce Bottlerocket OS on Managed Nodes**: Use AWS **Bottlerocket** rather than standard Amazon Linux. Bottlerocket has a minimal footprint, read-only root filesystem, no package manager, and uses containerized admin containers for updates, drastically shrinking the attack surface.
5. **Always Use `target-type: ip` with AWS Load Balancer Controller**: When provisioning ALBs for Kubernetes services, configure `alb.ingress.kubernetes.io/target-type: ip`. This routes traffic directly from the ALB into the target pod's VPC IP, bypassing `kube-proxy` `NodePort` hops and preserving source IPs.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Exhausting VPC Subnet IPs with VPC CNI** | Clusters running thousands of pods fail to launch new pods due to IP address exhaustion. | Enable `ENABLE_PREFIX_DELEGATION` or attach secondary VPC CIDR blocks dedicated to pods (Custom Networking). |
| **Using `NodePort` Mode Behind ALB** | Double-routing latency overhead (ALB -> Node -> Kube-Proxy -> Pod on different node). | Use `target-type: ip` in Ingress annotations so the ALB routes directly to the pod's ENI IP. |
| **Assigning IAM Roles to EC2 Node Profile** | Every pod on the node inherits node-level IAM permissions, violating least-privilege security. | Keep node IAM roles minimal (`AmazonEKSWorkerNodePolicy`). Assign granular permissions via EKS Pod Identity. |
| **Leaving Cluster Endpoint 100% Public** | Kubernetes API server open to internet brute force and scanning. | Enable Private Access and restrict Public Access CIDR blocks to corporate VPN/bastion IPs. |
| **Deploying Monolithic Helm Charts without Namespace RBAC** | Unrestricted access across development and production namespaces. | Implement strict Kubernetes RBAC `Role` and `RoleBinding` objects paired with IAM groups. |
| **Ignoring CoreDNS Scaling** | Large clusters suffer DNS lookup timeouts and cascading HTTP failures under load. | Deploy the Cluster Proportional Autoscaler to scale CoreDNS replicas linearly with cluster node count. |

---

## 8. Performance, Security & Cost Notes

### Control Plane Scaling & SLA
- The EKS control plane automatically scales `kube-apiserver` and `etcd` capacity based on cluster load, API throughput, and node count.
- Backed by an AWS Service Level Agreement of **99.95% availability**.

### Security Hardening
- **Secrets Encryption with AWS KMS**: Enable envelope encryption for Kubernetes Secrets using a customer-managed AWS KMS key when creating the cluster. All secrets stored in `etcd` are encrypted at rest.
- **Control Plane Logging**: Enable CloudWatch log types (`api`, `audit`, `authenticator`, `controllerManager`, `scheduler`) for security auditing and compliance tracking.

### Cost Breakdown
- **Control Plane Fee**: **$0.10 per hour** per EKS cluster (~$73/month).
- **Compute (Data Plane)**: Standard EC2 instance pricing or Fargate per-vCPU/GB rates.
- **Network Traffic**: Intra-AZ pod-to-pod communication is free. Cross-AZ pod-to-pod communication incurs standard $0.01/GB data transfer charges.

---

## 9. Comparison Table

| Attribute | Amazon EKS (Kubernetes) | Amazon ECS | AWS Lambda |
|:---|:---|:---|:---|
| **Control Plane API** | Open Standard (CNCF Kubernetes) | AWS Proprietary API | AWS Serverless API |
| **Multi-Cloud Portability** | High (Runs anywhere k8s runs) | None (AWS locked) | None (AWS locked) |
| **Autoscaling Mechanism** | Karpenter / HPA / KEDA | Service Auto Scaling / CAS | Instant per-request microVM |
| **Control Plane Cost** | $0.10 / hour ($73/month) | $0.00 | $0.00 |
| **Operational Complexity** | High (Helm, CRDs, CNI, CSI, RBAC) | Minimal / Low | Zero |
| **Networking Architecture** | Native VPC CNI / Pod IPs | `awsvpc` Task ENIs | Hyperplane ENI tunnel |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural role of the AWS VPC CNI in Amazon EKS?</strong></summary>

**Answer:**
The AWS VPC Container Network Interface (CNI) plugin integrates Kubernetes pod networking natively with AWS Amazon VPC:
- Instead of creating an isolated overlay network (such as Calico or Flannel with VXLAN encapsulation), the VPC CNI assigns native private IPv4/IPv6 addresses directly from the host node's VPC subnet to each Kubernetes pod.
- This allows pods to communicate with other VPC resources (databases, internal load balancers, EC2 instances) without undergoing Network Address Translation (SNAT).
- It enables native AWS Security Groups to be applied directly to individual pods and eliminates the packet encapsulation latency overhead associated with overlay networks.
</details>

<details>
<summary><strong>Q2: How does EKS Pod Identity differ from IAM Roles for Service Accounts (IRSA)?</strong></summary>

**Answer:**
- **IRSA (Legacy Standard)**: Relies on AWS IAM OIDC identity federation. The cluster administrator must create an IAM OIDC provider for the cluster, configure complex string conditions with OIDC issuer URLs in IAM trust policies, and annotate Kubernetes Service Accounts. Managing multiple clusters and rotating OIDC keys across environments is cumbersome.
- **EKS Pod Identity (Modern Standard)**: Eliminates OIDC federation setup entirely. It uses a native EKS daemonset agent running on the node (`eks-pod-identity-agent`). The agent intercepts credential requests and directly queries the EKS control plane to validate the pod's identity and assume the target IAM role. IAM trust policies only need to trust the `pods.eks.amazonaws.com` service principal, drastically simplifying multi-cluster management.
</details>

<details>
<summary><strong>Q3: How does Karpenter improve upon the standard Kubernetes Cluster Autoscaler?</strong></summary>

**Answer:**
- **Cluster Autoscaler**: Coupled to EC2 Auto Scaling Groups (ASGs). It monitors pending pods, triggers an ASG scale-out, and waits for the ASG to launch homogeneous instances. It cannot choose instance sizes dynamically based on pod resource requirements and takes 3 to 6 minutes to launch nodes.
- **Karpenter**: Bypasses EC2 Auto Scaling Groups entirely. It evaluates the exact aggregate CPU, memory, and architecture requirements of pending pods, selects the optimal instance types from hundreds of available EC2 SKUs (matching pricing, availability, and spot capacity), and calls `ec2:RunInstances` directly. Karpenter provisions nodes in under 45 seconds and continuously consolidates workloads to terminate underutilized nodes.
</details>

<details>
<summary><strong>Q4: Why should the AWS Load Balancer Controller use `target-type: ip` instead of `instance` mode?</strong></summary>

**Answer:**
- **`instance` Mode**: The ALB routes traffic to the EC2 worker node's physical IP on a high-order `NodePort`. Once the packet arrives at the node, `kube-proxy` (via iptables or IPVS) performs another network hop to route the packet to the actual node hosting the target pod. This introduces double-hop network latency and obscures the client's true IP.
- **`ip` Mode**: Because the AWS VPC CNI grants every pod a real VPC IP, the AWS Load Balancer Controller registers the pod's ENI IP directly with the ALB Target Group. Traffic flows in a single hop directly from the ALB to the target pod container, reducing latency and preserving source IPs cleanly.
</details>

<details>
<summary><strong>Q5: What is Prefix Delegation in the AWS VPC CNI and why is it used?</strong></summary>

**Answer:**
EC2 instances have physical limits on the number of Elastic Network Interfaces (ENIs) and secondary IPv4 addresses they can attach. For example, an `m5.large` can attach 3 ENIs with 10 IPs each, capping the maximum number of pods on that node to 29 (after reserving 1 IP for the node).

Prefix Delegation assigns an entire `/28` IPv4 subnet prefix (16 contiguous IP addresses) to a single slot instead of a single secondary IP. This increases the pod density per node by up to 16x, allowing cost-effective smaller instances to run 110 pods (the Kubernetes default maximum) without exhausting node ENI limits.
</details>

<details>
<summary><strong>Q6: How does EKS manage control plane updates and version deprecations?</strong></summary>

**Answer:**
EKS provides managed, in-place control plane upgrades:
1. When an upgrade is initiated (e.g., from 1.29 to 1.30), AWS provisions replacement control plane nodes running the new version alongside the old nodes across 3 AZs.
2. `etcd` data is synchronized and migrated.
3. API traffic is dynamically shifted to the new nodes, ensuring zero downtime for the `kube-apiserver`.
4. Once the control plane upgrade completes, the platform engineer must update cluster add-ons (VPC CNI, CoreDNS, Kube-Proxy) and execute rolling updates of the worker node groups.
</details>

<details>
<summary><strong>Q7: What is the purpose of the `aws-auth` ConfigMap vs. EKS Access Entries?</strong></summary>

**Answer:**
- **`aws-auth` ConfigMap (Deprecated)**: Stored in the `kube-system` namespace. Mapped AWS IAM roles to Kubernetes RBAC groups. Prone to syntax corruption, race conditions during automation, and difficult to manage declaratively with Terraform or CloudFormation.
- **EKS Access Entries (Modern Standard)**: Moves authentication management out of the Kubernetes ConfigMap and into the AWS EKS API. Cluster access can now be granted, audited, and revoked using standard AWS CLI, SDK, and Terraform commands with built-in access policies (`AmazonEKSClusterAdminPolicy`, etc.) without touching the Kubernetes cluster internals.
</details>

<details>
<summary><strong>Q8: When should an enterprise choose Amazon ECS over Amazon EKS?</strong></summary>

**Answer:**
Choose **Amazon ECS** when:
- The team has limited Kubernetes operational expertise and wants simple, robust container management.
- Deep, low-overhead native AWS integration is preferred over open-source ecosystem tools.
- Rapid onboarding and minimal architectural overhead are required.

Choose **Amazon EKS** when:
- The organization requires multi-cloud or hybrid portability across AWS, GCP, Azure, and on-premises environments.
- The platform relies on rich CNCF ecosystem tooling (Helm charts, operators, Istio, ArgoCD, Prometheus/Grafana operators, KEDA).
- Complex multi-tenant namespace isolation, custom CRDs, and fine-grained scheduling topologies are essential.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect EKS Node and Pod VPC IP Allocations</strong></summary>

**Objective**: Verify that pods receive native VPC subnet IP addresses from the VPC CNI.

**Commands**:
```bash
# Inspect node IPs
kubectl get nodes -o custom-columns=NAME:.metadata.name,INTERNAL-IP:.status.addresses[0].address

# Inspect pod IPs and verify they belong to the same subnet range
kubectl get pods -A -o custom-columns=NAMESPACE:.metadata.namespace,NAME:.metadata.name,POD-IP:.status.podIP,NODE:.spec.nodeName
```
</details>

<details>
<summary><strong>Exercise 2: Create an EKS Pod Identity Association</strong></summary>

**Objective**: Bind an IAM role to a Kubernetes service account using the AWS CLI.

**Command**:
```bash
aws eks create-pod-identity-association \
  --cluster-name test-cluster \
  --namespace default \
  --service-account test-sa \
  --role-arn arn:aws:iam::111122223333:role/TestPodRole
```
</details>

<details>
<summary><strong>Exercise 3: Enable EKS Control Plane Logging</strong></summary>

**Objective**: Enable API and Authenticator audit logs to Amazon CloudWatch.

**Command**:
```bash
aws eks update-cluster-config \
  --name test-cluster \
  --logging '{"clusterLogging":[{"types":["api","authenticator"],"enabled":true}]}'
```
</details>

<details>
<summary><strong>Exercise 4: Verify Prefix Delegation Status on VPC CNI</strong></summary>

**Objective**: Check whether the VPC CNI is configured to assign `/28` IPv4 prefixes.

**Command**:
```bash
kubectl get daemonset aws-node -n kube-system -o jsonpath='{.spec.template.spec.containers[0].env[?(@.name=="ENABLE_PREFIX_DELEGATION")].value}'
```
</details>

<details>
<summary><strong>Exercise 5: Deploy the AWS Load Balancer Controller</strong></summary>

**Objective**: Install the controller using Helm and verify deployment status.

**Commands**:
```bash
helm repo add eks https://aws.github.io/eks-charts
helm repo update

helm install aws-load-balancer-controller eks/aws-load-balancer-controller \
  -n kube-system \
  --set clusterName=test-cluster \
  --set serviceAccount.create=false \
  --set serviceAccount.name=aws-load-balancer-controller
```
</details>

<details>
<summary><strong>Exercise 6: Test Karpenter Dynamic Provisioning</strong></summary>

**Objective**: Create a high-replica deployment requiring 50 vCPUs and observe Karpenter provision optimal Graviton Spot instances within 45 seconds.

**Steps**:
1. Deploy a test deployment with `replicas: 20` and `resources.requests.cpu: "2"`.
2. Inspect Karpenter logs: `kubectl logs -f -n karpenter -l app.kubernetes.io/name=karpenter`.
3. Watch Karpenter select instance types and call `ec2:RunInstances`.
4. Delete deployment and watch Karpenter consolidate and terminate empty nodes.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                                Amazon EKS Architecture Cheat Sheet                    |
+---------------------------------------------------------------------------------------+
|  Component             | Standard / Tool                    | Function                |
|------------------------+------------------------------------+-------------------------|
|  Control Plane         | Managed Multi-AZ ($0.10/hr)        | 99.95% HA Kubernetes API|
|  Pod Identity          | EKS Pod Identity (Modern)          | Simple IAM role binding |
|  Node Autoscaler       | Karpenter                          | Fast sub-minute scaling |
|  Pod Networking        | AWS VPC CNI                        | Native VPC IP per pod   |
|  IP Expansion          | Prefix Delegation (/28)            | Eliminates IP shortages |
|  Ingress Controller    | AWS Load Balancer Controller       | Target-type: ip directly|
|  Operating System      | Bottlerocket OS                    | Read-only container host|
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon EKS User Guide](https://docs.aws.amazon.com/eks/latest/userguide/what-is-eks.html)
- [EKS Pod Identity Documentation](https://docs.aws.amazon.com/eks/latest/userguide/pod-identities.html)
- [Karpenter: Fast & Flexible Kubernetes Node Autoscaler](https://karpenter.sh/)
- [AWS Load Balancer Controller Documentation](https://kubernetes-sigs.github.io/aws-load-balancer-controller/)
- [AWS VPC CNI Plugin Documentation & Best Practices](https://github.com/aws/amazon-vpc-cni-k8s)
