# Amazon Simple Email Service (SES) & Enterprise Email Deliverability

> **Module**: `05-app-services`  
> **Topic**: Transactional Email, Easy DKIM, SPF, DMARC, Dedicated IPs & Deliverability Management  
> **Target Audience**: Cloud Engineers, DevOps Architects, Application Developers  
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

Sending high-volume email from self-hosted mail servers (e.g., Postfix, Sendmail) is fraught with operational challenges: managing complex IP warmup schedules, rotating encryption keys, handling feedback loops with major mailbox providers (Gmail, Outlook, Yahoo), and preventing your IP addresses from landing on global anti-spam blacklists (Spamhaus, Barracuda). A single compromised account sending spam can destroy the sender reputation of an entire enterprise IP block.

**Amazon Simple Email Service (Amazon SES)** is a cost-effective, flexible, and scalable email service that enables developers to send transactional, marketing, and newsletter emails from any application. Amazon SES:
1. **Guarantees High Deliverability**: Provides built-in support for industry authentication standards (**SPF**, **DKIM**, and **DMARC**).
2. **Protects Sender Reputation**: Continuously monitors bounce and complaint metrics, offering automated reputation dashboards and **Virtual Deliverability Manager**.
3. **Offers Flexible Ingestion**: Ingests email via SMTP interfaces, AWS SDKs, or high-throughput batch APIs.

---

## 2. Core Concepts

### 2.1 Domain & Identity Verification
- **Email Address Identity**: Verifies an individual sender address (`support@company.com`) via a verification link.
- **Domain Identity**: Verifies an entire root domain or subdomain (`company.com` or `mail.company.com`) using DNS TXT or CNAME records. Allows sending from any address under that domain without individual verifications.

### 2.2 Email Authentication Triad (SPF, DKIM, DMARC)
- **SPF (Sender Policy Framework)**: A DNS TXT record listing the authorized mail servers permitted to send email on behalf of your domain:
  `v=spf1 include:amazonses.com ~all`
- **DKIM (DomainKeys Identified Mail)**: Adds a cryptographic signature to the email header using an asymmetric 1024-bit or 2048-bit RSA key pair. The receiving mail server validates the signature against the public key published in your DNS.
  - *Easy DKIM*: Amazon SES generates three CNAME records that point to AWS-managed public keys, rotating keys automatically.
  - *BYODKIM (Bring Your Own DKIM)*: Allows enterprises to configure their own private keys.
- **DMARC (Domain-based Message Authentication, Reporting, and Conformance)**: A DNS policy instructing receiving mail servers what to do if an email fails SPF or DKIM validation (`p=none`, `p=quarantine`, or `p=reject`).

### 2.3 SES Sandbox Environment
- New AWS accounts begin in the **Amazon SES Sandbox**.
- In the sandbox:
  - You can only send email to **verified** recipient addresses or domains.
  - Maximum sending quota of **200 messages per 24 hours** at **1 message per second**.
- To send to arbitrary public recipients, you must submit a **Production Access Request** detailing use case, bounce management strategy, and recipient opt-in verification.

### 2.4 Reputation Metrics & Thresholds
Amazon SES actively monitors two critical sender reputation metrics:
- **Bounce Rate**: Must remain **below 5%**. Reaching **10%** causes AWS to place sending under review or pause account sending immediately.
- **Complaint Rate**: Must remain **below 0.1%** (1 complaint per 1,000 emails sent). Reaching **0.5%** results in immediate sending suspension.

### 2.5 Configuration Sets & Event Publishing
- **Configuration Sets**: Rules governing email sending behavior (e.g., tracking opens, clicks, deliveries, bounces, complaints).
- **Event Publishing**: Streams granular sending event metrics to Amazon Kinesis Firehose, Amazon SNS, or CloudWatch.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                            AMAZON SES EMAIL DELIVERY & REPUTATION PIPELINE                        |
|                                                                                                   |
|  [Application Service]                                                                            |
|         |                                                                                         |
|         v (SendEmail API: From: orders@company.com, To: buyer@gmail.com)                          |
|  +---------------------------------------------------------------------------------------------+  |
|  | AMAZON SES SENDING CONTROL PLANE                                                            |  |
|  | 1. Verified Identity Check (Is company.com domain verified?)                                |  |
|  | 2. Cryptographic DKIM Signing (Signs header with 2048-bit private key)                      |  |
|  | 3. Rate-Limiting & Quota Validation (Within daily sending volume and max send rate?)        |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Dispatched via Outbound SES Mail Transfer Agent - MTA)                                 |
|  +---------------------------------------------------------------------------------------------+  |
|  | RECIPIENT MAIL SERVER (e.g., Google Mail Server)                                            |  |
|  | 1. DNS Lookup: Fetches SPF record for company.com -> PASS                                   |  |
|  | 2. DNS Lookup: Fetches DKIM public key from CNAME -> Validates Signature -> PASS            |  |
|  | 3. DNS Lookup: Evaluates DMARC policy (_dmarc.company.com) -> PASS                          |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         +--- Scenario A: Delivered Cleanly ----------------------------------------------------+  |
|         |    Inbox Placement (Zero spam folder flag)                                           |  |
|         |                                                                                      |  |
|         +--- Scenario B: Hard Bounce (User mailbox does not exist) ----------------------------+  |
|         |    MTA returns SMTP 550 User Unknown                                                 |  |
|         |                                                                                      |  |
|         +--- Scenario C: Spam Complaint (Recipient clicks "Report Spam") ----------------------+  |
|              Feedback Loop (FBL) notice sent back to Amazon SES                                |  |
|                                                                                                   |
|         v (Feedback Returned to SES)                                                              |
|  +---------------------------------------------------------------------------------------------+  |
|  | SES EVENT PUBLISHING & REPUTATION FEEDBACK LOOP                                             |  |
|  | - Pushes Bounce / Complaint Event to Amazon SNS Topic                                       |  |
|  | - Lambda Consumer adds buyer@gmail.com to DynamoDB Account Suppression List                 |  |
|  | - Prevents future sending to that email address to protect sender reputation                |  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Verifying Domain Identity and DKIM via AWS CLI

```bash
# 1. Create a verified domain identity
aws sesv2 create-email-identity \
    --email-identity "company.com"

# 2. Configure Easy DKIM with 2048-bit RSA keys
aws sesv2 put-email-identity-dkim-signing-attributes \
    --email-identity "company.com" \
    --signing-attributes-origin "AWS_SES" \
    --next-signing-key-length "RSA_2048_BIT"

# 3. Retrieve DKIM CNAME tokens to publish in DNS
aws sesv2 get-email-identity \
    --email-identity "company.com" \
    --query "DkimAttributes.Tokens"
```

### 4.2 Sending Transactional Email via AWS CLI

```bash
aws sesv2 send-email \
    --from-email-address "orders@company.com" \
    --destination '{"ToAddresses": ["customer@example.com"]}' \
    --content '{
      "Simple": {
        "Subject": {"Data": "Your Order Confirmation #9021"},
        "Body": {
          "Html": {"Data": "<h1>Thank you for your order!</h1><p>Shipped via Express.</p>"},
          "Text": {"Data": "Thank you for your order! Shipped via Express."}
        }
      }
    }'
```

---

## 5. Practical Examples

### 5.1 Complete Domain Identity, Easy DKIM & Route 53 DNS Records (Terraform)

```hcl
# SES Domain Identity
resource "aws_ses_domain_identity" "main" {
  domain = var.domain_name
}

# SES Easy DKIM
resource "aws_ses_domain_dkim" "main" {
  domain = aws_ses_domain_identity.main.domain
}

# Route 53 DKIM CNAME Records (3 tokens generated by SES)
resource "aws_route53_record" "dkim" {
  count   = 3
  zone_id = var.hosted_zone_id
  name    = "${element(aws_ses_domain_dkim.main.dkim_tokens, count.index)}._domainkey.${var.domain_name}"
  type    = "CNAME"
  ttl     = 300
  records = ["${element(aws_ses_domain_dkim.main.dkim_tokens, count.index)}.dkim.amazonses.com"]
}

# Route 53 SPF TXT Record
resource "aws_route53_record" "spf" {
  zone_id = var.hosted_zone_id
  name    = var.domain_name
  type    = "TXT"
  ttl     = 300
  records = ["v=spf1 include:amazonses.com ~all"]
}

# Route 53 DMARC TXT Record (p=reject enforces strict quarantine)
resource "aws_route53_record" "dmarc" {
  zone_id = var.hosted_zone_id
  name    = "_dmarc.${var.domain_name}"
  type    = "TXT"
  ttl     = 300
  records = ["v=DMARC1; p=reject; rua=mailto:dmarc-reports@${var.domain_name}"]
}
```

### 5.2 Python Transactional Mailer with Template Rendering

```python
"""
Sends templated transactional emails using SESv2 client.
"""
import boto3
from botocore.exceptions import ClientError
import json

ses = boto3.client("sesv2", region_name="us-east-1")

def send_welcome_email(recipient: str, user_name: str, activation_url: str):
    try:
        response = ses.send_email(
            FromEmailAddress="no-reply@company.com",
            Destination={"ToAddresses": [recipient]},
            Content={
                "Template": {
                    "TemplateName": "UserWelcomeTemplate",
                    "TemplateData": json.dumps({
                        "name": user_name,
                        "link": activation_url
                    })
                }
            },
            ConfigurationSetName="production-tracking-set"
        )
        print(f"Email sent successfully! MessageId: {response['MessageId']}")
    except ClientError as e:
        print(f"Error sending email: {e.response['Error']['Message']}")
```

### 5.3 Automated Bounce & Complaint Suppression via SNS & Lambda

```python
"""
Lambda consumer capturing SES Bounces and Complaints, storing them in DynamoDB.
"""
import boto3
import json

dynamodb = boto3.resource("dynamodb")
suppression_table = dynamodb.Table("EmailSuppressionList")

def lambda_handler(event, context):
    for record in event["Records"]:
        sns_message = json.loads(record["Sns"]["Message"])
        notification_type = sns_message.get("notificationType")

        if notification_type == "Bounce":
            bounced_recipients = sns_message["bounce"]["bouncedRecipients"]
            for recipient in bounced_recipients:
                email = recipient["emailAddress"]
                suppression_table.put_item(
                    Item={"email": email, "reason": "HardBounce", "timestamp": sns_message["bounce"]["timestamp"]}
                )
                print(f"Added {email} to suppression list (Bounce).")

        elif notification_type == "Complaint":
            complained_recipients = sns_message["complaint"]["complainedRecipients"]
            for recipient in complained_recipients:
                email = recipient["emailAddress"]
                suppression_table.put_item(
                    Item={"email": email, "reason": "Complaint", "timestamp": sns_message["complaint"]["timestamp"]}
                )
                print(f"Added {email} to suppression list (Complaint).")
```

### 5.4 Configuration Set with Event Destination to CloudWatch

```hcl
resource "aws_ses_configuration_set" "tracking" {
  name = "production-tracking-set"
}

resource "aws_ses_event_destination" "cloudwatch" {
  name                   = "cloudwatch-metrics"
  configuration_set_name = aws_ses_configuration_set.tracking.name
  enabled                = true
  matching_types         = ["send", "reject", "bounce", "complaint", "delivery"]

  cloudwatch_destination {
    default_value  = "default"
    dimension_name = "ses_events"
    value_source   = "messageTag"
  }
}
```

---

## 6. Best Practices

1. **Verify Root Domain, Not Individual Addresses**: Verifying domain identities (`company.com`) allows dynamic sending from multiple service prefixes (`support@`, `billing@`, `notifications@`) without managing individual verification emails.
2. **Implement DMARC with `p=reject`**: Protect your brand from email spoofing and phishing attacks by enforcing strict DMARC alignment.
3. **Always Maintain a Suppression List**: Never attempt to resend emails to addresses that generated a hard bounce or spam complaint. Use SES Account-Level Suppression or an internal DynamoDB suppression database.
4. **Separate Transactional and Marketing Mailings**: Send critical transactional emails (password resets, invoices) from a dedicated subdomain (`mail.company.com`) on a dedicated IP pool, keeping marketing campaigns (`news.company.com`) isolated.
5. **Enable Virtual Deliverability Manager (VDM)**: An automated SES feature that optimizes inbox placement, diagnoses deliverability bottlenecks, and detects misconfigured DNS records.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Sending in Sandbox Mode | Account not moved to Production; sending to unverified recipients. | `MessageRejected`: Email address is not verified. | Submit Production Access Request in AWS SES Console. |
| Missing DMARC Record | Sending only SPF and DKIM without DMARC. | Gmail & Yahoo reject or flag mail as spam (2024+ sender requirement). | Publish a `_dmarc.<domain>` TXT record in DNS. |
| Ignoring Hard Bounces | Resending to invalid emails repeatedly. | Account Bounce Rate exceeds 10%; sending suspended by AWS. | Capture bounce events via SNS and automatically suppress failed emails. |
| Buying a Dedicated IP for Low Volume | Using a dedicated IP for $< 50,000$ emails/month. | Dedicated IP lacks sufficient volume; mailbox providers flag it cold. | Use SES Shared IP pools for low to moderate email volumes. |
| Using Shared Domain for Bulk Marketing | Marketing blast causes high complaints on corporate root domain. | Critical transactional emails (order confirmations) land in spam. | Isolate marketing into separate subdomains with distinct IP pools. |
| Custom MAIL FROM Domain SPF Misalignment | SPF passes for amazonses.com but fails domain alignment in DMARC. | DMARC validation fails. | Configure custom MAIL FROM domain (`bounce.company.com`) with MX and SPF. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Sending Throughput & Warmup
- **Shared IP Pools**: Amazon SES manages warming and reputation automatically across shared multi-tenant IP pools.
- **Dedicated IPs**: Cost \$24.95/month per IP. Require careful warmup over 30 days (gradually increasing volume from 100/day to 100,000/day) or enabling **SES Automated Dedicated IP Warmup**.

### 8.2 Security
- **TLS Enforcement**: Enforce opportunistic or required TLS (`STARTTLS`) on outbound SMTP connections.
- **IAM Sender Policies**: Restrict which microservices or Lambda functions can send emails from specific addresses using `ses:SendEmail` IAM conditions.

### 8.3 Cost Breakdown
- **Emails Sent**: \$0.10 per 1,000 emails sent.
- **Free Tier**: 62,000 emails/month free when sent from an application running inside Amazon EC2 (using AWS SDK or SMTP).
- **Attachments / Data Transfer**: \$0.12 per GB of email attachments processed.

---

## 9. Comparison Table

| Dimension | Amazon SES Shared IP Pool | Amazon SES Dedicated IPs | Self-Hosted Postfix Mailer |
|---|---|---|---|
| **Operational Overhead** | None (AWS managed) | Low (Warmup automated) | Extremely High (Manual blacklists) |
| **Setup Cost** | Zero | \$24.95 / month / IP | Server hardware / EC2 costs |
| **Volume Requirement** | Ideal for any volume | Best for $> 100,000$/month | Any |
| **Reputation Risk** | Minor noisy-neighbor risk| 100% controlled by you | High risk of instant blacklisting |
| **DDoS / Elastic Scale** | Elastic cloud scale | Elastic cloud scale | Bottlenecked by instance size |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the three email authentication mechanisms (SPF, DKIM, DMARC) and how do they work together?</strong></summary>

1. **SPF (Sender Policy Framework)**: A DNS TXT record that specifies which mail server IP addresses are authorized to send email for a domain.
2. **DKIM (DomainKeys Identified Mail)**: Adds a cryptographic digital signature to email headers. Receiving servers fetch the sender's public key from DNS to verify the email was not modified in transit.
3. **DMARC (Domain-based Message Authentication, Reporting, and Conformance)**: The policy enforcement layer. DMARC requires **Domain Alignment** (the domain in the "From" header must match SPF and/or DKIM domains). It instructs the receiver what action to take (None, Quarantine, or Reject) if SPF and DKIM fail.
</details>

<details>
<summary><strong>Q2: What happens if your Amazon SES account bounce rate reaches 5% or 10%?</strong></summary>

- **At 5% Bounce Rate**: Amazon SES places the account under **Review**. AWS sends a warning notification giving you a grace period to investigate and remediate the source of the invalid email addresses.
- **At 10% Bounce Rate**: Amazon SES **pauses outbound email sending** immediately to protect the global IP reputation of AWS and preventing blacklisting by Gmail, Microsoft, and Yahoo. You must submit a remediation plan to AWS Support before sending can be resumed.
</details>

<details>
<summary><strong>Q3: What is the Amazon SES Sandbox and how do you graduate to Production?</strong></summary>

The SES Sandbox is a restricted default environment to prevent spammers from abusing newly created AWS accounts. In the sandbox, you can only send up to 200 emails per 24-hour period, at 1 email/sec, and *only* to pre-verified email addresses. 
To graduate to Production, you submit a request via the SES Console detailing:
1. Expected sending volume and frequency.
2. How you collect recipient addresses (confirming strict opt-in).
3. How you manage bounces, complaints, and unsubscriptions.
</details>

<details>
<summary><strong>Q4: Why should an enterprise separate transactional emails from marketing emails?</strong></summary>

Marketing emails typically have lower open rates and higher spam complaint rates. If transactional emails (password resets, OTP codes, order confirmations) share the same IP address or domain as marketing campaigns, high marketing spam complaints can degrade the domain's reputation. This causes critical password reset emails to land in users' spam folders or be delayed. Separating them via distinct subdomains (`trans.example.com` vs `marketing.example.com`) and distinct Dedicated IP pools isolates deliverability risk.
</details>

<details>
<summary><strong>Q5: What is a Custom MAIL FROM Domain and why is it needed for DMARC alignment?</strong></summary>

By default, Amazon SES uses a generic bounce domain (e.g., `amazonses.com`) in the envelope sender (the `MAIL FROM` address). While DKIM will pass with your custom domain, SPF evaluates against `amazonses.com`, creating an SPF alignment failure under strict DMARC rules. Configuring a **Custom MAIL FROM Domain** (e.g., `mail.company.com`) aligns the envelope sender with your visible "From" address, satisfying SPF DMARC alignment.
</details>

<details>
<summary><strong>Q6: When is a Dedicated IP recommended over SES Shared IPs?</strong></summary>

A Dedicated IP is recommended only if:
1. You send a consistent high volume (at least 100,000+ emails per month).
2. You want 100% control over your sender reputation without exposure to other AWS customers on shared pools.
If your volume is low or erratic, a dedicated IP will become "cold" and mailbox providers may treat subsequent email spikes as suspicious spam.
</details>

<details>
<summary><strong>Q7: What is the difference between Easy DKIM and BYODKIM?</strong></summary>

- **Easy DKIM**: AWS generates a 1024-bit or 2048-bit RSA key pair, hosting the private key in SES and providing three CNAME records to publish in your DNS. AWS automatically rotates the keys periodically.
- **BYODKIM (Bring Your Own DKIM)**: You generate the RSA private/public key pair locally, upload the private key to SES, and publish the public key TXT record at your DNS provider. This is used by regulated enterprises with strict key ownership policies.
</details>

<details>
<summary><strong>Q8: What is Virtual Deliverability Manager (VDM)?</strong></summary>

Virtual Deliverability Manager is an automated deliverability optimization tool built into SES. It provides:
1. **Advisor**: Scans your SES configuration and recommends fixes for missing SPF, DKIM, or DMARC alignment.
2. **Guardian**: Automatically optimizes email sending rates and monitors reputation metrics at the ISP level to maximize inbox placement.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Verify a Test Email Address in the SES Sandbox</strong></summary>

**Goal**: Send a test email from the sandbox.
1. In the SES Console, verify your personal email address.
2. Click the confirmation link received in your inbox.
3. Use the AWS CLI to send a test message:
   `aws ses send-email --from-email-address <email> --destination ToAddresses=<email> --message ...`
4. Confirm successful delivery.
</details>

<details>
<summary><strong>Exercise 2: Setup Easy DKIM on a Route 53 Domain</strong></summary>

**Goal**: Configure cryptographic domain verification.
1. Create a domain identity in SES.
2. Generate 3 Easy DKIM tokens.
3. Create the corresponding CNAME records in Route 53.
4. Verify DKIM status changes from `Pending` to `Success`.
</details>

<details>
<summary><strong>Exercise 3: Publish a Strict DMARC Policy in DNS</strong></summary>

**Goal**: Enforce domain spoofing protection.
1. Add a TXT record for `_dmarc.yourdomain.com`.
2. Set value: `v=DMARC1; p=quarantine; pct=100; rua=mailto:dmarc@yourdomain.com`.
3. Use an online DMARC analyzer (e.g., MXToolbox) to validate record syntax.
</details>

<details>
<summary><strong>Exercise 4: Stream Email Metrics to CloudWatch</strong></summary>

**Goal**: Monitor send and delivery counts.
1. Create a Configuration Set `app-metrics`.
2. Add an Event Destination targeting CloudWatch for `send` and `delivery` events.
3. Send an email referencing `--configuration-set-name app-metrics`.
4. Inspect the generated metrics in CloudWatch.
</details>

<details>
<summary><strong>Exercise 5: Simulate a Bounce and Verify Suppression</strong></summary>

**Goal**: Test bounce handling using the AWS Mailbox Simulator.
1. Send an email to the Amazon SES mailbox simulator bounce address: `bounce@simulator.amazonses.com`.
2. Verify SES marks the email as a hard bounce without penalizing your actual account reputation.
3. Capture the bounce event via SNS.
</details>

<details>
<summary><strong>Exercise 6: Configure an SES Email Receiving Rule Set</strong></summary>

**Goal**: Ingest inbound emails into an Amazon S3 bucket.
1. In SES Inbound Configuration, create a Receipt Rule.
2. Route incoming emails for `support@yourdomain.com` to an S3 bucket.
3. Add the required S3 bucket policy granting `ses.amazonaws.com` `s3:PutObject` permissions.
4. Send an email to that address and verify the raw `.eml` file is stored in S3.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| SES REPUTATION MONITORING & BOUNDARIES                                                            |
+--------------------------+-----------------------+------------------------------------------------+
| Metric                   | Target Goal           | Critical Threshold (Action Taken)              |
+--------------------------+-----------------------+------------------------------------------------+
| Bounce Rate              | < 2.0%                | >= 5% (Review Warning) | >= 10% (Sending Paused)|
| Complaint Rate           | < 0.05%               | >= 0.1% (Review Warning)| >= 0.5% (Suspended)  |
| Sandbox Daily Quota      | N/A                   | 200 emails / 24 hours                          |
| Sandbox Send Rate        | N/A                   | 1 email / second                               |
| Dedicated IP Cost        | N/A                   | $24.95 / month per IP                          |
+--------------------------+-----------------------+------------------------------------------------+
```

- **SPF, DKIM, DMARC**: Mandatory industry standard for modern email deliverability.
- **Suppression Lists**: Automatically prevent resending to failed addresses to protect reputation.
- **Configuration Sets**: Provide granular tracking of opens, clicks, deliveries, and bounces.

---

## 13. Further Reading

- [Amazon SES Developer Guide](https://docs.aws.amazon.com/ses/latest/dg/Welcome.html)
- [DMARC.org: Domain-based Message Authentication Overview](https://dmarc.org/)
- [Virtual Deliverability Manager Guide](https://docs.aws.amazon.com/ses/latest/dg/vdm.html)
- [Managing Amazon SES Bounces & Complaints](https://docs.aws.amazon.com/ses/latest/dg/notification-contents.html)
- [05-app-services/02-sns-and-eventbridge.md](file:///04-devops/aws/05-app-services/02-sns-and-eventbridge.md)
