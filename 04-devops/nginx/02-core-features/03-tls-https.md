# TLS / HTTPS

## Concept
TLS encrypts traffic between clients and nginx. nginx typically **terminates TLS**: it decrypts HTTPS and talks plain HTTP to backends on a trusted network.

**Prerequisites:** [server-and-location.md](../01-fundamentals/server-and-location.md), [reverse-proxy.md](reverse-proxy.md)

## Minimal HTTPS Server
```nginx
server {
    listen 443 ssl;
    http2 on;                              # nginx 1.25.1+; older: listen 443 ssl http2;
    server_name example.com;

    ssl_certificate     /etc/ssl/example.com/fullchain.pem;
    ssl_certificate_key /etc/ssl/example.com/privkey.pem;

    root /var/www/example;
}
```
`ssl_certificate` must be the **full chain** (your certificate followed by intermediates), otherwise some clients fail validation.

## Redirect HTTP to HTTPS
```nginx
server {
    listen 80;
    server_name example.com www.example.com;
    return 301 https://example.com$request_uri;
}
```

## Getting Certificates
**Let's Encrypt with Certbot** (most common):
```bash
sudo apt install certbot python3-certbot-nginx
sudo certbot --nginx -d example.com -d www.example.com
sudo certbot renew --dry-run        # verify auto-renewal works
```
Certbot edits your config and installs a renewal timer. Alternatives: `acme.sh`, cloud-provider certificates, or your internal CA.

For manual testing only, a self-signed certificate:
```bash
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
  -keyout selfsigned.key -out selfsigned.crt -subj "/CN=localhost"
```

## Recommended Protocol Settings
```nginx
ssl_protocols TLSv1.2 TLSv1.3;
ssl_prefer_server_ciphers off;           # let modern clients choose
ssl_session_cache   shared:SSL:10m;
ssl_session_timeout 1d;
ssl_session_tickets off;
```
Cipher lists change over time. Generate current recommendations with the Mozilla SSL Configuration Generator rather than copying an old list. TLS 1.3 needs no cipher tuning.

## HSTS
```nginx
add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
```
Tells browsers to use HTTPS only. Enable after HTTPS works everywhere, because a mistake is cached by browsers for the full `max-age`. Start with a small value (e.g. `300`) and increase.

## OCSP Stapling
```nginx
ssl_stapling on;
ssl_stapling_verify on;
ssl_trusted_certificate /etc/ssl/example.com/chain.pem;
resolver 1.1.1.1 8.8.8.8 valid=300s;
```
Note: some CAs (including Let's Encrypt) have been phasing out OCSP, so check your CA's current status before relying on this.

## Backend Over TLS (Re-encryption)
```nginx
location / {
    proxy_pass https://backend.internal;
    proxy_ssl_server_name on;
    proxy_ssl_verify on;
    proxy_ssl_trusted_certificate /etc/ssl/internal-ca.pem;
}
```
`proxy_ssl_verify` is off by default. Turn it on, or the connection is encrypted but not authenticated.

## Common Mistakes
- Using `cert.pem` instead of `fullchain.pem`: browsers may work while curl or mobile clients fail.
- Wrong file permissions on the private key (readable by others).
- Forgetting to reload nginx after certificate renewal (Certbot's deploy hook usually handles this).
- Mixed content: the page is HTTPS but references `http://` assets.
- Backend generating `http://` redirects because `X-Forwarded-Proto` is not set.
- Enabling HSTS before HTTPS is fully working.

## Debugging
```bash
openssl s_client -connect example.com:443 -servername example.com   # inspect the chain
curl -vI https://example.com
echo | openssl s_client -connect example.com:443 2>/dev/null | openssl x509 -noout -dates
```
`-servername` matters: without SNI you may get the default server's certificate.

## Security Notes
- Disable TLS 1.0/1.1 and SSLv3.
- Keep private keys out of version control.
- Test your configuration with SSL Labs or `testssl.sh`.

## Related / Next
- [security-hardening](../05-security-performance/security-hardening.md): security headers and access control
- [variables-and-rewrites.md](variables-and-rewrites.md): redirect logic
