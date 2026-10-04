# AWS CLI

The AWS CLI is a command-line client for the AWS API. Every console action has a CLI equivalent, which makes it the fastest way to inspect resources, script repetitive work, and understand what AWS is actually doing. It also underpins most tooling (CDK, SAM and CI pipelines all rely on the same credential and profile mechanisms you set up here).

This note covers **AWS CLI v2** (the current major version; v1 is the older Python-package version).

---

## Install

```bash
# Linux (x86_64)
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
sudo ./aws/install

# macOS: use the official .pkg installer, or: brew install awscli
# Windows: official .msi installer, or: winget install Amazon.AWSCLI

aws --version
```

For ARM Linux use the `aarch64` zip instead. If the installer URL or steps differ on your platform, the [official install guide](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html) is authoritative.

---

## How the CLI finds credentials

Understanding this saves hours of "why is it using the wrong account?". The CLI looks in a **fixed order** and uses the first source that gives credentials. In rough order:

1. Command-line options (`--profile`, `--region`)
2. **Environment variables** (`AWS_ACCESS_KEY_ID`, `AWS_PROFILE`, ...)
3. Profile settings in `~/.aws/config` (assume-role, SSO, web identity)
4. Shared credentials file `~/.aws/credentials`
5. Container or **instance role** credentials (ECS task role, EC2 instance profile)

Consequences worth remembering:

- Environment variables **override** your profile. A stale `AWS_ACCESS_KEY_ID` in your shell will silently win.
- On EC2/ECS/Lambda you generally don't configure anything: the role is picked up automatically.

Two files, both in `~/.aws/`:

| File | Holds |
|---|---|
| `config` | Region, output format, SSO and assume-role settings (`[profile name]` sections) |
| `credentials` | Long-lived keys only (`[name]` sections), which you should avoid creating |

---

## Profiles and regions

A **profile** is a named bundle of credentials source + region + output format. Use one per account/role.

```bash
aws configure list-profiles
aws configure list                       # which credentials/region am I resolving right now?
aws s3 ls --profile dev                  # use a profile for one command
export AWS_PROFILE=dev                   # or for the whole shell session
export AWS_REGION=ap-south-1             # region override
```

Region resolution: `--region` flag → `AWS_REGION` / `AWS_DEFAULT_REGION` → profile's `region`. If none is set, many commands fail with an error about a missing region.

---

## SSO login (recommended for humans)

If your account uses **IAM Identity Center** (see [IAM](./03-iam.md)), you never create access keys. You log in through the browser and the CLI caches short-lived credentials.

Interactive setup:

```bash
aws configure sso
# prompts: session name, SSO start URL, SSO region, then pick account + role,
# then profile name, default region, output format
```

This writes something like the following to `~/.aws/config`:

```ini
[sso-session my-org]
sso_start_url = https://my-org.awsapps.com/start
sso_region = ap-south-1
sso_registration_scopes = sso:account:access

[profile dev]
sso_session = my-org
sso_account_id = 111122223333
sso_role_name = DeveloperAccess
region = ap-south-1
output = json
```

Daily use:

```bash
aws sso login --profile dev        # opens browser, refreshes the session
aws sts get-caller-identity --profile dev
aws sso logout
```

When the session expires you'll see an error about an expired or missing token. Just run `aws sso login` again. Multiple profiles can share one `sso-session`, so you log in once for all of them.

Note: the SSO start URL and region come from your organisation's Identity Center setup (ask your admin). The `sso_region` is where Identity Center lives, which may differ from where you deploy.

---

## Assume-role profiles

To use a role in another account (or a more privileged role) from an existing identity:

```ini
[profile prod-deploy]
role_arn = arn:aws:iam::444455556666:role/DeployRole
source_profile = dev
region = ap-south-1
```

```bash
aws s3 ls --profile prod-deploy    # CLI calls sts:AssumeRole for you and caches the result
```

`source_profile` must itself resolve to valid credentials (e.g. your SSO profile). The role's trust policy must allow that identity.

---

## Command anatomy

```
aws <service> <operation> [--parameters] [global options]
aws s3 ls
aws ec2 describe-instances --region ap-south-1
aws dynamodb get-item --table-name users --key '{"id":{"S":"123"}}'
```

Help is built in and is the best reference:

```bash
aws help
aws ec2 help
aws ec2 describe-instances help
```

Some services have a **high-level** command (`aws s3`) and a **low-level** one that maps 1:1 to the API (`aws s3api`). Use `s3` for copying/syncing, `s3api` for everything else.

---

## Everyday commands

```bash
# Who am I? (run this first when anything is weird)
aws sts get-caller-identity

# S3
aws s3 ls
aws s3 cp ./file.txt s3://my-bucket/path/file.txt
aws s3 sync ./dist s3://my-bucket/site --delete     # careful: --delete removes remote extras

# EC2
aws ec2 describe-instances \
  --filters Name=instance-state-name,Values=running \
  --query 'Reservations[].Instances[].[InstanceId,InstanceType,PrivateIpAddress]' \
  --output table

# Lambda
aws lambda list-functions --query 'Functions[].FunctionName'
aws lambda invoke --function-name my-fn --payload '{"hello":"world"}' \
  --cli-binary-format raw-in-base64-out out.json

# Logs: tail a Lambda's log group live
aws logs tail /aws/lambda/my-fn --follow

# CloudFormation (and CDK stacks)
aws cloudformation describe-stacks --query 'Stacks[].[StackName,StackStatus]' --output table
```

(The `--cli-binary-format` flag is needed in v2 when passing a raw JSON payload to `lambda invoke`; without it v2 expects base64.)

---

## Controlling output: `--query`, `--output`, `--filters`

Raw output is large JSON. Three tools trim it:

| Tool | Where it runs | Use for |
|---|---|---|
| `--filters` / `--filter-expression` etc. | **Server-side**, per service | Reduce what AWS returns (cheaper, faster) |
| `--query` | **Client-side**, [JMESPath](https://jmespath.org/) | Reshape/select fields from the response |
| `--output json\|table\|text\|yaml` | Client-side | Format for humans or scripts |

```bash
aws ec2 describe-instances \
  --query 'Reservations[].Instances[?State.Name==`running`].InstanceId[]' \
  --output text
```

Notes:

- JMESPath string literals use **backticks**, as above (watch shell quoting).
- For anything heavy in scripts, `--output json` piped into `jq` is also common.
- The CLI paginates automatically. `--no-paginate` disables it; `--max-items` / `--page-size` tune it.
- `aws <service> <cmd> --generate-cli-skeleton` prints a JSON template of the parameters, which is handy with `--cli-input-json file://input.json`.

---

## Useful flags for safe and diagnosable work

```bash
--dry-run       # supported by many EC2 operations: checks permissions without doing it
--debug         # full request/response trace, including which credentials were resolved
--no-cli-pager  # stop output opening in `less`
--cli-auto-prompt  # interactive autocomplete for commands and params (v2)
```

To disable the pager globally: `export AWS_PAGER=""`.

---

## Common mistakes

- **Wrong profile or account.** Run `aws sts get-caller-identity` before any destructive command. Many "I deleted prod" stories begin with a stale `AWS_PROFILE`.
- **Wrong region.** `aws ec2 describe-instances` returning nothing usually means "nothing in *this* region". Check with `aws configure list` or pass `--region`.
- **Stale env vars overriding profiles.** `env | grep AWS_` and unset what you don't intend.
- **Expired SSO token** shows up as auth errors on previously-working commands. Run `aws sso login`.
- **Long-lived keys in `~/.aws/credentials`** for human use. Prefer SSO; if you must use keys, scope them narrowly, enable MFA, and rotate them.
- **Committing credentials.** Never commit `~/.aws` contents or `.env` files with keys. If a key leaks, deactivate it in IAM at once.
- **`s3 sync --delete` without checking direction.** Source and destination order matters. Run with `--dryrun` first.
- **JMESPath quoting problems.** Single-quote the whole query in bash and use backticks for literals.

---

## Debugging checklist

```bash
aws --version
aws configure list                  # resolved profile, region, credential source
aws configure list-profiles
aws sts get-caller-identity         # confirms identity and account
aws <command> --debug 2>&1 | less   # see the exact credential source and API call
```

If it is an `AccessDenied`, move on to the IAM debugging steps in [IAM](./03-iam.md).

---

## Quick Summary

- The CLI is a thin client over the AWS API. Learn `aws <service> <operation>` and lean on `aws ... help`.
- Credentials are resolved in a **fixed order**. Env vars beat profiles, which is the source of many mysteries.
- For humans: **IAM Identity Center + `aws configure sso` + `aws sso login`**, with no long-lived keys. For cross-account: **assume-role profiles** with `role_arn` and `source_profile`.
- Always know your **identity and region** (`get-caller-identity`, `configure list`) before running anything that changes state.
- Use `--filters` (server-side) and `--query`/`--output` (client-side) to tame output; use `--dry-run` and `--debug` when unsure.

**Next:** [EC2](../02-compute/01-ec2.md)