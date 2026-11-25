# Git & GitHub Curriculum Completion Plan & Execution Blueprint

**Track**: `04-devops/git-github/`  
**Baseline**: Git 2.49+, GitHub Actions Node 20 runner baseline, GitHub Rulesets, OIDC, Artifacts v4  
**Total Target Files**: 124 Concept/Project/Interview Notes + 12 Module READMEs + 1 Track Root README = 137 Markdown Files  

---

## 1. Directory Structure & Rename Map

### Directory Renames (via `git mv`)
- `04-devops/git-github/00_setup/` $\rightarrow$ `04-devops/git-github/00-setup/`
- `04-devops/git-github/01_git_fundamentals/` $\rightarrow$ `04-devops/git-github/01-fundamentals/`
- `04-devops/git-github/02_git_commands/` $\rightarrow$ `04-devops/git-github/02-commands/`
- `04-devops/git-github/03_history_rewriting/` $\rightarrow$ `04-devops/git-github/03-history-rewriting/`
- `04-devops/git-github/04_github_actions/` $\rightarrow$ `04-devops/git-github/04-github-actions/`

### File Rename & Relocation Mapping
| Original Path | Target Path | Action |
|:--------------|:------------|:-------|
| `00_setup/getting-started.md` | `00-setup/01-getting-started.md` | Rename & rewrite to 13-section template |
| `01_git_fundamentals/01_git-vs-github.md` | `01-fundamentals/01-git-vs-github.md` | Rename & rewrite |
| `01_git_fundamentals/02_repositories.md` | `01-fundamentals/02-repositories.md` | Rename & rewrite |
| `01_git_fundamentals/03_working-tree-staging-area.md` | `01-fundamentals/03-working-tree-and-staging-area.md` | Rename & rewrite |
| `01_git_fundamentals/04_commits.md` | `01-fundamentals/04-commits.md` | Rename & rewrite |
| `01_git_fundamentals/05_git-objects.md` | `01-fundamentals/05-git-objects.md` | Rename & rewrite |
| `02_git_commands/01_init-clone.md` | `02-commands/01-init-clone.md` | Rename & rewrite |
| `02_git_commands/02_status-diff-log.md` | `02-commands/02-status-diff-log.md` | Rename & rewrite |
| `02_git_commands/03_add-commit.md` | `02-commands/03-add-commit.md` | Rename & rewrite |
| `02_git_commands/04_undoing-changes.md` | `02-commands/04-undoing-changes.md` | Rename & rewrite |
| `02_git_commands/05_clean-rm-mv.md` | `02-commands/05-clean-rm-mv.md` | Rename & rewrite |
| `02_git_commands/06_branching.md` | `02-commands/06-branching.md` | Rename & rewrite |
| `02_git_commands/07_merging.md` | `02-commands/07-merging.md` | Rename & rewrite |
| `02_git_commands/08_remotes.md` | `02-commands/09-remotes.md` | Rename & rewrite |
| `02_git_commands/09_tracking-upstream.md` | `02-commands/10-tracking-upstream.md` | Rename & rewrite |
| `03_history_rewriting/01_amend.md` | `03-history-rewriting/01-amend.md` | Rename & rewrite |
| `03_history_rewriting/02_rebase.md` | `03-history-rewriting/02-rebase.md` | Rename & rewrite |
| `03_history_rewriting/03_cherry-pick.md` | `03-history-rewriting/03-cherry-pick.md` | Rename & rewrite |
| `03_history_rewriting/04_reflog.md` | `03-history-rewriting/04-reflog-and-recovery.md` | Rename & rewrite |
| `04_github_actions/01_core-concepts.md` through `18_debugging...md` | `04-github-actions/01-core-concepts.md` through `18-...md` | Rename & rewrite with Node 20 & v4 actions |
| `04_github_actions/CHEATSHEET.md` | `11-cheatsheets/04-github-actions-syntax.md` | Move & format as cheatsheet |
| `04_github_actions/examples/nodejs-ci.yml` | Fold into `04-github-actions/14-ci-pipelines.md` | Embedded code block |
| `04_github_actions/examples/docker-build-push.yml` | Fold into `04-github-actions/16-docker-ci-cd.md` | Embedded code block |
| `04_github_actions/examples/deploy-with-approval.yml`| Fold into `04-github-actions/11-permissions-and-environments.md` | Embedded code block |
| `04_github_actions/examples/reusable-workflow-caller.yml`| Fold into `04-github-actions/12-reusable-workflows.md` | Embedded code block |
| `04_github_actions/examples/release-automation.yml` | Fold into `04-github-actions/19-release-automation.md` | Embedded code block |

---

## 2. Complete Numbered Target Curriculum

### Module 00: Setup & Installation (5 Notes + README)
1. `01-getting-started.md`
2. `02-install-and-configuration.md`
3. `03-ssh-keys-and-gpg-or-ssh-signing.md`
4. `04-gitconfig-in-depth.md`
5. `05-gitignore-and-gitattributes.md`

### Module 01: Core Fundamentals (10 Notes + README)
1. `01-git-vs-github.md`
2. `02-repositories.md`
3. `03-working-tree-and-staging-area.md`
4. `04-commits.md`
5. `05-git-objects.md`
6. `06-refs-branches-and-head.md`
7. `07-packfiles-and-garbage-collection.md`
8. `08-the-index-in-depth.md`
9. `09-hashes-and-sha-256.md`
10. `10-git-internals-walkthrough.md`

### Module 02: Essential Commands (21 Notes + README)
1. `01-init-clone.md`
2. `02-status-diff-log.md`
3. `03-add-commit.md`
4. `04-undoing-changes.md`
5. `05-clean-rm-mv.md`
6. `06-branching.md`
7. `07-merging.md`
8. `08-merge-conflicts-and-resolution-tools.md`
9. `09-remotes.md`
10. `10-tracking-upstream.md`
11. `11-stash.md`
12. `12-tags-and-releases.md`
13. `13-diff-and-patch-and-format-patch.md`
14. `14-blame-and-log-search.md`
15. `15-bisect.md`
16. `16-submodules-and-subtrees.md`
17. `17-worktrees.md`
18. `18-sparse-checkout-and-partial-clone.md`
19. `19-git-lfs.md`
20. `20-hooks.md`
21. `21-aliases-and-productivity.md`

### Module 03: History Rewriting & Recovery (8 Notes + README)
1. `01-amend.md`
2. `02-rebase.md`
3. `03-cherry-pick.md`
4. `04-reflog-and-recovery.md`
5. `05-filter-repo-and-removing-secrets.md`
6. `06-rewriting-shared-history-safely.md`
7. `07-reset-vs-revert-vs-restore.md`
8. `08-recovering-lost-work-playbook.md`

### Module 04: GitHub Actions CI/CD (26 Notes + README)
1. `01-core-concepts.md`
2. `02-workflow-syntax.md`
3. `03-events-and-triggers.md`
4. `04-jobs-and-steps.md`
5. `05-actions-and-marketplace.md`
6. `06-runners.md`
7. `07-variables-and-secrets.md`
8. `08-contexts-expressions-conditions.md`
9. `09-artifacts-caching-dependencies.md`
10. `10-matrix-and-concurrency.md`
11. `11-permissions-and-environments.md`
12. `12-reusable-workflows.md`
13. `13-custom-actions.md`
14. `14-ci-pipelines.md`
15. `15-cd-and-deployment.md`
16. `16-docker-ci-cd.md`
17. `17-security-hardening.md`
18. `18-debugging-and-workflow-commands.md`
19. `19-release-automation.md`
20. `20-oidc-and-cloud-auth.md`
21. `21-self-hosted-runners-and-scaling.md`
22. `22-workflow-performance-and-cost.md`
23. `23-testing-workflows-locally.md`
24. `24-monorepo-and-path-filters.md`
25. `25-composite-vs-javascript-vs-docker-actions.md`
26. `26-supply-chain-pinning-and-provenance.md`

### Module 05: GitHub Collaboration & Governance (11 Notes + README)
1. `01-pull-requests-and-code-review.md`
2. `02-issues-labels-and-milestones.md`
3. `03-github-projects-and-automation.md`
4. `04-forks-and-contribution-workflow.md`
5. `05-branch-protection-and-rulesets.md`
6. `06-codeowners.md`
7. `07-templates-and-community-health-files.md`
8. `08-github-pages.md`
9. `09-releases-and-packages.md`
10. `10-discussions-wikis-and-notifications.md`
11. `11-organisations-teams-and-permissions.md`

### Module 06: Workflows, Branching & Release Strategies (8 Notes + README)
1. `01-trunk-based-development.md`
2. `02-gitflow-and-github-flow.md`
3. `03-release-branching-and-hotfixes.md`
4. `04-conventional-commits-and-semantic-versioning.md`
5. `05-changelogs-and-release-automation.md`
6. `06-monorepo-vs-polyrepo-and-git-at-scale.md`
7. `07-commit-message-and-pr-best-practices.md`
8. `08-git-for-teams-playbook.md`

### Module 07: GitHub Enterprise Security & Compliance (8 Notes + README)
1. `01-secret-scanning-and-push-protection.md`
2. `02-dependabot-and-dependency-review.md`
3. `03-code-scanning-codeql.md`
4. `04-signed-commits-and-vigilant-mode.md`
5. `05-security-policies-and-advisories.md`
6. `06-sbom-and-supply-chain-security.md`
7. `07-least-privilege-tokens-and-permissions.md`
8. `08-leaked-secret-response-runbook.md`

### Module 08: GitHub CLI & Developer APIs (4 Notes + README)
1. `01-gh-cli-essentials.md`
2. `02-github-rest-and-graphql-api.md`
3. `03-webhooks-and-apps.md`
4. `04-automating-with-gh-and-actions.md`

### Module 09: Production Reference Projects (8 Projects + README)
1. `01-conflict-resolution-lab.md`
2. `02-rebase-and-recovery-lab.md`
3. `03-ci-pipeline-for-a-node-app.md`
4. `04-docker-build-and-push-to-ghcr.md`
5. `05-release-automation-with-conventional-commits.md`
6. `06-monorepo-ci-with-path-filters.md`
7. `07-reusable-workflow-library.md`
8. `08-secure-deploy-with-oidc-and-approvals.md`

### Module 10: Technical Interview Preparation (8 Guides + README)
1. `01-git-fundamentals-qa.md`
2. `02-branching-merging-rebasing-qa.md`
3. `03-internals-qa.md`
4. `04-troubleshooting-scenarios.md`
5. `05-github-collaboration-qa.md`
6. `06-github-actions-qa.md`
7. `07-security-qa.md`
8. `08-behavioural-and-project-discussion.md`

### Module 11: Quick Reference Cheatsheets (7 Cheatsheets + README)
1. `01-git-commands.md`
2. `02-undo-and-recovery-matrix.md`
3. `03-branching-strategies.md`
4. `04-github-actions-syntax.md` (from CHEATSHEET.md)
5. `05-gh-cli.md`
6. `06-git-config-and-aliases.md`
7. `07-common-errors-and-fixes.md`

---

## 3. Batch Execution Strategy

Execution will proceed sequentially module-by-module in strict dependency order:
- **Batch 1**: Modules 00 (Setup) & 01 (Fundamentals)
- **Batch 2**: Module 02 (Essential Commands)
- **Batch 3**: Module 03 (History Rewriting & Recovery)
- **Batch 4**: Module 04 (GitHub Actions CI/CD)
- **Batch 5**: Modules 05 (Collaboration) & 06 (Workflows & Strategies)
- **Batch 6**: Modules 07 (Security) & 08 (GitHub CLI & APIs)
- **Batch 7**: Module 09 (Production Reference Projects)
- **Batch 8**: Modules 10 (Interview Prep) & 11 (Cheatsheets)
- **Batch 9**: Track Root Integration, DevOps foundation updates, tree.txt, and final report.
