# TLS Certificates and PKI Management with OpenSSL and Certbot

| Metadata | Details |
| :--- | :--- |
| **Topic** | Public Key Infrastructure (PKI), TLS 1.3, X.509 Certificates, OpenSSL CLI, and Let's Encrypt / Certbot |
| **Module** | `04-networking` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `04-networking/01-networking-basics.md`, `04-networking/03-dns-resolution-and-tools.md` |
| **Target Audience** | Security Engineers, DevOps Engineers, SREs, Systems Architects |
| **Target Distribution** | Ubuntu 24.04 LTS (OpenSSL 3.0+), Rocky Linux / RHEL 9.4 |
| **Key Tools / Files** | `openssl`, `certbot`, `/etc/ssl/certs/`, `/etc/letsencrypt/`, `update-ca-certificates` |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Internal Working](#internal-working)
4. [Syntax & CLI Tools](#syntax--cli-tools)
5. [Progressive Practical Examples](#progressive-practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance, Security & Cost Implications](#performance-security--cost-implications)
9. [Comparison Table / Alternatives](#comparison-table--alternatives)
10. [Real-World Troubleshooting & Interview Q&A](#real-world-troubleshooting--interview-qa)
11. [Hands-On Practice Exercises](#hands-on-practice-exercises)
12. [Summary Cheat Sheet & Further Reading](#summary-cheat-sheet--further-reading)

---

## What & Why

In modern distributed computing, unencrypted plaintext communication across the network is unacceptable. Transport Layer Security (TLS 1.2 and TLS 1.3) provides:
- **Confidentiality**: Symmetric encryption (AES-GCM, ChaCha20-Poly1305) prevents eavesdropping.
- **Integrity**: Cryptographic MACs and AEAD prevent tampering or replay attacks.
- **Authentication**: Asymmetric digital signatures validate server (and optionally client) identity.

Expired certificates cause catastrophic enterprise outages; misconfigured cipher suites fail compliance audits; and broken certificate chains trigger browser security warnings (`ERR_CERT_AUTHORITY_INVALID`).

Systems and DevOps engineers must be fluent in:
- The **OpenSSL 3.x toolkit** for generating private keys, inspecting certificates, and generating Certificate Signing Requests (CSRs).
- Live diagnostic probing of remote endpoints using `openssl s_client`.
- Automated lifecycle management (issuance, DNS/HTTP validation, renewal) via the **ACME protocol** using **Certbot** and Let's Encrypt.

---

## Core Concepts

### The X.509 Public Key Infrastructure (PKI) Hierarchy
Trust in TLS relies on a chain of trust:
1. **Root Certificate Authority (Root CA)**: Self-signed certificate stored in the operating system's trusted certificate store (`/etc/ssl/certs/ca-certificates.crt`). Kept in an offline, air-gapped vault.
2. **Intermediate Certificate Authority (Intermediate CA)**: Signed by the Root CA. Used to sign end-entity certificates. If compromised, the intermediate can be revoked without invalidating the root.
3. **Leaf / Server Certificate**: The end-entity certificate presented by your web server or API gateway. Validates ownership of specific domain names.
4. **Certificate Bundle (Full Chain)**: Concatenation of the Leaf Certificate followed by all Intermediate Certificates (`fullchain.pem`).

### TLS 1.3 vs TLS 1.2 Handshake
- **TLS 1.2**: Required **two full round trips (2-RTT)** to complete the cryptographic handshake before application data could flow.
- **TLS 1.3 (RFC 8446)**: Reduced latency to **one round trip (1-RTT)** by combining key exchange and cipher suite negotiation in the initial `ClientHello`. Supports **0-RTT (Early Data)** for resumed sessions.
- TLS 1.3 completely removed vulnerable legacy cryptographic primitives: no RSA key exchange, no static Diffie-Hellman, no CBC mode ciphers, and no SHA-1 or MD5 hashing.

### Subject Alternative Name (SAN) vs Common Name (CN)
Historically, the domain name was specified in the certificate's `Common Name` (CN) field. 
- In modern standards (RFC 2818), **the CN field is deprecated and ignored by modern browsers**.
- All valid hostnames, wildcards (`*.example.com`), and IP addresses **MUST** be defined in the **Subject Alternative Name (SAN)** X.509 extension (`subjectAltName = DNS:example.com, DNS:www.example.com, IP:10.0.1.5`).

### Server Name Indication (SNI)
When multiple virtual hosts share a single public IP address (virtual hosting on Nginx or cloud load balancers), the client must indicate which hostname it is connecting to *before* the server can present the correct certificate. 
- In the TLS `ClientHello`, the client includes the **SNI extension** specifying `example.com`.
- The server selects and returns the matching certificate.
- Without SNI, the server falls back to its default certificate, triggering certificate mismatch errors.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    TLS 1.3 Handshake Sequence                           |
+-----------------------------------------------------------------------------------------+
       Client                                                             Server
         |                                                                   |
         |--- 1. ClientHello ----------------------------------------------->|
         |    - Supported TLS Versions (TLS 1.3)                             |
         |    - Supported Cipher Suites (e.g. TLS_AES_256_GCM_SHA384)        |
         |    - Key Share (Client's ephemeral ECDH public key)               |
         |    - SNI (Server Name Indication: api.company.com)                |
         |                                                                   |
         |<-- 2. ServerHello ------------------------------------------------|
         |    - Chosen Cipher Suite                                          |
         |    - Key Share (Server's ephemeral ECDH public key)               |
         |    [Both parties derive symmetric session keys in 1 RTT]          |
         |                                                                   |
         |<-- 3. Encrypted Extensions ---------------------------------------|
         |<-- 4. Certificate (Server Leaf + Intermediate Chain) -------------|
         |<-- 5. CertificateVerify (Signature proving possession of privkey)-|
         |<-- 6. Finished (HMAC verifying handshake integrity) --------------|
         |                                                                   |
         |--- 7. Finished -------------------------------------------------->|
         |                                                                   |
         |==================== Encrypted Application Data (HTTP/2 / HTTP/3) =|
```

### The ACME Protocol Architecture (Certbot & Let's Encrypt)
Automated Certificate Management Environment (ACME / RFC 8555) automates certificate issuance:
1. **Account Registration**: Certbot generates an authorized account keypair.
2. **Challenge Request**: Certbot requests a certificate for `api.example.com`. Let's Encrypt issues a cryptographic challenge.
3. **Challenge Validation**:
   - **HTTP-01**: Let's Encrypt requests a specific cryptographic token at `http://api.example.com/.well-known/acme-challenge/<TOKEN>`. Certbot provisions this file or runs a temporary standalone webserver.
   - **DNS-01**: Let's Encrypt requires a DNS `TXT` record at `_acme-challenge.api.example.com`. Essential for wildcard certificates (`*.example.com`).
4. **Certificate Issuance**: Let's Encrypt signs the CSR and returns the issued certificate chain (`fullchain.pem`).

---

## Syntax & CLI Tools

### Generating Keys and CSRs with `openssl`
```bash
# Generate a modern elliptic-curve private key (Prime256v1 / Secp256r1)
openssl ecparam -name prime256v1 -genkey -noout -out ec-private.key

# Generate a 4096-bit RSA private key
openssl genrsa -out rsa-4096.key 4096

# Generate a Certificate Signing Request (CSR) with SAN extensions
openssl req -new -key ec-private.key -out request.csr \
  -subj "/C=US/ST=California/L=San Francisco/O=TechCorp/CN=api.techcorp.internal" \
  -addext "subjectAltName=DNS:api.techcorp.internal,DNS:api-staging.techcorp.internal,IP:10.0.1.50"

# Generate a self-signed certificate valid for 365 days
openssl req -x509 -newkey rsa:4096 -sha256 -days 365 -nodes \
  -keyout selfsigned.key -out selfsigned.crt \
  -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
```

### Inspecting Local Certificates and Keys
```bash
# Decode and read human-readable details of a certificate
openssl x509 -in cert.pem -text -noout

# Check certificate expiration date and validity window
openssl x509 -in cert.pem -noout -dates

# Inspect Subject Alternative Names (SAN)
openssl x509 -in cert.pem -noout -ext subjectAltName

# Verify that a private key matches a certificate (modulus or pubkey hash check)
openssl x509 -in cert.pem -noout -pubkey | openssl sha256
openssl pkey -in key.pem -pubout | openssl sha256
# (Hashes MUST be identical!)
```

### Live Remote Endpoint Probing with `openssl s_client`
```bash
# Connect to a remote HTTPS server, presenting SNI (-servername)
openssl s_client -connect example.com:443 -servername example.com

# Print entire certificate chain presented by the remote server
openssl s_client -connect example.com:443 -servername example.com -showcerts

# Test if server supports TLS 1.3
openssl s_client -connect example.com:443 -servername example.com -tls1_3

# Extract remote expiration date in one command
echo | openssl s_client -connect example.com:443 -servername example.com 2>/dev/null \
  | openssl x509 -noout -dates
```

### Automated Lifecycle with `certbot`
```bash
# Obtain and install certificate for Nginx automatically
sudo certbot --nginx -d example.com -d www.example.com

# Obtain certificate standalone (spins up temporary webserver on port 80)
sudo certbot certonly --standalone -d api.example.com

# Obtain wildcard certificate using DNS validation
sudo certbot certonly --manual --preferred-challenges dns -d "*.example.com" -d "example.com"

# Dry-run test automatic renewal
sudo certbot renew --dry-run
```

---

## Progressive Practical Examples

### Example 1: Creating an Internal Private CA and Issuing Server Certs
In enterprise environments, internal microservices communicate using certificates signed by an internal Root CA.

```bash
#!/usr/bin/env bash
set -euo pipefail

WORKDIR="/tmp/internal-ca"
mkdir -p "${WORKDIR}" && cd "${WORKDIR}"

echo "==> 1. Generating Internal Root CA..."
# Generate Root CA private key
openssl ecparam -name prime256v1 -genkey -noout -out ca-root.key

# Generate Self-Signed Root Certificate (valid 10 years)
openssl req -x509 -new -nodes -key ca-root.key -sha256 -days 3650 \
  -out ca-root.crt \
  -subj "/C=US/O=Enterprise Internal/CN=Corp Internal Root CA"

echo "==> 2. Generating Application Server Key and CSR..."
openssl ecparam -name prime256v1 -genkey -noout -out server.key

# OpenSSL config with SAN for signing
cat << 'EOF' > server-ext.cnf
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth, clientAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = app.prod.internal
DNS.2 = *.app.prod.internal
IP.1 = 10.0.1.10
EOF

openssl req -new -key server.key -out server.csr \
  -subj "/C=US/O=Enterprise Internal/CN=app.prod.internal"

echo "==> 3. Signing Server Certificate with Internal Root CA..."
openssl x509 -req -in server.csr -CA ca-root.crt -CAkey ca-root.key \
  -CAcreateserial -out server.crt -days 365 -sha256 -extfile server-ext.cnf

echo "==> 4. Verifying signed certificate against Root CA..."
openssl verify -CAfile ca-root.crt server.crt
# Output: server.crt: OK
```

### Example 2: Installing Custom Internal Root CA System-Wide
Ensure `curl`, Python, and system tools trust your internal private CA certificate without `-k`.

```bash
# On Ubuntu / Debian:
sudo cp ca-root.crt /usr/local/share/ca-certificates/corp-internal-root.crt
sudo update-ca-certificates

# Verify certificate was added to the system bundle
grep -i "Corp Internal Root" /etc/ssl/certs/ca-certificates.crt

# On RHEL 9 / Rocky Linux:
sudo cp ca-root.crt /etc/pki/ca-trust/source/anchors/corp-internal-root.crt
sudo update-ca-trust
```

### Example 3: Automated Expiration Watchdog Script
Production monitoring script checking days until certificate expiration across a fleet of domains.

```bash
#!/usr/bin/env bash
set -euo pipefail

DOMAINS=("google.com" "github.com" "cloudflare.com")
WARNING_DAYS=30

echo "=== TLS Certificate Expiration Audit ==="

for DOMAIN in "${DOMAINS[@]}"; do
    # Extract end date in epoch seconds
    EXPIRY_DATE=$(echo | openssl s_client -connect "${DOMAIN}:443" -servername "${DOMAIN}" 2>/dev/null \
      | openssl x509 -noout -enddate | cut -d= -f2)
    
    EXPIRY_EPOCH=$(date -d "${EXPIRY_DATE}" +%s)
    NOW_EPOCH=$(date +%s)
    DAYS_LEFT=$(( (EXPIRY_EPOCH - NOW_EPOCH) / 86400 ))

    if [ "${DAYS_LEFT}" -lt "${WARNING_DAYS}" ]; then
        echo "ALERT: ${DOMAIN} expires in ${DAYS_LEFT} days! (${EXPIRY_DATE})"
    else
        echo "OK:    ${DOMAIN} valid for ${DAYS_LEFT} days. (${EXPIRY_DATE})"
    fi
done
```

### Example 4: Diagnosing Broken Certificate Chains with `s_client`
When servers fail to include intermediate certificates, desktop browsers might cache them via AIA (Authority Information Access), but mobile devices, Java apps, and `curl` crash.

```bash
# Test remote endpoint verifying against default system CA bundle
openssl s_client -connect my-api.com:443 -servername my-api.com \
  -CApath /etc/ssl/certs/

# If intermediate is missing, output displays:
# Verify return code: 21 (unable to verify the first certificate)

# Fix: Ensure Nginx uses 'fullchain.pem' (Leaf + Intermediates) rather than 'cert.pem' (Leaf only):
# ssl_certificate /etc/letsencrypt/live/my-api.com/fullchain.pem;
```

### Example 5: Non-Interactive Certbot Issuance with Route 53 DNS Plugin
How cloud automation provisions wildcard certificates using AWS Route 53 DNS challenges without human intervention.

```bash
# 1. Install certbot dns plugin
sudo apt-get install -y python3-certbot-dns-route53

# 2. Provision wildcard certificate (requires AWS IAM permissions for route53:ChangeResourceRecordSets)
sudo certbot certonly \
  --dns-route53 \
  -d "company.com" \
  -d "*.company.com" \
  --non-interactive \
  --agree-tos \
  --email "security-alerts@company.com"

# 3. Output files located at:
# /etc/letsencrypt/live/company.com/fullchain.pem
# /etc/letsencrypt/live/company.com/privkey.pem
```

---

## Best Practices

1. **Always Use `fullchain.pem` in Web Servers and Reverse Proxies**: In Nginx, Envoy, or Apache, never point `ssl_certificate` to `cert.pem`. `cert.pem` contains only the leaf certificate. Always point to `fullchain.pem` so clients receive both the leaf and the intermediate CAs necessary to build trust back to the Root CA.
2. **Never Commit Private Keys (`privkey.pem`) to Git Repositories**: Private keys must be injected at deploy time via secret management platforms (AWS Secrets Manager, HashiCorp Vault, Kubernetes Secrets). If a private key is accidentally committed, revoke the certificate immediately.
3. **Automate Certificate Renewal via Systemd Timers**: Never rely on calendar reminders to renew certificates. Certbot installs a systemd timer (`certbot.timer`) that runs twice daily, automatically renewing certificates within 30 days of expiration. Verify with `systemctl list-timers | grep certbot`.
4. **Deploy Post-Renewal Hooks to Reload Web Services**: When a certificate is renewed on disk, Nginx or HAProxy continues using the old in-memory certificate until reloaded. Configure a deploy hook in `/etc/letsencrypt/renewal-hooks/deploy/reload-nginx.sh`:
   ```bash
   #!/bin/sh
   systemctl reload nginx
   ```
5. **Enforce Modern Ciphers and Disable Weak Protocols**: In web server configurations, disable TLS 1.0 and TLS 1.1. Restrict protocols to `TLSv1.2 TLSv1.3`. Use AEAD cipher suites (AES-GCM and ChaCha20-Poly1305).
6. **Set Strict File Permissions on Private Keys**: Ensure private keys are owned by root and readable only by authorized groups (`chmod 600 key.pem` or `chmod 640 key.pem && chown root:ssl-cert key.pem`).

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Pointing Nginx `ssl_certificate` to `cert.pem` instead of `fullchain.pem` | Missing intermediate certificate; mobile apps and CLI tools fail with `unable to verify the first certificate`. | Use `ssl_certificate /.../fullchain.pem;`. |
| Running `openssl s_client` without `-servername <domain>` | The server does not receive SNI and returns its default fallback certificate, causing false failure alerts. | Always specify `-servername <domain>` matching the target host. |
| Generating certificates with only Common Name (CN) and no SAN | Ignored by modern browsers and OpenSSL 3.0+; fails with `ERR_CERT_COMMON_NAME_INVALID`. | Always populate `subjectAltName` in CSR extension configuration. |
| Forgetting to reload Nginx after Certbot auto-renews certificate | Nginx continues serving expired certificate held in memory until service reload. | Configure a deploy hook in `/etc/letsencrypt/renewal-hooks/deploy/`. |
| Using HTTP-01 ACME challenge for Wildcard certificates | Let's Encrypt requires proof of root DNS control for wildcards (`*.domain.com`); HTTP-01 is rejected. | Use DNS-01 ACME challenge with DNS provider API plugins. |
| Installing self-signed certificates in production | Lack of automated revocation, non-standard trust distribution, triggers browser security warnings. | Use Let's Encrypt for public sites or HashiCorp Vault / Step CA for internal PKI. |

---

## Performance, Security & Cost Implications

### Performance
- **TLS Session Resumption**: Repeating full TLS handshakes on every request degrades throughput. TLS 1.3 session tickets allow resumed connections to complete in 0-RTT or 1-RTT. Enable `ssl_session_cache shared:SSL:10m;` in Nginx.
- **ECDSA vs RSA Cryptographic Speed**: Generating signatures and completing handshakes with Elliptic Curve keys (ECDSA / Ed25519) is **3x–5x faster** on CPU utilization than legacy 4096-bit RSA keys.

### Security
- **Certificate Revocation Lists (CRL) vs OCSP Stapling**: CRLs require clients to download megabytes of revoked certificate serial numbers. **OCSP Stapling** (`ssl_stapling on;`) forces the web server to query the CA periodically and staple a cryptographically signed time-stamped proof of validity to the TLS handshake, eliminating client DNS/HTTP queries to the CA.
- **HSTS (HTTP Strict Transport Security)**: Protects against SSL-stripping attacks by commanding browsers to connect strictly over HTTPS for a defined duration (`Strict-Transport-Security: max-age=31536000; includeSubDomains; preload`).

### Cost
- In the past, wildcard or multi-domain SSL certificates cost hundreds of dollars annually per server. Using **Let's Encrypt** or **AWS Certificate Manager (ACM)** provides cryptographically validated certificates at **zero licensing cost**.

---

## Comparison Table / Alternatives

| Dimension | Let's Encrypt (Certbot) | Internal Private CA (OpenSSL / Step-CA) | Commercial CA (DigiCert, Sectigo) |
| :--- | :--- | :--- | :--- |
| **Trust Model** | Publicly trusted by all OS/browsers | Private trust (requires installing root CA) | Publicly trusted by all OS/browsers |
| **Cost** | 100% Free | Free | Paid ($100 - $1000/yr) |
| **Validation Method** | Automated ACME (HTTP-01, DNS-01) | Custom script / Vault / ACME | Manual verification / ACME |
| **Validity Period** | 90 Days (mandates automation) | Configurable (1–10 years) | 1 Year (398 days max) |
| **Best Used For** | Public websites, Cloud APIs | Internal VPC microservices, K8s mTLS | Extended Validation (EV) or air-gapped |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: Why does `curl https://api.mycompany.com` fail with `SSL certificate problem: unable to get local issuer certificate`?
<details>
<summary>View Solution</summary>

**Root Cause**: The client cannot verify the trust chain.
1. The server is misconfigured and sending only the leaf certificate instead of the full chain (`fullchain.pem`), omitting the intermediate CA.
2. Or, the certificate was signed by an internal private CA whose root certificate has not been added to the client machine's `/etc/ssl/certs/` trust store.

**Fix**:
- Server side: Update webserver config to point to `fullchain.pem`.
- Client side (for internal CAs): Add the root CA to `/usr/local/share/ca-certificates/` and run `sudo update-ca-certificates`.
</details>

### Q2: How do you verify whether a private key (`key.pem`) matches a certificate (`cert.pem`)?
<details>
<summary>View Solution</summary>

Extract and compare the cryptographic hash of the public keys derived from both files:
```bash
# Public key hash from certificate:
openssl x509 -in cert.pem -noout -pubkey | openssl sha256

# Public key hash from private key:
openssl pkey -in key.pem -pubout | openssl sha256
```
If the SHA-256 hashes match identically, the private key belongs to that certificate. If they differ, the private key does not match and web servers will fail on startup.
</details>

### Q3: What is OCSP Stapling and why is it critical for web performance and privacy?
<details>
<summary>View Solution</summary>

Without OCSP Stapling, when a user visits an HTTPS site, the user's browser must pause the connection and send a separate HTTP request to the CA's OCSP server to ask "Has this certificate been revoked?".
- **Performance penalty**: Adds 50ms–200ms latency to page loads.
- **Privacy breach**: The CA learns what website the user is visiting.

**With OCSP Stapling**: The web server queries the CA's OCSP server every hour in the background, caches the signed response, and "staples" it into the initial TLS handshake with the client. The client verifies the staple locally with 0 latency penalty.
</details>

### Q4: How do you test what TLS versions and cipher suites a remote server supports using OpenSSL?
<details>
<summary>View Solution</summary>

Force specific protocol flags using `openssl s_client`:
```bash
# Test TLS 1.2
openssl s_client -connect target.com:443 -servername target.com -tls1_2 < /dev/null

# Test TLS 1.3
openssl s_client -connect target.com:443 -servername target.com -tls1_3 < /dev/null

# Test specific cipher suite
openssl s_client -connect target.com:443 -servername target.com \
  -cipher "ECDHE-RSA-AES128-GCM-SHA256" < /dev/null
```
If the handshake succeeds, the protocol/cipher is supported; if it returns `handshake failure`, it is disabled.
</details>

### Q5: What is the difference between HTTP-01 and DNS-01 challenges in ACME?
<details>
<summary>View Solution</summary>

- **HTTP-01**: Let's Encrypt validates domain ownership by requesting a token over port 80 (`http://domain.com/.well-known/acme-challenge/<token>`). Requires port 80 to be open and reachable from the public internet. Cannot issue Wildcard certificates (`*.domain.com`).
- **DNS-01**: Let's Encrypt validates ownership by verifying the existence of a TXT record `_acme-challenge.domain.com`. Does not require open HTTP ports. Works for private/internal servers and is **mandatory for issuing wildcard certificates**.
</details>

### Q6: Why did Let's Encrypt set certificate validity to 90 days instead of 1 year?
<details>
<summary>View Solution</summary>

1. **Mandate Automation**: Short lifetimes force organizations to automate certificate issuance and renewal via CI/CD and systemd timers, eliminating human error.
2. **Limit Damage of Key Compromise**: If a private key or certificate is leaked, the window of vulnerability is capped at a maximum of 90 days without relying on fragile revocation mechanisms.
</details>

### Q7: What does the basicConstraints extension `CA:TRUE` vs `CA:FALSE` signify?
<details>
<summary>View Solution</summary>

`basicConstraints` is an essential X.509 extension:
- `CA:TRUE`: The certificate is a Certificate Authority. It is permitted to sign other certificates.
- `CA:FALSE`: The certificate is an **end-entity (leaf) certificate**. It cannot be used to sign other certificates. If an attacker compromises a leaf certificate with `CA:FALSE`, they cannot forge certificates for other domains.
</details>

### Q8: How can you convert a certificate between PEM and DER formats using OpenSSL?
<details>
<summary>View Solution</summary>

- **PEM to DER (ASCII Base64 to Binary)**:
  ```bash
  openssl x509 -in cert.pem -outform DER -out cert.der
  ```
- **DER to PEM (Binary to ASCII Base64)**:
  ```bash
  openssl x509 -in cert.der -inform DER -outform PEM -out cert.pem
  ```
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Generating and Inspecting a Self-Signed SAN Certificate
**Task**: Use OpenSSL to generate a private key and a self-signed certificate valid for `localhost` and `127.0.0.1`. Inspect its Subject Alternative Name extension.
<details>
<summary>View Solution</summary>

```bash
# 1. Generate key and certificate
openssl req -x509 -newkey rsa:2048 -nodes -keyout /tmp/lab.key -out /tmp/lab.crt -days 30 \
  -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

# 2. Inspect SAN extension
openssl x509 -in /tmp/lab.crt -noout -ext subjectAltName

# 3. Clean up
rm -f /tmp/lab.key /tmp/lab.crt
```
</details>

### Exercise 2: Auditing Remote Endpoint Certificate Chain
**Task**: Use `openssl s_client` to connect to `cloudflare.com:443`. Extract the subject, issuer, and expiration date of each certificate in the chain.
<details>
<summary>View Solution</summary>

```bash
echo | openssl s_client -connect cloudflare.com:443 -servername cloudflare.com -showcerts 2>/dev/null \
  | grep -E "s:|i:|Not After"
```
</details>

### Exercise 3: Validating Private Key and Certificate Match
**Task**: Generate a test key and certificate. Verify their public key hashes match. Then generate an unrelated key and observe the hash mismatch.
<details>
<summary>View Solution</summary>

```bash
# 1. Generate matched pair
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes \
  -keyout /tmp/valid.key -out /tmp/valid.crt -days 1 -subj "/CN=test"

# 2. Generate unrelated key
openssl ecparam -name prime256v1 -genkey -noout -out /tmp/unrelated.key

# 3. Compare valid pair (hashes MUST match)
openssl x509 -in /tmp/valid.crt -noout -pubkey | openssl sha256
openssl pkey -in /tmp/valid.key -pubout | openssl sha256

# 4. Compare unrelated pair (hashes DIFFER)
openssl pkey -in /tmp/unrelated.key -pubout | openssl sha256

rm -f /tmp/valid.key /tmp/valid.crt /tmp/unrelated.key
```
</details>

### Exercise 4: Testing Certbot Renewal Dry-Run
**Task**: If Certbot is installed on your Linux machine, execute a renewal simulation to test whether renewal timers and configuration hooks function properly.
<details>
<summary>View Solution</summary>

```bash
sudo certbot renew --dry-run
```
</details>

### Exercise 5: Converting Certificate Formats
**Task**: Download or generate a certificate in PEM format. Convert it to PKCS#12 (`.pfx` / `.p12`) format commonly required by Java and Windows services.
<details>
<summary>View Solution</summary>

```bash
# 1. Generate temporary cert and key
openssl req -x509 -newkey rsa:2048 -nodes -keyout /tmp/test.key -out /tmp/test.crt -days 1 -subj "/CN=test"

# 2. Export to PKCS#12 bundle
openssl pkcs12 -export -out /tmp/bundle.p12 -inkey /tmp/test.key -in /tmp/test.crt -passout pass:secret123

# 3. Verify bundle
openssl pkcs12 -in /tmp/bundle.p12 -noout -info -passin pass:secret123

rm -f /tmp/test.key /tmp/test.crt /tmp/bundle.p12
```
</details>

### Exercise 6: Testing TLS 1.3 Connection Locally
**Task**: Launch a temporary local OpenSSL TLS server on port `8443` using your self-signed certificate. Connect to it using `openssl s_client` and verify the cipher.
<details>
<summary>View Solution</summary>

```bash
# 1. Create cert
openssl req -x509 -newkey rsa:2048 -nodes -keyout /tmp/s.key -out /tmp/s.crt -days 1 -subj "/CN=localhost"

# 2. Start mock SSL server in background
openssl s_server -key /tmp/s.key -cert /tmp/s.crt -port 8443 -www &
SERVER_PID=$!
sleep 1

# 3. Connect client
echo "GET / HTTP/1.0" | openssl s_client -connect localhost:8443 -servername localhost 2>&1 | grep -E "Protocol|Cipher"

# 4. Clean up
kill "${SERVER_PID}" 2>/dev/null || true
rm -f /tmp/s.key /tmp/s.crt
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Inspect Certificate Details**| `openssl x509 -in cert.pem -text -noout` |
| **Check Expiration Date** | `openssl x509 -in cert.pem -noout -enddate` |
| **Check Remote Endpoint Cert**| `openssl s_client -connect host:443 -servername host` |
| **Generate EC Key** | `openssl ecparam -name prime256v1 -genkey -noout -out key.pem` |
| **Generate 4096 RSA Key** | `openssl genrsa -out key.pem 4096` |
| **Generate CSR with SAN** | `openssl req -new -key key.pem -out req.csr -addext "subjectAltName=DNS:site.com"` |
| **Verify Key & Cert Match** | `diff <(openssl x509 -in c.pem -pubkey) <(openssl pkey -in k.pem -pubout)` |
| **Install System Root CA** | `sudo cp root.crt /usr/local/share/ca-certificates/ && sudo update-ca-certificates` |
| **Certbot Auto-Renew Dry-Run**| `sudo certbot renew --dry-run` |

### Further Reading
- `man 1 openssl` and `man 1 openssl-x509`
- `man 1 openssl-s_client`
- [Certbot Official Documentation & Instructions](https://certbot.eff.org/)
- [Mozilla SSL Configuration Generator](https://ssl-config.mozilla.org/)
- [RFC 8446: The Transport Layer Security (TLS) Protocol Version 1.3](https://datatracker.ietf.org/doc/html/rfc8446)
