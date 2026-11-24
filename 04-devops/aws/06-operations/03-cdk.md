# AWS Cloud Development Kit (CDK v2): Programmable Cloud Infrastructure

> **Module**: `06-operations`  
> **Topic**: Modern IaC, Construct Levels (L1/L2/L3), Bootstrapping, Synthesis & CDK-Nag Security  
> **Target Audience**: Cloud Engineers, Full-Stack Developers, Platform Architects  
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

While declarative YAML and JSON templates in CloudFormation provide standardized infrastructure modeling, authoring large-scale architectures in raw YAML leads to massive verbosity. A basic multi-AZ VPC with public and private subnets, NAT Gateways, and route tables requires over 500 lines of repetitive YAML boilerplate. Furthermore, YAML lacks native loops, conditionals, object-oriented abstractions, type-checking, package management, and automated unit testing frameworks.

**AWS Cloud Development Kit (AWS CDK v2)** is an open-source software development framework that lets developers define cloud application resources using familiar modern programming languages (TypeScript, Python, Java, Go, C#). With AWS CDK:
1. **Expressive High-Level Abstractions**: Define complex architectures in a few lines of code (e.g., `new ec2.Vpc(this, 'Vpc')` automatically generates 400+ lines of CloudFormation with subnets, NATs, and routing).
2. **First-Class Software Engineering**: Benefit from IDE autocomplete, compile-time type-safety, refactoring tools, and standard package managers (`npm`, `pip`).
3. **Automated Unit Testing & Policy Enforcement**: Test infrastructure using standard testing frameworks (Jest, pytest) and enforce enterprise security policies using **cdk-nag**.

---

## 2. Core Concepts

### 2.1 The Construct Programming Model
The fundamental building block in AWS CDK is a **Construct**. Constructs exist at three distinct levels of abstraction:
1. **L1 Constructs (CfnPrimitives)**:
   - Direct, 1-to-1 mappings of underlying CloudFormation resource specifications (e.g., `s3.CfnBucket`).
   - Every property in CloudFormation YAML is mapped verbatim. No defaults or higher-level helpers are added.
2. **L2 Constructs (AWS-Curated Defaults)**:
   - Rich, object-oriented AWS-managed constructs (e.g., `s3.Bucket`, `ec2.Vpc`, `lambda.Function`).
   - Encapsulate AWS best practices, automated least-privilege IAM roles, security defaults, and convenient helper methods (e.g., `bucket.grantRead(myLambda)` automatically configures IAM policies without writing raw JSON).
3. **L3 Constructs (Patterns / Solutions)**:
   - Multi-service architectural patterns packaged together (e.g., `aws-ecs-patterns.ApplicationLoadBalancedFargateService` provisions an ALB, Fargate Service, Target Group, Security Groups, and Route 53 records in a single statement).

### 2.2 App, Stack, and Construct Hierarchy
- **App**: The root container for your CDK application; represents one or more Stacks.
- **Stack**: The deployable unit mapping directly to a single AWS CloudFormation stack.
- **Constructs**: Children of Stacks; forms a hierarchical Construct Tree.

### 2.3 CDK Lifecycle Stages
1. **Construction**: Instantiating constructs in code (e.g., `new Stack()`).
2. **Preparation**: Resolving cross-construct references and executing aspects.
3. **Synthesis (`cdk synth`)**: Compiles the construct tree down to pure, declarative AWS CloudFormation templates and asset bundles.
4. **Deployment (`cdk deploy`)**: Uploads synthesized templates and assets to S3/ECR and calls CloudFormation deployment APIs.

### 2.4 CDK Bootstrapping
- Before deploying a CDK application to an AWS account/region, the environment must be **bootstrapped** (`cdk bootstrap`).
- Provisions an S3 bucket (for CloudFormation templates and Lambda zip assets), an ECR repository (for Docker container images), and IAM execution roles assumed by CDK during deployments.

### 2.5 Security Governance with cdk-nag
- An open-source CDK tool that validates your synthesized CloudFormation templates against security benchmarks (AWS Solutions, HIPAA, NIST 800-53, PCI-DSS).
- Fails the build if resources violate best practices (e.g., unencrypted S3 buckets, open security groups, wildcards in IAM policies).

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                              AWS CDK COMPILATION & DEPLOYMENT LIFECYCLE                           |
|                                                                                                   |
|  [DevOps Engineer / TypeScript Code]                                                              |
|  - Defines L2/L3 Constructs: new Vpc(this, 'Vpc'); new Bucket(this, 'S3');                        |
|         |                                                                                         |
|         v (Executes: cdk synth)                                                                   |
|  +---------------------------------------------------------------------------------------------+  |
|  | JSII RUNTIME INTEROPERABILITY LAYER                                                         |  |
|  | (Translates TypeScript, Python, Java, or Go into the core TypeScript construct engine)        |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | CDK SYNTHESIS ENGINE & CDK-NAG SCAN                                                          |  |
|  | 1. Resolves Construct Tree & Evaluates Dependencies                                         |  |
|  | 2. Executes cdk-nag Aspects: Validates against PCI-DSS & AWS Solutions Rules                 |  |
|  | 3. Emits Cloud Assembly Directory (`cdk.out/`):                                             |  |
|  |    - `MyStack.template.json` (Pure Declarative CloudFormation YAML/JSON)                    |  |
|  |    - Asset manifests (Lambda zip bundles, Docker build context)                             |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Executes: cdk deploy)                                                                  |
|  +---------------------------------------------------------------------------------------------+  |
|  | CDK BOOTSTRAP ENVIRONMENT (AWS Target Account)                                              |  |
|  | 1. Uploads Lambda assets to Bootstrap S3 Bucket                                             |  |
|  | 2. Pushes Docker container images to Bootstrap ECR Repository                               |  |
|  | 3. Assumes `cdk-hnb659fds-cfn-exec-role` IAM Deployment Role                                |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v                                                                                         |
|  [AWS CloudFormation Service Control Plane executes Stack creation/update]                       |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 CLI Commands for CDK v2

```bash
# 1. Initialize a new CDK project in TypeScript
npx cdk init app --language typescript

# 2. Bootstrap target AWS environment (Account & Region)
npx cdk bootstrap aws://123456789012/us-east-1

# 3. Preview synthesized CloudFormation template
npx cdk synth

# 4. Compare local code against deployed CloudFormation stack
npx cdk diff

# 5. Deploy stack to AWS
npx cdk deploy --require-approval broadening
```

---

## 5. Practical Examples

### 5.1 Complete Multi-Tier Serverless API Stack with CDK v2 (TypeScript)

```typescript
import * as cdk from 'aws-cdk-lib';
import * as s3 from 'aws-cdk-lib/aws-s3';
import * as dynamodb from 'aws-cdk-lib/aws-dynamodb';
import * as lambda from 'aws-cdk-lib/aws-lambda';
import * as apigateway from 'aws-cdk-lib/aws-apigateway';
import { Construct } from 'constructs';

export class ServerlessApiStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    // 1. S3 Storage Bucket with auto-encryption & versioning
    const documentBucket = new s3.Bucket(this, 'DocumentBucket', {
      versioned: true,
      encryption: s3.BucketEncryption.S3_MANAGED,
      blockPublicAccess: s3.BlockPublicAccess.BLOCK_ALL,
      removalPolicy: cdk.RemovalPolicy.RETAIN,
    });

    // 2. DynamoDB Table (Pay-per-request)
    const ordersTable = new dynamodb.Table(this, 'OrdersTable', {
      partitionKey: { name: 'orderId', type: dynamodb.AttributeType.STRING },
      billingMode: dynamodb.BillingMode.PAY_PER_REQUEST,
      removalPolicy: cdk.RemovalPolicy.DESTROY, // For dev/test
    });

    // 3. Lambda Function
    const apiHandler = new lambda.Function(this, 'ApiHandler', {
      runtime: lambda.Runtime.NODEJS_20_X,
      handler: 'index.handler',
      code: lambda.Code.fromInline(`
        exports.handler = async (event) => {
          return { statusCode: 200, body: JSON.stringify({ message: "Hello from CDK!" }) };
        };
      `),
      environment: {
        BUCKET_NAME: documentBucket.bucketName,
        TABLE_NAME: ordersTable.tableName,
      },
    });

    // 4. Grant Least-Privilege IAM Permissions natively
    documentBucket.grantReadWrite(apiHandler);
    ordersTable.grantReadWriteData(apiHandler);

    // 5. REST API Gateway
    const api = new apigateway.RestApi(this, 'OrderServiceApi', {
      restApiName: 'Order Service',
      deployOptions: {
        stageName: 'prod',
        throttlingRateLimit: 500,
        throttlingBurstLimit: 1000,
      },
    });

    const ordersResource = api.root.addResource('orders');
    ordersResource.addMethod('POST', new apigateway.LambdaIntegration(apiHandler));
  }
}
```

### 5.2 L3 Pattern: Autoscaled Application Load Balanced Fargate Service

```typescript
import * as cdk from 'aws-cdk-lib';
import * as ec2 from 'aws-cdk-lib/aws-ec2';
import * as ecs from 'aws-cdk-lib/aws-ecs';
import * as ecsPatterns from 'aws-cdk-lib/aws-ecs-patterns';
import { Construct } from 'constructs';

export class FargateServiceStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    // Provisions VPC with 2 public & 2 private subnets + NAT Gateways
    const vpc = new ec2.Vpc(this, 'FargateVpc', { maxAzs: 2 });

    const cluster = new ecs.Cluster(this, 'FargateCluster', { vpc });

    // Single statement provisions ALB, Security Groups, Fargate Task, and Target Group
    const fargateService = new ecsPatterns.ApplicationLoadBalancedFargateService(this, 'WebService', {
      cluster,
      cpu: 512,
      memoryLimitMiB: 1024,
      desiredCount: 2,
      taskImageOptions: {
        image: ecs.ContainerImage.fromRegistry('public.ecr.aws/ecs-sample-image/amazon-ecs-sample:latest'),
        containerPort: 80,
      },
      publicLoadBalancer: true,
    });

    // Configure Auto-scaling on CPU
    const scaling = fargateService.service.autoScaleTaskCount({ maxCapacity: 10 });
    scaling.scaleOnCpuUtilization('CpuScaling', { targetUtilizationPercent: 70 });
  }
}
```

### 5.3 Enforcing Compliance with cdk-nag Aspects

```typescript
import { App, Aspects } from 'aws-cdk-lib';
import { AwsSolutionsChecks, NagSuppressions } from 'cdk-nag';
import { ServerlessApiStack } from './serverless-api-stack';

const app = new App();
const stack = new ServerlessApiStack(app, 'ProdServerlessStack');

// Apply AWS Solutions Security Pack across the entire CDK application
Aspects.of(app).add(new AwsSolutionsChecks({ verbose: true }));

// Suppress false-positives with auditable business justification
NagSuppressions.addStackSuppressions(stack, [
  {
    id: 'AwsSolutions-IAM4',
    reason: 'AWS Managed Policy AWSLambdaBasicExecutionRole is permitted for cloudwatch logging.',
  },
]);
```

### 5.4 Automated Infrastructure Unit Testing with Jest

```typescript
import { App } from 'aws-cdk-lib';
import { Template, Match } from 'aws-cdk-lib/assertions';
import { ServerlessApiStack } from '../lib/serverless-api-stack';

describe('ServerlessApiStack Unit Tests', () => {
  const app = new App();
  const stack = new ServerlessApiStack(app, 'TestStack');
  const template = Template.fromStack(stack);

  test('S3 Bucket Must Have Versioning and Public Access Blocked', () => {
    template.hasResourceProperties('AWS::S3::Bucket', {
      VersioningConfiguration: { Status: 'Enabled' },
      PublicAccessBlockConfiguration: {
        BlockPublicAcls: true,
        BlockPublicPolicy: true,
        IgnorePublicAcls: true,
        RestrictPublicBuckets: true,
      },
    });
  });

  test('DynamoDB Table uses Pay-Per-Request Billing', () => {
    template.hasResourceProperties('AWS::DynamoDB::Table', {
      BillingMode: 'PAY_PER_REQUEST',
    });
  });

  test('API Gateway Count Matches Exactly 1', () => {
    template.resourceCountIs('AWS::ApiGateway::RestApi', 1);
  });
});
```

---

## 6. Best Practices

1. **Prefer L2 Constructs Over L1 Primitives**: L2 constructs encapsulate security best practices and least-privilege IAM configuration. Use L1 (`Cfn...`) only when an AWS feature was launched so recently that an L2 construct does not yet exist.
2. **Never Generate New Secrets Inside CDK Code**: Never hardcode database passwords in TypeScript. Use `secretsmanager.Secret` to generate and manage dynamic passwords in AWS Secrets Manager.
3. **Use Grants Instead of Custom IAM Policies**: Use built-in grant helper methods (e.g., `bucket.grantRead(fn)`, `table.grantWriteData(fn)`). CDK automatically crafts exact least-privilege IAM statements.
4. **Enforce cdk-nag in CI/CD**: Run `cdk-nag` checks during `cdk synth`. Treat security warnings as build-breaking errors to catch misconfigurations before deployment.
5. **Separate Stateless and Stateful Stacks**: Place databases (RDS, DynamoDB) and storage (S3) in a stateful stack with `removalPolicy: RemovalPolicy.RETAIN`. Keep compute and APIs in a stateless stack with `DESTROY`, allowing rapid teardown without data risk.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Forgetting to Bootstrap Target Account | Running `cdk deploy` on a fresh account without `cdk bootstrap`. | Deployment fails with `SSM parameter /cdk-bootstrap/... not found`. | Run `npx cdk bootstrap aws://<account>/<region>` first. |
| Using Standard Programming Control Flow | Using dynamic `if/else` on CloudFormation tokens at runtime. | Code evaluates token string literal instead of actual value. | Use CloudFormation `CfnCondition` or compile-time flags. |
| Hardcoding Physical Names | Specifying `bucketName: "my-bucket"` on S3 constructs. | Updates requiring replacement fail with `BucketAlreadyExists`. | Omit explicit physical names; let CDK generate unique hashes. |
| Unintentional Resource Deletion on Destroy | Leaving default `removalPolicy` on stateful resources. | `cdk destroy` deletes production database and all records. | Set `removalPolicy: cdk.RemovalPolicy.RETAIN` on stateful constructs. |
| Tight Coupling via Stack References | Stack A passes an object reference directly into Stack B. | CloudFormation export collision; Stack A cannot be updated or destroyed. | Pass parameters via SSM Parameter Store or use nested stacks. |
| Monolithic Construct Files | Putting 50 resources in a single 2,000-line stack file. | Unmaintainable code; breaks clean modular design. | Decompose into custom reusable L3 construct classes. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Hot Reloading
- **CDK Watch (`cdk watch`)**: For serverless developers, `cdk watch` monitors local code changes and hot-swaps Lambda code and Step Functions definitions directly via AWS APIs without running a full CloudFormation stack update, reducing iteration cycle time from 90 seconds to under 3 seconds.

### 8.2 Security
- **Strict Least-Privilege IAM**: CDK's `.grant*` methods construct fine-grained IAM policies targeting exact resource ARNs and specific action subsets (e.g., `s3:GetObject` rather than `s3:*`).

### 8.3 Cost Breakdown
- **AWS CDK CLI**: 100% Free and open-source.
- **Synthesized Resources**: Billed at standard AWS pay-as-you-go rates.

---

## 9. Comparison Table

| Dimension | AWS CDK v2 | Raw CloudFormation (YAML) | HashiCorp Terraform |
|---|---|---|---|
| **Language** | TypeScript, Python, Java, Go | YAML / JSON | HCL (HashiCorp Config Lang)|
| **Abstraction Level** | High (L2/L3 Constructs) | Low (Raw 1:1 Resources) | Moderate (Reusable Modules) |
| **Logic / Loops** | Standard programming loops | Not supported (limited Fn::ForEach)| Dynamic blocks, for_each |
| **Type Checking** | Compile-time strong typing | Runtime validation in cloud | Static validation via CLI |
| **Unit Testing** | Native Jest, Pytest assertions | Limited to cfn-lint | Terratest (Go) / test framework |
| **Underlying Engine** | CloudFormation | CloudFormation | Terraform State Engine |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the three levels of Constructs in AWS CDK, and what are their use cases?</strong></summary>

1. **L1 (Layer 1 - Cfn Constructs)**: Direct 1-to-1 mappings of CloudFormation resources (e.g., `CfnBucket`). They are automatically generated from CloudFormation specifications. Used when new AWS features have not yet received an L2 abstraction, or when you need exact parity with raw CloudFormation properties.
2. **L2 (Layer 2 - Curated Constructs)**: AWS-managed abstractions (e.g., `Bucket`, `Vpc`, `Function`). They encapsulate security best practices, provide sensible defaults, manage IAM permissions automatically via `.grant*` methods, and eliminate boilerplate. This is the primary level used for day-to-day development.
3. **L3 (Layer 3 - Patterns)**: Multi-service architectural patterns (e.g., `ApplicationLoadBalancedFargateService`). They package multiple L2 constructs together into complete solutions (combining VPC, Load Balancer, ECS cluster, and Route 53 records in a single construct).
</details>

<details>
<summary><strong>Q2: What occurs during the cdk bootstrap command and why is it necessary?</strong></summary>

CDK applications often require auxiliary deployment assets—such as local Lambda code zip files, custom CloudFormation nested stack templates, and Docker container images. 
Running `cdk bootstrap`:
1. Provisions a dedicated CloudFormation stack named `CDKToolkit` in the target account and region.
2. Creates an **Amazon S3 bucket** for storing template files and Lambda zip artifacts.
3. Creates an **Amazon ECR repository** for caching and storing container image builds.
4. Provisions **IAM execution roles** that allow the CDK CLI to assume permissions for CloudFormation deployments, file publishing, and image uploads.
</details>

<details>
<summary><strong>Q3: How does AWS CDK handle IAM permissions differently than raw CloudFormation or Terraform?</strong></summary>

In raw CloudFormation or Terraform, developers must manually author verbose IAM policy JSON documents, detailing `Action`, `Effect`, `Resource` ARNs, and conditions. This frequently leads to developers using wildcards (`Action: "*"` and `Resource: "*"`) out of frustration, violating least-privilege.
AWS CDK provides **intent-based permission grants** (e.g., `bucket.grantRead(lambdaFn)` or `table.grantWriteData(lambdaFn)`). The construct framework automatically inspects the target resource ARN, extracts the minimal required IAM actions (e.g., `s3:GetObject`, `s3:GetBucketLocation`), and generates the least-privilege IAM role and policy automatically during synthesis.
</details>

<details>
<summary><strong>Q4: What is the difference between cdk synth, cdk diff, and cdk deploy?</strong></summary>

- **`cdk synth`**: Executes the TypeScript/Python application, builds the Construct Tree, executes security aspects (cdk-nag), and outputs synthesized CloudFormation templates into the `cdk.out/` directory. No changes are sent to AWS.
- **`cdk diff`**: Synthesizes the local code and compares the resulting CloudFormation template against the currently deployed stack in the AWS account, highlighting added, modified, or deleted resources in the terminal.
- **`cdk deploy`**: Synthesizes templates, uploads assets (Lambda code, Docker images) to the bootstrap S3/ECR locations, and triggers CloudFormation API calls to create or update the live stack.
</details>

<details>
<summary><strong>Q5: What is cdk-nag and how does it implement Policy as Code?</strong></summary>

`cdk-nag` is an open-source tool based on CDK **Aspects** (the Visitor pattern). When added to a CDK app, it visits every synthesized CloudFormation resource before deployment and validates its configuration against industry compliance packs (AWS Solutions, HIPAA, PCI-DSS, NIST 800-53). If a construct violates a rule (such as an S3 bucket missing encryption or a security group opening port 22 to `0.0.0.0/0`), `cdk-nag` generates an error and fails synthesis, preventing non-compliant code from ever reaching AWS APIs.
</details>

<details>
<summary><strong>Q6: Why is specifying explicit physical names on CDK constructs considered an anti-pattern?</strong></summary>

If you explicitly define physical names (e.g., `new s3.Bucket(this, 'Bucket', { bucketName: 'prod-orders' })`), you introduce two major issues:
1. **Replacement Failures**: If a property update requires resource replacement, CloudFormation attempts to provision the new resource with the same name before deleting the old one, failing with `ResourceAlreadyExistsException`.
2. **Environment Collisions**: The template cannot be deployed multiple times to the same account/region (e.g., for ephemeral feature-branch testing) because resource names must be globally unique.
Omit explicit names; CDK automatically appends unique alphanumeric hashes (e.g., `ProdOrdersStack-DocumentBucket12AB34CD`).
</details>

<details>
<summary><strong>Q7: How do you perform automated unit testing in AWS CDK?</strong></summary>

Using the `@aws-cdk/assertions` library alongside standard test runners like Jest or Pytest:
- **Fine-Grained Assertions**: Test specific resource properties (e.g., `template.hasResourceProperties('AWS::S3::Bucket', { VersioningConfiguration: { Status: 'Enabled' } })`).
- **Resource Counting**: Ensure only the expected number of resources exist (e.g., `template.resourceCountIs('AWS::EC2::VPC', 1)`).
- **Snapshot Testing**: Compares the entire synthesized CloudFormation JSON output against a stored baseline snapshot file to detect unintentional regressions.
</details>

<details>
<summary><strong>Q8: What is cdk watch and how does it accelerate development?</strong></summary>

`cdk watch` continuously monitors your local project directory for code changes. 
- For infrastructure changes (e.g., adding an S3 bucket), it triggers a standard CloudFormation deployment.
- For **asset code changes** (e.g., editing a Lambda handler, updating an ECS container code, or modifying a Step Functions state machine), it executes a **hot-swap** directly using AWS service APIs (e.g., `lambda:UpdateFunctionCode`), updating the live code in 2-3 seconds without waiting for a full CloudFormation change set and stack update.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Initialize a CDK Project and Inspect the Construct Tree</strong></summary>

**Goal**: Bootstrap and synthesize a fresh project.
1. Run `npx cdk init app --language typescript`.
2. Inspect `bin/` (App entrypoint) and `lib/` (Stack definition).
3. Run `npx cdk synth` and inspect the generated CloudFormation template in `cdk.out/`.
</details>

<details>
<summary><strong>Exercise 2: Create an S3 Bucket and Lambda with Least-Privilege Grants</strong></summary>

**Goal**: Master intent-based permissions.
1. Define an S3 bucket with public access blocked.
2. Define a Node.js Lambda function.
3. Call `bucket.grantRead(lambdaFn)`.
4. Run `npx cdk synth` and inspect the synthesized IAM policy to confirm exact S3 action scoping.
</details>

<details>
<summary><strong>Exercise 3: Deploy an L3 ECS Fargate Pattern</strong></summary>

**Goal**: Provision an autoscaled web tier with minimal code.
1. Install `@aws-cdk/aws-ecs-patterns`.
2. Instantiate `ApplicationLoadBalancedFargateService`.
3. Run `cdk deploy` and observe CDK provision the VPC, subnets, load balancer, security groups, and containers.
</details>

<details>
<summary><strong>Exercise 4: Enforce Security Guardrails with cdk-nag</strong></summary>

**Goal**: Catch security violations before deployment.
1. Install `cdk-nag`.
2. Add `Aspects.of(app).add(new AwsSolutionsChecks())` to `bin/`.
3. Intentionally define an unencrypted S3 bucket.
4. Run `cdk synth`; verify the build fails with an explicit rule violation.
</details>

<details>
<summary><strong>Exercise 5: Write a Jest Assertion Test</strong></summary>

**Goal**: Automate infrastructure verification.
1. Open `test/*.test.ts`.
2. Write a test asserting that all DynamoDB tables use `BillingMode: PAY_PER_REQUEST`.
3. Run `npm test` and verify test passes.
</details>

<details>
<summary><strong>Exercise 6: Test Rapid Iteration with cdk watch</strong></summary>

**Goal**: Experience sub-3-second Lambda hot-swapping.
1. Deploy a stack containing a Lambda function.
2. Start `npx cdk watch`.
3. Edit the Lambda return string in your local editor.
4. Observe `cdk watch` detect the change and hot-swap the code instantly without a CloudFormation update.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| AWS CDK V2 COMMON WORKFLOW CHEAT SHEET                                                            |
+--------------------------+-----------------------+------------------------------------------------+
| Command                  | Purpose               | Key Options                                    |
+--------------------------+-----------------------+------------------------------------------------+
| cdk bootstrap            | Provision AWS assets  | cdk bootstrap aws://<account>/<region>         |
| cdk synth                | Compile to CFN YAML   | --output <dir>, --quiet                        |
| cdk diff                 | Compare local vs live | Outputs added/modified/deleted resources       |
| cdk deploy               | Deploy stack to AWS   | --require-approval broadening, --all           |
| cdk watch                | Hot-swap code watch   | Continuously syncs assets without CFN overhead |
| cdk destroy              | Teardown stack        | Cleans up all non-retained resources           |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Construct Model**: L1 (Raw), L2 (Best-practice default), L3 (Complete architectural solutions).
- **Grants**: Use `.grantRead()` / `.grantWrite()` to automate least-privilege IAM policies.
- **cdk-nag**: Enforce enterprise security compliance during compilation.

---

## 13. Further Reading

- [AWS CDK v2 Official Developer Guide](https://docs.aws.amazon.com/cdk/v2/guide/home.html)
- [AWS CDK API Reference (v2)](https://docs.aws.amazon.com/cdk/api/v2/)
- [cdk-nag GitHub Repository](https://github.com/cdklabs/cdk-nag)
- [Construct Hub: Community & AWS Constructs](https://constructs.dev/)
- [06-operations/02-cloudformation.md](file:///04-devops/aws/06-operations/02-cloudformation.md)
