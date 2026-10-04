# AWS Cheatsheet

A one-page-ish lookup for "which service for what", side-by-side comparisons, the CLI commands you'll actually type, and a map from symptoms to likely causes. It deliberately repeats nothing explanatory. Follow the links for the *why*.

> **Numbers are defaults and quotas at the time of writing.** AWS changes limits and pricing often. For anything that matters, confirm in [Service Quotas](https://console.aws.amazon.com/servicequotas) and the service docs.

---

## 1. Which service for what

| I need to… | Use | Note |
|---|---|---|
| Run a VM I fully control | **EC2** | [EC2](../02-compute/01-ec2.md) |
| Run code on events, pay per use | **Lambda** | [Lambda](../02-compute/02-lambda.md) |
| Run a container, no servers to manage | **ECS on Fargate** (or ECS Express Mode for simple web apps) | [ECS/Fargate](../02-compute/03-ecs-fargate.md) |
| Run Kubernetes | **EKS** | Not covered; more complexity than ECS |
| Store files/objects/backups | **S3** | [S3](../03-storage-and-databases/01-s3.md) |
| Key-value/NoSQL at any scale | **DynamoDB** | [DynamoDB](../03-storage-and-databases/02-dynamodb.md) |
| Relational DB (joins, transactions) | **RDS** (PostgreSQL/MySQL…) or **Aurora** | [RDS/Aurora](../03-storage-and-databases/03-rds-and-aurora.md) |
| Shared POSIX file system | **EFS** | Not covered |
| Block disk for an EC2 instance | **EBS** (`gp3` default) | [EC2](../02-compute/01-ec2.md) |
| Isolated network | **VPC** | [VPC](../04-networking/01-vpc.md) |
| DNS | **Route 53** | [Route 53 and CloudFront](../04-networking/02-route53-and-cloudfront.md) |
| CDN, edge TLS, WAF in front | **CloudFront** | same |
| Distribute HTTP traffic to servers/containers | **ALB** | [Load Balancing](../04-networking/03-load-balancing-and-api-gateway.md) |
| TCP/UDP, static IPs, extreme throughput | **NLB** | same |
| Managed API front door for Lambda | **API Gateway** (HTTP API by default) | same |
| Decouple work (one consumer per message) | **SQS** | [SQS](../05-app-services/01-sqs.md) |
| Fan out one message to many | **SNS** (→ SQS) | [SNS/EventBridge](../05-app-services/02-sns-and-eventbridge.md) |
| Route events by content / react to AWS events / schedule | **EventBridge** (+ Scheduler) | same |
| Send email | **SES** | [SES](../05-app-services/03-ses-email.md) |
| Sign-up/sign-in for your app's users | **Cognito user pools** | [Cognito](../05-app-services/04-cognito.md) |
| Call foundation models | **Bedrock** | [Bedrock](../05-app-services/05-bedrock.md) |
| Store a secret (rotation) | **Secrets Manager** | [Security](../06-operations/03-security-and-secrets.md) |
| Store config / cheap secret | **SSM Parameter Store** | same |
| Encrypt with managed keys | **KMS** | same |
| Define infrastructure in code | **CDK** (or SAM / CloudFormation) | [IaC](../06-operations/01-infrastructure-as-code.md) |
| Deploy from GitHub without stored keys | **OIDC + IAM role** | [CI/CD](../06-operations/02-ci-cd-to-aws.md) |
| Logs, metrics, alarms | **CloudWatch** | [Observability](../06-operations/04-observability.md) |
| Distributed tracing | **OpenTelemetry → X-Ray / Application Signals** | same |
| Audit "who called which API" | **CloudTrail** | same |
| Human access to AWS | **IAM Identity Center** | [IAM](../01-foundations/03-iam.md) |
| Backups across services | **AWS Backup** | [Reliability and Cost](../06-operations/05-reliability-and-cost.md) |

---

## 2. Comparison tables

### Compute

| | **EC2** | **Lambda** | **ECS + Fargate** |
|---|---|---|---|
| Unit | VM | Function invocation | Container task |
| You manage | OS, patching, scaling | Code only | Image, task config |
| Scaling | Auto Scaling group | Automatic, to zero | Service autoscaling |
| Max run time | Unlimited | **15 min** | Unlimited |
| Idle cost | Pay while running | ~0 | Pay while tasks run |
| Best for | Control, special hardware, steady fleets | Spiky/event-driven | Existing containers, steady services, long jobs |

### Storage and databases

| | **S3** | **DynamoDB** | **RDS** | **Aurora** |
|---|---|---|---|---|
| Model | Objects | Key-value/document | Relational (managed engine) | Relational (AWS engine, shared storage) |
| Query | By key / list | By key; Query/Scan; GSIs | Full SQL | Full SQL |
| Scaling | Effectively unlimited | Horizontal, automatic | Vertical + read replicas | Vertical + up to many readers; Serverless v2 |
| Max size | Object 5 TB | **Item 400 KB** | Instance storage | Cluster volume grows automatically |
| Use for | Files, assets, data lake | Known access patterns, huge scale | Joins, transactions, ad-hoc SQL | Same, with faster failover/more readers |

### Messaging

| | **SQS** | **SNS** | **EventBridge** |
|---|---|---|---|
| Model | Queue (pull) | Pub/sub (push) | Event bus + rules |
| Consumers per message | **One** | **Many** | Many (per matching rule) |
| Ordering | FIFO queues only | FIFO topics only | Not guaranteed |
| Strength | Buffering, backpressure, DLQ | Fan-out | Content routing, AWS/SaaS events, schedules |
| Delivery | At-least-once (Standard) | At-least-once | At-least-once |

### Traffic entry points

| | **CloudFront** | **ALB** | **NLB** | **API Gateway (HTTP)** | **Lambda Function URL** |
|---|---|---|---|---|---|
| Layer | CDN / L7 | L7 | L4 | L7 managed API | L7, single function |
| Typical backend | S3, ALB, API GW | Containers/EC2/Lambda | TCP/UDP services | Lambda/HTTP | One Lambda |
| Extras | Caching, WAF, edge TLS | Path/host routing | Static IPs | Authorizers, throttling, CORS | Simplest possible |
| Timeout note | Origin timeouts apply | Idle timeout 60 s default | n/a | **~29 s** integration default | Lambda's own timeout |

### Secrets and config

| | **Secrets Manager** | **Parameter Store** |
|---|---|---|
| Rotation | **Built-in** | No |
| Cost | Per secret + per call | Standard params free |
| Size | 64 KB | 4 KB (standard) |
| Use for | DB creds, rotating API keys | Config, cheap/static secrets |

### Infrastructure as code

| | **CDK** | **SAM** | **CloudFormation** | **Terraform/OpenTofu** |
|---|---|---|---|---|
| Authoring | TypeScript (etc.) | Short YAML | JSON/YAML | HCL |
| Strength | Real code, grants, reuse | Serverless + `sam local` | The engine; always readable | Multi-cloud |
| State | CloudFormation | CloudFormation | CloudFormation | Your state file |

### IAM identities

| | Use for | Credentials |
|---|---|---|
| **Role** | Services, CI, cross-account, humans (via Identity Center) | Temporary |
| **IAM user** | Legacy / learning | Long-lived: avoid |
| **Root** | Account-level tasks only | MFA, never daily |
| **Cognito user pool** | **Your app's** end users, not AWS access | JWT tokens |

---

## 3. Things worth remembering (defaults/quotas: verify)

| Fact | Value |
|---|---|
| Lambda max timeout / memory | 15 min / 10 GB (CPU scales with memory) |
| Lambda container image size | Up to 10 GB |
| S3 object / single PUT | 5 TB / 5 GB (use multipart above) |
| DynamoDB item size | 400 KB |
| DynamoDB RCU / WCU | 4 KB strongly consistent read / 1 KB write per unit |
| SQS visibility timeout | Default 30 s, max 12 h |
| SQS retention | 1 min to 14 days (default 4 days) |
| SQS/SNS/EventBridge message | SNS 256 KB; EventBridge entry 256 KB; SQS larger (1 MiB at time of writing) |
| API Gateway integration timeout | ~29 s default |
| ALB idle timeout | 60 s default |
| Subnet reserved IPs | 5 per subnet |
| VPC CIDR size | `/16` to `/28` |
| Secrets Manager secret size | 64 KB |
| CloudFormation stack | 500 resources |
| Cognito access/ID token default | 1 hour; refresh 30 days |
| KMS key deletion wait | 7 to 30 days |
| Stopped RDS instance | Auto-restarts after 7 days |
| CloudTrail default history | 90 days (not an audit log; create a trail) |
| CloudFront ACM cert region | **`us-east-1`** |
| CloudFront-origin transfer from AWS origins | Free to CloudFront |

---

## 4. Resource naming

```text
ARN:  arn:aws:<service>:<region>:<account-id>:<resource>

arn:aws:s3:::my-bucket                         (bucket; global, no region/account)
arn:aws:s3:::my-bucket/*                       (objects: note the /*)
arn:aws:lambda:ap-south-1:111122223333:function:my-fn
arn:aws:dynamodb:ap-south-1:111122223333:table/Orders
arn:aws:sqs:ap-south-1:111122223333:orders
arn:aws:iam::111122223333:role/MyRole          (IAM is global: no region)
```

Region examples: `ap-south-1` Mumbai, `ap-south-2` Hyderabad, `us-east-1` N. Virginia, `eu-west-1` Ireland.

---

## 5. CLI commands you'll actually use

### Identity and config
```bash
aws sts get-caller-identity                 # who am I? (run before anything destructive)
aws configure list                          # resolved profile/region/credential source
aws configure list-profiles
aws configure sso && aws sso login --profile dev
export AWS_PROFILE=dev AWS_REGION=ap-south-1
aws ec2 describe-regions --output table
```

### Output control
```bash
--query 'Reservations[].Instances[].[InstanceId,State.Name]' --output table
--output json|table|text|yaml   --no-cli-pager   --debug   --dry-run (many EC2 ops)
```

### S3
```bash
aws s3 ls s3://bucket/prefix/
aws s3 cp file s3://bucket/key     aws s3 sync ./dist s3://bucket/site --dryrun
aws s3 presign s3://bucket/key --expires-in 300
aws s3api get-bucket-policy --bucket bucket
```

### EC2 / VPC
```bash
aws ec2 describe-instances --filters Name=instance-state-name,Values=running
aws ec2 describe-security-groups --group-ids sg-xxxx
aws ec2 describe-subnets --filters Name=vpc-id,Values=vpc-xxxx
aws ssm start-session --target i-xxxx            # shell without SSH
aws ec2 get-console-output --instance-id i-xxxx --output text
```

### Lambda / logs
```bash
aws lambda invoke --function-name fn --payload '{}' --cli-binary-format raw-in-base64-out out.json
aws lambda list-functions --query 'Functions[].[FunctionName,Runtime]' --output table
aws logs tail /aws/lambda/fn --follow
aws logs start-query --log-group-name /aws/lambda/fn --start-time $(date -d '1 hour ago' +%s) \
  --end-time $(date +%s) --query-string 'fields @timestamp,@message | filter @message like /ERROR/'
```

### ECS
```bash
aws ecs list-services --cluster prod
aws ecs describe-services --cluster prod --services web --query 'services[0].events[:5]'
aws ecs list-tasks --cluster prod --desired-status STOPPED
aws ecs describe-tasks --cluster prod --tasks <arn> --query 'tasks[0].[stoppedReason,containers[].reason]'
aws ecs execute-command --cluster prod --task <id> --container app --interactive --command /bin/sh
aws ecs update-service --cluster prod --service web --force-new-deployment
```

### Data stores and messaging
```bash
aws dynamodb get-item --table-name T --key '{"pk":{"S":"a"},"sk":{"S":"b"}}'
aws dynamodb query --table-name T --key-condition-expression 'pk = :p' --expression-attribute-values '{":p":{"S":"a"}}'
aws rds describe-db-instances --query 'DBInstances[].[DBInstanceIdentifier,DBInstanceStatus]' --output table
aws sqs get-queue-attributes --queue-url URL --attribute-names All
aws sqs receive-message --queue-url URL --visibility-timeout 0     # peek without consuming
aws events test-event-pattern --event-pattern file://p.json --event file://e.json
aws sesv2 get-account                                              # sandbox status + quotas
```

### Secrets and parameters
```bash
aws secretsmanager get-secret-value --secret-id prod/db --query SecretString --output text
aws ssm get-parameter --name /app/prod/key --with-decryption --query Parameter.Value --output text
aws ssm get-parameters-by-path --path /app/prod/ --recursive --with-decryption
```

### IAM debugging
```bash
aws iam list-attached-role-policies --role-name R
aws iam simulate-principal-policy --policy-source-arn <role-arn> --action-names s3:GetObject --resource-arns arn:aws:s3:::b/k
aws sts decode-authorization-message --encoded-message <blob>
aws sts assume-role --role-arn <arn> --role-session-name test
```

### CloudFormation / CDK
```bash
cdk bootstrap aws://ACCOUNT/REGION      # once per account+region
cdk synth | cdk diff | cdk deploy | cdk destroy
cdk diagnose                            # root cause of a failed deploy
aws cloudformation describe-stack-events --stack-name S \
  --query 'StackEvents[?contains(ResourceStatus,`FAILED`)].[LogicalResourceId,ResourceStatusReason]' --output table
sam build && sam deploy --guided        # SAM
```

### CloudFront / Route 53 / Cognito / Bedrock
```bash
aws cloudfront create-invalidation --distribution-id EXXX --paths "/index.html"
aws route53 list-resource-record-sets --hosted-zone-id ZXXX
dig +short app.example.com ; dig @<ns-of-zone> app.example.com
aws bedrock list-foundation-models --region us-east-1
aws bedrock list-inference-profiles --region us-east-1
```

---

## 6. Symptom → likely cause

| Symptom | First things to check |
|---|---|
| **Connection timed out** | Security group / route table / NACL / no NAT or IGW ([VPC](../04-networking/01-vpc.md)) |
| **Connection refused** | Nothing listening on that port/interface (`127.0.0.1` vs `0.0.0.0`) |
| `AccessDenied` (anywhere) | `get-caller-identity` (right account/profile?) → explicit deny/SCP → resource policy → **KMS** ([IAM](../01-foundations/03-iam.md)) |
| S3 `AccessDenied` with a correct policy | `bucket` vs `bucket/*` ARN; KMS key permissions; Block Public Access |
| Resources "disappeared" | Wrong **region** in console/CLI |
| Lambda `Task timed out` | Timeout too low; slow downstream; in VPC without NAT/endpoints |
| Lambda messages processed twice | At-least-once: be **idempotent**; SQS visibility < processing time |
| SQS growing | Consumers failing/throttled; check DLQ and `ApproximateAgeOfOldestMessage` |
| SNS/EventBridge delivers nothing | Target's **resource policy**, filter/pattern mismatch, unconfirmed subscription |
| ALB **502 / 503 / 504** | Bad response / no healthy targets / timeout ([LB note](../04-networking/03-load-balancing-and-api-gateway.md)) |
| API Gateway 502 with Lambda | Malformed proxy response, or the function errored |
| API Gateway "Missing Authentication Token" | Wrong path/method/stage; REST API not redeployed |
| ECS `CannotPullContainerError` | No route to ECR (NAT/endpoints), wrong image, execution role |
| ECS service flapping | ALB health check path/port; slow startup; app on `127.0.0.1` |
| CloudFront 403 on custom domain | Alternate domain name missing, or cert doesn't cover it |
| Stale content on CloudFront | Cache TTL; fingerprint assets; invalidate `index.html` |
| Cert not selectable for CloudFront | Not in `us-east-1` / not validated |
| DB `too many connections` | Pooling; **RDS Proxy**; Lambda concurrency |
| SES `MessageRejected` | Sandbox (unverified recipient) or unverified sender identity in *this region* |
| Cognito `redirect_mismatch` | Callback URL must match the app client exactly |
| Bedrock "on-demand throughput isn't supported" | Use an **inference profile** ID; allow profile + model ARNs in IAM |
| `cdk deploy`: "must be bootstrapped" | `cdk bootstrap` for that account/region |
| `sts:AssumeRoleWithWebIdentity` not authorized | OIDC `sub`/`aud` mismatch, missing `id-token: write`, wrong account |
| Stack stuck `UPDATE_ROLLBACK_FAILED` | Find first failed event; *Continue update rollback* |
| Surprise bill | Cost Explorer by service → usage type; NAT, idle LBs, logs, data transfer ([Accounts](../01-foundations/02-accounts-regions-and-billing.md)) |

---

## 7. Decision shortcuts

```text
Compute?    event-driven & short ........ Lambda
            container ................... Fargate
            need the box ................ EC2

Database?   known key access, huge scale  DynamoDB
            joins/SQL/transactions ...... RDS / Aurora
            files/blobs ................. S3 (pointer in DB)

Async?      one worker per message ...... SQS
            many subscribers ............ SNS → SQS
            route by content / AWS events EventBridge

Front door? static + cache + WAF ........ CloudFront (+ S3 via OAC)
            services you run ............ ALB
            APIs you expose (Lambda) .... API Gateway HTTP API

Auth?       app users ................... Cognito
            humans on AWS ............... IAM Identity Center
            CI/CD ....................... OIDC role
            services .................... IAM role

Secrets?    rotating creds .............. Secrets Manager
            config / static ............. Parameter Store

Reliability baseline:  2+ AZs, timeouts + backoff, DLQs, tested restores, alarms on symptoms
Cost baseline:         budget + anomaly alerts, tags, log retention, delete idle, right-size → then commit
```

---

## 8. One-line rules

- **Default deny; explicit deny wins.** Roles and temporary credentials beat keys.
- **Multi-AZ ≠ backup ≠ multi-region.**
- **At-least-once delivery → idempotent consumers.** Always add a **DLQ**.
- **Query, don't Scan.** Design DynamoDB from access patterns.
- **Create SDK clients outside the Lambda handler.**
- **Private subnets for data; reference security groups, not CIDRs.**
- **Read `cdk diff` before every deploy;** `RETAIN` stateful resources.
- **Budgets alert, they don't cap.** Tag everything.
- **Backups don't count until restored.**
- **Instrument with OpenTelemetry;** set log **retention**.