# HashiCorp Terraform & OpenTofu on AWS: Production Engineering

> **Module**: `06-operations`  
> **Topic**: AWS Provider, S3 & DynamoDB Backends, Modules, State Manipulation & Isolation  
> **Target Audience**: DevOps Engineers, Platform Architects, Infrastructure Leads  
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

While AWS CloudFormation and AWS CDK are tightly integrated first-party tools, many enterprise platform engineering teams require an infrastructure orchestration framework that spans beyond AWS to multi-cloud platforms, SaaS providers (Datadog, Cloudflare, PagerDuty), and on-premises virtualization (VMware, Kubernetes). 

**HashiCorp Terraform** (and its open-source fork **OpenTofu**) is the industry-standard declarative Infrastructure as Code tool. Authored in HashiCorp Configuration Language (HCL), Terraform allows teams to:
1. **Model Entire Ecosystems**: Provision AWS VPCs, configure Datadog monitors, create GitHub repositories, and register Cloudflare DNS records in a single cohesive execution graph.
2. **Standardize Modular Architectures**: Package corporate infrastructure standards into reusable, versioned modules shared across business units.
3. **Control State Granularly**: Inspect, import, migrate, and manipulate infrastructure state directly via robust CLI tooling (`terraform import`, `terraform state mv`).

---

## 2. Core Concepts

### 2.1 The AWS Provider
- The plugin responsible for understanding AWS API interactions, translating HCL resource declarations into AWS SDK calls.
- Supports assume-role configurations, multi-region aliases (e.g., managing Route 53 global records in `us-east-1` alongside workloads in `eu-west-1`), and default tags applied to every resource automatically.

### 2.2 Remote State Backend (S3 + DynamoDB)
- **`terraform.tfstate`**: A JSON document mapping declared HCL resources to real physical AWS ARNs, IDs, and attributes.
- **S3 Storage**: Stores the state file with versioning and KMS server-side encryption.
- **DynamoDB State Locking**: Prevents concurrent execution corruption by acquiring an exclusive write lock (using primary key `LockID`) before every state-modifying command.

### 2.3 Modular Architecture
- A **Module** is a container for multiple resources used together.
- Every Terraform directory is a module:
  - *Root Module*: The primary working directory where `terraform apply` is executed.
  - *Child Modules*: Reusable modules called from root configurations via `source = "git::..."` or local file paths.
  - Follows standard file structure: `main.tf` (resources), `variables.tf` (inputs), `outputs.tf` (return values), and `versions.tf` (providers).

### 2.4 State Isolation Strategies: Workspaces vs Directory-per-Environment
1. **Terraform Workspaces**:
   - Single directory managing multiple state files (`default`, `staging`, `production`) inside the same S3 backend prefix.
   - *Risk*: All environments share the exact same code and provider versions; high risk of blast-radius accidents.
2. **Directory-per-Environment (Recommended)**:
   - Dedicated folders: `environments/dev/`, `environments/staging/`, `environments/prod/`.
   - Each directory has an isolated state file, independent backend key, and separate access permissions.

### 2.5 State Management Commands
- `terraform state list`: Lists all resources currently tracked in the state file.
- `terraform state show <resource>`: Dumps the live state attributes for a specific resource.
- `terraform import <resource> <aws-id>`: Brings existing, manually created AWS resources under Terraform management without recreating them.
- `terraform state mv <source> <destination>`: Renames a resource in code without destroying and recreating the live AWS object.
- `terraform state rm <resource>`: Removes a resource from state tracking without deleting it from AWS.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        TERRAFORM / OPENTOFU AWS EXECUTION GRAPH ENGINE                            |
|                                                                                                   |
|  [DevOps Engineer]                                                                                |
|         |                                                                                         |
|         v (Executes: terraform apply)                                                             |
|  +---------------------------------------------------------------------------------------------+  |
|  | 1. BACKEND INITIALIZATION & DYNAMODB STATE LOCK ACQUISITION                                 |  |
|  | - Queries DynamoDB: `LockTable.put_item(LockID="corp/networking/terraform.tfstate")`        |  |
|  | - Fetches latest encrypted state file from Amazon S3 backend                                |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | 2. REFRESH & GRAPH EVALUATION                                                               |  |
|  | - Calls AWS APIs (e.g., `ec2:DescribeInstances`) to reconcile live reality with state file   |  |
|  | - Constructs Dependency Graph (DAG) evaluating `depends_on`, references, and module inputs  |  |
|  | - Generates Plan: Computes exact delta (Add (+), Modify (~), Destroy (-))                   |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (User / CI Approves Plan)                                                               |
|  +---------------------------------------------------------------------------------------------+  |
|  | 3. AWS PROVIDER API DISPATCHER                                                              |  |
|  | - Dispatches concurrent AWS SDK calls across independent graph branches:                     |  |
|  |   +-----------------------+   +-----------------------+   +-----------------------+         |  |
|  |   | aws:CreateSubnet (AZA)|   | aws:CreateSubnet (AZB)|   | aws:CreateSecurityGrp |         |  |
|  |   +-----------------------+   +-----------------------+   +-----------------------+         |  |
|  | - Waits for completion & captures physical AWS IDs (e.g., `subnet-0123456789`)              |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | 4. STATE PERSISTENCE & LOCK RELEASE                                                         |  |
|  | - Encrypts and uploads updated `terraform.tfstate` to Amazon S3                             |  |
|  | - Deletes lock item in DynamoDB: `LockTable.delete_item(LockID=...)`                        |  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 CLI Core Commands

```bash
# 1. Initialize working directory, download providers, configure backend
terraform init -upgrade

# 2. Format and validate syntax
terraform fmt -recursive
terraform validate

# 3. Generate execution plan and save to binary file
terraform plan -out=tfplan.binary

# 4. Apply saved execution plan
terraform apply tfplan.binary

# 5. Import existing unmanaged EC2 instance into Terraform state
terraform import aws_instance.web i-0123456789abcdef0
```

---

## 5. Practical Examples

### 5.1 Root Module with Multi-Region AWS Provider Aliases & Default Tags

```hcl
# versions.tf
terraform {
  required_version = ">= 1.6.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.30"
    }
  }

  backend "s3" {
    bucket         = "enterprise-tf-state-us-east-1"
    key            = "core/networking/terraform.tfstate"
    region         = "us-east-1"
    dynamodb_table = "enterprise-tf-locks"
    encrypt        = true
  }
}

# Primary Provider (Ireland)
provider "aws" {
  region = "eu-west-1"

  default_tags {
    tags = {
      Environment = "production"
      ManagedBy   = "Terraform"
      Repository  = "https://github.com/corp/infrastructure"
    }
  }
}

# Alias Provider (N. Virginia: Required for CloudFront ACM Certificates & Route 53)
provider "aws" {
  alias  = "us_east_1"
  region = "us-east-1"

  default_tags {
    tags = {
      Environment = "production"
      ManagedBy   = "Terraform"
    }
  }
}
```

### 5.2 Reusable VPC Subnetting Child Module

```hcl
# modules/vpc/variables.tf
variable "vpc_cidr" {
  type        = string
  description = "The IPv4 CIDR block for the VPC"
}

variable "environment" {
  type        = string
  description = "Deployment environment name"
}

# modules/vpc/main.tf
resource "aws_vpc" "main" {
  cidr_block           = var.vpc_cidr
  enable_dns_hostnames = true
  enable_dns_support   = true

  tags = {
    Name = "${var.environment}-vpc"
  }
}

resource "aws_subnet" "public" {
  count                   = 2
  vpc_id                  = aws_vpc.main.id
  cidr_block              = cidrsubnet(var.vpc_cidr, 4, count.index)
  availability_zone       = data.aws_availability_zones.available.names[count.index]
  map_public_ip_on_launch = true

  tags = {
    Name = "${var.environment}-public-subnet-${count.index + 1}"
  }
}

data "aws_availability_zones" "available" {
  state = "available"
}

# modules/vpc/outputs.tf
output "vpc_id" {
  value       = aws_vpc.main.id
  description = "The ID of the created VPC"
}

output "public_subnet_ids" {
  value       = aws_subnet.public[*].id
  description = "List of public subnet IDs"
}
```

### 5.3 Calling the Module in Root Configuration

```hcl
module "production_network" {
  source = "./modules/vpc"

  vpc_cidr    = "10.200.0.0/16"
  environment = "production"
}

# Output exposed from root module
output "network_vpc_id" {
  value = module.production_network.vpc_id
}
```

### 5.4 Refactoring State Safely with `moved` Blocks (Zero Downtime)

```hcl
# When refactoring code to place a standalone resource into a module,
# standard rename destroys and recreates the resource!
# Use declarative 'moved' blocks to rename state without modifying live AWS infrastructure:

moved {
  from = aws_security_group.web_sg
  to   = module.security.aws_security_group.web_sg
}
```

---

## 6. Best Practices

1. **Always Use DynamoDB State Locking**: Never rely solely on S3 for state storage. Without DynamoDB locking, two simultaneous CI/CD pipeline runs can corrupt or overwrite remote state.
2. **Use Declarative `moved` Blocks for Refactoring**: Never delete and recreate resources when moving code into modules. Use Terraform 1.1+ `moved` blocks so state is updated without terminating production resources.
3. **Pin Provider and Terraform Versions**: Always specify exact provider version constraints (`~> 5.30`) in `versions.tf`. Breaking provider updates could alter default behaviors and generate unexpected diffs.
4. **Isolate Environments by Directory, Not Workspaces**: Maintain separate directories for `dev`, `staging`, and `prod` with separate state keys and IAM credentials, eliminating accidental cross-environment blast radius.
5. **Never Hardcode Secrets in Plaintext HCL**: Pass sensitive variables via environment variables (`TF_VAR_db_password`) or fetch them dynamically using the AWS Secrets Manager data source.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Plaintext Secrets Committed in State | Storing database passwords in HCL variables or outputs. | Password is saved in plaintext JSON inside `terraform.tfstate`. | Restrict S3 state access; mark variables and outputs as `sensitive = true`. |
| Running Apply Without a Saved Plan | Executing `terraform apply` directly in automated CI pipelines. | The applied changes may differ from what was reviewed in PR if state drifted. | Run `terraform plan -out=tfplan` in PR, and `terraform apply tfplan` on merge. |
| State Lock Deadlock | CI job crashes or is killed midway through an apply. | DynamoDB lock item persists; future runs blocked with `Error acquiring lock`. | Inspect lock info and run `terraform force-unlock <LOCK-ID>`. |
| Modifying Resources in Console | Engineer edits security group in console. | Terraform detects drift on next run and attempts to revert changes. | Manage all changes through Git; or import modified state back into HCL. |
| Using Workspaces for Production Isolation | Sharing one code repository with `terraform workspace select prod`. | A syntax bug in the shared file breaks production during dev testing. | Use directory isolation: `environments/dev/` vs `environments/prod/`. |
| Forgetting `ignore_changes` on Autoscaling | Autoscaling group changes `desired_capacity` dynamically. | Terraform plan continually attempts to reset capacity back to initial value. | Add `lifecycle { ignore_changes = [desired_capacity] }`. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Concurrency
- **State Refresh Overhead**: On massive deployments (1,000+ resources), refreshing state takes several minutes. Accelerate development plans using `terraform plan -refresh=false` (use cautiously; never skip refresh in CI).
- **Concurrency Tuning**: Increase default parallelism with `terraform apply -parallelism=30` to speed up independent resource creation.

### 8.2 Security & IAM
- **Least-Privilege CI/CD Roles**: The CI/CD pipeline (e.g., GitHub Actions) should assume a scoped IAM role using OIDC federation rather than storing long-lived static AWS Access Keys.

### 8.3 Cost Breakdown
- **Terraform / OpenTofu CLI**: 100% Free and open-source.
- **S3 State Storage & DynamoDB Lock Table**: Fractions of a cent per month (~$0.25/month).

---

## 9. Comparison Table

| Dimension | Terraform / OpenTofu | AWS CloudFormation | AWS CDK (v2) |
|---|---|---|---|
| **Ecosystem Reach** | Multi-Cloud & SaaS (AWS, GCP, Datadog)| AWS Native Only | AWS Native (Compiles to CFN) |
| **Language** | HCL (Declarative) | YAML / JSON (Declarative) | TypeScript, Python (Imperative) |
| **State Storage** | Explicit S3 + DynamoDB | AWS Managed Internal | AWS Managed Internal |
| **State Manipulation**| Powerful CLI (`import`, `mv`, `rm`) | Limited to Stack Imports | Inherited from CFN |
| **Refactoring Safety**| Native `moved` blocks | Rename triggers recreation | Rename triggers recreation |
| **Ecosystem Modules** | Huge Terraform Registry | AWS QuickStarts | Construct Hub |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How do you bring an existing, manually created AWS resource under Terraform management without downtime?</strong></summary>

1. Write the matching HCL resource block in your `.tf` file:
   ```hcl
   resource "aws_security_group" "legacy_sg" {
     name        = "legacy-web-sg"
     description = "Migrated from manual console"
     # Define properties matching live configuration
   }
   ```
2. Execute the import command:
   `terraform import aws_security_group.legacy_sg sg-0123456789abcdef0`
   Terraform calls AWS APIs, retrieves the live configuration, and writes the resource into the remote `terraform.tfstate` file.
3. Run `terraform plan` to verify that zero diff exists (`No changes. Your infrastructure matches the configuration.`).
</details>

<details>
<summary><strong>Q2: Why is storing Terraform state in Amazon S3 with DynamoDB locking essential for teams?</strong></summary>

The Terraform state file tracks resource bindings, metadata, and dependencies. 
- **Amazon S3** provides a centralized, durable, encrypted remote store with bucket versioning to recover from accidental corruption.
- **DynamoDB** provides distributed mutex locking. When an engineer or CI runner executes a plan or apply, Terraform creates an item with primary key `LockID` in DynamoDB. Any concurrent execution detects the active lock and aborts, preventing concurrent race conditions and state file corruption.
</details>

<details>
<summary><strong>Q3: What is the purpose of the moved block introduced in Terraform 1.1?</strong></summary>

Historically, if an engineer refactored code by renaming a resource (`aws_s3_bucket.data` to `aws_s3_bucket.storage`) or moving it into a child module (`module.storage.aws_s3_bucket.data`), Terraform would treat the old resource as deleted and the new resource as newly created, destroying live databases or buckets! 
The **`moved` block** provides declarative state refactoring:
```hcl
moved {
  from = aws_s3_bucket.data
  to   = module.storage.aws_s3_bucket.data
}
```
During the plan, Terraform updates the internal state pointer without generating any destroy/create actions on the physical AWS infrastructure.
</details>

<details>
<summary><strong>Q4: Why should production and development environments be separated by directory rather than Terraform workspaces?</strong></summary>

With **Workspaces**, all environments share the exact same code files and provider versions; only the state file is switched (`terraform workspace select prod`).
- A developer testing a new provider version or experimental HCL syntax for `dev` inadvertently changes the code file that will be evaluated for `prod`.
- With **Directory-per-Environment**, `environments/dev/` and `environments/prod/` maintain isolated code directories, independent versions, separate state keys, and distinct IAM credentials. A breaking change in dev code physically cannot impact production.
</details>

<details>
<summary><strong>Q5: What is the function of the lifecycle block and its arguments (create_before_destroy, prevent_destroy, ignore_changes)?</strong></summary>

The `lifecycle` block customizes resource lifecycle behaviors:
- **`create_before_destroy = true`**: Provisions the replacement resource *before* destroying the old one (crucial for zero-downtime ALB listener or ASG replacements).
- **`prevent_destroy = true`**: Emits an error and rejects any plan that would destroy the resource (essential protection for production databases).
- **`ignore_changes = [tags, desired_capacity]`**: Ignores specific attribute diffs modified outside of Terraform (such as autoscaling capacity updates or tags added by AWS systems).
</details>

<details>
<summary><strong>Q6: How do you handle sensitive data (like database master passwords) in Terraform outputs and state files?</strong></summary>

1. Mark outputs and variables with `sensitive = true`. This prevents Terraform from printing the secret in terminal logs or CI execution logs.
2. Note that `sensitive = true` does **NOT** encrypt the value in the `terraform.tfstate` file—it is still stored in plaintext JSON. Therefore, the S3 backend bucket must enforce strict IAM read permissions, bucket policies, and KMS encryption.
3. For maximum security, use dynamic credential generation (e.g., HashiCorp Vault) or AWS Secrets Manager's native `ManageMasterUserPassword` feature.
</details>

<details>
<summary><strong>Q7: What is the difference between count and for_each when provisioning multiple resources?</strong></summary>

- **`count`**: Provisions resources based on an integer index (`count.index`). If you pass a list `["a", "b", "c"]` and remove `"a"`, all subsequent resources shift index (`b` becomes 0, `c` becomes 1), causing Terraform to destroy and recreate resources unnecessarily.
- **`for_each`**: Provisions resources based on a map or set of unique keys. Resources are tracked by their explicit key (`aws_subnet.public["subnet-a"]`). Adding or removing items affects only the targeted key, without shifting or recreating other resources.
</details>

<details>
<summary><strong>Q8: What is the purpose of the terraform_remote_state data source?</strong></summary>

The `terraform_remote_state` data source allows a root module to read the root output values from an entirely separate, decoupled state file. For example, an application compute stack can read the `vpc_id` and `subnet_ids` exported by a core networking stack without merging both into a single giant state file.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Configure S3 & DynamoDB Remote State Backend</strong></summary>

**Goal**: Initialize a remote backend.
1. Create an S3 bucket with versioning and encryption.
2. Create a DynamoDB table `tf-locks` with primary key `LockID` (String).
3. Configure your `backend "s3"` block.
4. Run `terraform init` and verify the state file appears in S3.
</details>

<details>
<summary><strong>Exercise 2: Write and Publish a Local Child Module</strong></summary>

**Goal**: Encapsulate an S3 bucket with standard security rules.
1. Create directory `modules/secure-bucket/`.
2. Define a bucket enforcing TLS and blocking public access.
3. Instantiate the module twice from `main.tf` with different names.
4. Run `terraform apply` and verify both buckets are provisioned.
</details>

<details>
<summary><strong>Exercise 3: Import an Existing AWS Resource</strong></summary>

**Goal**: Bring unmanaged resources into state.
1. Create a security group manually in the AWS Console.
2. Write the corresponding `aws_security_group` resource in Terraform.
3. Execute `terraform import aws_security_group.imported <sg-id>`.
4. Run `terraform plan` to verify zero diff.
</details>

<details>
<summary><strong>Exercise 4: Test Safe Refactoring with a moved Block</strong></summary>

**Goal**: Rename a resource in code without destroying it.
1. Deploy an S3 bucket named `aws_s3_bucket.original`.
2. Rename the resource in HCL to `aws_s3_bucket.renamed`.
3. Add a `moved { from = aws_s3_bucket.original to = aws_s3_bucket.renamed }` block.
4. Run `terraform plan`; verify plan indicates state move with 0 to add, 0 to change, 0 to destroy.
</details>

<details>
<summary><strong>Exercise 5: Use for_each to Provision Multi-AZ Subnets</strong></summary>

**Goal**: Avoid index shifting risks of `count`.
1. Define a map variable of subnets: `{ "az-a" = "10.0.1.0/24", "az-b" = "10.0.2.0/24" }`.
2. Use `for_each = var.subnets` in `aws_subnet`.
3. Add a third subnet and verify only the new subnet is planned for creation.
</details>

<details>
<summary><strong>Exercise 6: Test DynamoDB State Locking and Force Unlock</strong></summary>

**Goal**: Handle lock deadlocks safely.
1. Run `terraform plan` and simulate an unexpected crash or terminal halt.
2. Note the Lock ID in the error message if another run is blocked.
3. Execute `terraform force-unlock <Lock-ID>`.
4. Verify subsequent operations proceed normally.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| TERRAFORM AWS WORKFLOW COMMANDS CHEAT SHEET                                                       |
+--------------------------+-----------------------+------------------------------------------------+
| Command                  | Purpose               | Key Flag / Option                              |
+--------------------------+-----------------------+------------------------------------------------+
| terraform init           | Install plugins/backnd| -upgrade, -reconfigure                         |
| terraform plan           | Compute state diff    | -out=tfplan, -detailed-exitcode                |
| terraform apply          | Execute API changes   | -auto-approve, -parallelism=20                 |
| terraform import         | Ingest live resource  | terraform import <resource.name> <physical-id> |
| terraform state list     | List tracked items    | Shows all resources in state                   |
| terraform state mv       | Rename state pointer  | terraform state mv <source> <dest>             |
| terraform force-unlock   | Release stuck lock    | terraform force-unlock <LockID>                |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Remote State**: S3 for encrypted storage + DynamoDB for write locking.
- **`for_each` > `count`**: Prevents index-shifting destruction bugs.
- **`moved` Blocks**: Enables safe refactoring without destroying production assets.

---

## 13. Further Reading

- [HashiCorp Terraform Documentation](https://developer.hashicorp.com/terraform/docs)
- [OpenTofu Official Documentation](https://opentofu.org/docs/)
- [Terraform AWS Provider Registry](https://registry.terraform.io/providers/hashicorp/aws/latest/docs)
- [Terraform Best Practices by Anton Babenko](https://www.terraform-best-practices.com/)
- [06-operations/01-infrastructure-as-code.md](file:///04-devops/aws/06-operations/01-infrastructure-as-code.md)
