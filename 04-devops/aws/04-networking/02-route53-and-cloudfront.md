# Route 53 and CloudFront: DNS and CDN

Two services that sit at the front door of most web apps:

- **Route 53** is AWS's DNS service: it turns `app.example.com` into the address of your load balancer, CloudFront distribution or anything else, and can route by health, latency or weight.
- **CloudFront** is a CDN: a global network of edge locations that cache and accelerate your content, terminate TLS close to users, and shield your origin (S3, ALB, API Gateway…) from direct traffic.

They're usually used together: Route 53 points your domain at CloudFront, and CloudFront fetches from your origin.

```
User ──DNS──► Route 53 ── alias ──► CloudFront edge ──(cache miss)──► Origin
                                         │ cache hit                  (S3 / ALB / API GW)
                                         ▼
                                    cached response
```

Prerequisites: [S3](../03-storage-and-databases/01-s3.md) and a basic idea of DNS (records, TTL). Load balancers are in [Load Balancing and API Gateway](./03-load-balancing-and-api-gateway.md).

---

# Part 1: Route 53

## Concepts

| Concept | Meaning |
|---|---|
| **Hosted zone** | Container for DNS records of one domain (`example.com`). **Public** (internet) or **private** (resolves only inside chosen VPCs). |
| **Record** | A DNS entry: name + type + value + TTL. |
| **Registered domain** | Optionally register/transfer a domain with Route 53. Registration and DNS hosting are separate things. |
| **Name servers (NS)** | Each public hosted zone gets four NS records. The domain registrar must delegate to *these*. |

Setting up a domain: create a public hosted zone → copy its 4 NS values into your registrar (if the domain isn't registered with Route 53) → add records. Forgetting the delegation step, or pointing at the NS of an *old* hosted zone, is the classic "my records don't work" problem.

## Record types you'll actually use

| Type | Points to | Example use |
|---|---|---|
| **A** | IPv4 address | `api.example.com → 203.0.113.10` |
| **AAAA** | IPv6 address | same, for IPv6 |
| **CNAME** | Another hostname | `www → app.example.net` |
| **MX / TXT** | Mail servers / arbitrary text | Email routing, SPF/DKIM, domain verification |
| **NS / SOA** | Delegation / zone metadata | Created for you |
| **Alias** | An AWS resource (Route 53 specific) | `example.com → CloudFront / ALB / S3 website / API Gateway` |

### Alias vs CNAME

- A **CNAME can't exist at the zone apex** (`example.com` itself), because DNS forbids it alongside the mandatory SOA/NS records.
- An **alias record** is a Route 53 extension that *looks like* an A/AAAA record to clients but points at an AWS resource. It works at the apex, is **free for queries to AWS resources**, and follows the target's changing IPs automatically.

Rule: **use alias records for AWS targets** (CloudFront, ALB/NLB, API Gateway custom domains, S3 website endpoints, other records in the zone). Use CNAME for non-AWS hostnames on subdomains.

```bash
aws route53 change-resource-record-sets --hosted-zone-id Z123EXAMPLE --change-batch '{
  "Changes": [{
    "Action": "UPSERT",
    "ResourceRecordSet": {
      "Name": "example.com",
      "Type": "A",
      "AliasTarget": {
        "HostedZoneId": "Z2FDTNDATAQYW2",
        "DNSName": "d111111abcdef8.cloudfront.net",
        "EvaluateTargetHealth": false
      }
    }
  }]
}'
```

(`Z2FDTNDATAQYW2` is the fixed hosted zone ID used for **all CloudFront alias targets**. Other services have their own IDs. Look them up in the docs, and prefer IaC constructs that fill these in for you.)

## Routing policies

| Policy | Behaviour | Use for |
|---|---|---|
| **Simple** | One record, returns its value(s) | Default |
| **Weighted** | Splits traffic by weight | Canary / gradual migration (e.g. 90/10) |
| **Latency** | Returns the region with lowest latency to the user | Multi-region apps |
| **Failover** | Primary, with a secondary if the health check fails | Active-passive DR |
| **Geolocation / Geoproximity** | By user location | Regional content/compliance |
| **Multivalue answer** | Up to 8 healthy records | Simple DNS-level spreading |

Health checks (HTTP/HTTPS/TCP, or based on CloudWatch alarms) let failover/weighted/latency records skip unhealthy endpoints.

## TTL and "propagation"

- **TTL** is how long resolvers may cache an answer. Lower it (say 60 s) *before* a planned change, then raise it again afterwards.
- "DNS propagation takes 48 hours" is mostly myth. Route 53 itself updates within about a minute, but **other people's caches** hold old answers until their TTL expires. A previous high TTL is what actually makes changes slow.
- Test against the authoritative server to bypass caches: `dig @<one-of-the-NS> app.example.com`.

## Private hosted zones

A private zone resolves names only inside the VPCs you associate (e.g. `db.internal.example.com → RDS endpoint`). The VPC needs DNS support and DNS hostnames enabled (see [VPC](./01-vpc.md)).

---

# Part 2: CloudFront

## Concepts

| Concept | Meaning |
|---|---|
| **Distribution** | Your CloudFront config; gets a `dxxxx.cloudfront.net` domain, can have custom domains. |
| **Origin** | Where content comes from: S3 bucket, ALB, API Gateway, EC2, any HTTP server. |
| **Behavior** | Maps a **path pattern** to an origin and rules: `/api/*` → ALB (no cache), `/assets/*` → S3 (cache long), default `*` → S3. |
| **Cache policy** | What forms the cache key (headers/cookies/query strings) and TTLs. |
| **Origin request policy** | What extra info is forwarded to the origin *without* becoming part of the cache key. |
| **Edge location** | A point of presence that serves cached content near the user. |

**Cache key** is the single most important idea: two requests with the same key share one cached object. Add a header, cookie or query string to the key and you split the cache. Keep it minimal. Forward only what the origin truly needs.

## The standard pattern: static site or SPA on S3

A **private** S3 bucket + CloudFront with **Origin Access Control (OAC)**. The bucket is never public; only your distribution can read it.

```ts
import * as cdk from "aws-cdk-lib";
import * as s3 from "aws-cdk-lib/aws-s3";
import * as cloudfront from "aws-cdk-lib/aws-cloudfront";
import * as origins from "aws-cdk-lib/aws-cloudfront-origins";

const bucket = new s3.Bucket(this, "Site", {
  blockPublicAccess: s3.BlockPublicAccess.BLOCK_ALL,
});

const dist = new cloudfront.Distribution(this, "Dist", {
  defaultRootObject: "index.html",
  defaultBehavior: {
    origin: origins.S3BucketOrigin.withOriginAccessControl(bucket),
    viewerProtocolPolicy: cloudfront.ViewerProtocolPolicy.REDIRECT_TO_HTTPS,
  },
  // SPA: serve index.html for client-side routes. S3 returns 403 for missing keys.
  errorResponses: [
    { httpStatus: 403, responseHttpStatus: 200, responsePagePath: "/index.html" },
    { httpStatus: 404, responseHttpStatus: 200, responsePagePath: "/index.html" },
  ],
});
```

OAC replaces the older **Origin Access Identity (OAI)**. Use OAC for new work. The construct adds the bucket policy granting `cloudfront.amazonaws.com` read access scoped to your distribution.

Key points:

- **Default root object**: CloudFront only maps `/` to `index.html` at the **root**; subdirectory `/docs/` won't serve `docs/index.html` unless you add a function/rewrite.
- **SPA routing**: the error-response trick above (or a CloudFront Function rewrite) makes deep links work.
- **Don't use the S3 "website endpoint"** as an origin for a private bucket. It requires public access and doesn't support OAC.

## Custom domain and HTTPS

1. Request a **public certificate in ACM**, **in `us-east-1`** regardless of where your app lives (CloudFront only reads certificates from there). Public ACM certificates are free and renew automatically, if validated via **DNS validation** (a CNAME record in your hosted zone; Route 53 makes it one click).
2. Add the domain as an **alternate domain name (CNAME)** on the distribution and attach the certificate.
3. Create a Route 53 **alias A/AAAA** record to the distribution.

Forgetting step 2 gives the confusing `403 The request could not be satisfied` when browsing your custom domain.

## Fronting an API or ALB

Put CloudFront in front of dynamic origins for TLS termination near users, DDoS absorption, WAF, and caching of cacheable responses.

- Use a **cache policy that disables caching** (e.g. the managed `CachingDisabled`) for non-cacheable API routes, and an origin request policy to forward headers, cookies and query strings.
- To stop people bypassing CloudFront, restrict the origin. CloudFront supports **VPC origins** (keep an ALB/NLB/EC2 in a private subnet, reachable only via CloudFront), or fall back to a secret custom header the origin checks.

## Caching behavior and invalidation

- CloudFront respects origin `Cache-Control` headers within the min/max/default TTLs of your cache policy.
- **Best practice: fingerprint static assets** (`app.3f9c1a.js`) and cache them for a year with `immutable`. Deploy changes by uploading new filenames, and keep `index.html` on a short TTL (or `no-cache`) so it points at the new files.
- **Invalidations** (`aws cloudfront create-invalidation --paths "/index.html"`) force a refresh of cached paths. A monthly allowance of paths is free and beyond that they're billed. Use targeted paths, not `/*` on every deploy.

## Edge compute

| | CloudFront Functions | Lambda@Edge |
|---|---|---|
| Runs | Every edge location, JS only, sub-millisecond, very limited | Regional edge caches, full Lambda runtimes, heavier |
| Good for | URL rewrites, redirects, header manipulation, simple auth tokens | Complex logic, network calls, body processing |
| Cost | Cheaper | Pricier, with duration billing |

Prefer CloudFront Functions when they're sufficient.

## Security features

- **Signed URLs / signed cookies** for private content (paid downloads, authenticated media).
- **AWS WAF** integration: rate limiting, managed rule sets, bot control.
- **Geo restriction**, **field-level encryption**, **HTTPS-only to origin**, **response headers policies** (HSTS, CSP, CORS headers).
- DDoS protection (AWS Shield Standard) is included.

## Pricing, briefly

CloudFront offers **pay-as-you-go** pricing (data transfer out + requests, with a monthly free tier) and, since November 2025, **flat-rate plans** (Free, Pro, Business, Premium tiers) that bundle CDN, WAF/DDoS, DNS and more into a fixed monthly price with no overage charges. Not every feature is available on every plan. Compare on the [CloudFront pricing page](https://aws.amazon.com/cloudfront/pricing/) for your traffic and needs. Data transfer from an AWS origin (S3, ALB, API Gateway) *to* CloudFront is free, which is one reason serving S3 content through CloudFront is often **cheaper** than serving it straight from S3.

---

## Common mistakes

- **Certificate in the wrong region**: for CloudFront, ACM certificates must be in `us-east-1`.
- Forgetting to add the **alternate domain name** (CNAME) to the distribution.
- Using a **CNAME at the apex** instead of an alias record.
- **Registrar still points at the old name servers** after creating a new hosted zone.
- Making the S3 bucket public instead of using **OAC**.
- Caching **everything** (including personalised/API responses), or caching **nothing** (paying for CloudFront with a 0% hit ratio).
- Forwarding all headers/cookies/query strings, which **destroys cache hit ratio**.
- Relying on invalidations for every deploy instead of **fingerprinted filenames**.
- Setting a **long TTL on `index.html`**, so users keep loading old asset references.
- SPA deep links returning 403/404 (no error-response/rewrite configured).
- Changing DNS without lowering **TTL** first, then blaming "propagation".

---

## Debugging

| Symptom | Likely cause / check |
|---|---|
| `403 ERROR The request could not be satisfied` on the custom domain | Domain missing from distribution's alternate domain names, or cert doesn't cover it |
| 403 from an S3 origin | Bucket policy/OAC not granting this distribution; object key doesn't exist (S3 returns 403 without list permission) |
| Old content still served | Cache TTL; invalidate the path, or fix the `Cache-Control` and filenames |
| Stale `index.html` after deploy | Cache TTL on HTML is too long |
| `502/504` from CloudFront | Origin unreachable/slow: security group/ALB listener, TLS mismatch to origin, origin timeout |
| Domain doesn't resolve | Registrar NS ≠ hosted zone NS; record missing/typo; trailing caches |
| Domain resolves to the wrong place | Another record (or a conflicting CNAME) exists; check with `dig` |
| Cert can't be selected in CloudFront | Certificate not in `us-east-1`, or not yet validated (DNS validation CNAME missing) |

Useful checks:

```bash
dig +short app.example.com
dig @ns-123.awsdns-45.org app.example.com          # ask an authoritative server directly
curl -sI https://app.example.com/ | grep -i -E 'x-cache|age|via|x-amz-cf'
```

The `x-cache` response header tells you if it was a `Hit from cloudfront`, `Miss from cloudfront`, or `Error from cloudfront`, and `age` shows how long the object has been cached.

---

## Quick Summary

- **Route 53** = DNS: hosted zones hold records; **alias records** point at AWS resources (and work at the apex); policies (weighted, latency, failover) plus health checks route traffic.
- Delegate your domain to the hosted zone's **NS records**; lower **TTL** before changes.
- **CloudFront** = global CDN: distribution → origins → behaviors; the **cache key** drives hit ratio, so keep it minimal.
- Serve static sites from a **private S3 bucket via OAC**; fingerprint assets, keep HTML short-lived, and avoid blanket invalidations.
- HTTPS for CloudFront needs an **ACM cert in `us-east-1`** plus the alternate domain name on the distribution.
- Use CloudFront in front of ALBs/APIs for TLS, WAF and DDoS protection; restrict the origin so it can't be bypassed.
- Pricing: pay-as-you-go or flat-rate plans. Check the current page.

**Next:** [Load Balancing and API Gateway](./03-load-balancing-and-api-gateway.md)