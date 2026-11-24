# Amazon Cognito: User Pools, Identity Pools & Modern Authentication

> **Module**: `05-app-services`  
> **Topic**: Customer Identity (CIAM), OAuth 2.0 / OIDC, JWT Verification, Federated Identities & MFA  
> **Target Audience**: Cloud Security Architects, Full-Stack Developers, Backend Engineers  
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

Implementing identity management from scratch is one of the highest-risk endeavors in software engineering. Storing plaintext or weakly hashed passwords exposes companies to massive data breach liabilities, while building multi-factor authentication (MFA), password reset workflows, OAuth 2.0/OIDC token exchanges, social sign-in (Google, Apple, Facebook), and enterprise SSO federation (SAML 2.0) requires months of engineering overhead.

**Amazon Cognito** provides a fully managed Customer Identity and Access Management (CIAM) platform that easily adds authentication, authorization, and user management to web and mobile applications. Amazon Cognito scales to millions of users and divides identity into two complementary services:
1. **Cognito User Pools (CUP)**: A user directory that handles user registration, login, profile management, password recovery, MFA, and OAuth 2.0 token issuance.
2. **Cognito Identity Pools (Federated Identities)**: An authorization engine that exchanges authenticated tokens (from User Pools, Google, Apple, or SAML) for **temporary, least-privilege AWS IAM credentials**, allowing client apps to access AWS services (S3, DynamoDB) directly without custom backend servers.

---

## 2. Core Concepts

### 2.1 Cognito User Pools (Authentication & Directory)
- **User Directory**: Stores user attributes (username, email, phone number, custom attributes).
- **Authentication Flows**: Supports Secure Remote Password (SRP) protocol (passwords are never sent over the wire), custom authentication via Lambda triggers, and passwordless OTPs.
- **Hosted UI**: An AWS-managed, customizable sign-in/sign-up web page providing out-of-the-box OAuth 2.0 flows.
- **Federation**: Native integration with social identity providers (Google, Apple, Facebook, Amazon) and enterprise identity systems (SAML 2.0 and OIDC, such as Okta, Azure AD, or PingFederate).
- **Security Features**: Adaptive Authentication (risk-based assessment blocking compromised credentials), Multi-Factor Authentication (SMS OTP and TOTP authenticator apps), and leaked credential checks.

### 2.2 The JWT Token Triad
Upon successful authentication, Cognito User Pools issue three JSON Web Tokens (JWTs):
1. **ID Token**: Contains user identity claims (`sub`, `email`, `email_verified`, `cognito:groups`, custom attributes). Used by the frontend client to render user profile data.
2. **Access Token**: Contains authorization scopes and groups. Used to authorize API calls against Amazon API Gateway or backend microservices.
3. **Refresh Token**: Long-lived token (default 30 days) used to retrieve fresh ID and Access tokens without prompting the user to re-enter credentials.

### 2.3 Cognito Identity Pools (Authorization to AWS)
- Exchanges an identity token (from Cognito User Pools, Google, Apple, or SAML) for temporary AWS credentials (`AccessKeyId`, `SecretAccessKey`, `SessionToken`) generated via `sts:AssumeRoleWithWebIdentity`.
- Maps users to distinct IAM roles:
  - *Authenticated Role*: Permissions granted to verified logged-in users.
  - *Unauthenticated (Guest) Role*: Scoped permissions for guest app visitors (e.g., download public assets from S3).
- Supports **Role-Based Access Control (RBAC)** based on User Pool groups or custom claims.

### 2.4 Cognito Lambda Triggers
Cognito integrates with AWS Lambda at key lifecycle moments:
- `Pre Sign-up`: Validate domain allowlists or auto-verify users.
- `Post Confirmation`: Automatically seed a user record into a PostgreSQL or DynamoDB database.
- `Pre Authentication`: Run risk scoring or IP checks before login.
- `Custom Message`: Customize SMS/email verification templates.
- `User Migration`: Seamlessly migrate user passwords from legacy databases (e.g., Bcrypt hashes) on their first login without forcing password resets.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AMAZON COGNITO COMPLETE AUTHENTICATION & ACCESS FLOW                       |
|                                                                                                   |
|  [React / Mobile Client]                                                                          |
|         |                                                                                         |
|         | 1. Authenticate (User + Password via SRP)                                               |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | COGNITO USER POOL (Authentication Provider)                                                 |  |
|  | - Verifies SRP credentials                                                                  |  |
|  | - Executes Pre-Auth / Post-Confirmation Lambda Triggers                                     |  |
|  | - Issues JWT Tokens (ID Token, Access Token, Refresh Token)                                 |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         +---------------------------------------+                                                 |
|         |                                       |                                                 |
|         v (2a. API Access with Access Token)     v (2b. AWS Direct Access with ID Token)           |
|  +-----------------------------+         +-----------------------------------------------------+  |
|  | AMAZON API GATEWAY          |         | COGNITO IDENTITY POOL (Federated Identities)        |  |
|  | - Native JWT Authorizer     |         | - Validates User Pool ID Token                      |  |
|  | - Checks token validity/sig |         | - Calls STS: AssumeRoleWithWebIdentity              |  |
|  | - Extracts sub & claims     |         | - Assumes "Cognito_Auth_Role" IAM Role              |  |
|  +--------------+--------------+         +--------------------------+--------------------------+  |
|                 |                                                   |                             |
|                 v                                                   v (Temporary AWS Credentials) |
|  [Backend Microservice / Lambda]                                    |                             |
|  Processes business request                                         v                             |
|                                                  +---------------------------------------------+  |
|                                                  | AMAZON S3 DIRECT ACCESS (Direct from Mobile)|  |
|                                                  | Uploads photo directly to private bucket    |  |
|                                                  | path: s3://bucket/${cognito-identity.id}/*   |  |
|                                                  +---------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating a User Pool and App Client via AWS CLI

```bash
# 1. Create Cognito User Pool with Email Sign-in and MFA
aws cognito-idp create-user-pool \
    --pool-name "enterprise-user-pool" \
    --username-attributes "email" \
    --auto-verified-attributes "email" \
    --mfa-configuration "OPTIONAL" \
    --schema '[
      {"Name": "email", "Required": true, "Mutable": true},
      {"Name": "department", "AttributeDataType": "String", "Mutable": true}
    ]' \
    --policies '{
      "PasswordPolicy": {
        "MinimumLength": 12,
        "RequireUppercase": true,
        "RequireLowercase": true,
        "RequireNumbers": true,
        "RequireSymbols": true
      }
    }'

# 2. Create User Pool App Client for Web Application (No client secret for SPAs)
aws cognito-idp create-user-pool-client \
    --user-pool-id "us-east-1_AbCdEf123" \
    --client-name "web-react-frontend" \
    --no-generate-secret \
    --explicit-auth-flows "ALLOW_USER_SRP_AUTH" "ALLOW_REFRESH_TOKEN_AUTH"
```

---

## 5. Practical Examples

### 5.1 Cognito User Pool with Hosted UI and App Client (Terraform)

```hcl
# Cognito User Pool
resource "aws_cognito_user_pool" "pool" {
  name                     = "production-users"
  username_attributes      = ["email"]
  auto_verified_attributes = ["email"]

  password_policy {
    minimum_length    = 12
    require_lowercase = true
    require_numbers   = true
    require_symbols   = true
    require_uppercase = true
  }

  account_recovery_setting {
    recovery_mechanism {
      name     = "verified_email"
      priority = 1
    }
  }

  admin_create_user_config {
    allow_admin_create_user_only = false
  }
}

# Domain for Hosted UI
resource "aws_cognito_user_pool_domain" "main" {
  domain       = "auth-enterprise-corp-2026"
  user_pool_id = aws_cognito_user_pool.pool.id
}

# App Client for Single Page Application
resource "aws_cognito_user_pool_client" "client" {
  name         = "spa-client"
  user_pool_id = aws_cognito_user_pool.pool.id

  generate_secret                      = false
  prevent_user_existence_errors        = "ENABLED"
  allowed_oauth_flows_user_pool_client = true
  allowed_oauth_flows                  = ["code"] # Authorization Code Grant with PKCE
  allowed_oauth_scopes                 = ["email", "openid", "profile"]
  callback_urls                        = ["https://app.company.com/callback"]
  logout_urls                          = ["https://app.company.com/logout"]
  supported_identity_providers         = ["COGNITO"]
}
```

### 5.2 Cognito Identity Pool Granting Scoped S3 Access (Terraform)

```hcl
# Identity Pool
resource "aws_cognito_identity_pool" "id_pool" {
  identity_pool_name               = "app_identity_pool"
  allow_unauthenticated_identities = false

  cognito_identity_providers {
    client_id               = aws_cognito_user_pool_client.client.id
    provider_name           = aws_cognito_user_pool.pool.endpoint
    server_side_token_check = false
  }
}

# Authenticated IAM Role
resource "aws_iam_role" "authenticated" {
  name = "cognito_authenticated_role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect = "Allow"
      Principal = { Federated = "cognito-identity.amazonaws.com" }
      Action = "sts:AssumeRoleWithWebIdentity"
      Condition = {
        StringEquals = {
          "cognito-identity.amazonaws.com:aud" = aws_cognito_identity_pool.id_pool.id
        }
        "ForAnyValue:StringLike" = {
          "cognito-identity.amazonaws.com:amr" = "authenticated"
        }
      }
    }]
  })
}

# Scoped S3 Policy: User can only access their own user folder
resource "aws_iam_role_policy" "user_s3_folder_policy" {
  name = "user-s3-private-folder"
  role = aws_iam_role.authenticated.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect = "Allow"
      Action = ["s3:GetObject", "s3:PutObject", "s3:DeleteObject"]
      Resource = [
        "arn:aws:s3:::user-uploads-bucket-2026/$${cognito-identity.amazonaws.com:sub}/*"
      ]
    }]
  })
}
```

### 5.3 Verifying Cognito JWT Tokens in Python Backend (FastAPI / Lambda)

```python
"""
Validates Cognito JWT RS256 signature against User Pool JWKS public keys.
"""
import requests
from jose import jwt, jwk
from jose.utils import base64url_decode
import time

REGION = "us-east-1"
USER_POOL_ID = "us-east-1_AbCdEf123"
APP_CLIENT_ID = "1example23456789abcdefgh"
JWKS_URL = f"https://cognito-idp.{REGION}.amazonaws.com/{USER_POOL_ID}/.well-known/jwks.json"

# Cache JWKS keys in memory
jwks_keys = requests.get(JWKS_URL).json()["keys"]

def verify_cognito_token(token: str):
    headers = jwt.get_unverified_headers(token)
    kid = headers["kid"]

    # Search for matching public key in JWKS
    key = next((k for k in jwks_keys if k["kid"] == kid), None)
    if not key:
        raise ValueError("Public key not found in JWKS.")

    # Construct public key object
    public_key = jwk.construct(key)

    # Decode and verify signature, issuer, and audience
    claims = jwt.decode(
        token,
        public_key.to_pem().decode("utf-8"),
        algorithms=["RS256"],
        audience=APP_CLIENT_ID,
        issuer=f"https://cognito-idp.{REGION}.amazonaws.com/{USER_POOL_ID}"
    )

    # Verify expiration
    if time.time() > claims["exp"]:
        raise ValueError("Token has expired.")

    return claims # Validated identity payload
```

### 5.4 Pre Sign-Up Lambda Trigger for Corporate Domain Whitelisting

```python
"""
Cognito Pre Sign-Up Trigger: Denies signups from non-corporate domains.
"""
def lambda_handler(event, context):
    email = event["request"]["userAttributes"].get("email", "")
    allowed_domain = "@enterprise.com"

    if not email.endswith(allowed_domain):
        raise Exception(f"Sign-up denied. Only {allowed_domain} addresses are permitted.")

    # Auto-verify email to streamline onboarding
    event["response"]["autoConfirmUser"] = True
    event["response"]["autoVerifyEmail"] = True
    return event
```

---

## 6. Best Practices

1. **Never Generate Client Secrets for Public Clients (SPAs / Mobile)**: Client secrets cannot be protected in browser JavaScript or mobile binaries. Use the **OAuth 2.0 Authorization Code Grant with PKCE** (Proof Key for Code Exchange) with `generate_secret = false`.
2. **Enable `PreventUserExistenceErrors`**: Always configure `prevent_user_existence_errors = "ENABLED"` on app clients. This prevents account harvesting attacks by returning generic error messages on login or password reset failures.
3. **Use Scoped S3 Policies with `${cognito-identity.amazonaws.com:sub}`**: Never grant global `s3:PutObject` permissions. Scope IAM policies so users can only read and write to their own private user prefix folder.
4. **Enforce Adaptive Authentication & TOTP MFA**: Protect customer accounts from credential stuffing by requiring TOTP MFA (Google Authenticator, Authy) and enabling Cognito Advanced Security features.
5. **Cache JWKS Public Keys in Microservices**: Do not fetch the `.well-known/jwks.json` endpoint on every API call. Cache public keys in memory and refresh only when an unknown `kid` (Key ID) header is encountered.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Confusing User Pools with Identity Pools | Trying to get AWS credentials directly from User Pools. | Cannot access S3/DynamoDB directly. | Use Identity Pools to exchange User Pool tokens for temporary AWS IAM credentials. |
| Exposing Client Secret in Frontend Code | Enabling `generate_secret` in a React or iOS app. | Secret is exposed; attackers can bypass authentication controls. | Set `generate_secret = false` for SPAs and mobile apps. |
| Validating Token Locally Without Signature Check | Decoding base64 JWT payload without verifying RS256 signature. | Severe security vulnerability: attackers can forge tokens with arbitrary claims. | Always verify token signature against Cognito's `.well-known/jwks.json`. |
| Missing PKCE in Authorization Code Flow | Using Implicit Grant flow for browser SPAs. | Tokens returned in URL fragments; vulnerable to access token interception. | Use Authorization Code Grant with PKCE. |
| Infinite Loop in Migration Trigger | Lambda migration trigger fails without returning user attributes. | User login hangs or fails indefinitely. | Ensure User Migration Lambda returns complete required user attributes. |
| Hardcoding User Pool ID Across Environments | Using the same User Pool for `dev`, `staging`, and `prod`. | Development test users contaminate production directory. | Deploy an isolated User Pool per environment. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Token Lifecycle
- **Token Verification Overhead**: When backend services (or API Gateway JWT Authorizers) validate JWTs locally using cached JWKS public keys, authentication latency is near-instant (< 1ms) with zero network calls back to Cognito.
- **Refresh Token Window**: Can be tuned from 1 hour to 10 years (default 30 days).

### 8.2 Security
- **Data Protection**: User attributes and password hashes (hashed using Salted SHA-256) are encrypted at rest with AWS KMS.
- **Compliance**: Cognito User Pools are HIPAA eligible, PCI-DSS compliant, and SOC 1/2/3 certified.

### 8.3 Cost Breakdown
- **Cognito User Pools (MAU Billing)**:
  - First 50,000 Monthly Active Users (MAUs) are **Free**.
  - 50,001 to 100,000 MAUs: \$0.0055 per MAU.
  - 100,001 to 1,000,000 MAUs: \$0.0046 per MAU.
- **Cognito Identity Pools**: Always **100% Free**.

---

## 9. Comparison Table

| Dimension | Cognito User Pools (CUP) | Cognito Identity Pools (Federated) |
|---|---|---|
| **Primary Function** | User Authentication & Directory | AWS Service Authorization |
| **Tokens Issued** | OIDC JWTs (ID, Access, Refresh) | Temporary AWS IAM Credentials (STS) |
| **Direct AWS Resource Access**| No (Requires API Gateway / Backend)| Yes (Direct S3, DynamoDB, IoT access) |
| **External Federation** | Google, Apple, SAML 2.0, OIDC | User Pools, Google, Apple, SAML |
| **Pricing** | Free for 50k MAUs, then tiered | Always Free |
| **Custom Code Integration** | 10+ Lambda Triggers | Direct IAM Role Mapping |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural difference between Cognito User Pools and Cognito Identity Pools?</strong></summary>

- **Cognito User Pools (CUP)** handle **Authentication**: They act as the user directory (identity provider). They handle registration, credential verification, MFA, social sign-in, and issue standard OpenID Connect (OIDC) JWT tokens (ID, Access, and Refresh tokens).
- **Cognito Identity Pools (Federated Identities)** handle **Authorization to AWS Resources**: They exchange authentication tokens (issued by User Pools or third-party IdPs like Google or SAML) for **temporary, least-privilege AWS IAM credentials** (`AccessKeyId`, `SecretAccessKey`, `SessionToken`). This allows clients to talk directly to AWS services (like S3 or DynamoDB) without passing through an intermediate backend server.
</details>

<details>
<summary><strong>Q2: Explain the differences between the ID Token, Access Token, and Refresh Token issued by Cognito.</strong></summary>

1. **ID Token**: A signed JWT containing user profile identity assertions (such as `sub`, `email`, `given_name`, custom attributes). It is consumed primarily by the frontend client to display user information.
2. **Access Token**: A signed JWT containing authorization scopes and group memberships. It is passed in the `Authorization: Bearer <token>` header to authenticate against APIs and microservices. It does *not* contain identity attributes like email.
3. **Refresh Token**: An opaque string used to retrieve fresh ID and Access tokens from Cognito when they expire (default token lifetime is 1 hour), without requiring the user to re-enter their password.
</details>

<details>
<summary><strong>Q3: Why should public clients (SPAs and mobile apps) use Authorization Code with PKCE instead of Implicit Grant?</strong></summary>

In the legacy **Implicit Grant**, access tokens are returned directly in the browser's URL fragment (`#access_token=...`), leaving them vulnerable to browser history harvesting, open redirect attacks, and script interception. 
The **Authorization Code Grant with PKCE** (Proof Key for Code Exchange) eliminates this risk:
1. The client generates a random `code_verifier` and hashes it into a `code_challenge`.
2. Cognito receives the challenge during initial redirect and returns a temporary one-time authorization code.
3. The client exchanges the authorization code alongside the original `code_verifier` over a direct TLS POST request.
4. Cognito verifies the hash before issuing tokens, ensuring malicious intermediaries cannot intercept or redeem stolen authorization codes.
</details>

<details>
<summary><strong>Q4: How do you implement zero-downtime user migration from a legacy database into Cognito?</strong></summary>

Using the **Cognito User Migration Lambda Trigger**:
1. When an existing user logs into the app for the first time after migration, Cognito detects that the user does not exist in the User Pool and invokes the User Migration Lambda function.
2. The Lambda function queries the legacy database, verifies the user's password against the legacy hash algorithm (e.g., Bcrypt), and returns the user's profile attributes.
3. If valid, Cognito automatically creates the user record inside the User Pool and hashes the password using Cognito's secure algorithm.
4. Future logins are handled natively by Cognito without invoking the legacy database.
</details>

<details>
<summary><strong>Q5: How can a mobile application securely upload photos directly to Amazon S3 without exposing AWS credentials?</strong></summary>

1. The user logs in via Cognito User Pools and receives an ID Token.
2. The app passes the ID Token to Cognito Identity Pools, which calls `sts:AssumeRoleWithWebIdentity` to assume an authenticated IAM role.
3. The IAM role contains a policy granting access to S3 with a dynamic variable:
   `Resource: "arn:aws:s3:::photo-bucket/${cognito-identity.amazonaws.com:sub}/*"`
4. The mobile app uses the resulting temporary AWS credentials with the AWS SDK to upload the file directly to S3, bypassing backend servers entirely while ensuring strict isolation between users.
</details>

<details>
<summary><strong>Q6: How do you verify a Cognito Access Token in a custom backend microservice?</strong></summary>

1. Extract the token from the `Authorization: Bearer <token>` HTTP header.
2. Decode the token header to retrieve the `kid` (Key ID).
3. Fetch and cache the public JSON Web Key Set (JWKS) from:
   `https://cognito-idp.<region>.amazonaws.com/<userPoolId>/.well-known/jwks.json`.
4. Match the `kid` to locate the correct RSA public key and cryptographically verify the RS256 signature.
5. Verify standard claims: confirm `iss` matches your User Pool, `client_id` matches your app client, and `exp` is in the future.
</details>

<details>
<summary><strong>Q7: What is Cognito Adaptive Authentication?</strong></summary>

Adaptive Authentication is part of Cognito Advanced Security. It analyzes user sign-in attempts using machine learning to calculate a risk score based on device fingerprint, location, IP reputation, and login history. Based on the calculated risk level (Low, Medium, High), Cognito can automatically require additional MFA challenges or block the sign-in attempt entirely.
</details>

<details>
<summary><strong>Q8: What does prevent_user_existence_errors do?</strong></summary>

When enabled, Cognito returns identical generic error messages (e.g., `Incorrect username or password`) regardless of whether the entered username exists in the directory. Without this setting, Cognito explicitly returns `UserNotFoundException`, which allows attackers to enumerate valid email addresses and user accounts.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Cognito User Pool with Email Sign-in</strong></summary>

**Goal**: Provision a user pool and create a test user.
1. Create a user pool requiring email as the username attribute.
2. Create an App Client with no secret.
3. Use the CLI to sign up a user:
   `aws cognito-idp sign-up --client-id <id> --username test@example.com --password "SecurePass!123"`.
4. Admin confirm the user:
   `aws cognito-idp admin-confirm-sign-up --user-pool-id <pool-id> --username test@example.com`.
</details>

<details>
<summary><strong>Exercise 2: Authenticate User via InitiateAuth and Inspect JWT Tokens</strong></summary>

**Goal**: Retrieve and decode ID, Access, and Refresh tokens.
1. Authenticate the user via CLI:
   `aws cognito-idp initiate-auth --client-id <id> --auth-flow USER_PASSWORD_AUTH --auth-parameters USERNAME=test@example.com,PASSWORD="SecurePass!123"`.
2. Copy the returned `IdToken` and `AccessToken`.
3. Paste them into `jwt.io` to inspect claims (`sub`, `iss`, `cognito:groups`).
</details>

<details>
<summary><strong>Exercise 3: Setup Cognito Hosted UI with Custom Domain</strong></summary>

**Goal**: Test the AWS-managed sign-in page.
1. In the console, configure a domain prefix for your User Pool.
2. In App Client settings, enable Hosted UI with `code` grant and `email`, `openid` scopes.
3. Launch the Hosted UI URL in a browser, sign in, and observe the authorization code returned in the callback redirect.
</details>

<details>
<summary><strong>Exercise 4: Implement a Pre Sign-Up Domain Filter Lambda Trigger</strong></summary>

**Goal**: Restrict signups to corporate domains.
1. Write a Python Lambda function verifying `event["request"]["userAttributes"]["email"].endswith("@corp.com")`.
2. Attach the Lambda function to the Pre Sign-Up trigger.
3. Try signing up with a `@gmail.com` address and confirm the registration is blocked.
</details>

<details>
<summary><strong>Exercise 5: Configure Direct S3 Access via Identity Pools</strong></summary>

**Goal**: Obtain temporary AWS credentials from a User Pool token.
1. Create a Cognito Identity Pool linked to your User Pool.
2. Configure the authenticated IAM role with S3 read access.
3. Use the AWS CLI to exchange the ID token for AWS STS credentials:
   `aws cognito-identity get-id` and `aws cognito-identity get-credentials-for-identity`.
4. List the S3 bucket using the temporary credentials.
</details>

<details>
<summary><strong>Exercise 6: Secure an API Gateway Route with Cognito User Pool Authorizer</strong></summary>

**Goal**: Protect a backend API using native JWT validation.
1. In API Gateway, create a Cognito Authorizer pointing to your User Pool.
2. Attach the authorizer to a `GET /user/profile` route.
3. Curl the endpoint with `Authorization: Bearer <valid-access-token>` (verify 200 OK).
4. Curl without token (verify 401 Unauthorized).
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| COGNITO ARCHITECTURAL SELECTION MATRIX                                                            |
+--------------------------+-----------------------+------------------------------------------------+
| Requirement              | Cognito User Pools    | Cognito Identity Pools                         |
+--------------------------+-----------------------+------------------------------------------------+
| User Directory & Profiles| Yes                   | No                                             |
| Token Issuance           | OIDC JWTs             | Temporary AWS IAM Credentials                  |
| MFA & Password Policies  | Yes                   | No                                             |
| API Gateway Protection   | Direct JWT Authorizer | IAM SigV4 Authorization                        |
| Direct S3/DynamoDB Access| No                    | Yes (Fine-grained IAM role scoping)            |
| Free Tier Allowance      | 50,000 MAUs           | Unlimited (Always Free)                        |
+--------------------------+-----------------------+------------------------------------------------+
```

- **User Pools**: Authenticates users and manages credentials.
- **Identity Pools**: Authorizes temporary AWS IAM access.
- **PKCE**: Mandatory for public web/mobile OAuth 2.0 implementations.

---

## 13. Further Reading

- [Amazon Cognito Developer Guide](https://docs.aws.amazon.com/cognito/latest/developerguide/what-is-amazon-cognito.html)
- [OAuth 2.0 for Mobile and JavaScript Apps (PKCE)](https://datatracker.ietf.org/doc/html/rfc7636)
- [Validating a JSON Web Token in Amazon Cognito](https://docs.aws.amazon.com/cognito/latest/developerguide/amazon-cognito-user-pools-using-tokens-verifying-a-jwt.html)
- [AWS Amplify Framework Documentation](https://docs.amplify.aws/)
- [04-networking/08-api-gateway.md](file:///04-devops/aws/04-networking/08-api-gateway.md)
