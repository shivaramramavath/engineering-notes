---
topic: "AWS CLI Commands Production Cheatsheet"
module: "10-cheatsheets"
track: "aws"
difficulty: "reference"
last-tested: "2026-11"
tags:
  - "cheatsheet"
  - "aws-cli"
  - "jmespath"
  - "debugging"
  - "quick-reference"
---

# AWS CLI Commands Production Cheatsheet

A high-density reference card of essential AWS CLI commands for day-to-day cloud operations, debugging, and infrastructure management.

---

## 1. Identity, Authentication & Profile Configuration

```bash
# Verify current active identity and account ID (Always run before destructive operations!)
aws sts get-caller-identity

# List active configuration, profiles, and credential sources
aws configure list
aws configure list-profiles

# Login via AWS IAM Identity Center (SSO)
aws configure sso
aws sso login --profile production

# Set shell session environment variables
export AWS_PROFILE=production
export AWS_REGION=us-east-1

# Decode encoded authorization failure messages (requires sts:DecodeAuthorizationMessage)
aws sts decode-authorization-message --encoded-message <base64-encoded-error-blob> --query DecodedMessage --output text | jq .
```

---

## 2. Advanced JMESPath `--query` & Output Formatting

```bash
# Output formats: table, json, text, yaml
aws ec2 describe-instances --output table

# Query specific nested fields: Extract InstanceId and State for all running instances
aws ec2 describe-instances \
    --filters "Name=instance-state-name,Values=running" \
    --query "Reservations[].Instances[].[InstanceId,PrivateIpAddress,Tags[?Key=='Name'].Value|[0]]" \
    --output table

# Sort output: List S3 buckets sorted by creation date
aws s3api list-buckets --query "sort_by(Buckets, &CreationDate)[].[Name,CreationDate]" --output table

# Disable interactive pager (prevents less/more blocking in automation scripts)
export AWS_PAGER=""
# Or inline:
aws s3 ls --no-cli-pager
```

---

## 3. Amazon EC2, Systems Manager (SSM) & VPC

```bash
# Start an interactive shell session on an EC2 instance without SSH keys or open port 22
aws ssm start-session --target i-0123456789abcdef0

# Fetch latest system console log from an unbootable instance
aws ec2 get-console-output --instance-id i-0123456789abcdef0 --output text

# List all security groups associated with a VPC and their rules
aws ec2 describe-security-groups --filters "Name=vpc-id,Values=vpc-0123456789abcdef0" --output table

# Find all ENIs attached to a specific security group
aws ec2 describe-network-interfaces --filters "Name=group-id,Values=sg-0123456789abcdef0" --query "NetworkInterfaces[].[NetworkInterfaceId,PrivateIpAddress,Description]" --output table

# Describe VPC Subnets and remaining available IP addresses
aws ec2 describe-subnets --filters "Name=vpc-id,Values=vpc-0123456789abcdef0" --query "Subnets[].[SubnetId,AvailabilityZone,CidrBlock,AvailableIpAddressCount]" --output table
```

---

## 4. Amazon S3 & CloudFront Operations

```bash
# High-speed parallel copy and sync
aws s3 cp local-file.tar.gz s3://my-bucket/backups/
aws s3 sync ./dist s3://my-website-bucket/ --delete --dryrun

# Generate a pre-signed URL valid for 15 minutes (900 seconds)
aws s3 presign s3://my-bucket/reports/financials.pdf --expires-in 900

# Inspect S3 Bucket Policy
aws s3api get-bucket-policy --bucket my-bucket --query Policy --output text | jq .

# Invalidate CloudFront CDN cache for index.html and root
aws cloudfront create-invalidation --distribution-id E1234567890ABC --paths "/index.html" "/"

# Check CloudFront distribution status
aws cloudfront get-invalidation --distribution-id E1234567890ABC --id <invalidation-id>
```

---

## 5. AWS Lambda & CloudWatch Logs

```bash
# Invoke a Lambda function synchronously with JSON payload and format output
aws lambda invoke \
    --function-name order-processor \
    --cli-binary-format raw-in-base64-out \
    --payload '{"orderId": "12345", "amount": 99.00}' \
    response.json && cat response.json

# Tail real-time CloudWatch Logs in your terminal
aws logs tail /aws/lambda/order-processor --follow

# Search logs for ERROR or EXCEPTION within the last 1 hour
aws logs tail /aws/lambda/order-processor --since 1h --filter-pattern "ERROR"

# Execute a CloudWatch Logs Insights query via CLI
QUERY_ID=$(aws logs start-query \
    --log-group-name "/aws/lambda/order-processor" \
    --start-time $(date -u -d '1 hour ago' +%s) \
    --end-time $(date -u +%s) \
    --query-string 'fields @timestamp, @message | filter @message like /Exception/ | limit 10' \
    --query queryId --output text)

aws logs get-query-results --query-id "$QUERY_ID"
```

---

## 6. Amazon ECS (Elastic Container Service)

```bash
# List all running tasks in a cluster
aws ecs list-tasks --cluster production-cluster --desired-status RUNNING

# Describe task stop reasons and container exit codes (Triage crashing tasks)
aws ecs describe-tasks \
    --cluster production-cluster \
    --tasks arn:aws:ecs:us-east-1:123456789012:task/production-cluster/abc12345 \
    --query "tasks[0].[stoppedReason,containers[0].[exitCode,reason]]"

# Force a zero-downtime rolling deployment of an ECS Service
aws ecs update-service --cluster production-cluster --service api-service --force-new-deployment

# Execute interactive shell command inside an active Fargate container (ECS Exec)
aws ecs execute-command \
    --cluster production-cluster \
    --task <task-arn> \
    --container api-container \
    --interactive \
    --command "/bin/sh"
```

---

## 7. Amazon DynamoDB & Relational Databases

```bash
# Get Item from DynamoDB by Partition Key and Sort Key
aws dynamodb get-item \
    --table-name Users \
    --key '{"PK": {"S": "USER#1001"}, "SK": {"S": "PROFILE"}}'

# Query DynamoDB with Expression Attribute Values
aws dynamodb query \
    --table-name Orders \
    --key-condition-expression "PK = :pk" \
    --expression-attribute-values '{":pk": {"S": "CUSTOMER#44"}}'

# Check RDS instance status and endpoints
aws rds describe-db-instances \
    --query "DBInstances[].[DBInstanceIdentifier,DBInstanceStatus,Endpoint.Address,Engine,DBInstanceClass]" \
    --output table

# Trigger immediate manual failover of an Amazon Aurora DB Cluster
aws rds failover-db-cluster --db-cluster-identifier production-aurora-cluster
```

---

## 8. Asynchronous Messaging: SQS, SNS & EventBridge

```bash
# Peek at a message in SQS without consuming it or changing visibility
aws sqs receive-message --queue-url "https://sqs.us-east-1.amazonaws.com/123456789012/my-queue" --visibility-timeout 0

# Get Queue Attributes (Queue depth and in-flight counts)
aws sqs get-queue-attributes \
    --queue-url "https://sqs.us-east-1.amazonaws.com/123456789012/my-queue" \
    --attribute-names ApproximateNumberOfMessages ApproximateNumberOfMessagesNotVisible ApproximateNumberOfMessagesDelayed

# Publish a test event to an EventBridge Custom Bus
aws events put-events --entries '[{
  "EventBusName": "ecommerce-bus",
  "Source": "custom.checkout",
  "DetailType": "OrderPlaced",
  "Detail": "{\"orderId\": \"991\", \"amount\": 49.99}"
}]'

# Test an EventBridge Pattern against a sample event JSON file
aws events test-event-pattern --event-pattern file://pattern.json --event file://event.json
```

---

## 9. AWS Secrets Manager & SSM Parameter Store

```bash
# Fetch and print decrypted secret string
aws secretsmanager get-secret-value --secret-id "production/database/credentials" --query SecretString --output text | jq .

# Trigger immediate out-of-band secret rotation
aws secretsmanager rotate-secret --secret-id "production/database/credentials"

# Get decrypted SecureString parameter from SSM
aws ssm get-parameter --name "/app/prod/api-key" --with-decryption --query Parameter.Value --output text

# Recursively fetch all parameters under a path hierarchy
aws ssm get-parameters-by-path --path "/app/prod/" --recursive --with-decryption --query "Parameters[].[Name,Value]" --output table
```
