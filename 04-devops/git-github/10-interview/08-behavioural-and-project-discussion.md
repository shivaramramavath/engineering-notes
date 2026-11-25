<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Behavioral & Staff-Level Architectural Scenarios
  Track: Git & GitHub / DevOps
  Difficulty: Staff to Principal
  Estimated Time: 50 minutes
  Key Terminology: DORA Metrics, Trunk-Based Development Migration, Monorepo Architecture, Code Review Culture, Incident Management, Post-Mortems
================================================================================
-->

# Behavioral & Staff-Level Architectural Scenarios: 12 In-Depth Questions & Answers

[<- Back to Interview Overview](./README.md) | [Back to Interview Overview](./README.md)

---

## Overview

At Staff, Principal, and Lead DevOps/Platform engineering levels, technical mastery of Git commands must be paired with leadership, organizational change management, cross-functional persuasion, and architectural vision. This guide provides 12 comprehensive behavioral and architectural case studies frequently encountered in Staff+ leadership interviews.

---

## Scenarios Index

1. [Scenario 01: Leading the Organizational Migration from GitFlow to Trunk-Based Development](#scenario-01-leading-the-organizational-migration-from-gitflow-to-trunk-based-development)
2. [Scenario 02: Handling Severe Engineering Pushback Against Mandatory Signed Commits](#scenario-02-handling-severe-engineering-pushback-against-mandatory-signed-commits)
3. [Scenario 03: Architecting the Monorepo vs Polyrepo Strategy for 25 Microservices](#scenario-03-architecting-the-monorepo-vs-polyrepo-strategy-for-25-microservices)
4. [Scenario 04: Resolving Multi-Team Merge Hell Blocking an Urgent Production Release](#scenario-04-resolving-multi-team-merge-hell-blocking-an-urgent-production-release)
5. [Scenario 05: Designing and Tracking DORA Metrics via GitHub APIs and Webhooks](#scenario-05-designing-and-tracking-dora-metrics-via-github-apis-and-webhooks)
6. [Scenario 06: Cultivating a High-Velocity, Respectful Code Review Culture](#scenario-06-cultivating-a-high-velocity-respectful-code-review-culture)
7. [Scenario 07: Conducting a Blameless Post-Mortem for a Leaked Production Secret](#scenario-07-conducting-a-blameless-post-mortem-for-a-leaked-production-secret)
8. [Scenario 08: Mentoring a Junior Engineer Struggling with Git Mental Models](#scenario-08-mentoring-a-junior-engineer-struggling-with-git-mental-models)
9. [Scenario 09: Resolving Developer Frustration Over Slow CI/CD Pipelines (>45 mins)](#scenario-09-resolving-developer-frustration-over-slow-cicd-pipelines-45-mins)
10. [Scenario 10: Establishing an InnerSource Governance Model Across Siloed Business Units](#scenario-10-establishing-an-innersource-governance-model-across-siloed-business-units)
11. [Scenario 11: Managing Breaking Changes in a Widely Consumed Shared Action/Library](#scenario-11-managing-breaking-changes-in-a-widely-consumed-shared-actionlibrary)
12. [Scenario 12: Convincing Leadership to Invest in GitHub Enterprise & GHAS Tooling](#scenario-12-convincing-leadership-to-invest-in-github-enterprise--ghas-tooling)

---

## Behavioral Case Studies & Architectural Solutions

### Scenario 01: Leading the Organizational Migration from GitFlow to Trunk-Based Development
**Context**: A 120-person engineering organization operates a rigid GitFlow model with `develop`, `release`, `hotfix`, and long-lived feature branches lasting weeks. Releases occur monthly, require 3-day code freezes, and trigger massive merge debt. Leadership wants to adopt Trunk-Based Development (TBD) with continuous deployment.
**Approach (STAR Method)**:
- **Situation**: Monthly releases were causing customer churn, and release merges regularly broke production.
- **Task**: Transition 120 engineers to Trunk-Based Development without halting active roadmap delivery.
- **Action**:
  1. *Foundation First*: Migrated CI pipelines to run in under 5 minutes, adding automated integration test gates and strict status checks on PRs.
  2. *Feature Flags*: Introduced a feature flag management SDK (LaunchDarkly / OpenFeature), training teams on decoupling deployment from release. Code could now merge to `main` in disabled states.
  3. *Pilot Program*: Selected one high-visibility microservice team as a 4-week pilot. Documented metrics: deployment frequency increased from monthly to 4x daily; defect escape rate dropped 60%.
  4. *Phased Rollout*: Established a 24-hour branch lifetime rule enforced via Slack bots and PR size limits (<400 lines). Removed the `develop` branch.
- **Result**: Lead time for changes dropped from 28 days to 4 hours. DORA deployment frequency improved to multiple deployments per day, eliminating release freezes entirely.

---

### Scenario 02: Handling Severe Engineering Pushback Against Mandatory Signed Commits
**Context**: To satisfy SOC 2 and ISO 27001 audit controls, security mandates that all commits pushed to company repositories must be cryptographically signed. Engineers push back vehemently, citing broken terminal tools, GPG passphrases interrupting flow, and lost productivity.
**Resolution Strategy**:
1. **Empathize & Diagnose**: Acknowledge that traditional GPG setups are notoriously painful.
2. **Eliminate Friction via SSH Signing**: Instead of forcing GPG, introduce modern **SSH Key Signing** (Git 2.34+). Engineers can sign using their existing SSH authentication keys with zero new software or passphrases.
3. **Biometric Integration**: Provide 1-click documentation for biometric signing via 1Password or macOS Touch ID / Windows Hello.
4. **Evaluate Mode**: Enable GitHub Rulesets in "Evaluate Mode" for 3 weeks, giving teams automated warnings without blocking merges while platform teams assist lagging engineers.
5. **Outcome**: Achieved 100% compliance in 3 weeks with zero developer workflow disruption.

---

### Scenario 03: Architecting the Monorepo vs Polyrepo Strategy for 25 Microservices
**Context**: The engineering organization is splintered across 25 polyrepo microservices. Cross-service API changes require coordinating 5 separate PRs across repos, leading to broken contract deployments and integration testing friction. Several teams advocate merging everything into a single monorepo.
**Architectural Decision**:
- **Evaluation Criteria**: Analyze team autonomy, build pipeline complexity, shared library reuse, and deployment cadence.
- **Hybrid Recommendation**:
  - Rather than a massive "kitchen-sink" monorepo that strains Git performance and requires complex tooling, group services into **Domain-Oriented Monorepos** (e.g., `commerce-monorepo` housing checkout, billing, and cart services; `data-platform-monorepo` housing ingest and ETL).
  - Deploy **Selective CI** using `dorny/paths-filter` and Turborepo/Nx caching to prevent rebuilding unaffected services.
  - Implement **Contract Testing (Pact)** and standardized OpenAPI schemas versioned via npm packages on GitHub Packages.

---

### Scenario 04: Resolving Multi-Team Merge Hell Blocking an Urgent Production Release
**Context**: Two distributed teams simultaneously made extensive architectural refactors to a core shared authentication service. When attempting to cut the release candidate, Git reported 45 conflicting files across core routes and database models. Both teams blame each other and refuse to yield changes under intense executive pressure.
**Leadership Action**:
1. **Declare Incident Command**: Step in as unbiased technical arbiter. Halt further commits to the release branch.
2. **Pair Key Architects**: Convene a synchronous "War Room" pairing the lead architects from both teams.
3. **Deconstruct the Diff**: Break down the 45 conflicting files into 3 buckets:
   - *Trivial Syntactic Conflicts* (imports, formatting): Resolved immediately using automated linting.
   - *Independent Feature Logic*: Merged cleanly using `diff3` inspection.
   - *Architectural Collisions* (incompatible database schema and token models): Decide which model takes precedence based on long-term strategy, and refactor the second feature to use adapter interfaces.
4. **Automated Verification**: Run full end-to-end integration test suites in an isolated sandbox.
5. **Post-Incident Action**: Mandate feature flagging and short-lived branches (<24h) to ensure divergent work never accumulates for weeks again.

---

### Scenario 05: Designing and Tracking DORA Metrics via GitHub APIs and Webhooks
**Context**: Executive leadership wants data-driven visibility into engineering velocity and stability using the four DORA metrics (Deployment Frequency, Lead Time for Changes, Change Failure Rate, Time to Restore Service).
**Technical Architecture**:
1. **Event Capture**: Configure GitHub Organization Webhooks streaming `pull_request`, `push`, `workflow_job`, and `deployment_status` events to an AWS EventBridge / Lambda ingestion pipeline.
2. **Metric Calculation**:
   - *Deployment Frequency*: Count daily successful `deployment_status` events targeting `production`.
   - *Lead Time for Changes*: Time delta from first commit timestamp on a PR branch to the production deployment timestamp.
   - *Change Failure Rate*: Ratio of production hotfix tags or rollbacks relative to total production deployments.
   - *Mean Time to Restore (MTTR)*: Delta from P1 incident alert creation to the subsequent successful rollback or hotfix deployment.
3. **Dashboarding**: Stream metrics into BigQuery and display live operational dashboards in Grafana.

---

### Scenario 06: Cultivating a High-Velocity, Respectful Code Review Culture
**Context**: PR turnaround time averages 4 days. PRs sit un-reviewed, blocking sprints. When reviews do occur, discussions devolve into pedantic debates over styling and naming, generating inter-team friction.
**Interventions**:
1. **Automate the Pedantic**: Move 100% of formatting, linting, and style enforcement into automated CI pre-commit and PR checks (Prettier, ESLint, Checkstyle). If a linter doesn't catch it, it's not worth blocking a PR for.
2. **PR Size Limit**: Enforce a guideline of `<400 lines of diff`. Smaller PRs receive 3x faster reviews and uncover deeper architectural bugs.
3. **Review SLA**: Establish a team working agreement: "Review PRs within 4 working hours of assignment".
4. **Conventional Comments**: Standardize review comments with semantic labels: `praise:`, `suggestion:`, `question:`, `nitpick:`, `blocker:`. Reviewers must clarify if a comment is blocking or optional.

---

### Scenario 07: Conducting a Blameless Post-Mortem for a Leaked Production Secret
**Context**: A developer under tight deadline pressure accidentally pushed an `.env` file containing production Stripe and database credentials to a public GitHub repository. The secret was scraped by automated bots, triggering unauthorized infrastructure usage.
**Post-Mortem Protocol**:
1. **Establish Psychological Safety**: Begin the post-mortem by reiterating the core tenet: *Assume all engineers are skilled and well-intentioned. Failures are systemic, not personal.*
2. **Timeline Reconstruction**: Document exact timestamps: Commit created -> Push received -> Partner alert fired -> Key revoked -> History scrubbed.
3. **Identify Systemic Gaps**:
   - Why was `.env` not in `.gitignore`?
   - Why was GitHub Push Protection not enabled globally?
   - Why did developers need production secrets on local machines instead of synthetic test fixtures?
4. **Actionable Remediation**:
   - Enable GitHub Push Protection across all organization repositories immediately.
   - Implement local pre-commit hooks (`gitleaks`).
   - Transition production authentication to AWS Secrets Manager with dynamic IAM role assumption.

---

### Scenario 08: Mentoring a Junior Engineer Struggling with Git Mental Models
**Context**: A junior engineer frequently gets confused between local branches, remote tracking branches, merge commits, and unstaged changes, repeatedly asking senior engineers to untangle their local working tree.
**Mentoring Approach**:
1. **Replace Command Memorization with Visual Mental Models**: Stop telling them to copy-paste commands. Teach Git's three physical zones: **Working Tree**, **Index (Staging)**, and **Commit History (`HEAD`)**.
2. **Diagram the DAG**: Draw commits as immutable nodes on a whiteboard. Show that a branch is literally just a sticky note (pointer) referencing a commit hash.
3. **Demystify Disasters**: Demonstrate `git reflog`. Show them that making a mistake in Git almost never deletes data, removing their fear of breaking things.
4. **Alias Tooling**: Configure useful visual aliases: `git log --graph --oneline --decorate`.

---

### Scenario 09: Resolving Developer Frustration Over Slow CI/CD Pipelines (>45 mins)
**Context**: A growing test suite and Docker build pipeline ballooned CI execution times to 48 minutes per PR. Developers context-switch frequently, and PR merge queues stall.
**Optimization Playbook**:
1. **Audit Job Durations**: Inspect GitHub Actions job timeline breakdown to pinpoint bottlenecks.
2. **Parallelize Test Suites**: Split monolithic test suites across a 4-node matrix runner with test sharding (`jest --shard=1/4`).
3. **Layer Caching**: Migrate Docker builds to Buildx with GitHub Actions cache backend (`cache-to: type=gha,mode=max`).
4. **Dependency Caching**: Cache `node_modules` and Maven `.m2` caches using `actions/setup-*` cache parameters.
5. **Selective Execution**: Implement `dorny/paths-filter` so documentation or frontend edits skip heavy backend integration tests.
6. **Result**: Reduced CI turnaround time from 48 minutes to 6 minutes, increasing deployment throughput 8x.

---

### Scenario 10: Establishing an InnerSource Governance Model Across Siloed Business Units
**Context**: Multiple independent business units within a global enterprise keep writing duplicate libraries (logging, auth, payment adapters) because internal repositories are private and access-siloed.
**InnerSource Framework**:
1. **Default Visibility**: Change default repository visibility from Private to **Internal** across the GitHub Enterprise Cloud organization. Any employee can read code across business units.
2. **Standardized Contributing Guidelines**: Mandate clear `CONTRIBUTING.md`, `README.md`, and `CODEOWNERS` in all core repositories.
3. **Triangular Contribution Model**: Enable internal teams to fork projects within the organization, author PRs, and submit improvements without requiring direct write access.
4. **InnerSource Champions**: Establish a rotating platform engineering review board to review community PRs within 48-hour SLAs.

---

### Scenario 11: Managing Breaking Changes in a Widely Consumed Shared Action/Library
**Context**: You maintain a reusable GitHub Actions workflow library (`shared-workflows`) consumed by 80 microservice repositories. You need to introduce a major breaking upgrade (e.g., dropping Node 16 support and requiring new OIDC parameters) without breaking downstream pipelines.
**Execution Strategy**:
1. **Semantic Version Pinning**: Adhere strictly to SemVer. Publish the breaking change under a new major version tag (`@v2` / `v2.0.0`).
2. **Maintain Backward Compatibility**: Do not overwrite the moving `@v1` tag. Continue backporting critical security fixes to `@v1` for a designated deprecation window (e.g., 90 days).
3. **Automated Migration PRs**: Author a script or GitHub App that opens automated pull requests across all 80 downstream repositories, upgrading the workflow invocation from `@v1` to `@v2` with required parameter adaptations.
4. **Deprecation Warnings**: Emit non-blocking GitHub Actions workflow warnings (`echo "::warning::Workflow v1 is deprecated..."`) in `@v1` runs.

---

### Scenario 12: Convincing Leadership to Invest in GitHub Enterprise & GHAS Tooling
**Context**: Executive leadership views developer tooling as a pure cost center and resists purchasing GitHub Advanced Security (GHAS) and Enterprise Cloud licenses, suggesting free open-source scripts are sufficient.
**Business Case Formulation**:
1. **Frame in Terms of Risk & Financial Exposure**: Reference recent industry supply chain attacks. The average cost of a data breach is \$4.45M; the cost of a leaked root credential involves millions in fines, regulatory disclosures, and customer churn.
2. **Developer Velocity & Context Switching**: Demonstrate that disparate third-party scanners force developers to switch across 4 external dashboards. GHAS delivers security alerts natively inside developer pull requests, saving 2+ hours per developer per week.
3. **Push Protection ROI**: Show that catching a secret *before* it is pushed costs \$0. Scrubbing a leaked secret post-push requires emergency incident response, key rotation, and forensic auditing averaging 40 engineering hours per incident.
4. **Pilot Demonstration**: Run a 30-day GHAS trial on 5 critical services. Present concrete findings: 12 open CVEs remediated and 4 risky custom tokens caught.

---

[<- Back to Interview Overview](./README.md) | [Back to Interview Overview](./README.md)
