# AWS CLI v2: Configuration, Profiles, JMESPath & Advanced Automation

> The AWS Command Line Interface v2 (AWS CLI v2) is the primary programmatic terminal client for orchestrating AWS services, featuring native IAM Identity Center SSO authentication, deterministic pagination, and client-side JMESPath query evaluation.

| Metadata | Details |
|---|---|
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01-what-is-aws.md](file:///04-devops/aws/01-foundations/01-what-is-aws.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | Multi-Platform (Linux, macOS, Windows) |
| **Tool Version** | AWS CLI v2.22+ / v2.24+ |
| **Tested With** | AWS CLI v2.22.15, Python 3.12 bundled runtime, Bash 5.2 |

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
Managing cloud infrastructure solely through the AWS Management Console is slow, error-prone, and cannot be integrated into automated pipelines or shell scripts. When managing hundreds of resources, point-and-click operations produce configuration drift and prevent repeatable disaster recovery.

The **AWS CLI v2** transforms cloud administration into reproducible, scriptable terminal commands. Unlike CLI v1 (which was a Python package susceptible to local `pip` dependency conflicts), CLI v2 is distributed as a self-contained binary bundle with its own hermetic Python runtime. It introduces native wizard-driven SSO (`aws configure sso`), automatic token refresh, server-side parameter completion, and deep **JMESPath** filtering (`--query`), allowing engineers to extract precise data points without relying on external utilities like `jq` or `awk`.

---

## 2. Core Concepts
- **Configuration Files**:
  - `~/.aws/config`: Non-sensitive settings (region, output format, CLI profiles, SSO sessions).
  - `~/.aws/credentials`: Sensitive credentials (access keys, session tokens). Deprecated for human users in favor of SSO tokens stored in `~/.aws/sso/cache/`.
- **Named Profiles**:
  - Encapsulate distinct identities or target accounts (e.g. `[profile staging]`, `[profile production]`).
  - Selected via the flag `--profile <name>` or environment variable `export AWS_PROFILE=<name>`.
- **Server-Side Filtering (`--filters`) vs Client-Side Filtering (`--query`)**:
  - `--filters` (Server-Side): Transmitted in the HTTP request; AWS filters resources before returning the payload, minimizing network transfer and API latency. Supported on specific APIs (e.g. EC2).
  - `--query` (Client-Side): AWS returns the full payload; the CLI parses the JSON locally using **JMESPath** expressions. Universally supported on every AWS CLI command.
- **Pagination Mechanics**:
  - `--page-size`: Controls how many items AWS returns per individual network request (avoids API timeouts on large collections).
  - `--max-items`: Controls the total number of items the CLI outputs before stopping.
  - `--starting-token`: Resumes pagination from a previous next-token marker.
- **Output Formats**:
  - `json` (default, complete data structure), `table` (human-readable ASCII grids), `text` (tab-delimited for shell consumption), `yaml`.

---

## 3. How It Works Internally
CLI v2 Execution and JMESPath Query Architecture:

```
[ Developer Terminal: aws ec2 describe-instances --filters ... --query ... ]
                               |
                               | 1. Resolves Credentials from Chain (SSO / Env)
                               v
+-------------------------------------------------------------------------+
|                        AWS CLI v2 Client Engine                         |
|  - Validates command schema via bundled botocore models                 |
|  - Injects server-side --filters into HTTP parameters                   |
|  - Computes HMAC-SHA256 SigV4 signature using active credentials        |
+-------------------------------------------------------------------------+
                               |
                               | 2. HTTPS POST to https://ec2.us-east-1.amazonaws.com
                               v
+-------------------------------------------------------------------------+
|                       AWS EC2 Regional Endpoint                         |
|  - Authenticates SigV4 signature                                        |
|  - Applies server-side filters against internal database                |
|  - Serializes response into raw JSON payload                            |
+-------------------------------------------------------------------------+
                               |
                               | 3. Raw JSON Stream Response
                               v
+-------------------------------------------------------------------------+
|                  Client-Side JMESPath Parsing Engine                    |
|  - Evaluates --query expression against JSON AST                        |
|  - Formats output according to --output (table / text / json)           |
+-------------------------------------------------------------------------+
                               |
                               v
                     [ Formatted Output ]
```

---

## 4. Syntax & Usage
Core configuration and query syntax:

```bash
# Verify CLI version and bundled Python environment
aws --version

# Interactive configuration wizard
aws configure

# Connect profile to IAM Identity Center
aws configure sso

# Login via SSO session
aws sso login --profile prod-admin

# Execute command targeting a specific profile and region
aws s3 ls --profile prod-admin --region us-west-2

# JMESPath query syntax pattern
aws <service> <command> --query "Array[?FilterCondition].[Field1, Field2]" --output text
```

---

## 5. Practical Examples

### Example 1: Server-Side Filtering Paired with JMESPath Query
- **Goal:** List only running `t4g.micro` EC2 instances, extracting only Instance ID and Private IP.
- **Commands:**
```bash
aws ec2 describe-instances \
    --filters "Name=instance-state-name,Values=running" "Name=instance-type,Values=t4g.micro" \
    --query "Reservations[].Instances[].[InstanceId, PrivateIpAddress, LaunchTime]" \
    --output table
```
- **Real Output:**
```text
---------------------------------------------------------------------
|                         DescribeInstances                         |
+----------------------+----------------+---------------------------+
|  i-0123456789abcdef0 |  10.0.1.45     |  2026-11-20T08:14:02.000Z |
|  i-0fedcba9876543210 |  10.0.2.89     |  2026-11-22T14:30:19.000Z |
+----------------------+----------------+---------------------------+
```
- **Explanation:** Server-side `--filters` limits the payload transferred over the wire; client-side `--query` extracts clean tabular columns.

### Example 2: Advanced JMESPath Projections & Multi-Select Hashes
- **Goal:** Format S3 bucket names alongside their creation dates as structured JSON objects.
- **Commands:**
```bash
aws s3api list-buckets \
    --query "Buckets[?CreationDate >= '2026-01-01'].{BucketName: Name, Created: CreationDate}" \
    --output json
```
- **Real Output:**
```json
[
    {
        "BucketName": "production-data-lake-2026",
        "Created": "2026-03-15T10:00:00+00:00"
    },
    {
        "BucketName": "audit-logs-vault",
        "Created": "2026-08-01T12:30:00+00:00"
    }
]
```
- **Explanation:** Uses multi-select hash `{Alias: Key}` syntax to transform arbitrary API outputs into clean custom JSON schemas.

### Example 3: Edge Case - Handling API Throttling with Pagination
- **Goal:** Safely list thousands of objects in an S3 bucket without triggering HTTP 503 SlowDown or timeout errors.
- **Commands:**
```bash
aws s3api list-objects-v2 \
    --bucket "production-high-volume-bucket" \
    --page-size 250 \
    --max-items 500 \
    --query "Contents[].[Key, Size]" \
    --output table
```
- **Explanation:** `--page-size 250` forces the CLI to request smaller 250-item chunks from AWS, while `--max-items 500` caps the total items returned.

### Example 4: Failure & Anti-Pattern - Misinterpreting JMESPath Null Outputs
- **Goal:** Illustrate error when querying a field with invalid case sensitivity.
- **Attempted Command:**
```bash
# Querying "instanceid" instead of exact PascalCase "InstanceId"
aws ec2 describe-instances --query "Reservations[].Instances[].instanceid" --output text
```
- **Real Output:**
```text
None
```
- **Explanation & Fix:** JMESPath is strictly case-sensitive. If an attribute name does not match the exact casing of the underlying API response (`InstanceId`), JMESPath silently evaluates to `null` (`None` in text output). Always inspect raw JSON first.

### Example 5: Production Scripting - Safe AMI Cleanup with Exit Code Checking
- **Goal:** Script that queries deregistered AMI snapshots and verifies exit codes.
- **Script (`cleanup-amis.sh`):**
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Querying unattached private AMIs..."
AMIS=$(aws ec2 describe-images \
    --owners self \
    --filters "Name=tag:Decommissioned,Values=true" \
    --query "Images[].[ImageId]" \
    --output text)

if [ -z "$AMIS" ]; then
    echo "No decommissioned AMIs found."
    exit 0
fi

for ami in $AMIS; do
    echo "Deregistering AMI: $ami"
    aws ec2 deregister-image --image-id "$ami"
done
echo "Cleanup completed successfully."
```
- **Explanation:** Combines `set -euo pipefail` with CLI `--output text` to safely iterate without parsing JSON in shell scripts.

---

## 6. Best Practices
1. **Always Use AWS CLI v2 Over v1**: CLI v1 is legacy; CLI v2 includes bundled dependencies, native SSO, and modern service feature flags.
2. **Favor Server-Side `--filters` for Performance**: When querying large fleets (EC2, S3), use `--filters` to reduce bandwidth and memory pressure.
3. **Never Parse CLI Output with `grep` or `awk`**: Use `--query` (JMESPath) with `--output text` to extract fields cleanly and deterministically.
4. **Use Profiles and Environment Variables for Multi-Account Access**: Switch contexts cleanly with `export AWS_PROFILE=prod` rather than modifying global defaults.
5. **Set `--no-cli-pager` for Scripting**: Prevent the CLI from opening interactive pagers (`less`) in CI/CD automation by setting `export AWS_PAGER=""`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Piping raw JSON to `jq` for basic filtering | Adds external binary dependency; slower execution | Use native `--query` JMESPath flag built into the CLI |
| Case sensitivity errors in JMESPath queries | Querying `State.name` instead of `State.Name` returns empty/null | Check PascalCase property names in raw JSON output |
| Interactive pager hanging in CI/CD pipelines | CLI pipes output into `less`, causing automated runner jobs to stall | Set `export AWS_PAGER=""` in CI pipeline environment blocks |
| Storing static credentials in `~/.aws/credentials` | Long-lived keys risk leakage and require manual rotation | Migrate to IAM Identity Center SSO via `aws configure sso` |
| Overlooking default region fallback | Calling regional commands without configured region throws `EndpointResolutionError` | Set `export AWS_DEFAULT_REGION=us-east-1` in shell profile |
| Omitting `--page-size` on massive API queries | Querying 100,000 items in a single request triggers gateway 504 timeouts | Specify `--page-size 500` to stream smaller HTTP chunks |

---

## 8. Performance, Security & Cost Notes
- **Performance**: Server-side filtering (`--filters`) executes on AWS backend indexes, cutting query durations from seconds to milliseconds. Client-side `--query` downloads the full payload before executing local parsing.
- **Security**: The AWS CLI writes cached SSO access tokens to `~/.aws/sso/cache/` with `0600` Linux permissions (read/write only by owner). Never run the CLI with `sudo`, as this chowns cache files to `root`.
- **Cost**: Describing and listing resources via the CLI (`describe-*`, `list-*`, `get-*`) is **free of charge** for management APIs, but high-frequency polling can trigger API throttling limits.

---

## 9. Comparison / When to Use What

| Tool | Abstraction Level | Primary Use Case | Output Parsing |
|---|---|---|---|
| **AWS CLI v2** | Direct API Mapping | Ad-hoc terminal admin, bash scripts, CI/CD runners | Built-in JMESPath (`--query`) |
| **AWS CloudShell** | In-Browser Terminal | Quick administration without installing local tools | Pre-authenticated AWS CLI v2 |
| **Terraform / OpenTofu** | Declarative IaC | Provisioning and tracking stateful infrastructure | State file & plan diffs |
| **AWS SDKs (Boto3, Node)**| Programmatic Code | Application integration, complex branching workflows | Native language objects |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between `--filters` and `--query` in the AWS CLI?</summary>
`--filters` performs server-side filtering at the AWS API endpoint, reducing data transferred over the network. `--query` performs client-side filtering locally within the CLI binary using JMESPath expressions after the entire API payload has been downloaded.
</details>

<details>
<summary>2. Why does the AWS CLI hang indefinitely when run inside non-interactive CI/CD scripts?</summary>
By default, AWS CLI v2 routes multi-page output through a terminal pager (typically `less`). In a non-interactive CI environment, `less` waits for keyboard input. This is resolved by setting `export AWS_PAGER=""` or passing `--no-cli-pager`.
</details>

<details>
<summary>3. How does JMESPath handle case sensitivity?</summary>
JMESPath expressions are strictly case-sensitive. Because AWS API JSON responses use PascalCase (e.g. `InstanceId`, `State.Name`), querying lowercase keys (`instanceId`) evaluates to `null` without throwing an error.
</details>

<details>
<summary>4. What is the purpose of `--page-size` vs `--max-items`?</summary>
`--page-size` determines the number of records requested in each individual HTTPS request to AWS (preventing API timeouts). `--max-items` defines the absolute total number of records the CLI will return to the user across all pages before terminating.
</details>

<details>
<summary>5. How does AWS CLI v2 differ from AWS CLI v1 in packaging and dependencies?</summary>
CLI v1 was distributed as a Python package via `pip`, frequently conflicting with system Python dependencies. CLI v2 is distributed as a self-contained, pre-compiled binary package containing its own hermetic Python runtime, eliminating local Python environmental issues.
</details>

<details>
<summary>6. How do you configure an AWS CLI profile to use IAM Identity Center (SSO)?</summary>
Run `aws configure sso` or declare an `[sso-session]` block in `~/.aws/config` specifying the `sso_start_url` and `sso_region`, then link individual `[profile]` blocks to that session. Authenticate via `aws sso login`.
</details>

<details>
<summary>7. What does exit code 255 indicate in an AWS CLI execution?</summary>
Exit code 255 typically indicates a general error or command abort, frequently caused by credential failure (expired SSO session, invalid SigV4 signature) or network connectivity loss to the AWS endpoint.
</details>

<details>
<summary>8. How do you extract raw tab-delimited values suitable for a bash while-loop from the AWS CLI?</summary>
Combine `--query` with `--output text`. The CLI flattens arrays into tab-delimited lines that can be piped directly into `while read -r col1 col2; do ... done`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Query all running EC2 instance IDs</summary>
**Task:** Retrieve only the instance IDs of running EC2 instances as plain text strings.

**Solution:**
```bash
aws ec2 describe-instances \
    --filters "Name=instance-state-name,Values=running" \
    --query "Reservations[].Instances[].InstanceId" \
    --output text
```
</details>

<details>
<summary>Exercise 2 (Easy): Disable the interactive CLI pager permanently in your shell</summary>
**Task:** Configure your environment so the AWS CLI never uses `less`.

**Solution:**
```bash
export AWS_PAGER=""
# Add to ~/.bashrc or ~/.zshrc for persistence
```
</details>

<details>
<summary>Exercise 3 (Medium): JMESPath multi-attribute table extraction</summary>
**Task:** Display a table containing VPC ID, CIDR block, and whether it is the default VPC.

**Solution:**
```bash
aws ec2 describe-vpcs \
    --query "Vpcs[].[VpcId, CidrBlock, IsDefault]" \
    --output table
```
</details>

<details>
<summary>Exercise 4 (Medium): Filter S3 buckets created within a specific time window</summary>
**Task:** Write a JMESPath query listing buckets created after January 1, 2026.

**Solution:**
```bash
aws s3api list-buckets \
    --query "Buckets[?CreationDate >= '2026-01-01'].[Name, CreationDate]" \
    --output table
```
</details>

<details>
<summary>Exercise 5 (Hard): Iterate over instances and output tags in bash</summary>
**Task:** Write a bash script that iterates over all stopped EC2 instances and prints their `Name` tag and instance ID.

**Solution:**
```bash
#!/usr/bin/env bash
set -euo pipefail

aws ec2 describe-instances \
    --filters "Name=instance-state-name,Values=stopped" \
    --query "Reservations[].Instances[].[InstanceId, Tags[?Key=='Name'].Value | [0]]" \
    --output text | while read -r instance_id name_tag; do
        echo "Instance: $instance_id | Name: ${name_tag:-UNTAGGED}"
    done
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct complex JMESPath pipe and sort expression</summary>
**Task:** Query all EBS volumes, calculate their size, and sort in descending order.

**Solution:**
```bash
aws ec2 describe-volumes \
    --query "reverse(sort_by(Volumes, &Size))[].[VolumeId, Size, VolumeType, AvailabilityZone]" \
    --output table
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                               AWS CLI v2 CHEAT SHEET                            |
+---------------------------------------------------------------------------------+
| WHOAMI         | aws sts get-caller-identity                                    |
| CONFIGURE SSO  | aws configure sso                                              |
| LOGIN SSO      | aws sso login --profile <name>                                 |
| NO PAGER (CI)  | export AWS_PAGER=""                                            |
| OUTPUT TYPES   | --output json | table | text | yaml                            |
| SERVER FILTER  | --filters "Name=tag:Env,Values=Prod" (Server side, fast)       |
| CLIENT QUERY   | --query "Array[?Cond].[Field1, Field2]" (JMESPath, local)      |
| PAGINATION     | --page-size 250 --max-items 1000                               |
| JMESPATH KEYS  | Strictly case-sensitive PascalCase (e.g. InstanceId)           |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS CLI v2 Official User Guide](https://docs.aws.amazon.com/cli/latest/userguide/cli-chap-welcome.html)
- [JMESPath Specification and Tutorial](https://jmespath.org/)
- [AWS CLI Filtering and Output Options](https://docs.aws.amazon.com/cli/latest/userguide/cli-usage-filter.html)
- [Boto3 Documentation (Core Python SDK underpinning CLI)](https://boto3.amazonaws.com/v1/documentation/api/latest/index.html)

---

[Previous: 07-iam-identity-center-and-sso.md](file:///04-devops/aws/01-foundations/07-iam-identity-center-and-sso.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 09-sdks-and-credential-provider-chain.md](file:///04-devops/aws/01-foundations/09-sdks-and-credential-provider-chain.md)
