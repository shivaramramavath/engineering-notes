# Quick Reference: FastAPI CLI & Uvicorn Production Cheatsheet

## FastAPI CLI (FastAPI 0.111.0+)

```bash
# Start development server with live reload and auto-detected entrypoint
fastapi dev main.py

# Start production server with auto-tuned workers
fastapi run main.py --port 8000

# Inspect OpenAPI schema from CLI
fastapi openapi main.py
```

---

## Uvicorn Command-Line Execution

```bash
# Standard local development with hot reload
uvicorn src.main:app --reload --host 127.0.0.1 --port 8000

# Multi-worker production process manager (Bare metal / VM only)
uvicorn src.main:app --host 0.0.0.0 --port 8000 --workers 4 --proxy-headers --forwarded-allow-ips='*'

# High-performance event loop and parser overrides
uvicorn src.main:app --loop uvloop --http httptools --ws websockets
```

---

## Programmatic Uvicorn Startup (`src/main.py`)

```python
import uvicorn
from fastapi import FastAPI

app = FastAPI()

if __name__ == "__main__":
    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=8000,
        reload=False,
        log_level="info",
        access_log=False,  # Disable access logs for ultra-high throughput
        timeout_keep_alive=30,
    )
```

---

## Production Flags Matrix

| Flag | Purpose | Recommended Production Setting |
|:---|:---|:---|
| `--workers` | Number of worker child processes | 1 per K8s Pod; `(2 x CPU) + 1` on VMs |
| `--proxy-headers` | Reads `X-Forwarded-For` / `X-Forwarded-Proto` | **Always TRUE** behind Nginx / ALB |
| `--forwarded-allow-ips` | IP addresses permitted to set proxy headers | Set to VPC CIDR or `'*'` if behind private ALB |
| `--timeout-keep-alive` | Seconds to hold open idle TCP keep-alive sockets | `30` to `65` (Must exceed LB idle timeout) |
| `--limit-concurrency` | Maximum concurrent connections before 503 | `1000` to `5000` (Prevents runaway memory OOM) |
| `--limit-max-requests` | Restarts worker after N requests (curbs memory leaks) | `100000` |
| `--log-config` | Custom logging configuration dict or JSON/YAML | Structured JSON logging config |
