---
topic: "Terraform & AWS CDK Production Snippets Cheatsheet"
module: "10-cheatsheets"
track: "aws"
difficulty: "reference"
last-tested: "2026-11"
tags:
  - "cheatsheet"
  - "terraform"
  - "cdk"
  - "iac"
  - "opentofu"
  - "snippets"
---

# Terraform & AWS CDK Production Snippets Cheatsheet

A copy-paste reference card for Terraform (HCL) remote state backends, provider defaults, and AWS CDK v2 (TypeScript) constructs and patterns.

---

## 1. Terraform Production Foundation

### 1.1 Secure S3 Backend with DynamoDB State Locking

```hcl
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.40"
    }
  }

  backend "s3" {
    bucket         = "enterprise-terraform-state-us-east-1"
    key            = "workloads/production/terraform.tfstate"
    region         = "us-east-1"
    dynamodb_table = "terraform-state-lock"
    encrypt        = true
  }
}

provider "aws" {
  region = "us-east-1"

  # Applies immutable organizational tags to EVERY resource automatically
  default_tags {
    tags = {
      ManagedBy   = "Terraform"
      Environment = "Production"
      CostCenter  = "CC-1044"
      Repository  = "https://github.com/my-org/my-infra"
    }
  }
}
```

### 1.2 Refactoring State Without Recreating Resources (`moved` block)

```hcl
# Use 'moved' blocks to rename resources or modules without destroying live infrastructure
moved {
  from = aws_security_group.legacy_sg
  to   = aws_security_group.app_ingress_sg
}

moved {
  from = module.old_vpc
  to   = module.core_network.module.vpc
}
```

---

## 2. AWS CDK v2 (TypeScript) Production Patterns

### 2.1 Essential CDK CLI Commands

```bash
cdk bootstrap aws://123456789012/us-east-1   # Initialize CDK assets bucket
cdk synth                                     # Synthesize CloudFormation template
cdk diff                                      # Review drift and resource changes
cdk deploy --require-approval broadening      # Deploy stack
cdk deploy --hotswap                          # Fast code-only update for Lambda/ECS (Dev only!)
cdk destroy                                   # Tear down stack resources
```

### 2.2 Complete Microservice Stack (VPC, Lambda, DynamoDB)

```typescript
import * as cdk from 'aws-cdk-lib';
import * as ec2 from 'aws-cdk-lib/aws-ec2';
import * as lambda from 'aws-cdk-lib/aws-lambda';
import * as dynamodb from 'aws-cdk-lib/aws-dynamodb';
import { Construct } from 'constructs';

export class CoreMicroserviceStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    // 1. VPC across 2 AZs with 1 NAT Gateway
    const vpc = new ec2.Vpc(this, 'AppVpc', {
      maxAzs: 2,
      natGateways: 1,
      subnetConfiguration: [
        { name: 'Public', subnetType: ec2.SubnetType.PUBLIC, cidrMask: 24 },
        { name: 'Private', subnetType: ec2.SubnetType.PRIVATE_WITH_EGRESS, cidrMask: 20 },
      ],
    });

    // 2. DynamoDB Table with Point-in-Time Recovery
    const table = new dynamodb.Table(this, 'OrdersTable', {
      partitionKey: { name: 'PK', type: dynamodb.AttributeType.STRING },
      sortKey: { name: 'SK', type: dynamodb.AttributeType.STRING },
      billingMode: dynamodb.BillingMode.PAY_PER_REQUEST,
      pointInTimeRecovery: true,
      removalPolicy: cdk.RemovalPolicy.RETAIN, // Prevents accidental data deletion on stack destroy
    });

    // 3. Lambda Function running on Graviton (ARM64)
    const handler = new lambda.Function(this, 'OrderHandler', {
      runtime: lambda.Runtime.NODEJS_20_X,
      architecture: lambda.Architecture.ARM_64,
      handler: 'index.handler',
      code: lambda.Code.fromAsset('lambda'),
      memorySize: 1024,
      timeout: cdk.Duration.seconds(10),
      environment: {
        TABLE_NAME: table.tableName,
      },
    });

    // 4. Principle of Least Privilege Grant
    table.grantReadWriteData(handler);
  }
}
```

### 2.3 Automated Security Linting with `cdk-nag`

```typescript
import { App, Aspects } from 'aws-cdk-lib';
import { AwsSolutionsChecks } from 'cdk-nag';

const app = new App();
new CoreMicroserviceStack(app, 'ProductionStack');

// Enforce AWS Solutions Best Practices & Security Standards during synthesis
Aspects.of(app).add(new AwsSolutionsChecks({ verbose: true }));
app.synth();
```
