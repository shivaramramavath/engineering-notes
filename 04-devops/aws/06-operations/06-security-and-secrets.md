# AWS Secrets Management & Cryptography: Secrets Manager, SSM & KMS

> **Module**: `06-operations`  
> **Topic**: Secrets Manager, SSM Parameter Store, AWS KMS, Envelope Encryption & Key Policies  
> **Target Audience**: Cloud Security Engineers, DevOps Architects, Backend Developers  
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

Hardcoding database passwords, API tokens, and encryption keys directly in application source code, Dockerfiles, or configuration files is one of the leading causes of enterprise data breaches. Secrets committed to Git repositories persist forever in commit histories, and rotating compromised static credentials across dozens of running microservices can cause prolonged service downtime.

AWS provides a comprehensive, hardware-secured cryptographic and secret storage hierarchy:
1. **AWS Key Management Service (AWS KMS)**: A managed service backed by FIPS 140-2/3 Level 3 Hardware Security Modules (HSMs) that creates and manages cryptographic keys and controls their use across AWS services.
2. **AWS Secrets Manager**: A dedicated secrets lifecycle management service that securely stores, rotates, audits, and retrieves database credentials, API keys, and OAuth tokens, featuring automated Lambda rotation.
3. **AWS Systems Manager Parameter Store (SSM Parameter Store)**: A cost-effective, hierarchical store for configuration data and secrets management (via `SecureString` types encrypted by KMS).

---

## 2. Core Concepts

### 2.1 AWS Key Management Service (KMS)
- **Customer Master Keys (KMS Keys / CMKs)**: Cryptographic keys that never leave the security perimeter of AWS HSMs unencrypted.
- **Key Types**:
  - *AWS-Owned Keys*: Internal keys used by AWS services; free, unmanageable, invisible in your account.
  - *AWS-Managed Keys*: Default keys created automatically by AWS on your behalf (e.g., `aws/s3`, `aws/ebs`); free storage, no monthly fee, cannot be rotated manually or shared cross-account.
  - *Customer Managed Keys (CMKs)*: Created and controlled entirely by you (\$1.00/month/key). Full control over key policies, annual automated rotation, cross-account sharing, and immediate deletion cancellation.
- **Envelope Encryption**: The practice of encrypting plaintext data with a unique **Data Encryption Key (DEK)**, and then encrypting the DEK with a KMS root key. KMS decrypts only the lightweight DEK, offloading the encryption of multi-gigabyte files to local application memory.

### 2.2 Key Policies vs IAM Policies
- **Key Policy**: The primary access control mechanism for a KMS key. **A KMS key policy is mandatory**—IAM policies alone cannot grant access to a KMS key unless the key policy explicitly delegates authority to the account root (`"Principal": {"AWS": "arn:aws:iam::ACCOUNT_ID:root"}`).

### 2.3 AWS Secrets Manager
- Specifically engineered for secrets that require **dynamic automated rotation** (e.g., database passwords for RDS, Aurora, DocumentDB).
- Integrates with AWS Lambda to rotate credentials automatically on a schedule (e.g., every 30 days) without application restarts.
- Supports **Multi-Region Secret Replication**: Replicates secrets to secondary disaster recovery regions with automatic synchronization.

### 2.4 SSM Parameter Store
- Provides hierarchical storage for application configuration values (e.g., `/prod/ecommerce/db_host` or `/dev/auth/jwt_issuer`).
- **Parameter Types**: `String`, `StringList`, and `SecureString` (encrypted with a KMS key).
- **Tiers**:
  - *Standard Tier*: Up to 10,000 parameters per region; max 4 KB size; **100% Free**.
  - *Advanced Tier*: Up to 100,000 parameters per region; max 8 KB size; parameter policies (expiration, notifications); \$0.05 per parameter per month.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        ENVELOPE ENCRYPTION & SECRETS MANAGER ROTATION                             |
|                                                                                                   |
|  PART 1: ENVELOPE ENCRYPTION WORKFLOW                                                             |
|                                                                                                   |
|  [Application Service]                                                                            |
|         |                                                                                         |
|         v 1. Calls KMS: `GenerateDataKey(KeyId="alias/app-key")`                                  |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS KMS HARDWARE SECURITY MODULE (FIPS 140-3 Level 3 HSM)                                   |  |
|  | Generates Data Key (DEK):                                                                   |  |
|  | Returns: Plaintext DEK (256-bit AES) + Encrypted DEK (Ciphertext)                           |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v 2. Application receives both keys in memory                                             |
|  [Application Memory]                                                                             |
|  - Encrypts 5 GB database dump using Plaintext DEK locally via AES-GCM-256                        |
|  - Erases Plaintext DEK from RAM memory immediately!                                              |
|  - Appends Encrypted DEK ciphertext directly to the encrypted file header                         |
|  - Stores encrypted payload securely in Amazon S3                                                 |
|                                                                                                   |
|  -----------------------------------------------------------------------------------------------  |
|                                                                                                   |
|  PART 2: AUTOMATED 4-STEP SECRETS MANAGER ROTATION                                                |
|                                                                                                   |
|  [Cron Schedule (e.g., Day 30)] ───► Triggers Secrets Manager Service                             |
|                                             |                                                     |
|                                             v (Executes Rotation Lambda Function)                 |
|  +---------------------------------------------------------------------------------------------+  |
|  | ROTATION LAMBDA EXECUTION STEPS:                                                            |  |
|  | 1. createSecret: Generates new random password; stores as AWSPENDING staging label          |  |
|  | 2. setSecret: Connects to RDS DB using AWSCURRENT credentials; executes `ALTER USER`        |  |
|  | 3. testSecret: Validates login connection to RDS using the new AWSPENDING password         |  |
|  | 4. finishSecret: Swaps label: AWSPENDING becomes AWSCURRENT; old becomes AWSPREVIOUS         |  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 CLI Commands for KMS, Secrets Manager & SSM

```bash
# 1. Create a Customer Managed KMS Key with automatic annual rotation enabled
aws kms create-key \
    --description "Master Key for Production Databases" \
    --tags TagKey=Environment,TagValue=Production

aws kms enable-key-rotation \
    --key-id "12345678-1234-1234-1234-123456789012"

# 2. Store an encrypted parameter in SSM Parameter Store
aws ssm put-parameter \
    --name "/prod/ecommerce/stripe_api_key" \
    --type "SecureString" \
    --value "sk_live_sample_token_987654" \
    --key-id "alias/aws/ssm"

# 3. Create a secret in Secrets Manager with automated rotation
aws secretsmanager create-secret \
    --name "prod/rds/postgres" \
    --description "Production RDS PostgreSQL master password" \
    --secret-string '{"username":"pgadmin","password":"InitialMasterPassword!2026"}'
```

---

## 5. Practical Examples

### 5.1 Customer Managed Key (CMK) with Strict Key Policy (Terraform)

```hcl
# KMS Customer Managed Key
resource "aws_kms_key" "database_key" {
  description             = "CMK for RDS and S3 Database Backups"
  deletion_window_in_days = 30
  enable_key_rotation     = true

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      # 1. Root Account Delegate (Enables IAM policies to grant access)
      {
        Sid       = "EnableIAMUserPermissions"
        Effect    = "Allow"
        Principal = { AWS = "arn:aws:iam::${var.account_id}:root" }
        Action    = "kms:*"
        Resource  = "*"
      },
      # 2. Key Administrators
      {
        Sid       = "AllowKeyAdministrators"
        Effect    = "Allow"
        Principal = { AWS = var.admin_role_arn }
        Action = [
          "kms:Create*",
          "kms:Describe*",
          "kms:Enable*",
          "kms:List*",
          "kms:Put*",
          "kms:Update*",
          "kms:Revoke*",
          "kms:Disable*",
          "kms:Get*",
          "kms:Delete*",
          "kms:ScheduleKeyDeletion",
          "kms:CancelKeyDeletion"
        ]
        Resource = "*"
      },
      # 3. Cryptographic Operations for Application Role
      {
        Sid       = "AllowUseOfTheKey"
        Effect    = "Allow"
        Principal = { AWS = var.app_role_arn }
        Action = [
          "kms:Encrypt",
          "kms:Decrypt",
          "kms:ReEncrypt*",
          "kms:GenerateDataKey*",
          "kms:DescribeKey"
        ]
        Resource = "*"
      }
    ]
  })
}

# KMS Alias
resource "aws_kms_alias" "database_key_alias" {
  name          = "alias/production-database-key"
  target_key_id = aws_kms_key.database_key.key_id
}
```

### 5.2 Secrets Manager Secret with RDS Managed Password Rotation (Terraform)

```hcl
# AWS Secrets Manager Secret
resource "aws_secretsmanager_secret" "rds_secret" {
  name                    = "production/postgres/master"
  description             = "Master credentials for RDS PostgreSQL"
  kms_key_id              = aws_kms_key.database_key.arn
  recovery_window_in_days = 7
}

# Native RDS Automated Rotation (AWS managed, zero custom Lambda code needed)
resource "aws_secretsmanager_secret_rotation" "rds_rotation" {
  secret_id           = aws_secretsmanager_secret.rds_secret.id
  rotate_immediately  = true

  rotation_rules {
    automatically_after_days = 30
  }
}
```

### 5.3 In-Memory Secret Caching Client in Python

```python
"""
Retrieves and caches secrets in memory to prevent API throttling and billing costs.
"""
import boto3
import json
import time

secrets_client = boto3.client("secretsmanager", region_name="us-east-1")

class CachedSecretProvider:
    def __init__(self, secret_name: str, cache_ttl_seconds=300):
        self.secret_name = secret_name
        self.cache_ttl = cache_ttl_seconds
        self._cached_value = None
        self._last_fetched_time = 0

    def get_secret(self) -> dict:
        now = time.time()
        # Return cached copy if within TTL window
        if self._cached_value and (now - self._last_fetched_time < self.cache_ttl):
            return self._cached_value

        print(f"Cache miss. Fetching {self.secret_name} from AWS Secrets Manager...")
        response = secrets_client.get_secret_value(SecretId=self.secret_name)
        secret_string = response.get("SecretString")

        self._cached_value = json.loads(secret_string)
        self._last_fetched_time = now
        return self._cached_value

# Singleton instance initialized once per Lambda execution container
db_secrets = CachedSecretProvider("production/postgres/master", cache_ttl_seconds=600)

def handler(event, context):
    credentials = db_secrets.get_secret()
    username = credentials["username"]
    password = credentials["password"]
    # Connect to RDS database...
    return {"status": "connected", "user": username}
```

### 5.4 Fetching SSM SecureString Parameter in Shell / ECS Task

```bash
# Retrieve and decrypt secure parameter in a container startup script
DB_PASSWORD=$(aws ssm get-parameter \
    --name "/prod/ecommerce/db_password" \
    --with-decryption \
    --query "Parameter.Value" \
    --output text)

export DATABASE_PASSWORD="$DB_PASSWORD"
```

---

## 6. Best Practices

1. **Always Cache Secrets in Application Memory**: Never call `GetSecretValue` on every individual incoming HTTP request. Use the AWS-provided client caching libraries (or in-memory singletons) with a 5 to 15-minute TTL to prevent API throttling and excessive billing.
2. **Enable Automated Annual Key Rotation on CMKs**: Enable `enable_key_rotation = true` on all Customer Managed Keys. KMS automatically creates a new cryptographic backing key every year while seamlessly retaining older backing keys to decrypt historical data without re-encrypting existing databases.
3. **Use SSM Parameter Store for Non-Rotating Config, Secrets Manager for Rotating Passwords**: Store static configuration flags and simple tokens in SSM Parameter Store (free). Reserve Secrets Manager (\$0.40/secret/month) for credentials that require automated database password rotation.
4. **Never Disable KMS Keys Immediately**: When decommissioning a key, always schedule deletion with a minimum 7 to 30-day waiting period (`ScheduleKeyDeletion`). Once a KMS key is permanently deleted, **any data encrypted with that key is permanently unrecoverable**.
5. **Always Enforce Key Policy Delegation to Root**: Ensure every CMK key policy contains `"Principal": {"AWS": "arn:aws:iam::ACCOUNT_ID:root"}`. Omitting this statement locks the AWS account out of the key if the specific admin role is deleted.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Fetching Secrets on Every Function Call | Lambda handler calls `GetSecretValue` inside the request loop. | High latency (~50ms/req) and Secrets Manager API rate throttling (`400 MaxRequestsExceeded`). | Cache the secret outside the Lambda handler in memory. |
| Deleting KMS Key Instantly | Administrator schedules key deletion without checking dependencies. | Entire S3 buckets, RDS snapshots, and EBS volumes become permanently unreadable. | Set `deletion_window_in_days = 30` and audit CloudWatch KMS usage before deleting. |
| Forgetting Root Delegation in Key Policy | Key policy lists only a single IAM role and omits `:root`. | If the IAM role is deleted, the key is permanently orphaned and inaccessible. | Always include the `EnableIAMUserPermissions` root delegate block. |
| Plaintext Secrets in SSM | Storing passwords as type `String` instead of `SecureString`. | Password is saved and displayed in plaintext across AWS Console and logs. | Always use `Type: SecureString` and specify a KMS key ID. |
| Missing Rotation Lambda Permissions | Rotation Lambda lacks network access to private RDS VPC subnet. | Rotation fails at Step 2 (`setSecret`); password state becomes corrupted. | Deploy rotation Lambda inside private VPC subnets with RDS security group access. |
| Missing `--with-decryption` Flag in CLI | Calling `aws ssm get-parameter` without `--with-decryption`. | Returns the raw base64 encrypted ciphertext instead of plaintext. | Always pass `--with-decryption` to retrieve decrypted values. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Latency
- **KMS API Quotas**: Requests that use KMS keys directly have regional requests-per-second quotas (typically 10,000 to 50,000 req/sec). Envelope encryption ensures that your application calls KMS only once to generate a Data Key, rather than calling KMS for every chunk of data.

### 8.2 Security & Compliance
- **FIPS 140-2 / 140-3 Level 3 HSMs**: KMS cryptographic operations occur inside dedicated physical hardware modules. Even AWS root administrators have zero access to raw plaintext customer master keys.
- **Audit Trails**: Every key usage (`kms:Decrypt`, `kms:GenerateDataKey`) and secret access (`secretsmanager:GetSecretValue`) is permanently recorded in **AWS CloudTrail** with the caller identity, timestamp, and IP address.

### 8.3 Cost Breakdown
- **AWS KMS**:
  - AWS-managed keys (`aws/s3`, `aws/rds`): Free storage.
  - Customer Managed Keys (CMKs): \$1.00 per key per month.
  - API Requests: \$0.03 per 10,000 requests.
- **AWS Secrets Manager**:
  - \$0.40 per secret per month.
  - \$0.05 per 10,000 API requests.
- **SSM Parameter Store**:
  - Standard Parameters: **100% Free** (Storage and API calls).
  - Advanced Parameters: \$0.05 per parameter per month + \$0.05 per 10,000 interactions.

---

## 9. Comparison Table

| Dimension | AWS Secrets Manager | SSM Parameter Store (SecureString) | AWS KMS |
|---|---|---|---|
| **Primary Purpose** | Storing & rotating credentials | Configuration & secrets storage | Root cryptographic key management |
| **Automated Rotation** | Built-in (Native RDS & Lambda) | None (Requires custom code) | Annual automated key rotation |
| **Cost** | \$0.40 / secret / month | Free (Standard tier) | \$1.00 / key / month |
| **Max Payload Size** | 64 KB | 4 KB (Standard) / 8 KB (Advanced) | 4 KB (Direct Encrypt limit) |
| **Cross-Region Sync** | Built-in Multi-Region Replication | None (Requires manual scripting) | Multi-Region Keys |
| **Primary Use Case** | DB passwords, API tokens, OAuth | App config, environment variables | Encrypting EBS, S3, RDS, DEKs |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is Envelope Encryption and why is it used instead of direct encryption in AWS KMS?</strong></summary>

- **Envelope Encryption** is the cryptographic practice of encrypting plaintext data with a unique **Data Encryption Key (DEK)**, and then encrypting the DEK with a root **KMS Key**.
- **Why It Is Used**:
  1. **Performance & Size**: KMS direct encryption (`kms:Encrypt`) has a strict payload limit of **4 KB**. It cannot encrypt a 500 GB EBS volume or 10 GB S3 object directly.
  2. **Network Bottleneck Elimination**: Sending gigabytes of raw data over the network to KMS would cause massive latency and bandwidth congestion.
  3. **Cost & Throttling**: KMS bills per API call. With envelope encryption, KMS is invoked once to generate a DEK; the client then encrypts gigabytes of data locally in CPU memory.
</details>

<details>
<summary><strong>Q2: Why is a Key Policy mandatory on a KMS Customer Managed Key (CMK)?</strong></summary>

Unlike other AWS resources where access can be controlled purely via IAM identity policies, **a KMS key policy is the primary, indispensable access control mechanism for the key**. 
Even if an IAM policy grants `kms:*` to an administrator, the administrator will receive `AccessDenied` unless the KMS Key Policy contains a statement delegating authority to the account root (`"Principal": {"AWS": "arn:aws:iam::ACCOUNT_ID:root"}`). This design ensures that KMS key administrators can maintain strict cryptographic segregation of duties, even against broad account-level IAM admins.
</details>

<details>
<summary><strong>Q3: How does the 4-step automated rotation process in AWS Secrets Manager work?</strong></summary>

When rotation triggers, Secrets Manager invokes a dedicated Python/Node.js Lambda function that executes four state-machine steps:
1. **`createSecret`**: The Lambda function generates a new random password and stores it in Secrets Manager under the staging label `AWSPENDING`.
2. **`setSecret`**: The function connects to the target database (e.g., RDS PostgreSQL) using the current active credentials (`AWSCURRENT`) and updates the user's password in the database to match `AWSPENDING`.
3. **`testSecret`**: The function tests opening a new database connection using the `AWSPENDING` credentials to confirm they work.
4. **`finishSecret`**: Secrets Manager moves the `AWSCURRENT` label to the new password, marking the old password as `AWSPREVIOUS`.
</details>

<details>
<summary><strong>Q4: When should you choose AWS Systems Manager Parameter Store over AWS Secrets Manager?</strong></summary>

Choose **SSM Parameter Store** when:
1. You are storing static or infrequently changed configuration settings, feature flags, or API endpoints.
2. Cost optimization is a priority: Standard parameters (including `SecureString` encrypted with KMS) are **100% Free** (zero monthly fees and zero API call fees).
3. Secrets do not require automated rotation (e.g., third-party API tokens, license keys).
Choose **AWS Secrets Manager** when you require automated scheduled database password rotation (RDS/Aurora), built-in cross-region secret replication, or centralized rotation audit logging.
</details>

<details>
<summary><strong>Q5: What happens if an AWS KMS Customer Managed Key is scheduled for deletion? Can it be recovered?</strong></summary>

KMS enforces a mandatory waiting period between **7 and 30 days** when scheduling key deletion (`ScheduleKeyDeletion`). Immediate deletion is strictly impossible.
- During the waiting period, the key status moves to `PendingDeletion` and all cryptographic operations (`Encrypt`, `Decrypt`) are immediately rejected.
- **Recovery**: At any point before the waiting window expires, an administrator can call `aws kms cancel-key-deletion` to restore the key to active service.
- **Warning**: Once the timer expires and the key is permanently deleted, **any data encrypted with that key is permanently and mathematically unrecoverable**.
</details>

<details>
<summary><strong>Q6: How does Multi-Region Secret Replication work in AWS Secrets Manager?</strong></summary>

Secrets Manager allows you to designate a primary secret in one region (e.g., `us-east-1`) and configure replica secrets in one or more secondary regions (e.g., `eu-west-1`). 
- Secrets Manager automatically replicates the secret value, metadata, and tags to the target regions, encrypting the replica with a KMS key in the destination region.
- When the primary secret rotates, Secrets Manager synchronizes the new secret value across all replicas within minutes, enabling seamless multi-region active-active or disaster recovery failover.
</details>

<details>
<summary><strong>Q7: What is the difference between AWS-Managed Keys and Customer Managed Keys in KMS?</strong></summary>

- **AWS-Managed Keys** (e.g., `aws/s3`, `aws/ebs`): Created automatically by AWS services. They have no monthly storage fee (\$0.00/month). However, you cannot alter their key policy, cannot manually trigger rotation, cannot track them in AWS Organizations SCPs, and cannot share them with other AWS accounts.
- **Customer Managed Keys (CMKs)**: Created by you (\$1.00/month). You have full control over key policies, annual automated rotation, cross-account access delegation, and alias management.
</details>

<details>
<summary><strong>Q8: Why is in-memory secret caching critical for serverless Lambda applications?</strong></summary>

If a high-throughput Lambda function (e.g., handling 2,000 requests/second) calls `secretsmanager:GetSecretValue` inside its event handler:
1. **API Rate Throttling**: The account will quickly breach Secrets Manager's regional API rate limits (10,000 req/sec), throwing `ThrottlingException` and causing cascading application failures.
2. **Billing Spikes**: 2,000 req/sec equals ~5 billion requests/month, resulting in over \$25,000 in monthly API charges!
3. **Latency**: Each API call adds 20-50ms of network latency to every request.
Caching the secret in memory outside the handler with a 5-minute TTL reduces API calls by 99.99%.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Customer Managed Key with Annual Rotation</strong></summary>

**Goal**: Provision an encrypted KMS key.
1. Create a CMK with alias `alias/test-app-key`.
2. Enable automatic annual key rotation via CLI: `aws kms enable-key-rotation --key-id ...`.
3. Verify rotation status with `aws kms get-key-rotation-status`.
</details>

<details>
<summary><strong>Exercise 2: Encrypt and Decrypt Small String via KMS CLI</strong></summary>

**Goal**: Understand raw KMS cryptographic operations.
1. Encrypt a string:
   `aws kms encrypt --key-id alias/test-app-key --plaintext fileb://<(echo -n "MySecretData") --output text --query CiphertextBlob > encrypted.base64`.
2. Decrypt the string:
   `aws kms decrypt --ciphertext-blob fileb://<(base64 -d encrypted.base64) --output text --query Plaintext | base64 -d`.
3. Verify the decrypted output matches original text.
</details>

<details>
<summary><strong>Exercise 3: Store and Retrieve SSM SecureString Parameter</strong></summary>

**Goal**: Manage encrypted configuration.
1. Put parameter `/dev/database/password` with type `SecureString`.
2. Retrieve without decryption flag; observe raw base64 ciphertext.
3. Retrieve with `--with-decryption` flag; observe clean plaintext.
</details>

<details>
<summary><strong>Exercise 4: Create a Multi-Region Secret in Secrets Manager</strong></summary>

**Goal**: Replicate secrets for disaster recovery.
1. Create a secret in `us-east-1`.
2. Add replica region `eu-central-1` using `aws secretsmanager replicate-secret-to-regions`.
3. Update the secret value in `us-east-1`.
4. Inspect the secret in `eu-central-1` and verify the updated value has replicated.
</details>

<details>
<summary><strong>Exercise 5: Build In-Memory Secret Cache in Python</strong></summary>

**Goal**: Eliminate redundant API calls.
1. Write a script calling `GetSecretValue` inside a loop 5 times without caching (measure total execution time).
2. Wrap the call in an in-memory caching class with a 60-second TTL.
3. Run the loop again and observe execution time drop to sub-millisecond on iterations 2-5.
</details>

<details>
<summary><strong>Exercise 6: Test ScheduleKeyDeletion and Cancellation</strong></summary>

**Goal**: Practice key lifecycle disaster prevention.
1. Schedule deletion of a test key with a 7-day window: `aws kms schedule-key-deletion --key-id <id> --pending-window-in-days 7`.
2. Verify key status is `PendingDeletion` (encrypt operations fail).
3. Cancel deletion: `aws kms cancel-key-deletion --key-id <id>`.
4. Verify key returns to `Enabled` status.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| AWS CRYPTOGRAPHY & SECRETS MANAGEMENT CHEAT SHEET                                                 |
+--------------------------+-----------------------+------------------------------------------------+
| Service                  | Best Used For         | Key Differentiator                             |
+--------------------------+-----------------------+------------------------------------------------+
| AWS KMS                  | Root encryption keys  | FIPS 140-3 HSM backing; envelope encryption    |
| AWS Secrets Manager      | Rotating credentials  | Automated RDS Lambda rotation; multi-region sync|
| SSM Parameter Store      | App config & tokens   | Standard tier is 100% Free; hierarchical paths |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Envelope Encryption**: Solves the 4 KB KMS payload limit and prevents network bottlenecks.
- **Root Delegation**: Always delegate key permissions to the root account (`:root`) in key policies.
- **In-Memory Caching**: Mandatory for serverless functions accessing Secrets Manager.

---

## 13. Further Reading

- [AWS Key Management Service Developer Guide](https://docs.aws.amazon.com/kms/latest/developerguide/)
- [AWS Secrets Manager User Guide](https://docs.aws.amazon.com/secretsmanager/latest/userguide/)
- [AWS Systems Manager Parameter Store User Guide](https://docs.aws.amazon.com/systems-manager/latest/userguide/systems-manager-parameter-store.html)
- [AWS Cryptographic Details Whitepaper](https://docs.aws.amazon.com/whitepapers/latest/kms-cryptographic-details/)
- [06-operations/01-infrastructure-as-code.md](file:///04-devops/aws/06-operations/01-infrastructure-as-code.md)
