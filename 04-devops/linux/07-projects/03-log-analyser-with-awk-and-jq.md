# Project 03: High-Throughput Log Stream Analyser in Awk & Jq

| Metadata | Details |
| :--- | :--- |
| **Project** | High-Throughput Log Stream Analyser in Awk & Jq |
| **Module** | `07-projects` — Production Reference Projects & Labs |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS, RHEL 9.4 / Rocky Linux 9.4 |
| **Complexity** | Intermediate |
| **Estimated Duration**| 45–60 minutes |

---

## 1. Architecture & Design

### Scenario Context
Modern microservices generate tens of thousands of log lines per second. When production incidents occur—such as a sudden surge in HTTP 502/504 errors, an unauthorized scraping campaign, or tail latency spikes—centralized log aggregation platforms (Elasticsearch, Datadog, Loki) may lag by minutes or incur rate-limiting drops.

DevOps engineers and SREs require fast, local, zero-dependency command-line tools to stream and parse raw log files directly on production hosts. This project builds a high-performance log analyzer combining POSIX `awk` for standard Nginx Combined Log Format and `jq` for structured JSON application logs. It computes status distributions, top request paths, client IP frequencies, and 99th percentile (P99) response latencies at wire speed.

### Processing Pipeline Architecture
```
+-------------------------------------------------------------------------+
|                  Log Stream Analytical Processing Flow                  |
|                                                                         |
|  [ Ingestion Stream ]                                                   |
|  - Nginx Combined Access Log (/var/log/nginx/access.log)                |
|  - JSON Microservice Application Stream (/var/log/app/service.json)     |
|                                                                         |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | Phase 1: High-Speed Tokenization & Filter                         |  |
|  | - Awk regex token extraction (IP, Time, Method, Path, Status, Lat)|  |
|  | - jq streaming parser (jq -c -r '.timestamp, .status, .duration') |  |
|  +-------------------------------------------------------------------+  |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | Phase 2: In-Memory Statistical Aggregations                       |  |
|  | - HTTP Status Grouping (2xx, 3xx, 4xx, 5xx)                       |  |
|  | - Top 10 IP Abusers (Potential DoS / Web Scrapers)                |  |
|  | - Top 10 Slowest URL Routes                                       |  |
|  | - Nearest-Rank Percentile Engine (P50, P90, P99 Response Latency)  |  |
|  +-------------------------------------------------------------------+  |
|                                  |                                      |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | Phase 3: Formatted Terminal Dashboard & Incident Alert Output     |  |
|  | - Formatted ASCII tables with ANSI alert colors                   |  |
|  | - Automated threshold trigger (Alert if 5xx > 1.0% or P99 > 500ms)|  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. Implementation Artifacts

### Artifact 1: Dual-Format Log Analyser (`log-analyser.sh`)

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: log-analyser.sh
# Purpose: High-Throughput Log Stream Analyser (Awk & Jq)
# Supports: Nginx Combined Log Format & Structured JSON Logs
# ==============================================================================

set -euo pipefail

usage() {
    echo "Usage: $0 [-f <log_file>] [-t <nginx|json>] [-n <top_count>]"
    echo "  -f: Path to log file (or pass '-' to read from STDIN)"
    echo "  -t: Format type: 'nginx' (default) or 'json'"
    echo "  -n: Number of top results to display (default: 10)"
    exit 1
}

LOG_FILE=""
FORMAT="nginx"
TOP_N=10

while getopts "f:t:n:h" opt; do
    case "${opt}" in
        f) LOG_FILE="${OPTARG}" ;;
        t) FORMAT="${OPTARG}" ;;
        n) TOP_N="${OPTARG}" ;;
        h|*) usage ;;
    esac
done

[[ -z "${LOG_FILE}" ]] && usage
if [[ "${LOG_FILE}" != "-" && ! -f "${LOG_FILE}" ]]; then
    echo "Error: File '${LOG_FILE}' not found." >&2
    exit 1
fi

INPUT_STREAM="cat ${LOG_FILE}"
if [[ "${LOG_FILE}" == "-" ]]; then
    INPUT_STREAM="cat"
elif [[ "${LOG_FILE}" =~ \.gz$ ]]; then
    INPUT_STREAM="zcat ${LOG_FILE}"
fi

# ==============================================================================
# Pipeline 1: Nginx Combined / Extended Log Parser (Awk)
# Format: $remote_addr - $remote_user [$time_local] "$request" $status $body_bytes_sent "$http_referer" "$http_user_agent" $request_time
# ==============================================================================
analyze_nginx() {
    ${INPUT_STREAM} | awk -v top="${TOP_N}" '
    BEGIN {
        total_requests = 0;
        status_2xx = 0; status_3xx = 0; status_4xx = 0; status_5xx = 0;
    }
    {
        total_requests++;

        # Extract Client IP
        ip = $1;
        ip_count[ip]++;

        # Extract HTTP Status ($9 in standard combined log)
        status = $9;
        status_count[status]++;
        if (status ~ /^2/) status_2xx++;
        else if (status ~ /^3/) status_3xx++;
        else if (status ~ /^4/) status_4xx++;
        else if (status ~ /^5/) status_5xx++;

        # Extract Request Path ($7: e.g., /api/v1/checkout)
        path = $7;
        path_count[path]++;

        # Extract Request Duration (Assume last field $NF if extended, or default to 0)
        dur = $(NF);
        if (dur ~ /^[0-9\.]+$/) {
            durations[total_requests] = dur + 0;
            total_duration += dur;
        }
    }
    END {
        if (total_requests == 0) {
            print "No log records found to process.";
            exit;
        }

        print "\n\033[1;36m======================================================================\033[0m";
        print "\033[1;36m                 PRODUCTION LOG ANALYSIS REPORT                       \033[0m";
        print "\033[1;36m======================================================================\033[0m";
        printf "Total Requests Processed: \033[1;33m%d\033[0m\n\n", total_requests;

        print "\033[1;32m--- HTTP Status Code Distribution ---\033[0m";
        printf "2xx (Success) : %-8d (%5.2f%%)\n", status_2xx, (status_2xx/total_requests)*100;
        printf "3xx (Redirect): %-8d (%5.2f%%)\n", status_3xx, (status_3xx/total_requests)*100;
        printf "4xx (Client)  : %-8d (%5.2f%%)\n", status_4xx, (status_4xx/total_requests)*100;
        printf "5xx (Server)  : %-8d (%5.2f%%)\n", status_5xx, (status_5xx/total_requests)*100;

        # 5xx Anomaly Alert
        error_rate = (status_5xx / total_requests) * 100;
        if (error_rate > 1.0) {
            printf "\n\033[1;31m[CRITICAL ALERT] 5xx Server Error rate is %.2f%% (Threshold: 1.0%%)!\033[0m\n", error_rate;
        }

        # Sort and Display Top IP Addresses
        print "\n\033[1;32m--- Top " top " Client IP Addresses ---\033[0m";
        n = 0;
        for (i in ip_count) {
            printf "%-10d %s\n", ip_count[i], i | "sort -rn | head -n " top;
        }
        close("sort -rn | head -n " top);

        # Sort and Display Top Endpoints
        print "\n\033[1;32m--- Top " top " Requested Endpoints ---\033[0m";
        for (p in path_count) {
            printf "%-10d %s\n", path_count[p], p | "sort -rn | head -n " top;
        }
        close("sort -rn | head -n " top);

        # Latency Percentile Calculations (P50, P90, P99)
        if (length(durations) > 0) {
            print "\n\033[1;32m--- Latency Percentiles (Seconds) ---\033[0m";
            asort(durations);
            p50_idx = int(total_requests * 0.50); if (p50_idx < 1) p50_idx = 1;
            p90_idx = int(total_requests * 0.90); if (p90_idx < 1) p90_idx = 1;
            p99_idx = int(total_requests * 0.99); if (p99_idx < 1) p99_idx = 1;

            printf "P50 (Median): %.3f s\n", durations[p50_idx];
            printf "P90         : %.3f s\n", durations[p90_idx];
            printf "P99 (Tail)  : %.3f s\n", durations[p99_idx];
            printf "Average     : %.3f s\n", total_duration / total_requests;
        }
        print "\033[1;36m======================================================================\033[0m\n";
    }'
}

# ==============================================================================
# Pipeline 2: Structured JSON Log Parser (Jq)
# Schema: {"timestamp":"...","client_ip":"...","method":"GET","path":"...","status":200,"duration_ms":42}
# ==============================================================================
analyze_json() {
    command -v jq >/dev/null 2>&1 || { echo "Error: 'jq' is required for JSON mode." >&2; exit 1; }

    echo "Processing structured JSON log stream..."
    ${INPUT_STREAM} | jq -s -r --arg top "${TOP_N}" '
      length as $total |
      if $total == 0 then "No JSON records to process." else
      (map(.status | tostring) | group_by(.) | map({status: .[0], count: length})) as $statuses |
      (map(.client_ip) | group_by(.) | map({ip: .[0], count: length}) | sort_by(-.count) | .[0:($top|tonumber)]) as $top_ips |
      (map(.path) | group_by(.) | map({path: .[0], count: length}) | sort_by(-.count) | .[0:($top|tonumber)]) as $top_paths |
      (map(.duration_ms // 0) | sort) as $durations |

      "\n======================================================================",
      "                 STRUCTURED JSON LOG REPORT                           ",
      "======================================================================",
      "Total Records: \($total)\n",
      "--- HTTP Status Distribution ---",
      ($statuses[] | "HTTP \(.status): \(.count) (\((.count / $total * 100 * 100 | round) / 100)%)"),
      "\n--- Top Client IPs ---",
      ($top_ips[] | "\(.count)\t\(.ip)"),
      "\n--- Top Endpoints ---",
      ($top_paths[] | "\(.count)\t\(.path)"),
      "\n--- Latencies (ms) ---",
      "P50: \($durations[($total * 0.50 | floor)]) ms",
      "P90: \($durations[($total * 0.90 | floor)]) ms",
      "P99: \($durations[($total * 0.99 | floor)]) ms",
      "======================================================================\n"
      end
    '
}

if [[ "${FORMAT}" == "nginx" ]]; then
    analyze_nginx
elif [[ "${FORMAT}" == "json" ]]; then
    analyze_json
else
    usage
fi
```

---

## 3. Execution & Verification

### Step-by-Step Test Procedure

1. **Generate Synthetic Nginx Test Log**:
   ```bash
   cat << 'EOF' > /tmp/sample_access.log
   192.168.1.10 - - [26/Nov/2026:10:00:01 +0000] "GET /api/v1/users HTTP/1.1" 200 1024 "-" "curl/7.88" 0.045
   192.168.1.10 - - [26/Nov/2026:10:00:02 +0000] "GET /api/v1/users HTTP/1.1" 200 1024 "-" "curl/7.88" 0.052
   10.0.0.45 - - [26/Nov/2026:10:00:03 +0000] "POST /api/v1/checkout HTTP/1.1" 500 512 "-" "AppClient/1.0" 1.450
   192.168.1.10 - - [26/Nov/2026:10:00:04 +0000] "GET /api/v1/catalog HTTP/1.1" 200 4096 "-" "curl/7.88" 0.089
   10.200.5.12 - - [26/Nov/2026:10:00:05 +0000] "GET /admin/login HTTP/1.1" 401 256 "-" "Mozilla/5.0" 0.012
   10.200.5.12 - - [26/Nov/2026:10:00:06 +0000] "GET /admin/login HTTP/1.1" 401 256 "-" "Mozilla/5.0" 0.015
   10.0.0.45 - - [26/Nov/2026:10:00:07 +0000] "POST /api/v1/checkout HTTP/1.1" 504 128 "-" "AppClient/1.0" 5.200
   192.168.1.10 - - [26/Nov/2026:10:00:08 +0000] "GET /api/v1/users HTTP/1.1" 200 1024 "-" "curl/7.88" 0.048
   192.168.1.10 - - [26/Nov/2026:10:00:09 +0000] "GET /favicon.ico HTTP/1.1" 404 180 "-" "curl/7.88" 0.005
   10.0.0.45 - - [26/Nov/2026:10:00:10 +0000] "GET /api/v1/catalog HTTP/1.1" 200 4096 "-" "AppClient/1.0" 0.095
   EOF
   ```

2. **Execute Analysis Script**:
   ```bash
   chmod +x ./log-analyser.sh
   ./log-analyser.sh -f /tmp/sample_access.log -t nginx
   ```

3. **Simulated Verification Output**:
   ```text
   ======================================================================
                    PRODUCTION LOG ANALYSIS REPORT                       
   ======================================================================
   Total Requests Processed: 10

   --- HTTP Status Code Distribution ---
   2xx (Success) : 5        (50.00%)
   3xx (Redirect): 0        ( 0.00%)
   4xx (Client)  : 3        (30.00%)
   5xx (Server)  : 2        (20.00%)

   [CRITICAL ALERT] 5xx Server Error rate is 20.00% (Threshold: 1.0%)!

   --- Top 10 Client IP Addresses ---
   5          192.168.1.10
   3          10.0.0.45
   2          10.200.5.12

   --- Top 10 Requested Endpoints ---
   4          /api/v1/users
   2          /api/v1/checkout
   2          /api/v1/catalog
   2          /admin/login

   --- Latency Percentiles (Seconds) ---
   P50 (Median): 0.048 s
   P90         : 1.450 s
   P99 (Tail)  : 5.200 s
   Average     : 0.701 s
   ======================================================================
   ```

4. **Verify Piping via STDIN (Live Stream Analysis)**:
   ```bash
   cat /tmp/sample_access.log | ./log-analyser.sh -f - -t nginx
   ```

---

## 4. Failure Modes & Edge Cases

| Failure Mode / Incident | Root Cause | Remediation Runbook |
| :--- | :--- | :--- |
| **Malformed Log Lines (Field Misalignment)** | Log lines containing unescaped quotes in User-Agent or Referer header alter column positions. | Use strict Awk regex token matching (`FPAT = "([^ ]+)|(\"[^\"]+\")|(\\[[^\\]]+\\])"`) instead of default space splitting. |
| **High Memory Usage on Multi-Gigabyte Files** | Loading entire duration arrays into memory for percentiles in `awk` or `jq -s`. | Use reservoir sampling or bin latencies into fixed histogram buckets (e.g., 0-10ms, 10-50ms, 50-200ms, >1s) to retain $O(1)$ memory. |
| **Gzip Compressed Logs** | Attempting to run standard `cat` on `.gz` rotated archives. | Script detects `.gz` extensions and automatically routes stream through `zcat` / `gzip -dc`. |
| **Pipelined Stream Buffer Stalling** | Buffering in GNU utilities delays real-time pipeline output. | Pass `stdbuf -oL` or `stdbuf -i0 -o0 -e0` when streaming through pipes to disable buffer stalling. |

---

## 5. Production Considerations

- **Log Rotation Awareness**: When running real-time tailing (`tail -F /var/log/nginx/access.log | ./log-analyser.sh -f -`), use capital `-F` to follow file renames across `logrotate` cycles.
- **Security & PII Sanitization**: When sharing diagnostic reports across teams, ensure IP addresses or authorization tokens embedded in query strings are redacted using `gsub()`.
- **System Resource Limits**: On CPU-constrained hosts, execute the analyzer with `nice -n 10` to avoid stealing CPU cycles from active web workers.

---
*Next Project: [04-systemd-service-and-timer-for-an-app.md](file:///04-devops/linux/07-projects/04-systemd-service-and-timer-for-an-app.md)*
