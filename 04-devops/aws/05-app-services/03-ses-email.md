# SES: Sending (and Receiving) Email

SES (Simple Email Service) is AWS's email infrastructure: you hand it a message through an API or SMTP, and it delivers to the recipient's mail provider. It's built for **transactional email** (password resets, receipts, notifications) and also supports marketing/bulk sending. It's cheap and scales well, but the hard part of email isn't sending, it's **getting delivered and staying out of spam**. Most of this note is about that.

Prerequisites: [IAM](../01-foundations/03-iam.md), basic DNS ([Route 53](../04-networking/02-route53-and-cloudfront.md)); [SNS/EventBridge](./02-sns-and-eventbridge.md) for event handling.

---

## How email delivery actually works (the part SES can't hide)

When you send from `noreply@example.com`, the receiving provider (Gmail, Outlook…) asks: *is this message really from example.com, and is the sender trustworthy?* It checks DNS records you control:

| Mechanism | What it proves | Where it lives |
|---|---|---|
| **SPF** | The sending server is authorised for the domain | TXT record on the domain (or the custom MAIL FROM domain) |
| **DKIM** | The message wasn't altered and was signed by the domain | CNAME/TXT records publishing the public key; SES signs each message |
| **DMARC** | What to do if SPF/DKIM fail, and where to send reports; requires **alignment** with the visible `From:` domain | TXT record at `_dmarc.example.com` |

Major mailbox providers now **require** authentication (SPF and/or DKIM, plus DMARC for bulk senders) and low complaint rates; unauthenticated mail increasingly lands in spam or is rejected. Set all three up from day one, and verify current sender requirements from the providers you target.

---

## Core concepts

| Concept | Meaning |
|---|---|
| **Identity** | A verified **domain** or **email address** you're allowed to send from. **Verify the domain** (not individual addresses) for production. |
| **Easy DKIM** | SES generates DKIM keys; you publish the CNAME records it gives you (a click in Route 53). |
| **Custom MAIL FROM domain** | Aligns the SMTP envelope sender (used by SPF) to your own subdomain such as `mail.example.com`, so SPF aligns for DMARC. |
| **Configuration set** | Bundle of settings applied to sends: event destinations (SNS/EventBridge/Firehose/CloudWatch), reputation tracking, dedicated IP pool, suppression options. |
| **Sandbox** | Every new account starts here (below). |
| **Suppression list** | Addresses SES won't send to (previous hard bounces/complaints). |
| **Dedicated IPs** | Optional; only worth it at high, steady volume. |

The current API is **SESv2** (`SendEmail` in `@aws-sdk/client-sesv2`).

---

## The sandbox

New accounts are in the **SES sandbox**: you can send **only to verified addresses/domains**, with **very low daily and per-second limits**. It exists to protect sender reputation. Before real users can receive mail you must **request production access** (console: *Account dashboard → Request production access*), explaining your use case, how you collect addresses, and how you handle bounces and complaints. Approval is per **region**, so do it in the region you send from, and plan ahead since review isn't instant.

Region matters in other ways too: **identities, DKIM, quotas and reputation are all per region.**

---

## Sending an email

```ts
import { SESv2Client, SendEmailCommand } from "@aws-sdk/client-sesv2";
const ses = new SESv2Client({ region: "ap-south-1" });

await ses.send(new SendEmailCommand({
  FromEmailAddress: "Acme <noreply@example.com>",      // must be on a verified identity
  Destination: { ToAddresses: ["user@example.org"] },
  ReplyToAddresses: ["support@example.com"],
  Content: {
    Simple: {
      Subject: { Data: "Your receipt", Charset: "UTF-8" },
      Body: {
        Html: { Data: "<p>Thanks for your order.</p>", Charset: "UTF-8" },
        Text: { Data: "Thanks for your order.",        Charset: "UTF-8" },
      },
    },
  },
  ConfigurationSetName: "transactional",               // enables event tracking
}));
```

Practical points:

- **Always include a plain-text part** alongside HTML. It improves deliverability and accessibility.
- Use `ConfigurationSetName` so you receive delivery/bounce/complaint events.
- **Templates** (`CreateEmailTemplate` + `Content.Template`) store reusable subject/body with `{{placeholders}}` for bulk sends, or keep templates in your own code (often easier to version and test).
- The IAM permission is `ses:SendEmail` (scope it to the identity ARN). The calling role needs it. Your app doesn't need SMTP credentials if it uses the API with a role.
- **SMTP interface** exists for software that only speaks SMTP: you create dedicated SMTP credentials (derived from an IAM user's key). Use TLS, and store those credentials as secrets.
- Message size is limited (around 40 MB including attachments after encoding), and large attachments hurt deliverability. Prefer a link to a file in [S3](../03-storage-and-databases/01-s3.md) (presigned URL).

---

## Bounces, complaints and reputation (non-optional)

SES monitors your **bounce rate** and **complaint rate**. If they get too high, SES puts the account **under review** and can **pause sending**. Treat these as hard operational requirements. Keep both rates low (AWS documents thresholds and review levels: check the current ones).

| Event | Meaning | What you must do |
|---|---|---|
| **Hard bounce** | Address doesn't exist/permanently undeliverable | **Stop sending to it.** SES adds it to the account suppression list, but also record it in your own data. |
| **Soft bounce** | Temporary issue (mailbox full, server down) | SES retries; repeated soft failures effectively become hard failures. |
| **Complaint** | Recipient clicked "Report spam" | **Stop sending to that address** and fix the cause. |

Wire up event handling:

```
SES send ─► configuration set ─► event destination ─► SNS topic ─► SQS ─► your handler
                                  (Bounce, Complaint, Delivery,        marks address as suppressed
                                   Reject, Open, Click…)               in your DB
```

```bash
aws sesv2 create-configuration-set --configuration-set-name transactional
aws sesv2 create-configuration-set-event-destination \
  --configuration-set-name transactional --event-destination-name to-sns \
  --event-destination '{"Enabled":true,"MatchingEventTypes":["BOUNCE","COMPLAINT","DELIVERY"],
    "SnsDestination":{"TopicArn":"arn:aws:sns:ap-south-1:111122223333:ses-events"}}'
```

Good sending hygiene:

- Send only to people who **asked for it**. Use **double opt-in** for lists, and make unsubscribing easy (one-click `List-Unsubscribe` headers are expected for bulk mail).
- **Warm up** gradually when increasing volume or using new/dedicated IPs.
- Don't buy lists, don't reuse old addresses blindly, and validate addresses at signup.
- Use a **separate subdomain** (and ideally separate configuration sets) for marketing vs transactional so a marketing problem doesn't damage password-reset delivery.
- Test without hurting reputation using the **mailbox simulator** addresses (`success@simulator.amazonses.com`, `bounce@…`, `complaint@…`), which don't count against your rates.

---

## Receiving email

SES can also **receive** mail for your domain (set the domain's MX record to the regional SES inbound endpoint), then apply **receipt rules**: store in S3, trigger Lambda, publish to SNS, or bounce. Useful for support inboxes, reply-by-email features and parsing attachments. It isn't a mailbox hosting product. For human mailboxes use a mail provider (Google Workspace, Microsoft 365, WorkMail).

---

## Costs

Pay per message sent (and per attachment/data volume), plus extras like dedicated IPs, receiving, and optional deliverability/reputation dashboards. Sending is inexpensive; the **real cost of getting it wrong is reputation**. Check the pricing page for current figures.

---

## Common mistakes

- Staying in the **sandbox** (or requesting production access in the wrong **region**).
- **Single-address verification** instead of verifying the domain.
- No **DKIM/SPF/DMARC**, or DMARC that fails alignment because of a mismatched `From:` domain.
- **Ignoring bounces and complaints**, so the account gets paused.
- Sending **HTML only** (no text part), or huge image-only emails.
- Mixing **marketing and transactional** on one identity/domain.
- Sending "from" a free-mail address (e.g. gmail.com) you don't own.
- Hardcoding SMTP credentials in the repo.
- Retrying blindly on errors without exponential backoff (and hitting **throttling**).
- Testing with real addresses and creating **bounces** instead of the simulator.
- Forgetting that **SES is regional**: identities and quotas don't carry over.

---

## Debugging

| Symptom | Check |
|---|---|
| `MessageRejected: Email address is not verified` | Still in sandbox (recipient not verified) or sender identity not verified in **this region** |
| Mail sends but lands in **spam** | SPF/DKIM/DMARC pass? Check headers in a received message (`Authentication-Results`). Content (spammy text, broken links, no text part), sender reputation, complaint rate |
| Sends succeed, nothing arrives | Look at the configuration set events: Bounce? Reject? Suppressed? Check the **account-level suppression list** |
| `Throttling` / `Maximum sending rate exceeded` | Respect per-second quota; queue sends through [SQS](./01-sqs.md) with controlled consumer concurrency |
| `AccessDenied` | Role missing `ses:SendEmail` for the **identity ARN** (and configuration set ARN if used) |
| DKIM stays "pending" | DNS records not published/propagated or typo; verify with `dig CNAME <selector>._domainkey.example.com` |
| DMARC failing | `From:` domain doesn't align with the SPF/DKIM-authenticated domain, so configure custom MAIL FROM and DKIM on the same domain |
| Account paused | Review bounce/complaint metrics, fix the cause, and respond to the SES review notice |

Useful: `aws sesv2 get-account` (sandbox status, quotas, enforcement status) and `aws sesv2 get-email-identity --email-identity example.com` (verification and DKIM state).

---

## Quick Summary

- SES = scalable email sending/receiving; **deliverability** is the real work.
- Verify your **domain**; set up **DKIM (Easy DKIM), SPF/custom MAIL FROM, and DMARC**.
- New accounts are in the **sandbox**: request **production access** in each region you send from.
- Use **SESv2 `SendEmail`**, include **text + HTML**, attach a **configuration set**.
- **Handle bounces and complaints** via event destinations (SNS/EventBridge) and **stop sending** to those addresses, or risk a paused account.
- Separate transactional from marketing, warm up volume, test with the **mailbox simulator**.
- Everything (identities, quotas, reputation) is **per region**.

**Next:** [Cognito](./04-cognito.md)
