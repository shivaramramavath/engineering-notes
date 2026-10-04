# Infrastructure as Code: CDK (and SAM)

Clicking through the console is fine for learning. It is a bad way to run anything real, because clicks aren't reviewable, repeatable or reversible. **Infrastructure as Code (IaC)** describes your AWS resources in files you version, review in pull requests, test, and deploy the same way every time, to dev, staging and prod.

On AWS everything ultimately runs through **CloudFormation**. The tools in this note generate or extend CloudFormation templates:

| Tool | What you write | Good for |
|---|---|---|
| **AWS CDK** | TypeScript/Python/Java/C#/Go (this repo: **TypeScript**) | General infrastructure; loops, functions, types, reuse |
| **AWS SAM** | Short YAML (a CloudFormation extension) | Serverless apps (Lambda, API Gateway, DynamoDB, SQS) with great local tooling |
| **CloudFormation** | JSON/YAML directly | The underlying engine; verbose, but you must be able to read it |
| **Terraform / OpenTofu** | HCL | Multi-cloud or existing Terraform shops (not covered here) |

Pick **CDK** as the default for a TypeScript developer; pick **SAM** if your app is mostly Lambda and you value its simplicity and `sam local`. They aren't exclusive. SAM can even build/test CDK-defined Lambdas.

Prerequisites: [AWS CLI](../01-foundations/04-aws-cli.md) (credentials/profiles), [IAM](../01-foundations/03-iam.md).

---

## How it works: CloudFormation underneath

```
CDK app (TypeScript) ──synth──► CloudFormation template (JSON) ──deploy──► CloudFormation stack
                                                                              │
                                                       creates/updates/deletes real resources
```

- A **stack** is a unit of deployment: a set of resources managed together.
- On update CloudFormation computes a **change set** (what will be added/modified/replaced/deleted) and applies it. If something fails it **rolls back** to the previous state.
- Each resource has a **logical ID** in the template (e.g. `OrdersTable1234ABCD`). **Changing a logical ID tells CloudFormation to delete the old resource and create a new one.** For a database, that means data loss. Keep this in mind whenever you rename or move constructs.
- Some property changes need **replacement** (a new physical resource), others are in-place. The docs mark each property's update behaviour, and `cdk diff` shows replacements clearly.
- **Drift** means someone changed a resource outside CloudFormation. Detect it with drift detection; prefer fixing the code over hand edits.

---

## CDK concepts

```
App
 └── Stack  (→ one CloudFormation stack)
      └── Construct  (building block, may contain more constructs)
```

| Level | What | Example |
|---|---|---|
| **L1** (`Cfn*`) | 1:1 with a CloudFormation resource; every property, no defaults | `CfnBucket` |
| **L2** | Opinionated, with sensible defaults, helper methods and **grants** | `s3.Bucket`, `lambda.Function`, `dynamodb.Table` |
| **L3 / patterns** | Multi-resource solutions | `ApplicationLoadBalancedFargateService` |

Use **L2** by default, dropping to L1 (or an "escape hatch" on the L2) when a feature isn't exposed.

### A real stack: Lambda + DynamoDB + HTTP API

```ts
// lib/orders-stack.ts
import * as cdk from "aws-cdk-lib";
import { Construct } from "constructs";
import * as dynamodb from "aws-cdk-lib/aws-dynamodb";
import * as lambda from "aws-cdk-lib/aws-lambda";
import { NodejsFunction } from "aws-cdk-lib/aws-lambda-nodejs";
import * as apigwv2 from "aws-cdk-lib/aws-apigatewayv2";
import { HttpLambdaIntegration } from "aws-cdk-lib/aws-apigatewayv2-integrations";

export class OrdersStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    const table = new dynamodb.Table(this, "Orders", {
      partitionKey: { name: "pk", type: dynamodb.AttributeType.STRING },
      sortKey: { name: "sk", type: dynamodb.AttributeType.STRING },
      billingMode: dynamodb.BillingMode.PAY_PER_REQUEST,
      pointInTimeRecoverySpecification: { pointInTimeRecoveryEnabled: true },
      removalPolicy: cdk.RemovalPolicy.RETAIN,        // never delete data just because the stack is deleted
    });

    const createOrder = new NodejsFunction(this, "CreateOrder", {
      entry: "src/handlers/create-order.ts",          // bundled with esbuild, TypeScript supported
      runtime: lambda.Runtime.NODEJS_22_X,
      architecture: lambda.Architecture.ARM_64,
      timeout: cdk.Duration.seconds(10),
      environment: { TABLE_NAME: table.tableName },
    });

    table.grantReadWriteData(createOrder);            // generates a least-privilege IAM policy for you

    const api = new apigwv2.HttpApi(this, "Api");
    api.addRoutes({
      path: "/orders",
      methods: [apigwv2.HttpMethod.POST],
      integration: new HttpLambdaIntegration("CreateOrderIntegration", createOrder),
    });

    new cdk.CfnOutput(this, "ApiUrl", { value: api.apiEndpoint });
  }
}
```

```ts
// bin/app.ts
import * as cdk from "aws-cdk-lib";
import { OrdersStack } from "../lib/orders-stack";

const app = new cdk.App();
new OrdersStack(app, "Orders-dev", {
  env: { account: process.env.CDK_DEFAULT_ACCOUNT, region: process.env.CDK_DEFAULT_REGION },
});
```

Things to notice:

- **Grants** (`table.grantReadWriteData(fn)`) are CDK's best feature: they write the IAM policy from the relationships between resources, which keeps permissions tight without hand-writing JSON ([IAM](../01-foundations/03-iam.md)).
- References between resources (`table.tableName`) become CloudFormation references, so ordering and wiring are automatic.
- **`RemovalPolicy.RETAIN`** on stateful resources (tables, buckets, databases) means `cdk destroy` won't delete your data. The default for many resources is to *retain* or *destroy* depending on the type, so set it deliberately.
- Passing `env` with an explicit account/region makes the stack **environment-specific**, enabling lookups (VPCs, hosted zones) and some features. Without it the stack is environment-agnostic.

---

## The CDK workflow

```bash
npm install -g aws-cdk          # the CLI (or use npx); versions are released frequently
cdk init app --language typescript

cdk bootstrap aws://111122223333/ap-south-1   # ONCE per account+region
cdk synth                       # generate CloudFormation into cdk.out/
cdk diff                        # what will change? (always read this)
cdk deploy                      # create/update (shows IAM/security changes for approval)
cdk destroy                     # delete the stack(s)
```

| Command | Purpose |
|---|---|
| `cdk bootstrap` | Creates the **CDKToolkit** stack: an S3 bucket and ECR repo for assets, plus IAM roles used for deployments. Required once per environment before the first deploy. |
| `cdk diff` | Shows a diff against the deployed stack, including IAM policy and security group changes. **Run it before every deploy, and in CI on every PR.** |
| `cdk deploy` | Synthesises, uploads assets, and runs CloudFormation. Prompts before broadening IAM/security permissions unless told otherwise. |
| `cdk watch` / `--hotswap` | **Dev only.** Hot-swaps Lambda code and similar changes directly, bypassing CloudFormation for speed. Never use it for production, since it creates drift. |
| `cdk import` | Bring existing resources under CDK/CloudFormation management. |
| `cdk refactor` | Move or rename resources between stacks/constructs *without* replacing them (maps old logical IDs to new). Check the docs for current constraints. |
| `cdk diagnose` | Analyses a failed deployment and summarises the root cause. |
| `cdk migrate` | Generate a CDK app from existing resources or templates. |

The CDK is under constant, fast development (the library and CLI release roughly weekly), so pin versions in `package.json`, upgrade deliberately, and read release notes. The toolkit CLI and `aws-cdk-lib` are versioned separately but must be compatible.

### Keeping stacks healthy

- **Split stateful from stateless.** Put databases, buckets and user pools in a long-lived "data" stack and application code in another. Redeploying the app then can't threaten the data.
- **Cross-stack references** (exports/imports) create hard dependencies: you can't change or delete an exported value while another stack imports it. Pass values explicitly through construct props within one app, and prefer SSM parameters or explicit config for loose coupling across apps.
- **Resource limits**: a stack is capped at 500 resources (CDK-heavy constructs consume many). If you approach it, split the stack.
- **Context lookups** (`Vpc.fromLookup`, etc.) cache results in `cdk.context.json`. **Commit it** so synth is deterministic in CI.
- **Never put secrets in code or templates.** Reference them by ARN or use dynamic references (`{{resolve:secretsmanager:...}}`) so the value is fetched at deploy time and not stored in the template ([Security and Secrets](./03-security-and-secrets.md)).
- **Names:** let CDK generate physical names unless you must fix them. Hard-coded names block replacement updates ("resource already exists") and can't be reused across environments.
- **Environments as parameters:** instantiate the same stack class per environment (`Orders-dev`, `Orders-prod`) with different props, rather than copy-pasting.

### Testing CDK

```ts
import { Template } from "aws-cdk-lib/assertions";
import * as cdk from "aws-cdk-lib";
import { OrdersStack } from "../lib/orders-stack";

test("table has PITR and is retained", () => {
  const stack = new OrdersStack(new cdk.App(), "T");
  const t = Template.fromStack(stack);
  t.hasResource("AWS::DynamoDB::Table", {
    DeletionPolicy: "Retain",
    Properties: { PointInTimeRecoverySpecification: { PointInTimeRecoveryEnabled: true } },
  });
});
```

Fine-grained assertions on important properties (encryption on, public access blocked, retention policies) beat brittle full-template snapshots. CDK also supports synth-time validation plugins and linting (e.g. cdk-nag) to flag insecure configurations early.

---

## SAM: the serverless-focused alternative

SAM (Serverless Application Model) is a CloudFormation **transform** that gives you short resource types for serverless apps, plus a CLI for building, testing locally, and deploying.

```yaml
# template.yaml
AWSTemplateFormatVersion: "2010-09-09"
Transform: AWS::Serverless-2016-10-31

Globals:
  Function:
    Runtime: nodejs22.x
    Architectures: [arm64]
    Timeout: 10

Resources:
  OrdersTable:
    Type: AWS::DynamoDB::Table
    DeletionPolicy: Retain
    Properties:
      BillingMode: PAY_PER_REQUEST
      AttributeDefinitions:
        - { AttributeName: pk, AttributeType: S }
        - { AttributeName: sk, AttributeType: S }
      KeySchema:
        - { AttributeName: pk, KeyType: HASH }
        - { AttributeName: sk, KeyType: RANGE }

  CreateOrderFn:
    Type: AWS::Serverless::Function
    Properties:
      Handler: index.handler
      CodeUri: src/create-order/
      Environment:
        Variables:
          TABLE_NAME: !Ref OrdersTable
      Policies:
        - DynamoDBCrudPolicy: { TableName: !Ref OrdersTable }
      Events:
        PostOrder:
          Type: HttpApi
          Properties: { Path: /orders, Method: post }

Outputs:
  ApiUrl:
    Value: !Sub "https://${ServerlessHttpApi}.execute-api.${AWS::Region}.amazonaws.com"
```

```bash
sam build                          # package code/dependencies
sam local invoke CreateOrderFn -e event.json   # run a function locally in a container
sam local start-api                # local API Gateway emulation
sam deploy --guided                # first deploy; writes samconfig.toml
sam sync --watch                   # dev loop: fast sync of code changes (dev only)
sam logs --stack-name my-stack --tail
```

Use **SAM** when the app is mostly serverless and a compact YAML file plus `sam local` suit you. Use **CDK** when you need real programming (loops, conditions, shared constructs), non-serverless resources (VPCs, ECS, RDS), or strong typing and refactoring support. Both produce the same kind of stack, and both support Lambda, API Gateway and EventBridge integration. CDK adds more abstraction; SAM stays close to the template.

---

## Choosing and conventions

| Question | Guidance |
|---|---|
| Repo layout | One repo per application with its infra (`/infra` or `/lib`), or a separate platform repo for shared network/accounts |
| Environments | Same code, different stack instances/accounts; promote through dev → staging → prod |
| Deployments | From **CI**, not laptops ([CI/CD to AWS](./02-ci-cd-to-aws.md)) |
| Approvals | `cdk diff` in PRs; require review of IAM/security changes |
| State | CloudFormation holds state for you (no state file to manage, unlike Terraform) |

---

## Common mistakes

- **Renaming/moving a construct** (changing its logical ID) and replacing a stateful resource. Always read `cdk diff` and look for `[-]`/`[~]` on tables and buckets (use `cdk refactor` for planned renames).
- **Forgetting `RemovalPolicy.RETAIN`** on data, then deleting a stack.
- **Hard-coding physical names**, causing "already exists" errors on replacement.
- **Skipping `cdk bootstrap`** (or running an outdated bootstrap) and getting asset/role errors.
- **Using `--hotswap` / `cdk watch` beyond dev**, leaving drift.
- **Deploying from laptops** with admin credentials.
- **Secrets in code/templates/context.**
- **Not committing `cdk.context.json`**, so CI synth differs from local.
- **One giant stack** (500-resource limit, slow deploys, big blast radius).
- **Heavy cross-stack exports**, which create deployment deadlocks.
- **Editing resources in the console** (drift) instead of the code.
- Mixing **CDK/CLI versions** carelessly.

---

## Debugging

| Symptom | What to check |
|---|---|
| `This stack uses assets, so the toolkit stack must be deployed` | Run `cdk bootstrap` for that account/region (or upgrade the bootstrap stack if the error mentions a version) |
| `Resource already exists` | A hard-coded name collides with an existing resource, or a prior failed/retained resource is still there |
| Stack in **`UPDATE_ROLLBACK_FAILED`** | Read stack **Events** for the first failure; fix the cause and use *Continue update rollback* (skipping the stuck resource only as a last resort) |
| `ROLLBACK_COMPLETE` on first create | The stack can't be updated; delete it and redeploy after fixing the error |
| Deploy hangs | A resource is slow to create (CloudFront, RDS, NAT Gateway) or waiting on a dependency; check Events |
| `Access Denied` during deploy | The deploying identity can't assume the CDK bootstrap roles, or lacks permission for a resource; check CloudTrail |
| Cryptic template errors | `cdk synth` and inspect `cdk.out/*.template.json`; use `cdk diagnose` after a failed deploy |
| Unexpected replacement | Look at `cdk diff` for properties requiring replacement; check docs "Update requires: Replacement" |
| Resource drifted | Run drift detection; re-deploy or `cdk deploy --revert-drift` if available in your CLI version |

```bash
aws cloudformation describe-stack-events --stack-name Orders-dev \
  --query 'StackEvents[?contains(ResourceStatus,`FAILED`)].[LogicalResourceId,ResourceStatusReason]' --output table
```

---

## Quick Summary

- **IaC = infrastructure in version control**: reviewable, repeatable, reversible. On AWS it runs through **CloudFormation** stacks.
- **CDK** (TypeScript) builds CloudFormation from code: **App → Stack → Construct**, use **L2** constructs and **grants** for least-privilege IAM.
- Workflow: **`bootstrap` once → `synth` → `diff` → `deploy`**; read the diff, especially replacements and IAM changes.
- Protect state: **`RemovalPolicy.RETAIN`**, separate stateful/stateless stacks, don't rename carelessly, no hard-coded names.
- **`--hotswap`/`watch` is dev-only**; deploy to real environments from CI.
- **SAM** is the compact alternative for serverless apps, with `sam local` for fast iteration.
- Keep secrets out of templates; commit `cdk.context.json`; test key properties with assertions.

**Next:** [CI/CD to AWS](./02-ci-cd-to-aws.md)
