# AWS SDKs & The Default Credential Provider Chain

> The AWS Default Credential Provider Chain is the universal resolution algorithm implemented across all official AWS SDKs that sequentially inspects environment variables, container endpoints, and instance metadata to authenticate workloads without hardcoded secrets.

| Metadata | Details |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [05-iam-fundamentals.md](file:///04-devops/aws/01-foundations/05-iam-fundamentals.md), [08-aws-cli.md](file:///04-devops/aws/01-foundations/08-aws-cli.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | Multi-Language Application Runtimes (Node.js, Python, Java, Go) |
| **Tool Version** | AWS SDK for JS v3, AWS SDK for Java v2, Boto3, AWS SDK for Go v2 |
| **Tested With** | Node.js 20/22, Python 3.12, OpenJDK 21, AWS CLI v2.22+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Security & Cost Notes](#8-performance-security--cost-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
When deploying an application across development laptops, CI/CD runners, Docker containers, ECS tasks, and EC2 instances, hardcoding access keys or writing environment-specific authentication branches (`if (env === 'local') ... else ...`) introduces security risks and maintenance complexity.

All official AWS SDKs implement the **Default Credential Provider Chain**. This standardized algorithm enables identical application binaries to run anywhere: on a developer laptop, it reads local SSO tokens; in a GitHub Actions runner, it exchanges OIDC web tokens; inside an ECS container, it queries the task role URI; and on an EC2 instance, it fetches credentials from IMDSv2. The application code simply instantiates the AWS client without arguments, and the SDK automatically discovers and refreshes valid cryptographic credentials.

---

## 2. Core Concepts
- **Universal Provider Chain Order**:
  Every official AWS SDK (Python Boto3, Node.js `@aws-sdk`, Java v2, Go v2) evaluates credentials in the exact sequence:
  1. *Java System Properties* (Java SDK only): `-Daws.accessKeyId` and `-Daws.secretKey`.
  2. *Environment Variables*: `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, and `AWS_SESSION_TOKEN`.
  3. *Web Identity Token / OIDC Credentials*: `AWS_WEB_IDENTITY_TOKEN_FILE` and `AWS_ROLE_ARN` (used by EKS Pod Identity, Kubernetes IRSA, and GitHub Actions CI).
  4. *Shared Credentials and Config Files*: `~/.aws/credentials` and `~/.aws/config` (or path in `AWS_SHARED_CREDENTIALS_FILE`).
  5. *ECS Container Credentials*: `AWS_CONTAINER_CREDENTIALS_RELATIVE_URI` or `AWS_CONTAINER_CREDENTIALS_FULL_URI` (used by ECS Task Roles).
  6. *EC2 Instance Metadata Service (IMDSv2)*: Local link-local address `http://169.254.169.254/latest/meta-data/iam/security-credentials/<role-name>` (used by EC2 Instance Profiles).
- **Client Configuration Invariants**:
  - Never instantiate SDK clients with hardcoded strings like `new S3Client({ credentials: { accessKeyId: '...' } })`.
  - Always instantiate default clients: `new S3Client({})` or `boto3.client('s3')`.
- **Cross-Track Reference**:
  - For Node.js-specific AWS SDK modular v3 patterns, see [`03-backend/nodejs/20-aws-sdk/`](file:///03-backend/nodejs/20-aws-sdk/).

---

## 3. How It Works Internally
The SDK Default Credential Provider Chain Evaluation Flow:

```
[ Application Calls: new DynamoDBClient({}) ]
                      |
                      v
+-------------------------------------------------------------+
| 1. Check Environment Variables?                             |
|    - AWS_ACCESS_KEY_ID & AWS_SECRET_ACCESS_KEY              |
+-------------------------------------------------------------+
       | Found: Use Env Keys.      | Not Found
       v                           v
+-------------------------------------------------------------+
| 2. Check Web Identity Token File?                           |
|    - AWS_WEB_IDENTITY_TOKEN_FILE & AWS_ROLE_ARN (EKS / OIDC) |
+-------------------------------------------------------------+
       | Found: Exchange via STS.  | Not Found
       v                           v
+-------------------------------------------------------------+
| 3. Check Shared Config / Credentials?                       |
|    - ~/.aws/config & ~/.aws/credentials (Active Profile)    |
+-------------------------------------------------------------+
       | Found: Use Profile.       | Not Found
       v                           v
+-------------------------------------------------------------+
| 4. Check ECS Container Metadata URI?                        |
|    - AWS_CONTAINER_CREDENTIALS_RELATIVE_URI (ECS Task Role) |
+-------------------------------------------------------------+
       | Found: Query Task URI.    | Not Found
       v                           v
+-------------------------------------------------------------+
| 5. Check EC2 IMDSv2 Endpoint?                               |
|    - http://169.254.169.254/... (EC2 Instance Profile)      |
+-------------------------------------------------------------+
       | Found: Query IMDSv2.      | Not Found
       v                           v
[ Credentials Discovered! ]    [ Throws CredentialsProviderError ]
```

### Automatic In-Memory Token Refresh
When the SDK retrieves temporary credentials (`ASIA...`) from STS, ECS, or IMDSv2, it inspects the `Expiration` timestamp. As the expiration window approaches (typically within 5 minutes of expiry), the SDK automatically issues a non-blocking background request to renew the token, ensuring active requests are never interrupted.

---

## 4. Syntax & Usage
Idiomatic zero-credential instantiation across major programming languages:

### Node.js (AWS SDK v3)
```typescript
import { S3Client, ListBucketsCommand } from "@aws-sdk/client-s3";

// Automatically evaluates Default Credential Provider Chain
const s3 = new S3Client({});

const response = await s3.send(new ListBucketsCommand({}));
console.log(response.Buckets);
```

### Python (Boto3)
```python
import boto3

# Automatically evaluates Default Credential Provider Chain
dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('Orders')

response = table.get_item(Key={'OrderId': '12345'})
print(response.get('Item'))
```

### Java (AWS SDK v2)
```java
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.model.ListBucketsResponse;

// Default chain automatically wired via builder
S3Client s3 = S3Client.builder().build();

ListBucketsResponse response = s3.listBuckets();
response.buckets().forEach(b -> System.out.println(b.name()));
```

---

## 5. Practical Examples

### Example 1: Node.js Multi-Environment Execution
- **Goal:** Write an AWS SDK v3 client that operates seamlessly on a developer laptop and in ECS production without code changes.
- **Implementation (`src/s3-service.ts`):**
```typescript
import { S3Client, GetObjectCommand } from "@aws-sdk/client-s3";

export class DocumentStorageService {
  private readonly s3Client: S3Client;

  constructor() {
    // Zero hardcoded credentials or region strings
    // Resolves AWS_REGION, AWS_PROFILE, or ECS Task Role automatically
    this.s3Client = new S3Client({});
  }

  async getFile(bucket: string, key: string): Promise<string> {
    const command = new GetObjectCommand({ Bucket: bucket, Key: key });
    const response = await this.s3Client.send(command);
    return await response.Body?.transformToString() ?? "";
  }
}
```
- **Explanation:** In local development, the client resolves credentials from `AWS_PROFILE`. In ECS Fargate, it transparently uses the ECS Task Execution Role.

### Example 2: Boto3 Exponential Backoff & Retry Configuration
- **Goal:** Configure client-level retry policies and connection pooling in Python.
- **Implementation:**
```python
import boto3
from botocore.config import Config

custom_config = Config(
    region_name='us-east-1',
    retries={
        'max_attempts': 5,
        'mode': 'adaptive'  # Automatically throttles request rate on 503 SlowDown
    },
    max_pool_connections=50
)

s3_client = boto3.client('s3', config=custom_config)
```
- **Explanation:** The `adaptive` retry mode includes exponential backoff with full jitter, protecting against thundering herd problems during downstream service brownouts.

### Example 3: Edge Case - Overriding Profile via Environment Variable
- **Goal:** Direct an application running on a developer machine to use a specific SSO profile without changing code.
- **Commands:**
```bash
export AWS_PROFILE=staging-developer
export AWS_REGION=us-west-2

node dist/index.js
```
- **Explanation:** Setting `AWS_PROFILE` intercepts Step 3 of the provider chain, forcing the SDK to load the staging profile.

### Example 4: Failure & Anti-Pattern - Hardcoded Credentials in Code
- **Goal:** Demonstrate catastrophic security risk of static keys in source files.
- **Anti-Pattern Code:**
```python
# EXTREME VULNERABILITY: Hardcoded static keys
s3 = boto3.client(
    's3',
    aws_access_key_id='AKIAIOSFODNN7EXAMPLE',
    aws_secret_access_key='wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY'
)
```
- **Vulnerability:** If committed to GitHub, automated scanners compromise the credentials in seconds. Rotating these keys requires recompiling and redeploying the application binary. Never use this pattern.

### Example 5: Simulating the Chain Resolution Locally
- **Goal:** Inspect which provider supplied credentials using the AWS CLI / Python script.
- **Python Verification Script (`whoami_provider.py`):**
```python
import botocore.session

session = botocore.session.get_session()
credentials = session.get_credentials()

print(f"Access Key ID: {credentials.access_key[:8]}...")
print(f"Credential Method: {credentials.method}")
```
- **Real Output (on EC2):**
```text
Access Key ID: ASIA1234...
Credential Method: iam-role (instance-profile)
```
- **Real Output (in Local Dev):**
```text
Access Key ID: ASIA5678...
Credential Method: sso
```
- **Explanation:** Demonstrates dynamic resolution across distinct underlying environments.

---

## 6. Best Practices
1. **Never Pass Static Credentials to SDK Constructors**: Always let the default provider chain resolve credentials.
2. **Configure Adaptive Retries**: Set client retry mode to `adaptive` or `standard` with jitter to avoid amplifying cloud outages.
3. **Reuse Client Instances as Singletons**: Creating a new SDK client per HTTP request exhausts socket connections and forces repeated credential resolutions. Instantiate clients once as application singletons.
4. **Use Web Identity Tokens for CI/CD and EKS**: Authenticate GitHub Actions and Kubernetes Pods via OIDC federated web identity tokens rather than baking IAM user keys.
5. **Set Explicit Socket Timeouts**: Configure connect and read timeouts (e.g. connect: 2000ms, socket: 5000ms) to prevent thread exhaustion when downstream cloud services lag.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Hardcoding `credentials: { accessKeyId: '...' }` | Exposes keys, prevents IAM role usage, fails in production | Instantiate SDK clients with empty configuration object |
| Instantiating a new client inside an API handler function | Exhausts file descriptors and connection pools under load | Initialize the client once outside the handler as a singleton |
| Forgetting that environment variables override IAM roles | Local `AWS_ACCESS_KEY_ID` in `.env` overrides the EC2 instance profile | Unset `AWS_ACCESS_KEY_ID` in container environments |
| Blocking on long-lived IMDS timeouts on non-AWS hosts | SDK waits 5 seconds attempting to reach `169.254.169.254` | Set `AWS_EC2_METADATA_DISABLED=true` when running outside AWS |
| Ignoring STS token expiration in custom credential providers | Token expires after 1 hour, causing sudden `ExpiredToken` crashes | Rely on native SDK providers which manage refresh transparently |
| Assuming SDK clients share global HTTP connections | In Node.js, multiple client instances create distinct HTTP agent pools | Pass a shared `NodeHttpHandler` with a custom `http.Agent` |

---

## 8. Performance, Security & Cost Notes
- **Performance**: IMDSv2 requires an HTTP PUT request to fetch an access token followed by an HTTP GET for credentials. The SDK caches this credential in memory until near expiration, so subsequent API calls incur zero IMDS overhead.
- **Security**: The credential provider chain strictly isolates credentials in memory. Temporary credentials (`ASIA...`) expire automatically, minimizing the attack window if memory dumps are compromised.
- **Cost**: Calling STS for role assumption (`sts:AssumeRole`) is **free of charge**. Credential resolution incurs zero billing fees.

---

## 9. Comparison / When to Use What

| Provider Mechanism | Environment | Security Level | Rotation Automation |
|---|---|---|---|
| **Environment Variables** | Local testing, CI scripts | Low (Visible in process table) | None (Static) |
| **IAM Identity Center (SSO)**| Developer workstation | High (Short-lived, MFA) | Handled by `aws sso login` |
| **ECS Task Role** | Amazon ECS / Fargate | Maximum (Container-isolated) | Automatic by ECS agent |
| **EC2 Instance Profile** | Amazon EC2 virtual machines | Maximum (Nitro-isolated) | Automatic via IMDSv2 |
| **Web Identity Token (OIDC)** | EKS Pods, GitHub Actions | Maximum (Zero static secrets) | Automatic STS exchange |

---

## 10. Interview Questions

<details>
<summary>1. Explain the sequence of the AWS Default Credential Provider Chain.</summary>
The SDK evaluates: 1. Java System Properties (Java only), 2. Environment Variables (`AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`), 3. Web Identity Token (`AWS_WEB_IDENTITY_TOKEN_FILE`), 4. Shared credentials/config (`~/.aws/credentials`), 5. ECS Container Credentials, 6. EC2 Instance Metadata Service (IMDSv2).
</details>

<details>
<summary>2. Why does an application running on EC2 suddenly fail with ExpiredToken if environment variables are set?</summary>
Because Environment Variables sit *higher* in the resolution chain than EC2 Instance Metadata. If expired or invalid static keys exist in the environment, the SDK uses them and fails, completely ignoring the valid Instance Profile credentials available from IMDS.
</details>

<details>
<summary>3. Why should AWS SDK clients be instantiated as singletons rather than inside request handler functions?</summary>
Instantiating clients creates HTTP connection pools, TLS session caches, and credential refresh timers. Instantiating a client per request leads to socket exhaustion, memory bloat, and repeated credential lookups.
</details>

<details>
<summary>4. How does the AWS SDK authenticate in Kubernetes (EKS) without static credentials?</summary>
Via EKS Pod Identity or IRSA (IAM Roles for Service Accounts). The Kubernetes mutating webhook injects `AWS_WEB_IDENTITY_TOKEN_FILE` and `AWS_ROLE_ARN`. The SDK picks up these environment variables (Step 2/3) and calls `sts:AssumeRoleWithWebIdentity` to fetch temporary credentials.
</details>

<details>
<summary>5. What is "jitter" in AWS SDK retry strategies?</summary>
Jitter adds randomized noise to exponential backoff wait times (`delay = random_between(0, 2^attempt * base)`). It prevents the "thundering herd" problem where hundreds of throttled clients retry at the exact same millisecond intervals.
</details>

<details>
<summary>6. How can you disable the IMDSv2 check when running an SDK application outside of AWS?</summary>
Set the environment variable `AWS_EC2_METADATA_DISABLED=true`. This instructs the SDK to immediately skip the final IMDSv2 check, preventing 5-second socket timeout delays during startup on non-AWS hosts.
</details>

<details>
<summary>7. What is the modular design philosophy of AWS SDK for JavaScript v3 compared to v2?</summary>
SDK v2 imported a monolithic `aws-sdk` package containing all 200+ services, bloating bundle sizes. SDK v3 is split into separate scoped packages (e.g. `@aws-sdk/client-s3`), supporting tree-shaking and dramatically reducing Lambda cold start times.
</details>

<details>
<summary>8. How does an ECS container receive credentials distinct from the underlying EC2 host?</summary>
The ECS agent injects `AWS_CONTAINER_CREDENTIALS_RELATIVE_URI` into the container. The SDK detects this variable and queries the local container agent endpoint (`http://169.254.170.2`), which vends the Task Role credentials, ensuring containers on the same host maintain strict isolation.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Print active SDK credentials using Python Boto3</summary>
**Task:** Write a Python one-liner to print the credential access key ID in use.

**Solution:**
```python
import boto3; print(boto3.Session().get_credentials().access_key)
```
</details>

<details>
<summary>Exercise 2 (Easy): Verify IMDS credential lookup manually</summary>
**Task:** On an EC2 instance, use `curl` to fetch an IMDSv2 token and query the active role name.

**Solution:**
```bash
TOKEN=$(curl -s -X PUT "http://169.254.169.254/latest/api/token" -H "X-aws-ec2-metadata-token-ttl-seconds: 60")
curl -s -H "X-aws-ec2-metadata-token: $TOKEN" http://169.254.169.254/latest/meta-data/iam/security-credentials/
```
</details>

<details>
<summary>Exercise 3 (Medium): Configure Node.js SDK client with custom retry limit</summary>
**Task:** Instantiate an S3Client in TypeScript with max retries set to 6.

**Solution:**
```typescript
import { S3Client } from "@aws-sdk/client-s3";

const client = new S3Client({
  maxAttempts: 6
});
```
</details>

<details>
<summary>Exercise 4 (Medium): Test credential resolution fallback order in bash</summary>
**Task:** Export temporary bogus environment keys, run an AWS command, assert it fails, then unset them and verify it succeeds via SSO/IMDS.

**Solution:**
```bash
export AWS_ACCESS_KEY_ID="AKIA_BOGUS_TEST_KEY"
export AWS_SECRET_ACCESS_KEY="BOGUS_SECRET"
aws sts get-caller-identity || echo "Failed as expected using bogus env keys."

unset AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY
aws sts get-caller-identity && echo "Succeeded using fallback provider."
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement an automated client credential inspector</summary>
**Task:** Write a Node.js script that outputs the resolved credential provider type (`env`, `sso`, `instance-profile`, `ecs`).

**Solution:**
```javascript
import { defaultProvider } from "@aws-sdk/credential-provider-node";

const provider = defaultProvider();
const creds = await provider();

console.log("Resolved AccessKeyId:", creds.accessKeyId.slice(0, 8) + "...");
console.log("Has Session Token:", !!creds.sessionToken);
console.log("Expiration Date:", creds.expiration);
```
</details>

<details>
<summary>Exercise 6 (Hard): Build a robust S3 downloader with adaptive retry logic</summary>
**Task:** Write a Python Boto3 script that fetches an object using an adaptive retry configuration and catches specific client exceptions.

**Solution:**
```python
import boto3
from botocore.config import Config
from botocore.exceptions import ClientError

config = Config(retries={'mode': 'adaptive', 'max_attempts': 5})
s3 = boto3.client('s3', config=config)

try:
    obj = s3.get_object(Bucket='my-bucket', Key='data.json')
    data = obj['Body'].read().decode('utf-8')
    print("Downloaded:", len(data), "bytes")
except ClientError as e:
    if e.response['Error']['Code'] == 'NoSuchKey':
        print("Object does not exist.")
    else:
        print("API Error:", e)
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                    CREDENTIAL PROVIDER CHAIN CHEAT SHEET                        |
+---------------------------------------------------------------------------------+
| RESOLUTION 1   | Environment Variables (AWS_ACCESS_KEY_ID, AWS_SECRET_...)      |
| RESOLUTION 2   | Web Identity Token (AWS_WEB_IDENTITY_TOKEN_FILE for EKS/OIDC)  |
| RESOLUTION 3   | Shared Credentials / Config (~/.aws/credentials via Profile)   |
| RESOLUTION 4   | ECS Task Role (AWS_CONTAINER_CREDENTIALS_RELATIVE_URI)         |
| RESOLUTION 5   | EC2 Instance Profile via IMDSv2 (http://169.254.169.254)       |
| CLIENT PATTERN | Always instantiate with empty options: new S3Client({})        |
| DISABLE IMDS   | export AWS_EC2_METADATA_DISABLED=true (On non-AWS hosts)       |
| RETRIES        | Use 'adaptive' retry mode with exponential backoff & jitter    |
| REFRESH        | SDKs automatically refresh temporary tokens before expiry      |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS SDKs and Tools Reference Guide](https://docs.aws.amazon.com/sdkref/latest/guide/overview.html)
- [Credentials and Configuration Resolution Order](https://docs.aws.amazon.com/sdkref/latest/guide/standardized-credentials.html)
- [AWS SDK for JavaScript v3 Developer Guide](https://docs.aws.amazon.com/sdk-for-javascript/v3/developer-guide/welcome.html)
- [Boto3 Documentation (Python SDK)](https://boto3.amazonaws.com/v1/documentation/api/latest/index.html)
- [Cross-Track Reference: Node.js AWS SDK Modular v3](file:///03-backend/nodejs/20-aws-sdk/)

---

[Previous: 08-aws-cli.md](file:///04-devops/aws/01-foundations/08-aws-cli.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 10-console-cloudshell-and-tagging.md](file:///04-devops/aws/01-foundations/10-console-cloudshell-and-tagging.md)
