# Git vs GitHub

One of the most common points of confusion for beginners: **Git** and **GitHub** are not the same thing.

## Git

Git is a **version control system** — a program that runs on your computer and tracks changes to files over time. It was created by Linus Torvalds in 2005 to manage the Linux kernel's source code.

Git is:

- Installed locally on your machine
- Free and open source
- A command-line tool (`git init`, `git add`, `git commit`, etc.)
- Fully functional with **no internet connection and no account of any kind**

You can use Git for your entire career and never touch GitHub. Every repository has a complete copy of its own history — there's no central server Git _requires_ you to talk to.

## GitHub

GitHub is a **website that hosts Git repositories** in the cloud, plus a set of collaboration features built around Git:

- Pull requests (proposing and reviewing changes)
- Issues (tracking bugs/tasks)
- A place to store a remote copy of your repository, so others can access it
- Actions (CI/CD automation)
- Project boards, wikis, discussions

GitHub is a company (owned by Microsoft) offering a product. Git is the underlying technology that product is built on.

## The relationship

```
Your computer                    GitHub (remote server)
─────────────                    ──────────────────────
   Git repo    ──── push ────►      Git repo (copy)
   Git repo    ◄─── pull ────       Git repo (copy)
```

You use **Git commands** to interact with a repository hosted **on GitHub**:

```bash
git clone https://github.com/user/repo.git   # copy a GitHub repo locally
git push origin main                          # send local commits to GitHub
git pull origin main                          # fetch commits from GitHub
```

GitHub doesn't add new version-control concepts — it's still branches, commits, and merges under the hood. It adds a place to host that repository and a UI for collaborating around it.

## GitHub isn't the only option

Because Git and GitHub are separate, several other companies host Git repositories with similar features:

| Platform        | Notes                                                                    |
| --------------- | ------------------------------------------------------------------------ |
| **GitHub**      | Most popular, owned by Microsoft                                         |
| **GitLab**      | Open-source option, can also be self-hosted                              |
| **Bitbucket**   | Owned by Atlassian, integrates with Jira                                 |
| **Self-hosted** | Run your own Git server (e.g. `git init --bare` on a server you control) |

Whichever you use, the Git commands you run locally are identical — `clone`, `push`, `pull`, `fetch` all work the same way regardless of where the repository is hosted.

## Quick summary

|                      | Git                      | GitHub                           |
| -------------------- | ------------------------ | -------------------------------- |
| What it is           | Version control software | A hosting/collaboration platform |
| Runs                 | Locally, on your machine | On the web                       |
| Requires an account? | No                       | Yes                              |
| Requires internet?   | No                       | Yes, to push/pull/clone          |
| Alternatives         | (it's the standard)      | GitLab, Bitbucket, self-hosted   |

> **Git** tracks your changes. **GitHub** is one of several places you can put a copy of that tracked history so others can see and collaborate on it.
