# AWS Deployment Targets for Spring Boot: ECS Fargate, EKS, and Lambda SnapStart

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 14-production-and-deployment |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts & AWS Target Architectures](#2-core-concepts--aws-target-architectures)
- [3. Target 1: AWS ECS Fargate (Serverless Containers)](#3-target-1-aws-ecs-fargate-serverless-containers)
- [4. Target 2: AWS EKS with IAM Roles for Service Accounts (IRSA)](#4-target-2-aws-eks-with-iam-roles-for-service-accounts-irsa)
- [5. Target 3: AWS Lambda with SnapStart (Sub-200ms Serverless)](#5-target-3-aws-lambda-with-snapstart-sub-200ms-serverless)
- [6. Configuration & Spring Cloud AWS Reference](#6-configuration--spring-cloud-aws-reference)
- [7. Practical Examples](#7-practical-examples)
- [8. Best Practices & Production Guidelines](#8-best-practices--production-guidelines)
- [9. Common Mistakes & Pitfalls](#9-common-mistakes--pitfalls)
- [10. Comparison / Architectural Selection Matrix](#10-comparison--architectural-selection-matrix)
- [11. Interview Questions & Answers](#11-interview-questions--answers)
- [12. Practice Exercises](#12-practice-exercises)
- [13. Summary / Cheat Sheet](#13-summary--cheat-sheet)
- [14. Further Reading](#14-further-reading)

---

## 1. What & Why

### Problem Statement
Deploying Spring Boot applications to Amazon Web Services (AWS) is not a one-size-fits-all problem. Choosing the wrong AWS compute abstraction results in severe cost inefficiency, operational toil, or unacceptable latency:
- Running standard Spring Boot JARs on traditional EC2 virtual machines requires manual OS patching, AMI baking, and complex autoscaling scripts.
- Deploying monolithic Spring Boot APIs to standard AWS Lambda without optimization produces painful 8-to-15-second cold-start delays.
- Managing raw Kubernetes (EKS) clusters for simple microservices introduces excessive infrastructure complexity when managed container platforms (ECS Fargate) would suffice.

### Historical Evolution
AWS Java deployments evolved through four distinct eras:
1. **EC2 & Elastic Beanstalk**: Virtual machine deployments with heavy OS overhead.
2. **Amazon ECS (Elastic Container Service)**: Docker container management, perfected with **AWS Fargate** (serverless compute without managing EC2 instances).
3. **Amazon EKS (Elastic Kubernetes Service)**: Standard enterprise Kubernetes backed by cloud-native autoscaling with **Karpenter** and fine-grained security via **IRSA**.
4. **AWS Lambda with SnapStart**: Introduced in late 2022, SnapStart utilizes Firecracker MicroVM snapshot restoration to deliver **sub-200ms cold starts** for standard Java Spring Boot serverless applications.

### Production Relevance
Evaluating and mastering the three primary AWS targets ensures that:
- Microservices scale elastically without managing underlying EC2 server infrastructure.
- Applications access AWS services (S3, SQS, DynamoDB, Secrets Manager) securely without static IAM credentials.
- Serverless event-driven functions run with near-instant responsiveness at fraction-of-a-cent costs.

---

## 2. Core Concepts & AWS Target Architectures

```
+------------------------------------------------------------------------------------+
|                         SPRING BOOT AWS COMPUTE SPECTRUM                           |
+------------------------------------------------------------------------------------+
       |                                |                               |
       v                                v                               v
+-----------------------+    +-----------------------+    +-----------------------+
|   AWS ECS FARGATE     |    |        AWS EKS        |    |  LAMBDA + SNAPSTART   |
| (Serverless Container)|    | (Managed Kubernetes)  |    | (Event-Driven Server) |
|                       |    |                       |    |                       |
| - Zero VM management  |    | - Full K8s ecosystem  |    | - Pay-per-invocation  |
| - Task Definitions    |    | - Helm & ArgoCD GitOps|    | - Sub-200ms cold start|
| - ALB integration     |    | - Karpenter autoscaler|    | - Firecracker snapshot|
| - Ideal for REST APIs |    | - Ideal for enterprise|    | - Ideal for webhooks, |
|   & background jobs   |    |   multi-service fleets|    |   queues, async tasks |
+-----------------------+    +-----------------------+    +-----------------------+
```

---

## 3. Target 1: AWS ECS Fargate (Serverless Containers)

### Architecture
Amazon ECS with AWS Fargate launches containers directly on serverless AWS infrastructure. You define CPU and Memory requirements (e.g. 1 vCPU, 2GB RAM), and AWS provisions the isolated Firecracker execution environment automatically.

### Key Components
1. **Task Definition**: The blueprint declaring container image, CPU/RAM, environment variables, port mappings, and logging.
2. **Task Execution Role**: IAM role used by the *ECS agent* to pull images from ECR and stream logs to AWS CloudWatch.
3. **Task Role**: IAM role used by the *Spring Boot application code* inside the container to access AWS services (S3, SQS, Secrets Manager).
4. **Application Load Balancer (ALB)**: Routes public internet traffic to dynamic private Fargate task IPs across availability zones.

---

## 4. Target 2: AWS EKS with IAM Roles for Service Accounts (IRSA)

### Architecture
Amazon EKS provides standard CNCF-compliant Kubernetes. To authenticate pods against AWS IAM without hardcoding credentials or granting broad node permissions, EKS uses **IAM Roles for Service Accounts (IRSA)**.

### How IRSA Works
1. An OIDC (OpenID Connect) identity provider is associated with the EKS cluster.
2. An IAM Role is created with a trust policy allowing the OIDC provider to assume it.
3. A Kubernetes `ServiceAccount` is annotated with the IAM Role ARN:
   ```yaml
   apiVersion: v1
   kind: ServiceAccount
   metadata:
     name: order-service-sa
     annotations:
       eks.amazonaws.com/role-arn: arn:aws:iam::123456789012:role/OrderServiceS3Role
   ```
4. The EKS pod mutating webhook injects an AWS token file into `/var/run/secrets/eks.amazonaws.com/serviceaccount/token` and sets `AWS_ROLE_ARN`.
5. The AWS SDK in Spring Boot automatically exchanges this token for temporary, short-lived IAM credentials.

---

## 5. Target 3: AWS Lambda with SnapStart (Sub-200ms Serverless)

### Architecture
Traditional Java on AWS Lambda suffered from 8-to-15 second cold starts because the JVM had to initialize classes, run reflection, and compile JIT bytecode on every new execution environment.

**AWS Lambda SnapStart** completely eliminates this penalty:
1. **At Deployment Time**: AWS boots the Spring Boot application, executes initialization routines, publishes the Spring `ApplicationReadyEvent`, and brings the JVM to a fully warmed state.
2. **Snapshot Creation**: AWS freezes the Firecracker microVM execution environment (memory and disk state), creates an encrypted snapshot, and caches it in a high-speed multi-tier cache.
3. **At Invocation Time (Cold Start)**: When an event arrives, AWS restores the microVM memory state directly from the snapshot in **under 200 milliseconds**!

### CRaC Compatibility
SnapStart natively integrates with the OpenJDK **Coordinated Restore at Checkpoint (CRaC)** API (`org.crac.Resource`), firing `beforeCheckpoint()` hooks before snapshot capture and `afterRestore()` hooks upon restoration.

---

## 6. Configuration & Spring Cloud AWS Reference

### Spring Cloud AWS Starter (`pom.xml`)
```xml
<dependencyManagement>
    <dependencies>
        <dependency>
            <groupId>io.awspring.cloud</groupId>
            <artifactId>spring-cloud-aws-dependencies</artifactId>
            <version>3.2.1</version>
            <type>pom</type>
            <scope>import</scope>
        </dependency>
    </dependencies>
</dependencyManagement>

<dependencies>
    <!-- Automatic AWS Secrets Manager & SQS integration -->
    <dependency>
        <groupId>io.awspring.cloud</groupId>
        <artifactId>spring-cloud-aws-starter-secrets-manager</artifactId>
    </dependency>
    <dependency>
        <groupId>io.awspring.cloud</groupId>
        <artifactId>spring-cloud-aws-starter-sqs</artifactId>
    </dependency>
</dependencies>
```

### Application Configuration (`application.yml`)
```yaml
spring:
  config:
    import:
      # Automatically imports secrets from AWS Secrets Manager on boot!
      - aws-secretsmanager:/secrets/order-service/production
  cloud:
    aws:
      region:
        static: us-east-1
      credentials:
        # Default credential provider chain: picks up IRSA or ECS Task Role automatically!
        use-default-credentials-provider: true
```

---

## 7. Practical Examples

### Example 1: Complete ECS Fargate Task Definition (`ecs-task-def.json`)
```json
{
  "family": "order-service",
  "networkMode": "awsvpc",
  "requiresCompatibilities": ["FARGATE"],
  "cpu": "1024",
  "memory": "2048",
  "executionRoleArn": "arn:aws:iam::123456789012:role/ecsTaskExecutionRole",
  "taskRoleArn": "arn:aws:iam::123456789012:role/orderServiceTaskRole",
  "containerDefinitions": [
    {
      "name": "order-service-app",
      "image": "123456789012.dkr.ecr.us-east-1.amazonaws.com/order-service:1.2.0",
      "essential": true,
      "portMappings": [
        {
          "containerPort": 8080,
          "hostPort": 8080,
          "protocol": "tcp"
        },
        {
          "containerPort": 9090,
          "hostPort": 9090,
          "protocol": "tcp"
        }
      ],
      "environment": [
        { "name": "SPRING_PROFILES_ACTIVE", "value": "prod" },
        { "name": "JAVA_TOOL_OPTIONS", "value": "-XX:MaxRAMPercentage=75.0 -XX:+ExitOnOutOfMemoryError" }
      ],
      "secrets": [
        {
          "name": "DATABASE_PASSWORD",
          "valueFrom": "arn:aws:secretsmanager:us-east-1:123456789012:secret:orders/db-password"
        }
      ],
      "healthCheck": {
        "command": ["CMD-SHELL", "curl -f http://localhost:9090/actuator/health/readiness || exit 1"],
        "interval": 10,
        "timeout": 3,
        "retries": 3,
        "startPeriod": 45
      },
      "logConfiguration": {
        "logDriver": "awslogs",
        "options": {
          "awslogs-group": "/ecs/order-service",
          "awslogs-region": "us-east-1",
          "awslogs-stream-prefix": "app"
        }
      }
    }
  ]
}
```

### Example 2: AWS Lambda with SnapStart Function Implementation
Using Spring Cloud Function to create a serverless order processor with sub-200ms cold start:

```java
package com.example.aws.lambda;

import org.crac.Context;
import org.crac.Core;
import org.crac.Resource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Bean;

import java.util.UUID;
import java.util.function.Function;

@SpringBootApplication
public class OrderLambdaApplication implements Resource {

    private static final Logger log = LoggerFactory.getLogger(OrderLambdaApplication.class);
    private volatile String executionInstanceId;

    public OrderLambdaApplication() {
        // Register with CRaC to handle SnapStart lifecycle hooks
        Core.getGlobalContext().register(this);
        this.executionInstanceId = UUID.randomUUID().toString();
    }

    // Function bean routed by Spring Cloud Function
    @Bean
    public Function<OrderEvent, OrderResponse> processOrder() {
        return event -> {
            log.info("Processing order {} on Lambda instance {}", event.orderId(), executionInstanceId);
            return new OrderResponse(event.orderId(), "CONFIRMED", executionInstanceId);
        };
    }

    @Override
    public void beforeCheckpoint(Context<? extends Resource> context) {
        log.info("SnapStart Checkpoint: Pre-warming complete, freezing snapshot...");
        this.executionInstanceId = null; // Clear instance-specific ID
    }

    @Override
    public void afterRestore(Context<? extends Resource> context) {
        // Generates fresh UUID and reseeds SecureRandom upon microVM restoration
        this.executionInstanceId = UUID.randomUUID().toString();
        log.info("SnapStart Restore: MicroVM restored with fresh instanceId {}", executionInstanceId);
    }
}

record OrderEvent(String orderId, double amount) {}
record OrderResponse(String orderId, String status, String instanceId) {}
```

### Example 3: SQS Message Listener Using Spring Cloud AWS 3
```java
package com.example.aws.sqs;

import io.awspring.cloud.sqs.annotation.SqsListener;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

@Component
public class PaymentQueueConsumer {

    private static final Logger log = LoggerFactory.getLogger(PaymentQueueConsumer.class);

    // Automatically pulls credentials via IAM Task Role / IRSA
    @SqsListener("${app.queues.payment-events}")
    public void handlePaymentMessage(String payload) {
        log.info("Received message from SQS queue: {}", payload);
        // Process message
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (Static IAM User Keys)
```yaml
# ANTI-PATTERN: Embedding static AWS access keys in application.yml
# Vulnerability: Stolen keys grant permanent access to AWS infrastructure!
spring:
  cloud:
    aws:
      credentials:
        access-key: AKIAIOSFODNN7EXAMPLE # NEVER DO THIS!
        secret-key: wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY

# PRODUCTION FIX: Default Credential Provider Chain with IAM Roles
# AWS SDK automatically resolves credentials from ECS Task Role or EKS IRSA!
spring:
  cloud:
    aws:
      credentials:
        use-default-credentials-provider: true
```

---

## 8. Best Practices & Production Guidelines

1. **Never Hardcode AWS Credentials**: Use the AWS Default Credentials Provider Chain. In ECS, assign an IAM **Task Role**. In EKS, assign an **IAM Role for Service Accounts (IRSA)**.
2. **Separate Task Execution Role from Task Role**:
   - `executionRoleArn`: Granted permissions only to pull from ECR and write logs to CloudWatch.
   - `taskRoleArn`: Granted business permissions (read from specific S3 bucket, read specific Secrets Manager secret).
3. **Use Lambda SnapStart for Serverless Workloads**: If deploying to AWS Lambda, enable SnapStart in your SAM or CDK template. It transforms a 12-second cold start into a 180ms response with zero code modifications.
4. **Use Spring Cloud AWS Secrets Manager Integration**: Use `spring.config.import=aws-secretsmanager:/path` to load database credentials directly into the Spring environment without committing secrets or writing custom SDK boilerplate.
5. **Enable Container Health Checks**: In ECS Fargate, define native container health checks querying `/actuator/health/readiness` on port 9090 with a 30–45s `startPeriod` to prevent premature task termination during boot.

---

## 9. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Confusing Task Execution Role with Task Role | App cannot access S3 because S3 permissions were added to the execution role instead of task role. | Add application permissions to the `taskRoleArn`. |
| Cold starts on standard Lambda | JVM cold boots take 10+ seconds, causing API Gateway timeouts. | Enable Lambda SnapStart with Java 21 runtime. |
| Missing `startPeriod` on ECS health checks | ECS kills task 15 seconds after launch because Spring Boot is still initializing beans. | Set `startPeriod: 45` in Task Definition healthCheck. |
| Hardcoding region or credentials | Code cannot run portably across staging (us-east-1) and prod (eu-west-1). | Rely on `AWS_REGION` and default credential chain. |
| Leaking static state across SnapStart restores | Pseudo-random seeds or static UUIDs frozen into snapshot are duplicated across all Lambda instances. | Implement `org.crac.Resource` to re-seed values in `afterRestore()`. |
| Missing EKS OIDC provider setup | Pod ServiceAccount cannot assume IAM role; throws `AccessDeniedException`. | Configure IAM OIDC provider for EKS cluster. |

---

## 10. Comparison / Architectural Selection Matrix

| Dimension | AWS ECS Fargate | AWS EKS | AWS Lambda + SnapStart |
|---|---|---|---|
| **Operational Overhead** | **Low (Serverless containers)** | High (Kubernetes cluster management)| **Lowest (Pure serverless)** |
| **Startup / Scaling Speed** | Medium (30 – 60s per task) | Fast (10 – 30s with Karpenter) | **Instant (< 200ms)** |
| **Cost Model** | Pay per vCPU/RAM per second | Base cluster fee + Node compute | Pay per request invocation |
| **Protocol Support** | HTTP, gRPC, WebSockets, TCP | HTTP, gRPC, WebSockets, TCP | Request-response HTTP / Events |
| **Execution Time Limit** | Unlimited | Unlimited | **15 minutes max** |
| **Recommended Use Case** | 80% of standard enterprise Spring Boot REST APIs | Large multi-service teams with shared K8s platform | Webhooks, queue consumers, asynchronous jobs |

---

## 11. Interview Questions & Answers

### Q1: What is the difference between an ECS Task Execution Role and an ECS Task Role?
<details>
<summary>Click to view answer</summary>

- **Task Execution Role (`executionRoleArn`)**: Used by the underlying AWS ECS container agent itself before the application container starts. It needs permissions to pull images from Amazon ECR and send container logs to Amazon CloudWatch.
- **Task Role (`taskRoleArn`)**: Used by the actual Spring Boot application code running *inside* the container. It contains permissions for your business logic, such as reading an S3 bucket, publishing to SQS, or querying DynamoDB.
</details>

### Q2: How does AWS Lambda SnapStart achieve sub-200ms cold starts for Spring Boot?
<details>
<summary>Click to view answer</summary>

SnapStart uses Firecracker microVM snapshot technology:
1. When you publish a Lambda function version, AWS boots the Spring Boot application, loads all beans, runs JIT compilation, and brings the JVM to a fully ready state.
2. AWS takes an encrypted snapshot of the microVM's memory and disk state and caches it.
3. When invocations occur (cold starts), instead of booting a new JVM from scratch, AWS restores the microVM memory state from the snapshot in under 200ms, resuming execution immediately with a pre-warmed JIT compiler.
</details>

### Q3: What is IAM Roles for Service Accounts (IRSA) in Amazon EKS?
<details>
<summary>Click to view answer</summary>

IRSA is an EKS feature that allows assigning specific AWS IAM roles to individual Kubernetes pods. An OIDC identity provider links the EKS cluster with AWS IAM. A Kubernetes `ServiceAccount` is annotated with the IAM Role ARN. The EKS pod webhook injects an OIDC token into the pod. The AWS SDK in Spring Boot uses this token to call `sts:AssumeRoleWithWebIdentity`, obtaining short-lived IAM credentials with fine-grained access specific only to that pod.
</details>

### Q4: Why must random seeds and network sockets be handled carefully with Lambda SnapStart?
<details>
<summary>Click to view answer</summary>

Because SnapStart freezes the memory state of the JVM into a reusable snapshot:
1. Any pseudo-random sequence generated before the checkpoint will be identical across all restored Lambda instances, compromising encryption keys and token uniqueness.
2. Any open TCP connections (e.g. database connections) will be invalid upon restoration because the remote database will have severed the idle connection.  
Developers must use the **CRaC API** (`org.crac.Resource`) to close sockets in `beforeCheckpoint()` and generate fresh keys and connections in `afterRestore()`.
</details>

### Q5: How does Spring Cloud AWS resolve credentials in an ECS or EKS environment?
<details>
<summary>Click to view answer</summary>

By default, Spring Cloud AWS enables the **AWS Default Credentials Provider Chain**. It evaluates credentials in the following order:
1. Environment variables (`AWS_ACCESS_KEY_ID`).
2. Java system properties.
3. Web Identity Token credentials from AWS STS (used by **EKS IRSA**).
4. Container credentials from the ECS metadata endpoint (used by **ECS Task Role**).
5. Instance profile credentials (EC2 metadata service).
This allows the identical Spring Boot JAR to run securely in local dev, ECS, and EKS without changing configuration.
</details>

### Q6: What is Karpenter and why is it popular for scaling Spring Boot on EKS?
<details>
<summary>Click to view answer</summary>

Karpenter is an open-source, high-performance Kubernetes node autoscaler designed for AWS. Unlike the standard Kubernetes Cluster Autoscaler (which scales fixed EC2 Auto Scaling Groups), Karpenter evaluates unschedulable pod requirements directly (CPU, RAM, GPU, architecture) and provisions the exact right-sized EC2 instances in seconds, slashing node provisioning time from 5 minutes down to 45 seconds.
</details>

### Q7: Can a Spring Boot application running on ECS Fargate access AWS Secrets Manager directly?
<details>
<summary>Click to view answer</summary>

Yes, in two ways:
1. **ECS Native Injection**: In the Task Definition `secrets` array, reference the Secrets Manager ARN. ECS fetches the secret and injects it as a plain environment variable into the container.
2. **Spring Cloud AWS Integration**: Add `spring-cloud-aws-starter-secrets-manager` and configure `spring.config.import=aws-secretsmanager:/path`. Spring Boot queries Secrets Manager directly via the IAM Task Role on startup.
</details>

### Q8: What is the maximum execution duration for an AWS Lambda function?
<details>
<summary>Click to view answer</summary>

The maximum execution timeout for an AWS Lambda function is **15 minutes**. For long-running batch jobs, continuous streaming, or persistent WebSocket connections, AWS ECS Fargate or Amazon EKS must be used instead.
</details>

---

## 12. Practice Exercises

### Exercise 1: Configure Spring Cloud AWS Secrets Manager Import (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write an `application.yml` snippet that imports secrets from AWS Secrets Manager path `/config/payment-service/prod` and configures the US-West-2 region.

**Solution**:
```yaml
spring:
  config:
    import:
      - aws-secretsmanager:/config/payment-service/prod
  cloud:
    aws:
      region:
        static: us-west-2
```
</details>

### Exercise 2: Annotated Kubernetes ServiceAccount for EKS IRSA (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a Kubernetes `ServiceAccount` manifest named `billing-sa` annotated to assume IAM role `arn:aws:iam::123456789012:role/BillingS3AccessRole`.

**Solution**:
```yaml
apiVersion: v1
kind: ServiceAccount
metadata:
  name: billing-sa
  namespace: production
  annotations:
    eks.amazonaws.com/role-arn: arn:aws:iam::123456789012:role/BillingS3AccessRole
```
</details>

### Exercise 3: Complete SAM Template for Spring Cloud Function with SnapStart (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write an AWS Serverless Application Model (SAM) `template.yaml` for a Spring Boot 3 Java 21 Lambda function with SnapStart enabled and an HTTP API Gateway event trigger.

**Solution**:
```yaml
AWSTemplateFormatVersion: '2010-09-09'
Transform: AWS::Serverless-2016-10-31
Description: Serverless Spring Boot API with SnapStart

Resources:
  OrderFunction:
    Type: AWS::Serverless::Function
    Properties:
      Handler: org.springframework.cloud.function.adapter.aws.FunctionInvoker::handleRequest
      Runtime: java21
      CodeUri: target/order-lambda-1.0.jar
      MemorySize: 2048
      Timeout: 30
      AutoPublishAlias: live
      SnapStart:
        ApplyOn: PublishedVersions # Enables SnapStart snapshot creation on publish!
      Environment:
        Variables:
          SPRING_PROFILES_ACTIVE: prod
          JAVA_TOOL_OPTIONS: "-XX:+TieredCompilation -XX:TieredStopAtLevel=1"
      Events:
        ApiEvent:
          Type: HttpApi
          Properties:
            Path: /orders
            Method: post
```
</details>

---

## 13. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| AWS Compute Target                          | Key Strengths & Configuration Rule                          |
+---------------------------------------------+-------------------------------------------------------------+
| AWS ECS Fargate                             | Zero-VM serverless containers; use Task Role for IAM access |
| Amazon EKS + IRSA                           | Kubernetes with fine-grained pod IAM roles via OIDC tokens  |
| AWS Lambda SnapStart                        | Sub-200ms cold starts via Firecracker microVM snapshots     |
| Task Execution Role                         | IAM role for ECS agent (ECR image pull, CloudWatch logs)    |
| Task Role                                   | IAM role for Spring Boot business code (S3, SQS, Secrets)   |
| Spring Cloud AWS                            | Auto-imports Secrets Manager via spring.config.import       |
| org.crac.Resource                           | Lifecycle hook for re-seeding keys on SnapStart restore     |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 14. Further Reading
- AWS Official Documentation: Best Practices for Java on AWS Lambda & SnapStart
- Amazon ECS Developer Guide: Task Definition Parameters for Fargate
- Amazon EKS User Guide: IAM Roles for Service Accounts (IRSA)
- Spring Cloud AWS Official Reference Guide (v3.x)