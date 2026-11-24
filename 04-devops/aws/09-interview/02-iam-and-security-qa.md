---
topic: "AWS IAM, Cryptography & Security Engineering Technical Interview Q&A"
module: "09-interview"
track: "aws"
difficulty: "advanced"
last-tested: "2026-11"
tags:
  - "interview"
  - "iam"
  - "security"
  - "kms"
  - "envelope-encryption"
  - "guardduty"
  - "waf"
---

# AWS IAM, Cryptography & Security Engineering Technical Interview Q&A

## Table of Contents
- [1. IAM Policy Evaluation Logic & Precedence](#1-iam-policy-evaluation-logic--precedence)
- [2. Identities, Roles, and Delegation](#2-identities-roles-and-delegation)
- [3. Cryptography & Key Management (KMS & Secrets)](#3-cryptography--key-management-kms--secrets)
- [4. Threat Detection & Security Posture Services](#4-threat-detection--security-posture-services)
- [5. Deep-Dive Security Scenarios & Triage](#5-deep-dive-security-scenarios--triage)

---

## 1. IAM Policy Evaluation Logic & Precedence

### Q1: Walk through the exact algorithm AWS IAM uses to evaluate authorization for an API request.
**Answer**:
IAM policy evaluation follows a strict, deterministic sequence:
1. **Default Deny**: By default, all requests are implicitly denied.
2. **Explicit Deny Evaluation**: AWS evaluates all applicable policies (SCPs, Resource Policies, IAM Identity Policies, Permissions Boundaries, Session Policies). If **any policy** contains an explicit `Deny` matching the request, the final decision is immediately **DENY**, regardless of any `Allow` statements.
3. **Organizations Service Control Policies (SCPs)**: If the account is in an AWS Organization, the action must be permitted by all parent SCPs. If blocked by an SCP, evaluation terminates in **DENY**.
4. **Resource-Based Policies**: If a resource policy allows the action (e.g., S3 Bucket Policy):
   - *Same-Account*: An `Allow` in either the resource policy OR the identity policy is sufficient.
   - *Cross-Account*: An `Allow` must exist in **both** the resource policy AND the identity policy.
5. **IAM Permissions Boundaries**: If a permission boundary is attached to the IAM principal, the action must be explicitly permitted by the boundary.
6. **Session Policies**: If temporary credentials were generated via `sts:AssumeRole` with a session policy, the action must be permitted by the session policy.
7. **Identity-Based Policies**: If an applicable IAM identity policy has an explicit `Allow`, the final decision is **ALLOW**.
8. **Final Fallback**: If no explicit `Allow` was found, the decision remains the original **Default Deny**.

```
+----------------------------------------------------------------------------------------------------+
|                                IAM POLICY EVALUATION FLOWCHART                                     |
+----------------------------------------------------------------------------------------------------+

  [ Incoming API Call ]
            |
            v
  Is there an EXPLICIT DENY anywhere?
            |-- YES --> [ DENIED ]
            |-- NO
            v
  Is the request permitted by AWS Organizations SCP?
            |-- NO  --> [ DENIED ]
            |-- YES
            v
  Is it a Cross-Account request?
            |-- YES --> Do BOTH Account Identity Policy AND Resource Policy permit it?
            |             |-- NO  --> [ DENIED ]
            |             |-- YES --> Continue below
            |-- NO
            v
  Does an IAM Identity Policy OR Resource Policy have an EXPLICIT ALLOW?
            |-- NO  --> [ DENIED (Default Deny) ]
            |-- YES
            v
  Is the action permitted by Permissions Boundaries & Session Policies (if present)?
            |-- NO  --> [ DENIED ]
            |-- YES --> [ ALLOWED ]
```

### Q2: What is an IAM Permissions Boundary, and how does it prevent privilege escalation?
**Answer**:
An **IAM Permissions Boundary** is an advanced management feature that sets the maximum permissions an identity-based policy can grant to an IAM principal.
- *Use Case*: Delegating IAM role creation to developers without granting them the ability to escalate privileges to full administrators.
- *Mechanism*: An administrator creates a boundary policy (e.g., allowing access only to S3 and DynamoDB). The developer is granted permissions to create new IAM roles, but an enforced condition requires that any role they create **must** have that specific permissions boundary attached:
  ```json
  "Condition": {
    "StringEquals": {
      "iam:PermissionsBoundary": "arn:aws:iam::123456789012:policy/DeveloperBoundary"
    }
  }
  ```
- *Result*: Even if the developer attaches `AdministratorAccess` (`*:*`) to their newly created role, the effective permissions of that role are capped at S3 and DynamoDB. The developer cannot create a role with full administrative rights to hijack the account.

---

## 2. Identities, Roles, and Delegation

### Q3: What is the technical difference between an IAM User, an IAM Role, and an IAM Group?
**Answer**:
- **IAM User**: A long-lived identity with permanent static credentials (a console password or static API Access Key / Secret Key pairs). IAM users represent single human operators or legacy applications. *Architectural rule: Avoid IAM users; prefer short-lived temporary roles.*
- **IAM Role**: An identity with specific permissions that is **assumed** by trusted entities (an EC2 instance, a Lambda function, a GitHub Actions runner, or a federated user). Roles do not have static credentials; assuming a role invokes **AWS STS** (`sts:AssumeRole`), which issues temporary, auto-expiring security credentials (valid for 15 minutes to 12 hours).
- **IAM Group**: A management construct used to bundle multiple IAM users together to attach common policies (e.g., `DevTeamGroup`). **A group is NOT an IAM principal**: it cannot be referenced in a resource policy `Principal` block, cannot assume roles, and cannot be used by services.

### Q4: Explain the two policies that govern an IAM Role.
**Answer**:
Every IAM Role is governed by exactly two distinct policies:
1. **Trust Policy (Assume Role Policy)**: A resource-based policy that defines **who or what is permitted to assume the role**. It specifies trusted principals (e.g., `{ "Service": "lambda.amazonaws.com" }` or `{ "AWS": "arn:aws:iam::123456789012:root" }`) and optional conditions (e.g., requiring MFA or an external ID).
2. **Permissions Policy**: An identity-based policy attached to the role that defines **what actions the role can execute on which AWS resources** once assumed.
- *Debugging Tip*: If an entity gets an error attempting to assume a role (`AccessDenied: Not authorized to perform sts:AssumeRole`), the bug is in the **Trust Policy**. If the entity assumes the role successfully but fails to execute an action on S3 or DynamoDB, the bug is in the **Permissions Policy**.

### Q5: How does Cross-Account IAM access work, and why must both accounts grant permission?
**Answer**:
When Principal in **Account A** needs to access a resource (e.g., an S3 bucket or KMS key) in **Account B**:
1. **Account B (Resource Owner)** must permit Account A in its resource-based policy (e.g., S3 Bucket Policy or KMS Key Policy), OR define an IAM Role with a Trust Policy permitting Account A.
2. **Account A (Caller Account)** must permit its own IAM principal to access the target resource or execute `sts:AssumeRole` on Account B's role.
- **Why both sides are required**: This dual-authorization prevents unauthorized resource usage from either direction:
  - Account B must authorize Account A to protect its data from unauthorized external access.
  - Account A must authorize its principal to prevent rogue internal users from exfiltrating data to external accounts or assuming untrusted foreign roles.

---

## 3. Cryptography & Key Management (KMS & Secrets)

### Q6: Explain AWS KMS Envelope Encryption. Why doesn't KMS encrypt multi-gigabyte files directly?
**Answer**:
AWS KMS limits direct cryptographic operations (`kms:Encrypt` / `kms:Decrypt`) to payloads of **4 KB or smaller**. Direct transmission of multi-gigabyte datasets over network TLS to KMS APIs would create massive network bottlenecks, high API latency, and prohibitive KMS request costs (\$0.03 per 10,000 requests).

To solve this, AWS uses **Envelope Encryption**:
1. **Data Key Generation**: When encrypting a large file (e.g., a 5 GB S3 object), S3 calls `kms:GenerateDataKey` specifying the Customer Master Key (KMS KMS Key / CMK).
2. **Key Pair Return**: KMS returns two versions of a unique symmetric key:
   - A **Plaintext Data Key** (kept in memory).
   - An **Encrypted Data Key (Ciphertext DEK)** (encrypted under the KMS master key).
3. **Local Encryption**: The application/service uses the Plaintext Data Key to encrypt the 5 GB file locally using AES-256-GCM.
4. **Memory Wipe**: The Plaintext Data Key is immediately erased from system memory.
5. **Storage**: The Encrypted Data Key is stored alongside the encrypted data as object metadata.
6. **Decryption**: To read the file, the service passes the Encrypted Data Key back to KMS via `kms:Decrypt`. KMS decrypts the key and returns the Plaintext Data Key in memory to decrypt the file.

```
+----------------------------------------------------------------------------------------------------+
|                               AWS KMS ENVELOPE ENCRYPTION WORKFLOW                                 |
+----------------------------------------------------------------------------------------------------+

  [ S3 / Application ]                         [ AWS KMS ]
           |                                        |
           | 1. GenerateDataKey(KMS Master Key)     |
           |--------------------------------------->|
           |                                        |  - Generates Plaintext DEK
           |                                        |  - Encrypts DEK with Master Key
           | 2. Returns: [Plaintext DEK] + [Encrypted DEK]
           |<---------------------------------------|
           |
           | 3. Encrypts 5GB Payload locally with [Plaintext DEK]
           | 4. Wipes [Plaintext DEK] from memory!
           | 5. Stores [Encrypted Payload] + [Encrypted DEK] together on disk
```

### Q7: What is the difference between AWS Secrets Manager and AWS Systems Manager Parameter Store?
**Answer**:
| Feature / Dimension | AWS Secrets Manager | AWS SSM Parameter Store |
|:---|:---|:---|
| **Primary Purpose** | Database credentials, API tokens, OAuth secrets | Configuration data, strings, flags, license keys |
| **Automated Rotation** | **Native** (Built-in Lambda rotation for RDS, Aurora, Redshift)| Manual (Must build custom EventBridge + Lambda) |
| **Cross-Account Access** | Native cross-account sharing via resource policies | Limited (Requires IAM role assumption) |
| **Random Password Gen** | Built-in CLI password generator | Not supported |
| **Cost** | **\$0.40 per secret / month** + \$0.05 / 10k API calls | **Free** for Standard tier; \$0.05/mo for Advanced |

*Architectural Recommendation*: Use **SSM Parameter Store (Standard)** for general application configuration parameters, environment variables, and public keys. Use **AWS Secrets Manager** for sensitive credentials requiring automated password rotation (e.g., database passwords).

---

## 4. Threat Detection & Security Posture Services

### Q8: What does Amazon GuardDuty do, and how does it detect threats without installing host agents?
**Answer**:
**Amazon GuardDuty** is an intelligent, continuous threat detection service that uses machine learning, anomaly detection, and integrated threat intelligence feeds to identify unauthorized behavior and malicious activity across an AWS environment.

**Agentless Architecture**:
GuardDuty does **not** install host agents or inspect application memory. Instead, it operates entirely at the AWS infrastructure telemetry plane, ingesting and analyzing:
1. **AWS CloudTrail Management & Data Events**: Detects suspicious API calls (e.g., API calls from Tor exit nodes, enumeration of IAM roles, disabling CloudTrail).
2. **VPC Flow Logs**: Identifies network anomalies (e.g., outbound cryptocurrency mining traffic, unusual port scans, compromised EC2 instances communicating with known command-and-control botnets).
3. **DNS Query Logs**: Detects DNS data exfiltration attempts and communication with malicious domains.
4. **EKS Audit Logs & S3 Data Events**: Identifies unauthorized pod privilege escalation and suspicious bulk S3 object downloads.
- *Performance & Cost Benefit*: Because GuardDuty operates out-of-band at the hypervisor/log layer, it incurs **zero performance impact** on compute instances and does not require complex agent maintenance.

### Q9: Compare AWS WAF, AWS Shield Standard, and AWS Shield Advanced.
**Answer**:
- **AWS WAF (Web Application Firewall)**:
  - Operates at **Layer 7 (Application)**.
  - Attached to Application Load Balancers, CloudFront, API Gateway, and AppSync.
  - Inspects HTTP/HTTPS headers, bodies, URIs, query strings, and SQL injection / cross-site scripting (XSS) patterns. Enforces rate limits per IP.
- **AWS Shield Standard**:
  - Automatically enabled for **all AWS customers at zero cost**.
  - Operates at **Layer 3 and Layer 4 (Network/Transport)**.
  - Defends against common volumetric infrastructure attacks (SYN floods, UDP reflection attacks) across CloudFront, Route 53, and ALBs.
- **AWS Shield Advanced**:
  - Premium managed DDoS protection service (\$3,000/month subscription).
  - Provides 24/7 dedicated access to the **AWS DDoS Response Team (DRT)**.
  - Offers **DDoS Cost Protection**: Reimburses excess auto-scaling charges incurred during a volumetric DDoS spike on EC2, ALB, or CloudFront.

---

## 5. Deep-Dive Security Scenarios & Triage

### Scenario 1: Triaging a Compromised AWS Access Key
**Interviewer**: *"You receive an alert from Amazon GuardDuty at 2:00 AM: `UnauthorizedAccess:IAMUser/TorIPCaller`. An IAM user's static access key is executing API calls from a Tor exit node. Walk me through your incident response runbook."*

**Structured Candidate Response**:
I would execute a 4-phase incident response containment and eradication procedure:
1. **Immediate Containment (Revoke Access)**:
   - Immediately deactivate the compromised access key via AWS CLI:
     ```bash
     aws iam update-access-key --user-name <user> --access-key-id <key-id> --status Inactive
     ```
   - Attach an inline explicit `Deny` policy to the IAM user to terminate all active sessions:
     ```json
     { "Effect": "Deny", "Action": "*", "Resource": "*" }
     ```
2. **Determine Blast Radius (Triage with CloudTrail)**:
   - Query CloudTrail for all events initiated by that access key ID within the past 24-48 hours.
   - Specifically look for persistence establishment: creation of new IAM users, generation of new access keys, creation of IAM roles, modification of trust policies, or security group openings (`0.0.0.0/0` on port 22/3389).
   - Check for unauthorized resource provisioning (e.g., launching high-GPU EC2 instances for crypto mining or downloading S3 objects).
3. **Eradication**:
   - Terminate any rogue EC2 instances or Lambda functions launched by the compromised identity.
   - Delete all unauthorized IAM users, roles, and access keys created during the breach.
   - Permanently delete the compromised access key.
4. **Post-Mortem & Preventative Hardening**:
   - Transition the human user or service to **IAM Identity Center** or **GitHub OIDC federation** to eliminate static keys permanently.
   - Enforce an SCP preventing the creation of static IAM access keys across the organization.

### Scenario 2: EC2 SSRF Attack and IMDSv2
**Interviewer**: *"How does a Server-Side Request Forgery (SSRF) vulnerability allow an attacker to steal an EC2 instance's IAM role credentials, and how does IMDSv2 completely mitigate this attack?"*

**Structured Candidate Response**:
- **The Vulnerability (IMDSv1)**:
  An EC2 instance retrieves its IAM role's temporary credentials by querying the Instance Metadata Service at the link-local IP `http://169.254.169.254/latest/meta-data/iam/security-credentials/<role-name>`.
  Under IMDSv1, this is a standard unauthenticated HTTP `GET` request. If a web application running on the instance has an SSRF vulnerability (e.g., accepting a user-supplied URL to fetch an avatar or PDF), an attacker can supply `http://169.254.169.254/...`. The vulnerable web server executes the `GET` request and returns the IAM temporary access key, secret key, and session token directly to the attacker.
- **The Mitigation (IMDSv2)**:
  IMDSv2 introduces **session-oriented authentication** requiring a two-step handshake:
  1. The client must first issue an HTTP `PUT` request with a specific header to request a session token:
     `PUT http://169.254.169.254/latest/api/token` with header `X-aws-ec2-metadata-token-ttl-seconds: 21600`.
  2. Subsequent metadata requests must present this token in the header:
     `GET http://169.254.169.254/latest/meta-data/...` with header `X-aws-ec2-metadata-token: <token>`.
  - *Why SSRF fails*: Most web application SSRF vulnerabilities cannot force the backend server to make a `PUT` request with custom arbitrary headers.
  - *Enforcement*: Enforce IMDSv2 account-wide via SCP or instance setting `HttpTokens=required`.
