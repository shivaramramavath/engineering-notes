# Serving Static Files

## Concept
nginx excels at serving static content (HTML, CSS, JS, images) directly from disk. This is the first practical feature most users configure.

**Prerequisites:** [server-and-location.md](server-and-location.md)

## Minimal Example
```nginx
server {
    listen 80;
    server_name example.com;

    root /var/www/example;
    index index.html;

    location / {
        try_files $uri $uri/ =404;
    }
}
```

## `root` vs `alias`

| Directive | Behavior | Example: request `/img/a.png` |
|---|---|---|
| `root /data;` | Appends the **full URI** to the path | `/data/img/a.png` |
| `alias /data/;` | **Replaces** the matched location prefix | with `location /img/ { alias /data/; }` gives `/data/a.png` |

Rules for `alias`:
- If the location ends with `/`, the alias path should too.
- Prefer `root` when the URI structure mirrors the filesystem. Use `alias` only when it does not.
- `alias` combined with a regex location or `try_files` has known edge cases, so test carefully.

## `index` and `try_files`
```nginx
index index.html index.htm;           # files tried when a directory is requested

location / {
    try_files $uri $uri/ /index.html; # SPA fallback: unknown paths serve the app shell
}
```
`try_files` checks each option in order and uses the last argument as the fallback (a URI, a named location, or a status code like `=404`).

## MIME Types
```nginx
http {
    include       mime.types;
    default_type  application/octet-stream;
}
```
Wrong or missing MIME types cause browsers to download files instead of rendering them, or to refuse to execute scripts.

## Cache Headers for Static Assets
```nginx
location ~* \.(css|js|png|jpg|svg|woff2)$ {
    expires 30d;
    add_header Cache-Control "public, immutable";
}
```
Use long expiry only for fingerprinted filenames (e.g. `app.3f9a1c.js`). HTML should have short or no caching.

## Directory Listing
```nginx
location /downloads/ {
    autoindex on;
}
```
Off by default. Enable only intentionally, since it exposes file names.

## Custom Error Pages
```nginx
error_page 404 /404.html;
error_page 500 502 503 504 /50x.html;
location = /50x.html { root /usr/share/nginx/html; }
```

## Common Mistakes
- **403 Forbidden:** the nginx user cannot read the file or traverse a parent directory. Check permissions with `namei -l /var/www/example/index.html`.
- **404 with `alias`:** missing or extra trailing slash.
- **Using `root` inside every location** instead of once at the `server` level.
- **SPA routes returning 404:** missing `try_files ... /index.html`.
- **SELinux (RHEL):** files must have the `httpd_sys_content_t` label.

## Performance Note
`sendfile`, compression, and open-file caching improve static serving. These are covered in [performance-tuning](../05-security-performance/performance-tuning.md).

## Quick Reference

| Goal | Directive |
|---|---|
| Set web root | `root /path;` |
| Map URL prefix to a different folder | `alias /path/;` |
| Default file | `index index.html;` |
| SPA fallback | `try_files $uri $uri/ /index.html;` |
| Browser caching | `expires 30d;` |
| Directory listing | `autoindex on;` |

## Related / Next
- [reverse-proxy](../02-core-features/reverse-proxy.md): forwarding requests to an application server
- [tls-https](../02-core-features/tls-https.md): serving the same content over HTTPS
