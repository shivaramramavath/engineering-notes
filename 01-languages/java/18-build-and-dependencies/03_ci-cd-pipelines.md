# CI/CD Pipelines

A build that only runs on one developer's laptop isn't really a build. **Continuous Integration (CI)** runs the build and tests automatically on **every change**, on a clean machine, so breakage is found in minutes instead of weeks. **Continuous Delivery/Deployment (CD)** extends that to getting the tested artifact into environments, up to production, in a repeatable, low-risk way.

The pipeline is where everything in this module (wrappers, reproducible builds, dependency scanning) and the next ones (testing, packaging, deployment) come together.

**Prerequisites:** [Maven](00_maven.md) or [Gradle](01_gradle.md), [Dependency Management](02_dependency-management.md).

---

## 1. What CI and CD mean

| Term | Meaning |
|---|---|
| **Continuous Integration** | Developers merge small changes to the shared branch frequently. Each change triggers an automated build + test. A broken build is fixed immediately |
| **Continuous Delivery** | Every change that passes the pipeline is **releasable**. Deploying to production is a (usually manual) button press |
| **Continuous Deployment** | Every change that passes the pipeline is **deployed to production automatically** |

The point isn't tooling. It's **fast, trustworthy feedback** and **small, reversible releases**. Teams that deploy small changes often generally have fewer and shorter failures than teams that deploy rarely and in large batches.

---

## 2. Anatomy of a Java pipeline

```text
 push / pull request
     │
     ▼
 ┌─────────┐  ┌──────────┐  ┌────────────┐  ┌───────────┐  ┌──────────┐
 │ checkout│→ │ build +  │→ │ static     │→ │ package   │→ │ publish  │   ← runs on every PR
 │         │  │ unit test│  │ analysis + │  │ artifact/ │  │ artifact │
 │         │  │          │  │ dependency │  │ image     │  │          │
 └─────────┘  └──────────┘  │ scan       │  └───────────┘  └────┬─────┘
                            └────────────┘                      │
                                                                ▼
        ┌──────────────┐   ┌─────────────┐   ┌────────────┐   ┌───────────────┐
        │ integration  │ ← │ deploy to   │ ← │ (approve?) │ ← │ same artifact │  ← runs on main / tags
        │ / smoke tests│   │ staging     │   │            │   │ promoted      │
        └──────┬───────┘   └─────────────┘   └────────────┘   └───────────────┘
               ▼
        deploy to production (rolling / blue-green / canary) → verify health → done (or roll back)
```

Core principles:

- **Same build everywhere.** Use the wrapper (`./mvnw`, `./gradlew`) so CI and your laptop run the same tool versions, and a pinned Java version.
- **Fast feedback first.** Run the cheap checks (compile, unit tests, lint) before the slow ones, and fail fast.
- **Build once, deploy many.** Build the artifact **once**, and **promote the same artifact** (same checksum/image digest) through staging to production. Never rebuild for each environment, because then you aren't deploying what you tested.
- **Pipeline as code.** The pipeline definition lives in the repository, reviewed like any other code.
- **Keep logic in scripts/build files, not buried in YAML.** If `./mvnw verify` (or a script) works locally, the CI file just calls it.
- **Immutable artifacts.** A release is never modified after it's published. Config differs per environment, but the artifact doesn't ([Configuration Management](../22-production-engineering/01_configuration-management.md)).

---

## 3. A GitHub Actions workflow

```yaml
# .github/workflows/ci.yml
name: CI

on:
  push:
    branches: [main]
  pull_request:

permissions:
  contents: read                    # least privilege for the default token

concurrency:
  group: ci-${{ github.ref }}
  cancel-in-progress: true          # cancel superseded runs of the same branch/PR

jobs:
  build:
    runs-on: ubuntu-latest
    timeout-minutes: 20
    steps:
      - uses: actions/checkout@v4                   # use the current major version; pin to a commit SHA for stricter security

      - uses: actions/setup-java@v4
        with:
          distribution: temurin
          java-version: '21'
          cache: maven                              # caches ~/.m2 keyed on your POM files

      - name: Build and test
        run: ./mvnw -B -ntp verify                  # batch mode, no download progress noise

      - name: Upload test reports
        if: always()                                # even when tests fail
        uses: actions/upload-artifact@v4
        with:
          name: test-reports
          path: '**/target/surefire-reports/**'

      - name: Upload the built jar
        uses: actions/upload-artifact@v4
        with:
          name: app
          path: target/*.jar
```

For Gradle, use `cache: gradle` in `setup-java` or the dedicated Gradle setup action (which also manages the Gradle caches and build-cache behavior), and run `./gradlew build`.

**Matrix builds** test a library against several Java versions:

```yaml
    strategy:
      matrix:
        java: [17, 21, 25]
    steps:
      - uses: actions/setup-java@v4
        with: { distribution: temurin, java-version: '${{ matrix.java }}' }
```

(Compile with `--release` for the minimum supported version, and run tests on each runtime.)

The same ideas apply on **GitLab CI** (`.gitlab-ci.yml`), **Jenkins** (`Jenkinsfile`), **Azure DevOps**, **CircleCI**, and **Tekton**. Syntax differs and concepts don't.

### Caching

Downloading the world on every run is slow and flaky. Cache `~/.m2/repository` / `~/.gradle`, **keyed on the dependency files** (POMs, version catalog, lockfiles), so the cache invalidates when dependencies change. Gradle's remote **build cache** lets CI share task outputs, which can skip entire compile/test tasks.

---

## 4. Test stages in the pipeline

Separate tests by **speed and needs**, so most feedback is quick ([Testing](../19-testing/README.md)):

| Stage | What | Tools | When |
|---|---|---|---|
| **Unit tests** | Fast, no I/O | JUnit, Mockito. Maven **surefire**, Gradle `test` | Every commit |
| **Integration tests** | Real database/broker/HTTP | Testcontainers; Maven **failsafe** (`*IT`) in `verify`, Gradle `integrationTest` source set | Every PR (if reasonably fast) or on main |
| **End-to-end / contract / smoke tests** | Deployed system, or consumer-provider contracts | | After deploy to staging |
| **Performance / load tests** | Throughput and latency regressions | JMH ([Benchmarking](../20-performance/01_benchmarking-with-jmh.md)), load tools | Nightly or before release |

Practices:

- **Docker-based integration tests** need a Docker-capable runner. GitHub-hosted Linux runners have it. Self-hosted ones need setup ([Integration Testing](../19-testing/03_integration-testing.md)).
- **Fail the build on test failures**, and publish reports (JUnit XML) so the CI UI shows what failed.
- **Flaky tests destroy trust.** Fix or quarantine them quickly. Don't normalize "just rerun it".
- **Coverage** (JaCoCo) is a signal, not a goal. Enforce a floor if you must, and prefer reviewing coverage of *changed code* ([Code Coverage and Mutation Testing](../19-testing/05_code-coverage-and-mutation-testing.md)).
- **Static analysis and formatting:** Checkstyle, SpotBugs, PMD, Error Prone, Sonar, and a formatter (Spotless) run in CI so style debates and common bug patterns never reach review.
- **Dependency checks:** vulnerability scan (OWASP Dependency-Check, OSV-Scanner, Dependabot alerts) and optionally license checks ([Dependency Management](02_dependency-management.md#6-repositories-and-supply-chain-security)).

---

## 5. Packaging and artifacts

What does the pipeline produce?

| Artifact | Notes |
|---|---|
| **Library JAR** | Published to a Maven repository with sources/Javadoc, and signed for Central |
| **Application JAR** | "Fat/uber" JAR (all dependencies), or a **layered** Spring Boot JAR that caches well in images |
| **Container image** | The usual deployable unit today |
| **Native binary / custom runtime** | GraalVM native image, or `jlink` minimal runtimes for specific needs |

### Container images

A multi-stage Dockerfile keeps build tools out of the final image:

```dockerfile
FROM eclipse-temurin:21-jdk AS build
WORKDIR /src
COPY . .
RUN ./mvnw -B -ntp -DskipTests package

FROM eclipse-temurin:21-jre
RUN useradd --system --create-home app
# don't run as root
USER app
WORKDIR /app
COPY --from=build /src/target/app.jar app.jar
ENTRYPOINT ["java", "-XX:MaxRAMPercentage=75", "-jar", "app.jar"]
```

Alternatives that skip writing a Dockerfile: **Jib** (builds optimized, layered images straight from Maven/Gradle without a Docker daemon) and **Cloud Native Buildpacks** (for example Spring Boot's `build-image`).

Image practices:

- **Tag with an immutable identifier** (the git commit SHA or version), not just `latest`, and deploy by **digest** or exact tag.
- Use **small, maintained base images** and rebuild regularly to pick up OS/JDK security patches.
- **Scan images** (Trivy, Grype, or your registry's scanner) in the pipeline.
- Set **container-aware JVM flags** (`-XX:MaxRAMPercentage`) and account for non-heap memory ([JVM Memory Areas](../15-jvm-internals/02_jvm-memory-areas.md#7-sizing-for-containers), [Packaging and Deployment](../22-production-engineering/02_packaging-and-deployment.md)).
- Use a `.dockerignore` so `target/`, `.git`, and secrets never enter the build context.

Publish artifacts to a **registry/repository** (a Maven repository manager for JARs, a container registry for images) and have deployments pull from there.

---

## 6. Versioning and releases

- Version with **SemVer** for libraries. For services, a **build number or commit SHA** is often more useful than a human-chosen version.
- Avoid hand-editing version numbers in POMs on every release. Options: Maven's **CI-friendly versions** (`<version>${revision}</version>` with `-Drevision=1.4.2` and the flatten plugin), the Maven Release Plugin (stateful, commits and tags, widely used but clunky), or **tag-driven releases** where pushing a git tag `v1.4.2` triggers a pipeline that builds and publishes that version.
- **Release automation tools** (release-please, semantic-release, changelog generators) derive versions and changelogs from conventional commit messages.
- Releases are **tagged in git**, reproducible from the tag, and **immutable**. Fix forward with a new version.
- Make builds **reproducible** (`project.build.outputTimestamp`, pinned plugins and dependencies) so a rebuild yields identical artifacts, which helps verification ([Maven](00_maven.md#9-reproducible-and-clean-builds)).

---

## 7. Secrets and supply chain in the pipeline

CI systems hold powerful credentials (registry, cloud, signing keys), so they're prime attack targets.

- **Never commit secrets**, and keep them out of logs. Use the CI platform's **secret store**, masked in output. Avoid echoing environment variables ([Secrets Management](../21-security/03_secrets-management.md)).
- **Prefer short-lived credentials via OIDC federation** (the CI job proves its identity to the cloud provider and receives temporary credentials) over long-lived access keys stored as secrets.
- **Least privilege:** the default token should be read-only (`permissions: contents: read`), granting extra permissions per job only where needed. Use separate credentials per environment.
- **Be careful with pull requests from forks:** don't expose secrets to workflows triggered by untrusted code. Understand the difference between events such as `pull_request` and `pull_request_target`, and never run untrusted PR code with privileged tokens.
- **Treat third-party actions/plugins as dependencies:** pin them to a **full commit SHA** (tags can be moved), prefer well-maintained ones, and minimize their number. Avoid `curl ... | bash` in pipelines.
- **Protect branches:** required reviews and status checks, no force-pushes to main, code owners for sensitive paths (pipeline definitions, build scripts).
- **Produce provenance:** generate an **SBOM** (CycloneDX/SPDX), **sign** artifacts and images (for example with Sigstore's cosign), and record build **attestations/provenance** (SLSA-style) so consumers can verify what built an artifact. Run **dependency review** on PRs that change dependencies.
- **Separate build from deploy credentials.** The job that runs untrusted-ish test code shouldn't hold production deploy rights.

---

## 8. Deployment strategies

How a new version reaches users matters as much as the build:

| Strategy | How it works | Trade-offs |
|---|---|---|
| **Rolling** | Replace instances gradually | Simple, needs backward-compatible versions running side by side |
| **Blue-green** | Run the new version fully beside the old, then switch traffic | Fast rollback by switching back. Doubles capacity temporarily |
| **Canary** | Send a small percentage of traffic to the new version, watch metrics, then expand | Catches problems with limited blast radius. Needs good observability |
| **Feature flags** | Deploy code dark, enable features separately from deploys | Decouples deploy from release. Adds flag-management discipline |

Essentials for any strategy:

- **Health checks:** readiness (can serve traffic) and liveness probes, so bad instances never receive traffic ([Observability](../22-production-engineering/03_observability.md)).
- **Database migrations must be backward-compatible** with the *previous* app version (expand/contract), because old and new versions run together during a rollout ([JDBC Patterns](../16-jdbc-and-databases/06_jdbc-patterns.md#7-schema-migrations)). Run migrations as a pipeline step with Flyway/Liquibase.
- **Automated smoke tests** after deploy, and **automatic rollback** on failed health or error-rate checks.
- **Observability and alerting** tied to deploys, so you can see what a release changed.
- **GitOps** (Argo CD, Flux) treats the deployed state as declarative config in git, with the cluster reconciling toward it.
- **Approvals** for production where required, but don't use manual gates to compensate for weak automated tests.

Teams commonly measure delivery health with the four **DORA metrics**: deployment frequency, lead time for changes, change failure rate, and time to restore service.

---

## 9. Keeping pipelines healthy

- **Speed:** aim for PR feedback in **minutes** (a common target is under ~10). Slow pipelines get bypassed. Cache dependencies, run independent jobs in parallel, build only affected modules (`-pl ... -am`, Gradle's up-to-date/caching), and split test suites.
- **Reliability:** pin tool and action versions, retry only genuinely transient *infrastructure* steps (not tests), and fix flaky tests.
- **Visibility:** publish test reports and logs, and surface build times and failure rates. A pipeline you can't debug is a liability.
- **Reproducibility:** you should be able to run the same steps locally (`./mvnw verify`, a script, or a container). If the only way to test pipeline changes is to push commits, iteration will be slow.
- **Maintenance:** treat pipeline code and build scripts as production code: review, refactor, delete dead jobs, and keep the dependency updates flowing (Dependabot/Renovate cover GitHub Actions and Docker base images too).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Rebuilding the artifact for each environment | Build once, promote the same artifact/digest |
| Using system Maven/Gradle/JDK on CI | Wrapper + explicit Java version setup |
| Secrets in the repo, in logs, or as long-lived keys | Secret store, OIDC, least privilege |
| Unpinned third-party actions/plugins | Pin by commit SHA, review, minimize |
| Running unit and slow integration tests as one big step | Separate stages (surefire/failsafe, test tasks) |
| Tolerating flaky tests ("just re-run") | Fix or quarantine, and track them |
| `latest` image tags in production | Immutable tags/digests |
| Containers running as root with full JDKs | Non-root user, JRE/slim base, scanned images |
| No dependency or image scanning | Automated SCA and image scans, plus SBOM |
| Backward-incompatible DB migrations deployed with the app | Expand/contract, migrate first, then deploy |
| Manual production steps nobody documented | Automate, or at least script and document |
| No rollback plan | Blue-green/canary/feature flags and tested rollback |
| Pipeline logic only in YAML, impossible to run locally | Move logic into scripts/build tool tasks |
| Giant 40-minute pipelines | Parallelize, cache, split, run affected modules |

### Debugging: "works locally, fails in CI" (or vice versa)

1. **Different Java/tool versions** → print `java -version` and `./mvnw -v` in the log. Use the wrapper and a pinned JDK.
2. **Environment defaults** → time zone, locale, and default charset differ ([Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md), [I/O Streams](../11-io-and-networking/00_io-streams-readers-writers.md#6-charsets-the-usual-source-of-garbled-text)). Set them explicitly in tests and builds.
3. **Case-sensitive file systems** (Linux CI vs macOS/Windows laptops): `Foo.java` vs `foo.java`, resource paths.
4. **Missing network/Docker/services** in CI: integration tests needing them.
5. **Resource limits:** the build is killed (exit code 137 often means out-of-memory). Tune `MAVEN_OPTS` / `org.gradle.jvmargs`, reduce parallelism.
6. **Order-dependent or concurrent tests** that pass sequentially on a laptop but collide in parallel CI.
7. **Stale or poisoned caches:** include dependency file hashes in the cache key. Try a run with the cache disabled.
8. **Uncommitted generated or local files** that your laptop has and the repo doesn't.
9. **Verbose logs:** `./mvnw -X`, `./gradlew --info --stacktrace`, and your CI's step-debug option. Read the *first* error, not the last.
10. Reproduce locally in a clean container (`docker run` with the same image and commands as CI).

---

## Quick Summary

- **CI** = automatic build + test on every change; **CD** = every passing change is releasable (delivery) or released (deployment). The goal is **fast, trustworthy feedback and small, reversible releases**.
- Pipeline shape: checkout → build + unit tests → static analysis + dependency scan → package → publish → deploy staging → integration/smoke → deploy production.
- **Use the wrapper and a pinned JDK**, cache dependencies, keep pipeline logic in scripts, and **build once, promote the same immutable artifact**.
- Separate **unit** (surefire/`test`) from **integration** (failsafe/Testcontainers) stages. Fix flaky tests. Run static analysis and scans in CI.
- Package as JARs and **container images** (multi-stage, non-root, immutable tags, scanned), publish to registries.
- Guard the pipeline: **secret stores, OIDC, least privilege, pinned actions, branch protection, SBOM, signing**.
- Deploy with **rolling/blue-green/canary** plus feature flags, health checks, backward-compatible migrations, and a rollback plan.

**Next module:** [Testing](../19-testing/README.md)