<!--
================================================================================
Metadata:
  Title: Quick Reference Cheat Sheet - Production Container Security Checklist
  Track: Docker / DevOps
  Difficulty: All Levels
  Estimated Time: 25 minutes
  Key Terminology: CIS Benchmark, Least Privilege, Seccomp, AppArmor, Hardening, Non-Root
================================================================================
-->

# Production Container Security Checklist

[<- 05-buildx-and-buildkit.md](./05-buildx-and-buildkit.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 07-debugging-flowchart.md ->](./07-debugging-flowchart.md)

---

## 1. Image Build & Dockerfile Checklist

- [ ] **Run as Non-Root**: Explicitly declare `USER 10001:10001` in the final runtime stage. Never execute as default `root` (UID 0).
- [ ] **Multi-Stage Minimization**: Use multi-stage builds to discard compilers, package managers, and SDKs. Final images should be `distroless` or `alpine`.
- [ ] **Pin Exact Versions**: Never use `:latest` or broad floating tags. Pin semantic tags (`node:20.12.2-alpine3.19`) or immutable digests (`@sha256:...`).
- [ ] **Maintain Strict `.dockerignore`**: Exclude `.git`, `.env*`, `node_modules`, build artifacts, and private SSH keys from build contexts.
- [ ] **Use BuildKit Secrets**: Never pass secrets via `ARG` or `ENV`. Use `RUN --mount=type=secret,id=...` for credentials needed during compilation.
- [ ] **Clean Package Caches in Same Layer**: Chain package installation with cache cleanup: `apt-get clean && rm -rf /var/lib/apt/lists/*`.
- [ ] **Avoid `ADD`**: Use `COPY` exclusively unless local tarball extraction is explicitly required.
- [ ] **Use JSON Exec Form**: Declare `ENTRYPOINT ["executable", "arg1"]` to guarantee signal forwarding (`SIGTERM`).

---

## 2. Supply Chain & Registry Checklist

- [ ] **Automate CVE Vulnerability Scanning**: Run `trivy` or `grype` in CI/CD pipelines. Fail builds on unpatched Critical/High CVEs (`--exit-code 1`).
- [ ] **Generate and Attach SBOMs**: Produce CycloneDX or SPDX Software Bills of Materials using `syft` and attach them to registry manifests.
- [ ] **Sign Images Cryptographically**: Sign image digests using `cosign` and enforce signature verification prior to production admission.
- [ ] **Enable Registry Authentication**: Private registries must enforce TLS/HTTPS encryption and token or `htpasswd` authentication.
- [ ] **Enable Immutable Tags**: Configure cloud registries (AWS ECR, Harbor) with immutable tag rules to prevent tag tampering.

---

## 3. Container Runtime Hardening Checklist

- [ ] **Drop All Linux Capabilities**: Run with `--cap-drop=ALL`. Add back only strictly required capabilities (e.g. `--cap-add=NET_BIND_SERVICE`).
- [ ] **Enable `no-new-privileges`**: Enforce `--security-opt=no-new-privileges:true` to block SUID/SGID privilege escalation.
- [ ] **Enforce Read-Only Root Filesystem**: Run `--read-only` combined with ephemeral in-memory `--tmpfs /tmp:rw,noexec,nosuid,size=64m`.
- [ ] **Never Mount Host Docker Socket**: Absolutely forbid mounting `/var/run/docker.sock` in public-facing application containers.
- [ ] **Enforce Process Ceilings**: Set `--pids-limit=200` to prevent fork bombs from exhausting host kernel process tables.
- [ ] **Enforce Hard Memory and CPU Limits**: Always configure `-m <limit> --memory-swap <limit>` and `--cpus="<limit>"` to prevent noisy neighbors and host kernel OOM panics.
- [ ] **Keep Default Seccomp Profile**: Never run `--security-opt seccomp=unconfined` in production.
- [ ] **Bind Published Ports to Localhost**: Map ports strictly to localhost loopback (`-p 127.0.0.1:8080:8080`) unless public routing is intentional.
- [ ] **Never Use `--privileged`**: Disallow `--privileged` flag across all production container manifests.

---

## 4. Host OS and Docker Daemon Checklist

- [ ] **Enable `live-restore`**: Set `"live-restore": true` in `daemon.json` to prevent container teardown during daemon restarts.
- [ ] **Enforce Global Log Rotation**: Configure daemon-level log options (`max-size: 50m`, `max-file: 3`) to prevent root disk partition exhaustion.
- [ ] **Disable Userland Proxy**: Set `"userland-proxy": false` to route traffic directly through kernel netfilter rules.
- [ ] **Disable Default Bridge ICC**: Set `"icc": false` to prevent inter-container communication on the default bridge network.
- [ ] **Consider Rootless Docker**: Run Docker daemon under an unprivileged user account to eliminate host root attack surfaces.
- [ ] **Audit Host with CIS Benchmark**: Periodically run `docker-bench-security` against production host machines:
  ```bash
  docker run --rm --net host --pid host --userns host --cap-add audit_control \
    -v /etc:/etc:ro -v /var/lib:/var/lib:ro -v /var/run/docker.sock:/var/run/docker.sock \
    docker/docker-bench-security
  ```

---

[<- 05-buildx-and-buildkit.md](./05-buildx-and-buildkit.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 07-debugging-flowchart.md ->](./07-debugging-flowchart.md)
