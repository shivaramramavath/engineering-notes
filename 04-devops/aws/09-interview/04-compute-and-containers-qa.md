---
topic: "AWS Compute, Containers & Serverless Technical Interview Q&A"
module: "09-interview"
track: "aws"
difficulty: "advanced"
last-tested: "2026-11"
tags:
  - "interview"
  - "compute"
  - "ec2"
  - "lambda"
  - "ecs"
  - "fargate"
  - "eks"
  - "karpenter"
---

# AWS Compute, Containers & Serverless Technical Interview Q&A

## Table of Contents
- [1. Amazon EC2 Lifecycle, Spot & Operations](#1-amazon-ec2-lifecycle-spot--operations)
- [2. AWS Lambda Execution Model & Performance](#2-aws-lambda-execution-model--performance)
- [3. Container Orchestration: ECS, Fargate & EKS](#3-container-orchestration-ecs-fargate--eks)
- [4. Modern Kubernetes Autoscaling with Karpenter](#4-modern-kubernetes-autoscaling-with-karpenter)
- [5. Deep-Dive Compute Scenarios & Production Diagnostics](#5-deep-dive-compute-scenarios--production-diagnostics)

---

## 1. Amazon EC2 Lifecycle, Spot & Operations

### Q1: An EC2 instance is in the `Stopped` state. Are you still being charged for it? What is the difference between `Stop` and `Terminate`?
**Answer**:
When an EC2 instance is in the `Stopped` state:
- **Compute (vCPU and RAM) billing ceases immediately**.
- **Storage and ancillary resources continue to accrue charges**: You continue paying for attached **Amazon EBS storage volumes**, EBS snapshots, and any **Elastic IP (EIP)** addresses associated with the stopped instance (\$0.005/hour for idle EIPs).
- **Difference between Stop and Terminate**:
  - `Stop`: The virtual machine is shut down, but its configuration and EBS root volume persist. It can be started again later (potentially on different underlying physical hardware). RAM contents are lost unless Hibernation is enabled.
  - `Terminate`: The instance is permanently deleted. The underlying EBS root volume is automatically deleted by default (governed by the `DeleteOnTermination` attribute). This action is **irreversible**.

### Q2: How do EC2 Spot Instances work, and how should an application handle the 2-minute Spot interruption warning?
**Answer**:
Spot Instances allow customers to bid on unused AWS EC2 spare compute capacity at discounts of up to 90% compared to On-Demand pricing.
- **The Trade-Off**: AWS can reclaim Spot capacity at any time when On-Demand demand rises.
- **Interruption Warning**: AWS provides an automated **2-minute interruption notice** before terminating the instance.
- **Handling Interruption in Production**:
  1. **EventBridge Rule**: Listen to the `EC2 Spot Instance Interruption Warning` event emitted to Amazon EventBridge.
  2. **Metadata Polling**: Software agents can poll the instance metadata endpoint at `http://169.254.169.254/latest/meta-data/spot/instance-action` every 5 seconds.
  3. **Graceful Drain Action**: When the warning is received:
     - The target group immediately deregisters the instance from the Application Load Balancer to stop receiving new connections.
     - In-flight HTTP transactions are drained (connection draining timeout: 60-90s).
     - Checkpoint batch computation state to S3 or DynamoDB.
     - Exit cleanly before the 120-second timer expires.

### Q3: Why should organizations mandate AWS Systems Manager (SSM) Session Manager over traditional SSH key pairs?
**Answer**:
Traditional SSH management introduces severe enterprise security liabilities:
- Requires opening inbound TCP port 22 in Security Groups to the internet or corporate CIDRs.
- Requires managing, distributing, and rotating static public/private SSH key pairs (`.pem` files).
- Risk of lost private keys locking administrators out, or leaked keys granting rogue access.

**SSM Session Manager Advantages**:
1. **Zero Open Inbound Ports**: The SSM Agent running inside the instance initiates an **outbound HTTPS connection** (port 443) to the AWS SSM endpoint. Port 22 remains closed.
2. **Centralized IAM Authentication**: Access is granted strictly via IAM policies; no SSH keys exist.
3. **Full Audit Logging**: Every keystroke and bash command executed during a session is recorded and streamed to Amazon CloudWatch Logs and an immutable S3 bucket.
4. **Integration with MFA & Session Limits**: Can require IAM MFA prompts before initiating a shell session.

---

## 2. AWS Lambda Execution Model & Performance

### Q4: Explain the AWS Lambda execution environment lifecycle. What constitutes a "Cold Start", and what are 4 actionable ways to reduce it?
**Answer**:
When an event invokes a Lambda function, AWS allocates an execution environment inside a **Firecracker MicroVM**.
- **Cold Start Lifecycle**:
  1. *Download*: Lambda downloads the deployment package (zip) or container image layers from S3/ECR.
  2. *Initialize Environment*: Launches the MicroVM runtime and extensions.
  3. *Execute Init Code*: Runs all code **outside** the `exports.handler` function (importing libraries, initializing AWS SDK clients, opening database connections).
  - *Duration*: 100ms (Node.js/Python) up to 3-5+ seconds (Java/C# or large Docker images).
- **Warm Invocation**: If another event arrives while the environment is active, Lambda delivers the event directly to the handler, bypassing steps 1-3.

**4 Ways to Reduce Cold Starts**:
1. **Provisioned Concurrency**: Allocates pre-warmed execution environments that run init code ahead of time, eliminating cold starts entirely for latency-critical APIs.
2. **Lambda SnapStart (Java runtimes)**: Takes a Firecracker MicroVM snapshot of the initialized memory state after startup, caching it on disk. New invocations resume from the snapshot in under 200ms.
3. **Minimize Deployment Bundle Size**: Use tree-shaking bundlers (esbuild, rollup) to strip unused dependencies; keep packages under 10 MB.
4. **Choose Lightweight Runtimes**: Node.js, Python, Rust, and Go initialize an order of magnitude faster than JVM or .NET runtimes.

### Q5: Why is AWS Lambda memory configuration considered a direct cost and performance optimization lever?
**Answer**:
In AWS Lambda, **allocating memory proportionally scales allocated vCPU power and network bandwidth**.
- At 1,769 MB of RAM, Lambda provisions exactly 1 full dedicated vCPU. At 10,240 MB, it provisions 6 vCPUs.
- If a compute-heavy task (e.g., cryptographic hashing, image resizing, or large JSON parsing) takes 2,000ms at 256 MB RAM, increasing memory to 1,024 MB (4x more CPU) may cut execution duration down to 400ms (5x faster).
- Since Lambda billing is calculated as $\text{Memory (GB)} \times \text{Duration (Seconds)}$, running 5x faster at 4x the memory results in **lower total billing cost per invocation while improving P99 user latency**.
- *Best Practice*: Use the open-source **AWS Lambda Power Tuning** state machine to empirically benchmark execution cost curves across memory allocations.

### Q6: What happens when an SQS-triggered Lambda function encounters an unhandled exception for 1 record in a batch of 10?
**Answer**:
By default, if any single record in an SQS batch throws an unhandled error, **Lambda fails the entire invocation**.
- As a result, SQS considers all 10 messages unacknowledged.
- When the SQS visibility timeout expires, **all 10 messages become visible again in the queue**.
- The 9 messages that succeeded originally will be re-processed by another worker, causing duplicate operations unless the consumer is strictly idempotent.
- **The Correct Solution**: Enable **ReportBatchItemFailures** in the Lambda Event Source Mapping. The handler catches errors in a `try/catch` block and returns a response containing:
  ```json
  { "batchItemFailures": [{ "itemIdentifier": "failed-message-id-3" }] }
  ```
  SQS deletes the 9 successful messages and retries *only* the specific failed message ID.

---

## 3. Container Orchestration: ECS, Fargate & EKS

### Q7: What is the fundamental difference between the ECS Task Execution Role and the ECS Task Role?
**Answer**:
- **ECS Task Execution Role (`execution_role_arn`)**:
  - Used by the **AWS ECS Container Agent** and Fargate infrastructure layer.
  - Grants permissions needed to **bootstrap and run the container**: pulling container images from private Amazon ECR repositories, fetching decrypted secrets from AWS Secrets Manager/SSM, and streaming container logs (`awslogs`) to CloudWatch Logs.
- **ECS Task Role (`task_role_arn`)**:
  - Used by the **application process running inside the container**.
  - Grants permissions needed by application business logic: querying Amazon DynamoDB, reading from Amazon S3, publishing to SNS topics, or assuming cross-account roles.
- *Failure Mode*: If an application gets `AccessDenied` trying to query DynamoDB, granting permissions to the Task Execution Role will not resolve it; permissions must be added to the Task Role.

### Q8: An ECS service using Fargate keeps restarting containers in an endless crash loop. What are the top 5 things you check?
**Answer**:
1. **Container Exit Code**:
   - Exit code `137`: Out of Memory (OOM killed). The container exceeded the task memory allocation.
   - Exit code `1`: Application runtime crash (unhandled exception or missing environment variable).
2. **ALB Health Check Path & Port**: Verify the Application Load Balancer health check path matches the application route (e.g., `/health`) and port (e.g., 8080). If the app responds with HTTP 404 or fails to respond within the grace period, the ALB kills the task.
3. **Application Port Binding (`0.0.0.0` vs `127.0.0.1`)**: If the container process listens on `127.0.0.1` (localhost) rather than `0.0.0.0` (all interfaces), incoming traffic forwarded from the ALB over the task's ENI will be rejected.
4. **Health Check Grace Period**: If the container has a slow startup (e.g., Spring Boot taking 45 seconds to initialize), the ALB may check health at second 30 and terminate the container before it finishes booting. Increase `health_check_grace_period_seconds`.
5. **CPU Architecture Mismatch**: Building a Docker image on an Apple Silicon Mac (`linux/arm64`) and deploying to an ECS Task configured for `X86_64` causes an immediate `exec format error` container exit.

---

## 4. Modern Kubernetes Autoscaling with Karpenter

### Q9: How does Karpenter fundamentally improve upon the standard Kubernetes Cluster Autoscaler on Amazon EKS?
**Answer**:
The traditional Kubernetes **Cluster Autoscaler** operates by adjusting the desired capacity of pre-configured **EC2 Auto Scaling Groups (ASGs)**:
- *Limitations*: Constrained to static instance types defined in the ASG; slow to react (evaluates every 10-30s, launches ASG node, waits 3-5 minutes for EC2 boot and node join); poor bin-packing efficiency.

**Karpenter** is a next-generation, high-performance node autoscaler designed directly for AWS and EKS:
1. **Direct Fleet API Integration**: Karpenter bypasses EC2 Auto Scaling Groups entirely. It calls the AWS EC2 `CreateFleet` APIs directly.
2. **Just-in-Time, Right-Sized Node Provisioning**: It inspects pending unschedulable pods, analyzes their exact resource requests (CPU, memory, GPU, tolerations, topology spread constraints), and chooses the optimal, lowest-cost EC2 instance type from hundreds of available instance families in milliseconds.
3. **Fast Scaling**: Launches new worker nodes and joins them to the EKS cluster in under **45 seconds**.
4. **Automated Node Consolidation**: Continuously monitors the cluster; when workloads scale down, Karpenter automatically drains, terminates, and consolidates fragmented nodes to eliminate wasted idle capacity.

---

## 5. Deep-Dive Compute Scenarios & Production Diagnostics

### Scenario 1: Debugging a Lambda VPC Timeout Outage
**Interviewer**: *"You attach an existing Lambda function to a private VPC subnet to access an internal RDS database. Suddenly, the Lambda function begins timing out on all invocations when calling external payment APIs (Stripe) and Amazon S3. What happened, and how do you fix it?"*

**Structured Candidate Response**:
- **Root Cause**: By default, Lambda functions run in a secure AWS-managed VPC with direct access to the public internet. The moment you configure a Lambda function with VPC subnets, AWS attaches a Hyperplane Elastic Network Interface (ENI) to your private subnet.
  - From that moment onward, **all outbound network traffic from Lambda is routed through your VPC's route table**.
  - If the private subnet does not have a route table path to a **NAT Gateway** (or if the Lambda was erroneously placed in a public subnet, where it cannot receive a public IP), the Lambda loses all internet connectivity.
  - Calls to public HTTPS APIs (Stripe) and public AWS endpoints (S3, DynamoDB) hang until the function reaches its timeout.
- **The Remediation**:
  1. For external internet APIs (Stripe): Ensure the Lambda subnets have a route table entry `0.0.0.0/0 -> nat-xxxx` targeting a functioning NAT Gateway in a public subnet.
  2. For AWS native services (S3 and DynamoDB): Deploy **VPC Gateway Endpoints** for S3 and DynamoDB in the VPC. This routes AWS SDK traffic directly over the internal AWS backbone, bypassing the NAT Gateway, eliminating NAT data transfer costs, and improving latency.

### Scenario 2: Container OOM Kill Diagnostics
**Interviewer**: *"An engineer reports that an ECS Fargate microservice task disappears every 20 minutes under load, but there are no errors in application logs. How do you confirm it was an OOM kill and resolve the issue?"*

**Structured Candidate Response**:
1. **Confirm OOM Kill**:
   - Execute `aws ecs describe-tasks --cluster <cluster> --tasks <task-id>`.
   - Inspect the `containers[0].exitCode` and `stoppedReason`.
   - If the `exitCode` is `137` and `stoppedReason` states `OutOfMemory: Container killed due to memory usage`, the Linux kernel OOM killer terminated the process.
   - Application logs show zero traces because the OS kernel sent an immediate uncatchable `SIGKILL` (`kill -9`).
2. **Inspect Memory Metrics**:
   - Check Amazon CloudWatch Container Insights metrics for `MemoryUtilized` vs `MemoryReserved`.
   - Look for a steady upward memory consumption curve, which indicates a **memory leak** (e.g., unbounded in-memory caching or unclosed connections).
3. **Remediation**:
   - As an immediate operational fix: Increase task memory in the task definition (e.g., from 1024 MB to 2048 MB) to prevent immediate cascading crashes.
   - For long-term root cause: Profile application heap dumps to locate memory leaks, or configure JVM flags (e.g., `-XX:MaxRAMPercentage=75.0`) so the runtime respects container cgroup memory limits.
