# Load Testing & Reverse Proxy Balancing in Node.js

> Measuring backend throughput with Autocannon, k6, tuning HTTP keep-alive, and configuring Nginx load balancing.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Benchmarking Tools** | `autocannon`, `k6` |
| **Reverse Proxies** | Nginx, AWS ALB |

---

## 1. Benchmarking with `autocannon`

Autocannon tests the maximum capacity of your Node.js endpoints:

```bash
# Basic benchmark: 100 concurrent connections for 20 seconds
npx autocannon -c 100 -d 20 http://localhost:3000/api/users

# Benchmark with pipeline requests (HTTP/1.1 pipelining)
npx autocannon -c 100 -d 20 -p 10 http://localhost:3000/api/users
```

Interpreting the Output:
- **Req/Sec (Requests Per Second)**: Higher is better.
- **Latency Percentiles (p50, p90, p99)**: The p99 metric shows tail latency experienced by the slowest 1% of users. A healthy backend keeps p99 $< 100$ms under load.

---

## 2. Nginx Reverse Proxy Load Balancing Configuration

Distribute traffic across multiple local Node.js process instances:

```nginx
upstream node_backend_cluster {
  # Least connections algorithm
  least_conn;

  server 127.0.0.1:3001 max_fails=3 fail_timeout=10s;
  server 127.0.0.1:3002 max_fails=3 fail_timeout=10s;
  server 127.0.0.1:3003 max_fails=3 fail_timeout=10s;
  server 127.0.0.1:3004 max_fails=3 fail_timeout=10s;

  # Maintain persistent keep-alive connections to Node.js backend
  keepalive 64;
}

server {
  listen 80;
  server_name api.example.com;

  location / {
    proxy_pass http://node_backend_cluster;
    proxy_http_version 1.1;
    proxy_set_header Connection "";
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
  }
}
```
Enabling `keepalive 64` prevents Nginx from renegotiating TCP handshakes on every proxied request.
