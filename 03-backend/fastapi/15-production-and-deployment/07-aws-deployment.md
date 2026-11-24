---
title: "AWS Deployment for FastAPI"
description: "Architecting enterprise FastAPI deployments on AWS: ECS Fargate, App Runner, EKS, Application Load Balancers, Secrets Manager, and IAM Roles for Service Accounts (IRSA)."
fastapi_version: "0.115+"
python_version: "3.12+"
aws_sdk_version: "1.35+"
free_threading_compatible: true
tested_with: "AWS ECS Fargate, AWS ALB, FastAPI 0.115.6, boto3 1.35.74"
---

# AWS Deployment for FastAPI

## 1. Conceptual Foundation & Mental Model

Deploying FastAPI services on Amazon Web Services (AWS) requires choosing the appropriate compute abstraction based on operational complexity, latency requirements, and scaling needs:
1. **AWS App Runner (PaaS / Fully Managed)**: Lowest operational overhead. Automatically builds containers or pulls from Amazon ECR, terminates TLS, provisions load balancers, and autoscales based on concurrent requests. Ideal for startups and internal tools.
2. **Amazon ECS on AWS Fargate (Serverless Containers)**: Enterprise standard for production microservices. Provides fine-grained AWS VPC networking, IAM task execution roles, integration with Application Load Balancers (ALBs), Secrets Manager integration, and zero EC2 server management.
3. **Amazon EKS (Managed Kubernetes)**: Highest flexibility and control. Preferred when the organization uses standard Kubernetes tooling, multi-cloud deployments, or complex service meshes.

The mental model for AWS-native FastAPI relies on **Zero Static Secrets & Principle of Least Privilege**:
- Compute instances assume IAM roles directly (**ECS Task Roles** or **EKS IRSA**).
- FastAPI uses `boto3` or `aioboto3` to interact with AWS services (S3, SQS, DynamoDB) without long-lived static API keys.
- Application Load Balancers (ALBs) terminate TLS and route requests to private VPC subnets.

```
[ Internet Users ]
       │
     HTTPS (TLS 1.3 / ACM Certificate)
       │
       ▼
[ Application Load Balancer (ALB) ] (Public Subnet)
       ├── Target Group Health Check: /health
       ├── Deregistration Delay: 30s
       └── Forwards traffic across Availability Zones
               │
               ▼ Private Subnet (No Public IP)
   ┌───────────────────────────────────────────────┐
   │             AWS ECS Fargate Tasks             │
   │  Task 1 (AZ-1a)              Task 2 (AZ-1b)   │
   │  - IAM Task Role             - IAM Task Role  │
   │  - Uvicorn / FastAPI         - Uvicorn / FastAPI
   └───────┬───────────────────────────────┬───────┘
           │                               │
           ▼                               ▼
[ Amazon RDS PostgreSQL ]       [ AWS Secrets Manager ]
(Multi-AZ Private Subnet)       (Private VPC Endpoint)
```

---

## 2. Architecture & Internal Mechanics

```
+---------------------------------------------------------------+
|                       AWS VPC (10.0.0.0/16)                   |
|                                                               |
|  +---------------------------------------------------------+  |
|  |                 Public Subnet (10.0.1.0/24)             |  |
|  |  [ Application Load Balancer (ALB) ]                    |  |
|  |  - Internet Gateway Attached                            |  |
|  |  - Security Group: Allow 443 from 0.0.0.0/0             |  |
|  +-----------------------------│---------------------------+  |
|                                │ Forward to Target Group      |
|                                ▼                              |
|  +---------------------------------------------------------+  |
|  |                 Private Subnet (10.0.10.0/24)           |  |
|  |  [ ECS Fargate Container Task ]                         |  |
|  |  - Security Group: Allow 8000 ONLY from ALB SG          |  |
|  |  - Pulls image from Amazon ECR via VPC Endpoint         |  |
|  |  - Retrieves DB secret via AWS Secrets Manager          |  |
|  +-----------------------------│---------------------------+  |
|                                │                              |
|                                ▼                              |
|  +---------------------------------------------------------+  |
|  |            Database Private Subnet (10.0.20.0/24)       |  |
|  |  [ Amazon Aurora Serverless v2 PostgreSQL ]             |  |
|  |  - Security Group: Allow 5432 ONLY from ECS Task SG     |  |
|  +---------------------------------------------------------+  |
+---------------------------------------------------------------+
```

### ECS IAM Roles: Task Execution Role vs Task Role
- **Task Execution Role (`executionRoleArn`)**: Used by the ECS agent infrastructure *before* the container starts. Authorizes pulling container images from private ECR registries and fetching environment secrets from AWS Secrets Manager.
- **Task Role (`taskRoleArn`)**: Assumed by the FastAPI application *inside* the running container. Authorizes application code to call AWS APIs (e.g., uploading files to S3 or reading DynamoDB tables).

---

## 3. Comprehensive Syntax & Core Mechanics

### ECS Fargate Task Definition (`task-definition.json`)
```json
{
  "family": "fastapi-production-task",
  "networkMode": "awsvpc",
  "requiresCompatibilities": ["FARGATE"],
  "cpu": "1024",
  "memory": "2048",
  "executionRoleArn": "arn:aws:iam::123456789012:role/FastAPITaskExecutionRole",
  "taskRoleArn": "arn:aws:iam::123456789012:role/FastAPITaskAppRole",
  "containerDefinitions": [
    {
      "name": "fastapi-api",
      "image": "123456789012.dkr.ecr.us-east-1.amazonaws.com/fastapi-app:latest",
      "essential": true,
      "portMappings": [
        {
          "containerPort": 8000,
          "protocol": "tcp"
        }
      ],
      "environment": [
        { "name": "APP_ENV", "value": "production" },
        { "name": "AWS_REGION", "value": "us-east-1" }
      ],
      "secrets": [
        {
          "name": "DATABASE_URL",
          "valueFrom": "arn:aws:secretsmanager:us-east-1:123456789012:secret:prod/db_url-aBcDeF"
        },
        {
          "name": "JWT_SECRET_KEY",
          "valueFrom": "arn:aws:secretsmanager:us-east-1:123456789012:secret:prod/jwt_key-xYz123"
        }
      ],
      "logConfiguration": {
        "logDriver": "awslogs",
        "options": {
          "awslogs-group": "/ecs/fastapi-production",
          "awslogs-region": "us-east-1",
          "awslogs-stream-prefix": "api"
        }
      },
      "healthCheck": {
        "command": [
          "CMD-SHELL",
          "python -c \"import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/health')\" || exit 1"
        ],
        "interval": 30,
        "timeout": 5,
        "retries": 3,
        "startPeriod": 15
      }
    }
  ]
}
```

---

## 4. Practical Implementation: 5 Production Scenarios

### Scenario 1: Presigned S3 Upload URLs (Direct Browser Uploads)
```python
from fastapi import FastAPI, Depends, HTTPException, status
from pydantic import BaseModel
import boto3
import uuid

app = FastAPI()
s3_client = boto3.client("s3", region_name="us-east-1")
BUCKET_NAME = "enterprise-customer-uploads-prod"

class PresignedUrlRequest(BaseModel):
    filename: str
    content_type: str

class PresignedUrlResponse(BaseModel):
    upload_url: str
    file_key: str

@app.post("/uploads/presigned-url", response_model=PresignedUrlResponse)
async def generate_presigned_upload(payload: PresignedUrlRequest):
    # Offload large uploads completely from FastAPI to S3 directly!
    file_key = f"raw/{uuid.uuid4()}-{payload.filename}"
    
    try:
        url = s3_client.generate_presigned_url(
            ClientMethod="put_object",
            Params={
                "Bucket": BUCKET_NAME,
                "Key": file_key,
                "ContentType": payload.content_type
            },
            ExpiresIn=300 # 5-minute validity window
        )
        return PresignedUrlResponse(upload_url=url, file_key=file_key)
    except Exception as exc:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to generate AWS S3 URL: {str(exc)}"
        )
```

### Scenario 2: Async SQS Event Dispatching with `aioboto3`
```python
from fastapi import FastAPI, status
import aioboto3
import orjson
import uuid

app = FastAPI()
session = aioboto3.Session()
QUEUE_URL = "https://sqs.us-east-1.amazonaws.com/123456789012/order-events.fifo"

@app.post("/orders/dispatch", status_code=status.HTTP_202_ACCEPTED)
async def dispatch_order_event(order_id: str, amount_cents: int):
    event_payload = {
        "event_id": str(uuid.uuid4()),
        "order_id": order_id,
        "amount_cents": amount_cents
    }
    
    async with session.client("sqs", region_name="us-east-1") as sqs:
        await sqs.send_message(
            QueueUrl=QUEUE_URL,
            MessageBody=orjson.dumps(event_payload).decode("utf-8"),
            MessageGroupId="orders",
            MessageDeduplicationId=str(uuid.uuid4())
        )
        
    return {"status": "DISPATCHED"}
```

### Scenario 3: IAM Roles for Service Accounts (IRSA) on Amazon EKS
```yaml
# Kubernetes ServiceAccount configured with AWS IAM Role ARN
apiVersion: v1
kind: ServiceAccount
metadata:
  name: fastapi-serviceaccount
  namespace: production
  annotations:
    eks.amazonaws.com/role-arn: arn:aws:iam::123456789012:role/FastAPIEksDynamoRole
```

### Scenario 4: ALB Ingress Controller Manifest on EKS
```yaml
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: fastapi-alb-ingress
  namespace: production
  annotations:
    kubernetes.io/ingress.class: alb
    alb.ingress.kubernetes.io/scheme: internet-facing
    alb.ingress.kubernetes.io/target-type: ip
    alb.ingress.kubernetes.io/listen-ports: '[{"HTTPS":443}]'
    alb.ingress.kubernetes.io/certificate-arn: arn:aws:acm:us-east-1:123456789012:certificate/12345678-abcd
    alb.ingress.kubernetes.io/healthcheck-path: /health
    alb.ingress.kubernetes.io/healthcheck-interval-seconds: '15'
spec:
  rules:
    - host: api.enterprise.internal
      http:
        paths:
          - path: /
            pathType: Prefix
            backend:
              service:
                name: fastapi-service
                port:
                  number: 80
```

### Scenario 5: Dynamic AWS Secrets Manager Rotation Hook
```python
from fastapi import FastAPI
import boto3
import json

app = FastAPI()

def fetch_secret(secret_id: str) -> dict:
    client = boto3.client("secretsmanager", region_name="us-east-1")
    resp = client.get_secret_value(SecretId=secret_id)
    return json.loads(resp["SecretString"])
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **ALB Connection Drop on Target Deregistration (Deregistration Delay)**:
   By default, AWS ALB sets `deregistration_delay.timeout_seconds` to 300 seconds. If set too short (e.g. 5 seconds) during an ECS deployment, in-flight HTTP requests are terminated with HTTP 502/504 errors. Set `deregistration_delay.timeout_seconds` to 30-60 seconds.

2. **Blocking Boto3 Calls in Async Routes**:
   Standard `boto3` makes synchronous HTTP calls using `urllib3`. Calling `s3_client.put_object()` inside an `async def` route stalls the Uvicorn event loop! Use `aioboto3` or wrap synchronous boto3 calls in `asyncio.to_thread`.

3. **NAT Gateway Data Transfer Costs**:
   Pulling container images from Amazon ECR or fetching secrets through a standard NAT Gateway incurs expensive AWS data processing charges ($0.045/GB). Configure AWS VPC Endpoints (PrivateLink) for ECR, S3, and Secrets Manager to route traffic internally and save costs.

4. **Security Group Protocol Mismatches**:
   If the ECS Task security group allows ingress on port 8000 but the ALB target group is configured to forward to port 80, health checks will fail indefinitely, keeping tasks in `DRAINING` status.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Hardcoding AWS Access Key ID and Secret in code | Critical security breach if repository is compromised | Use IAM Task Roles (ECS) or IRSA (EKS) |
| Streaming large file uploads through FastAPI to S3 | High RAM usage and network bottleneck on API instances | Generate S3 Presigned URLs for direct client upload |
| Calling synchronous `boto3` directly in `async def` | Freezes the event loop, causing severe latency spikes | Use `aioboto3` or `asyncio.to_thread` with boto3 |
| Assigning public IP addresses to ECS tasks | Containers directly exposed to internet attacks | Place containers in private subnets behind an ALB |
| Setting ALB health check to heavy DB queries | High database load from thousands of periodic health pings | Keep health check endpoint lightweight (`/health`) |
| Omitting VPC Endpoints for AWS services | High NAT Gateway data fees on image pulls and API calls | Deploy VPC Endpoints for ECR, S3, and Secrets Manager |

---

## 7. Performance & Scalability Considerations

- **Graviton (ARM64) Instances**: Deploying ECS Fargate tasks on ARM64 (`"runtimePlatform": {"cpuArchitecture": "ARM64"}`) delivers up to 40% better price-performance compared to x86_64.
- **ALB Idle Timeout**: Default ALB idle timeout is 60 seconds. Ensure Uvicorn's keep-alive timeout is configured higher (e.g., 65 seconds) to prevent race conditions where Uvicorn closes a connection just as the ALB forwards a request.

---

## 8. Security & OWASP Guardrails

- **IMDSv2 Enforcement**: Require Instance Metadata Service Version 2 (IMDSv2) on all underlying EC2 nodes (`HttpTokens=required`) to prevent SSRF vulnerabilities from extracting IAM credentials.
- **KMS Secret Encryption**: Encrypt all secrets in AWS Secrets Manager using customer-managed AWS KMS keys (CMK) with strict key access policies.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from moto import mock_aws
import boto3

@pytest.fixture
def mock_s3():
    with mock_aws():
        client = boto3.client("s3", region_name="us-east-1")
        client.create_bucket(Bucket="test-bucket")
        yield client

def test_s3_presigned_url_generation(mock_s3):
    url = mock_s3.generate_presigned_url(
        ClientMethod="put_object",
        Params={"Bucket": "test-bucket", "Key": "test.txt"},
        ExpiresIn=300
    )
    assert "test-bucket" in url
    assert "AWSAccessKeyId" in url
```

---

## 10. Real-World Production Incident

**Incident**: An enterprise media platform on AWS ECS Fargate suffered massive 502 Bad Gateway errors during a major deployment, with 35% of all user upload requests failing.

**Root Cause**: The engineering team set `deregistration_delay.timeout_seconds = 5` on their ALB Target Group to speed up deployments. When ECS initiated a rolling update, old Fargate tasks were terminated after only 5 seconds while actively handling 30-second user video uploads, dropping hundreds of in-flight TCP connections.

**Resolution**: Increased Target Group `deregistration_delay.timeout_seconds` to 60 seconds and configured Uvicorn's graceful timeout to match. Deployments completed cleanly without a single dropped connection.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the difference between an ECS Task Execution Role and an ECS Task Role?</summary>

- **Task Execution Role**: Used by the AWS ECS container agent to manage container setup. It grants permissions to pull images from Amazon ECR and read encrypted secret values from AWS Secrets Manager or SSM Parameter Store before container start.
- **Task Role**: Assumed directly by the application code running inside the container via the container credentials endpoint. It authorizes application calls to AWS services (e.g., S3, DynamoDB, SQS).
</details>

<details>
<summary>2. Why should file uploads be handled using S3 Presigned URLs rather than proxying through FastAPI?</summary>

Proxying large file uploads through FastAPI forces the application server to buffer bytes in RAM, consumes Uvicorn worker connections, and saturates container network bandwidth. Generating an S3 Presigned URL allows the client to upload the file directly to S3 storage over high-speed AWS edge networks. FastAPI only handles a lightweight JSON metadata request, scaling to millions of concurrent uploads without memory bloat.
</details>

<details>
<summary>3. Why is calling synchronous <code>boto3</code> directly inside <code>async def</code> dangerous?</summary>

`boto3` is a synchronous library built on top of `urllib3`. When you invoke a boto3 call (like `s3.get_object()`) inside an `async def` route, the call blocks the main thread's `asyncio` event loop while waiting for the AWS HTTP response. During this block, no other concurrent request coroutines can run on that worker. Use `aioboto3` or `asyncio.to_thread` to execute boto3 calls without freezing the event loop.
</details>

<details>
<summary>4. What are the cost and latency benefits of AWS VPC Endpoints (PrivateLink)?</summary>

VPC Endpoints allow private subnets to communicate with AWS services (ECR, S3, Secrets Manager, SQS) over the internal AWS backbone network without traversing a public NAT Gateway. This eliminates NAT Gateway data processing fees ($0.045/GB), lowers network latency, and enhances security by ensuring traffic never leaves the private AWS network.
</details>

<details>
<summary>5. How does IAM Roles for Service Accounts (IRSA) work in Amazon EKS?</summary>

IRSA integrates Kubernetes ServiceAccounts with AWS IAM. EKS acts as an OIDC identity provider. When a pod references a ServiceAccount annotated with an IAM role ARN, the EKS mutating webhook injects an OIDC token and AWS credentials environment variables into the pod. The AWS SDK reads this token and assumes the IAM role via `AssumeRoleWithWebIdentity`, eliminating the need for shared node IAM credentials.
</details>

<details>
<summary>6. How should Uvicorn keep-alive timeout be configured relative to ALB idle timeout?</summary>

The ALB idle timeout should always be slightly **shorter** than Uvicorn's keep-alive timeout. If the ALB idle timeout is 60 seconds and Uvicorn's keep-alive timeout is 60 seconds, a race condition occurs where Uvicorn closes a connection just as the ALB forwards a new request, resulting in intermittent HTTP 502 Bad Gateway errors. Setting Uvicorn's keep-alive timeout to 65 seconds prevents this race.
</details>

<details>
<summary>7. What does the ALB Deregistration Delay setting control?</summary>

Deregistration Delay (also known as connection draining) is the duration an ALB waits before forcefully terminating connections to a target instance being removed. During this window, the ALB stops sending new requests to the target and allows in-flight requests to complete cleanly before the target is stopped.
</details>

<details>
<summary>8. How do you deploy FastAPI onto AWS Graviton processors?</summary>

Build container images using Docker Buildx targeting `linux/arm64`:
`docker buildx build --platform linux/arm64 -t <ecr-repo>:latest .`
In the ECS Task Definition, configure `"runtimePlatform": {"cpuArchitecture": "ARM64", "operatingSystemFamily": "LINUX"}`. Graviton processors deliver ~40% better price-performance over standard x86 architectures.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Generate S3 Presigned Download URL
**Task**: Write an async FastAPI route `/download/{file_id}` that returns a presigned S3 download URL valid for 15 minutes.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI
import boto3

app = FastAPI()
s3 = boto3.client("s3", region_name="us-east-1")

def get_presigned_download(key: str) -> str:
    return s3.generate_presigned_url(
        "get_object",
        Params={"Bucket": "my-secure-bucket", "Key": key},
        ExpiresIn=900
    )

@app.get("/download/{file_id}")
async def download_file(file_id: str):
    url = await asyncio.to_thread(get_presigned_download, f"files/{file_id}.pdf")
    return {"download_url": url}
```
</details>

### Exercise 2 (Easy): AWS ECS Container Healthcheck Definition
**Task**: Write the JSON health check block for an ECS task definition that pings `/health` using python's `urllib`.
<details>
<summary>Solution</summary>

```json
"healthCheck": {
  "command": [
    "CMD-SHELL",
    "python -c \"import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/health')\" || exit 1"
  ],
  "interval": 15,
  "timeout": 3,
  "retries": 2,
  "startPeriod": 10
}
```
</details>

### Exercise 3 (Medium): Asynchronous DynamoDB Item Read with `aioboto3`
**Task**: Implement a FastAPI endpoint that reads an item from Amazon DynamoDB asynchronously using `aioboto3`.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, HTTPException, status
import aioboto3

app = FastAPI()
session = aioboto3.Session()

@app.get("/items/{item_id}")
async def get_dynamo_item(item_id: str):
    async with session.client("dynamodb", region_name="us-east-1") as ddb:
        resp = await ddb.get_item(
            TableName="Inventory",
            Key={"item_id": {"S": item_id}}
        )
        if "Item" not in resp:
            raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Item not found")
        return {"id": item_id, "name": resp["Item"]["name"]["S"]}
```
</details>

### Exercise 4 (Medium): ALB Target Group Deregistration Configuration
**Task**: Write the Terraform or CloudFormation snippet to configure an AWS ALB Target Group with a 30-second deregistration delay.
<details>
<summary>Solution</summary>

```hcl
# Terraform HCL
resource "aws_lb_target_group" "fastapi_tg" {
  name        = "fastapi-production-tg"
  port        = 8000
  protocol    = "HTTP"
  vpc_id      = var.vpc_id
  target_type = "ip"

  deregistration_delay = 30

  health_check {
    path                = "/health"
    protocol            = "HTTP"
    matcher             = "200"
    interval            = 15
    timeout             = 3
    healthy_threshold   = 2
    unhealthy_threshold = 2
  }
}
```
</details>

### Exercise 5 (Hard): Async S3 Multipart Upload Session Orchestrator
**Task**: Write an endpoint that initializes an S3 Multipart Upload, returns upload IDs and presigned URLs for 5 chunks, and provides a completion endpoint.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI
import boto3

app = FastAPI()
s3 = boto3.client("s3", region_name="us-east-1")
BUCKET = "large-files-bucket"

@app.post("/multipart/start")
async def start_multipart(filename: str):
    key = f"uploads/{filename}"
    res = await asyncio.to_thread(s3.create_multipart_upload, Bucket=BUCKET, Key=key)
    upload_id = res["UploadId"]

    # Generate 5 presigned part URLs
    parts = []
    for part_num in range(1, 6):
        url = await asyncio.to_thread(
            s3.generate_presigned_url,
            "upload_part",
            Params={"Bucket": BUCKET, "Key": key, "UploadId": upload_id, "PartNumber": part_num},
            ExpiresIn=3600
        )
        parts.append({"part_number": part_num, "upload_url": url})

    return {"upload_id": upload_id, "key": key, "parts": parts}
```
</details>

### Exercise 6 (Hard): Automated ECS Task Rolling Deploy Script
**Task**: Write a Python script using `boto3` that triggers an ECS service deployment, registers a new task definition revision, and polls until deployment status is `COMPLETED`.
<details>
<summary>Solution</summary>

```python
import time
import boto3

def deploy_ecs_service(cluster_name: str, service_name: str):
    ecs = boto3.client("ecs", region_name="us-east-1")
    
    print("Triggering new deployment with latest task definition...")
    ecs.update_service(
        cluster=cluster_name,
        service=service_name,
        forceNewDeployment=True
    )
    
    waiter = ecs.get_waiter("services_stable")
    print("Waiting for deployment to achieve steady state...")
    waiter.wait(
        cluster=cluster_name,
        services=[service_name],
        WaiterConfig={"Delay": 15, "MaxAttempts": 40}
    )
    print("Deployment completed successfully!")

if __name__ == "__main__":
    deploy_ecs_service("production-cluster", "fastapi-service")
```
</details>
