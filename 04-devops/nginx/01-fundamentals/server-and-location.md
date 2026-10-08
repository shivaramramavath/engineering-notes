# Server and Location Matching

## Concept
For every request, nginx makes two decisions: which **`server`** block handles it, then which **`location`** inside that server. Most routing bugs come from misunderstanding one of these.

**Prerequisites:** [config-structure.md](config-structure.md)

## Step 1: Choosing a `server` Block

1. nginx matches the request's IP and port against `listen` directives.
2. Among the matching servers, it compares the `Host` header against `server_name`.
3. If nothing matches, the **default server** for that IP:port is used. That is the one marked `default_server`, otherwise the first one defined.

```nginx
server {
    listen 80 default_server;
    server_name _;
    return 444;                 # drop requests for unknown hosts
}

server {
    listen 80;
    server_name example.com www.example.com;
    root /var/www/example;
}
```

### `server_name` match priority
1. Exact name: `example.com`
2. Longest wildcard starting with `*`: `*.example.com`
3. Longest wildcard ending with `*`: `mail.*`
4. First matching regex (in file order): `~^(?<sub>.+)\.example\.com$`
5. Default server

## Step 2: Choosing a `location`

### Modifiers

| Syntax | Meaning |
|---|---|
| `location = /path` | Exact match |
| `location ^~ /path` | Prefix match; if it is the best prefix, skip regex checks |
| `location ~ regex` | Case-sensitive regex |
| `location ~* regex` | Case-insensitive regex |
| `location /path` | Plain prefix match |

### Matching algorithm
1. Check for an **exact** (`=`) match. If found, use it and stop.
2. Find the **longest matching prefix** location. If it uses `^~`, use it and stop.
3. Test **regex** locations in the order they appear in the file. The first match wins.
4. If no regex matched, use the longest prefix match from step 2.

Key point: regex locations are checked by **file order**, prefix locations by **length**.

```nginx
location = /          { return 200 "exact root"; }
location /            { return 200 "default"; }
location /images/     { root /data; }
location ^~ /static/  { root /var/www; }       # beats regex below
location ~* \.(jpg|png)$ { expires 30d; }      # /images/a.jpg hits this, not /images/
```

## Trailing Slashes and `proxy_pass`
- `location /api/` with `proxy_pass http://backend;` forwards the full URI `/api/x` unchanged.
- `location /api/` with `proxy_pass http://backend/;` (note trailing `/`) replaces the matched prefix, so `/api/x` becomes `/x`.

See [reverse-proxy](../02-core-features/reverse-proxy.md) for details.

## Named and Internal Locations
```nginx
location @fallback { proxy_pass http://backend; }   # target for try_files / error_page
location /protected/ { internal; }                  # not reachable directly by clients
```

## Common Mistakes
- Assuming regex locations are matched by specificity. They are matched by order.
- Overlapping regexes where the wrong one wins because it appears first.
- Forgetting `default_server`, so unexpected Host headers hit a real site.
- Mixing up `root` and `alias` (see [static-files.md](static-files.md)).

## Debugging
```bash
curl -I -H "Host: example.com" http://127.0.0.1/some/path
sudo nginx -T | grep -A5 "server_name"
```
Add a temporary `add_header X-Location "images";` in each location to see which one handled the request.

## Quick Reference
Order of precedence: `=` exact, then `^~` longest prefix, then regex (first match), then longest plain prefix.

## Related / Next
- [static-files.md](static-files.md): serving files from the matched location
- [request-lifecycle](../04-internals/request-lifecycle.md): where location selection fits in request processing
