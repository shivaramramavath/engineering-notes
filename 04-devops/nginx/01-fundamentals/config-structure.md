# Config Structure

## Concept
nginx is configured by a text file made of **directives** grouped into nested **contexts**. Understanding contexts and inheritance explains most "why doesn't this work?" problems.

**Prerequisites:** [installation.md](installation.md)

## Anatomy

```nginx
user www-data;                      # main context (top level)
worker_processes auto;

events {                            # events context
    worker_connections 1024;
}

http {                              # http context
    include       mime.types;
    default_type  application/octet-stream;

    server {                        # server context
        listen 80;
        server_name example.com;

        location / {                # location context
            root /var/www/html;
        }
    }
}
```

## Directive Types
- **Simple directive:** a name, parameters, and a semicolon: `listen 80;`
- **Block directive:** a name and a `{ ... }` block that can contain other directives: `server { ... }`
- A missing semicolon or unbalanced brace is the most common syntax error.

## Contexts

| Context | Contains | Notes |
|---|---|---|
| main | global settings (`user`, `worker_processes`, `pid`) | Top level of the file |
| `events` | connection handling | Required |
| `http` | all web server config | Holds `server` and `upstream` blocks |
| `server` | one virtual host | Chosen by `listen` and `server_name` |
| `location` | rules for a URI | Nested in `server`; can nest in another `location` |
| `upstream` | backend server pool | Lives in `http`, see [load-balancing](../03-traffic-management/load-balancing.md) |
| `stream` | TCP/UDP proxying | Sibling of `http`, only if needed |

A directive is only valid in specific contexts. The nginx docs list the allowed contexts for each directive.

## Inheritance Rules
Values set in an outer context are inherited by inner contexts unless overridden.

```nginx
http {
    gzip on;                 # applies to everything below
    server {
        location /api/ {
            gzip off;        # overrides for this location only
        }
    }
}
```

Important caveats:
- **Array-style directives are replaced, not merged.** If a child context defines any `add_header`, it drops all `add_header` lines inherited from the parent. The same applies to `proxy_set_header`, `error_page`, and similar directives.
- Inheritance flows from outer to inner. A `location` setting never affects its parent `server`.

## Organizing with `include`
```nginx
http {
    include /etc/nginx/mime.types;
    include /etc/nginx/conf.d/*.conf;       # one file per site
    include /etc/nginx/snippets/ssl.conf;   # reusable fragments
}
```
Keep `nginx.conf` small. Put each site in its own file and reuse common blocks as snippets.

## Test and Apply
```bash
sudo nginx -t          # syntax and semantic check
sudo nginx -T          # test and dump the full merged config (includes expanded)
sudo nginx -s reload   # apply changes gracefully
```
Always run `nginx -t` before reloading.

## Common Mistakes
- Placing a directive in the wrong context (`nginx -t` reports "directive is not allowed here").
- Expecting `add_header` or `proxy_set_header` to merge with parent values.
- Editing a file that is not actually included.
- Using `if` inside `location` for logic better handled by `try_files`, `map`, or `return`. See [variables-and-rewrites](../02-core-features/variables-and-rewrites.md).

## Best Practices
- One `server` file per site under `conf.d/` or `sites-enabled/`.
- Put shared settings in the `http` context, and site-specific overrides in `server` or `location`.
- Comment non-obvious values and keep configs in version control.

## Related / Next
- [server-and-location.md](server-and-location.md): how nginx chooses which block handles a request
- [request-lifecycle](../04-internals/request-lifecycle.md): how directives merge at runtime
