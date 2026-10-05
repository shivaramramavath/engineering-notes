# SES (Email)

Amazon SES sends email from your own domain at low cost. Calling the API is the easy 10%. The other 90% is **deliverability**: proving you own the domain, staying out of spam folders, and not sending to addresses that bounce. SES will pause an account that gets this badly wrong, so this note covers both the code and the operational rules.

Prerequisites: [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md). Use the **SESv2** client; it's the current API (`@aws-sdk/client-sesv2`). The older `client-ses` still exists but new work should use v2.

```bash
npm install @aws-sdk/client-sesv2
```

---

## Before any code works

SES is **region-specific**: identities, quotas and settings live in the region you use. Do this in your sending region.

1. **Verify an identity**: either a single email address or (better) a whole **domain**. For a domain, SES gives you DNS records to publish:
   - **DKIM** (Easy DKIM CNAME records) signs your mail. Required for good deliverability.
   - Optionally a **custom MAIL FROM** domain so SPF aligns with your domain.
   - Publish a **DMARC** record for your domain (a TXT record at `_dmarc.yourdomain`); mailbox providers increasingly expect it, especially for bulk senders.
2. **Get out of the sandbox.** New accounts start in a *sandbox*: you can send only **to verified addresses**, with low daily and per-second limits. Request production access in the console (you'll describe your use case, how you handle bounces and unsubscribes). Until then, `MessageRejected: Email address is not verified` is expected for any real recipient.
3. Optional but strongly recommended: a **configuration set** so you can track bounces, complaints and deliveries (see below).

---

## Sending

Three content types in `SendEmailCommand`: **Simple**, **Template**, **Raw**.

### Simple (subject, HTML, text)

```ts
import { SESv2Client, SendEmailCommand } from "@aws-sdk/client-sesv2";

const ses = new SESv2Client({ region: process.env.AWS_REGION });

await ses.send(new SendEmailCommand({
  FromEmailAddress: "Acme <no-reply@acme.example>",
  Destination: { ToAddresses: ["user@example.com"] },
  Content: {
    Simple: {
      Subject: { Data: "Your receipt", Charset: "UTF-8" },
      Body: {
        Html: { Data: "<p>Thanks for your order.</p>", Charset: "UTF-8" },
        Text: { Data: "Thanks for your order." , Charset: "UTF-8" },
      },
    },
  },
  ConfigurationSetName: "app-default",
  ReplyToAddresses: ["support@acme.example"],
}));
```

- The `From` domain (or address) must be a verified identity in this region.
- **Always send a text part alongside HTML.** It helps spam scoring and accessibility.
- The response contains a `MessageId`; log it, since it's how you correlate later bounce events.

### Templates

SES can store templates with `{{variable}}` placeholders.

```ts
import { CreateEmailTemplateCommand } from "@aws-sdk/client-sesv2";

await ses.send(new CreateEmailTemplateCommand({
  TemplateName: "welcome",
  TemplateContent: {
    Subject: "Welcome, {{name}}",
    Html: "<p>Hi {{name}}, confirm here: <a href=\"{{link}}\">{{link}}</a></p>",
    Text: "Hi {{name}}, confirm here: {{link}}",
  },
}));

await ses.send(new SendEmailCommand({
  FromEmailAddress: "no-reply@acme.example",
  Destination: { ToAddresses: ["user@example.com"] },
  Content: {
    Template: {
      TemplateName: "welcome",
      TemplateData: JSON.stringify({ name: "Asha", link: "https://acme.example/confirm/abc" }),
    },
  },
}));
```

`TemplateData` is a **JSON string**. For many recipients with different data, `SendBulkEmailCommand` takes a default template plus per-recipient replacement data, with a per-call entry limit (check the API reference).

Trade-off: SES-stored templates keep rendering simple, but they're outside your repo and test suite. Many teams instead **render HTML in their own code** (React Email, MJML, Handlebars) and send it as `Simple` content. You get version control, tests and previews. That's usually the better choice once emails get non-trivial.

### Attachments: raw MIME

`Simple` can't carry attachments. You need a full MIME message. Building MIME by hand is error-prone, so use nodemailer's SES transport:

```bash
npm install nodemailer @types/nodemailer
```

```ts
import nodemailer from "nodemailer";
import { SESv2Client, SendEmailCommand } from "@aws-sdk/client-sesv2";

const transport = nodemailer.createTransport({
  SES: { sesClient: new SESv2Client({ region: process.env.AWS_REGION }), SendEmailCommand },
});

await transport.sendMail({
  from: "no-reply@acme.example",
  to: "user@example.com",
  subject: "Invoice",
  text: "Attached.",
  attachments: [{ filename: "invoice.pdf", content: pdfBuffer }],
});
```

(Check nodemailer's SES transport docs for the current option shape. It has changed between versions.) Total message size, attachments included, is capped, so link to large files (a presigned [S3](./01-s3.md) URL) instead.

---

## Bounces and complaints (don't skip this)

- A **bounce** means the address is invalid or unreachable (hard bounce: permanent; soft: temporary).
- A **complaint** means the recipient hit "report spam".

SES tracks your rates. If bounces or complaints climb past its published thresholds (bounces in the low single-digit percent, complaints around a tenth of a percent), the account can go **under review** and then have sending **paused**. So: **stop sending to addresses that bounce or complain.**

How:

```text
SES send ──▶ Configuration set ──▶ Event destination ──▶ SNS / EventBridge / Firehose
              (BOUNCE, COMPLAINT,                              │
               DELIVERY, ...)                                  ▼
                                                 Your handler: mark address as
                                                 suppressed in your DB (DynamoDB)
```

1. Create a configuration set and an **event destination** for `BOUNCE` and `COMPLAINT` (and optionally `DELIVERY`, `REJECT`) pointing at [SNS or EventBridge](./04-sns-and-eventbridge.md).
2. Consume those events (SNS → SQS → worker is typical) and record the address as suppressed in your own store.
3. Check your suppression list *before* sending.

SES also keeps an **account-level suppression list** that automatically blocks sends to addresses that previously hard-bounced or complained, which is a safety net rather than a replacement for tracking it yourself. Manage entries with `PutSuppressedDestinationCommand` and friends.

Also include a working **unsubscribe** path for marketing/bulk mail (and the `List-Unsubscribe` header where appropriate).

---

## Sending patterns in an app

- **Don't send email inline in a request handler** if it can fail or be slow. Enqueue a job ([SQS](./03-sqs.md)) and have a worker send, so a transient SES error becomes a retry instead of a failed signup.
- Handle throttling: SES enforces a **max send rate** (messages per second) and a **daily quota**. Hitting it gives `TooManyRequestsException` / `LimitExceededException`; the SDK retries some automatically ([errors and retries](../04-production/01-errors-and-retries.md)). Throttle your worker to the account's rate (shown in the console and via `GetAccountCommand`).
- Make the send **idempotent** on your side. A retried message can mean a duplicate email.

---

## Permissions

```json
{
  "Effect": "Allow",
  "Action": ["ses:SendEmail", "ses:SendBulkEmail"],
  "Resource": "arn:aws:ses:ap-south-1:111122223333:identity/acme.example",
  "Condition": { "StringEquals": { "ses:FromAddress": "no-reply@acme.example" } }
}
```

Scoping to the identity and `ses:FromAddress` stops a compromised service from sending as any address. If you use a configuration set or templates, the policy may also need their ARNs; the error message tells you which one.

---

## Common mistakes and debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `MessageRejected: Email address is not verified` | Sandbox (recipient unverified) or `From` identity not verified in this region | Verify identities in the *sending* region; request production access |
| Works in one region, fails in another | Identities are regional | Verify in the region your client uses |
| Mail lands in spam | No DKIM/SPF/DMARC alignment, no text part, spammy content | Complete DNS setup; send text + HTML; warm up volume gradually |
| `TooManyRequestsException` | Over per-second send rate | Throttle the worker; request a higher limit |
| Account sending paused | High bounce/complaint rate | Suppress bad addresses, clean your list, follow the review instructions |
| `NotFoundException` on template send | Template missing in this region or wrong name | Create it in the same region |
| Template variables show up as `{{name}}` | `TemplateData` keys don't match the placeholders (or wasn't JSON) | Match names exactly; `JSON.stringify` |
| Garbled characters | Missing charset | Set `Charset: "UTF-8"` |
| Duplicate emails | Retry after timeout or at-least-once queue | Idempotency key per email in your DB |

---

## Quick summary

- SESv2 client: `SendEmailCommand` with `Simple`, `Template` or `Raw` content.
- Verify a **domain** with DKIM (plus SPF/MAIL FROM and DMARC), in your sending region, and leave the **sandbox**.
- Always send text + HTML; render templates in your own code for testability.
- Attachments need raw MIME: use nodemailer's SES transport, or link to S3.
- Track bounces/complaints via a configuration set → SNS/EventBridge, and **suppress** those addresses.
- Send through a queue; respect the send rate; scope IAM to the identity and `ses:FromAddress`.

## Next

[Cognito](./06-cognito.md): sign-up emails (confirmation codes) and the auth flow that triggers them.
