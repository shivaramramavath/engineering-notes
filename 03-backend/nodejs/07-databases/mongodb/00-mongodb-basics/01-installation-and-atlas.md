# Installation & MongoDB Atlas

Two ways to get a running MongoDB instance: install it locally, or create a free cluster on MongoDB Atlas, the official managed cloud service. Most real projects — and every Mongoose example later in this guide — just need a valid connection string; where that database actually lives is up to you.

## Option 1: Install locally

```bash
# macOS
brew tap mongodb/brew
brew install mongodb-community
brew services start mongodb-community
```

```bash
# Linux (Debian/Ubuntu)
curl -fsSL https://pgp.mongodb.com/server-7.0.asc | sudo gpg --dearmor -o /usr/share/keyrings/mongodb-server-7.0.gpg
echo "deb [signed-by=/usr/share/keyrings/mongodb-server-7.0.gpg] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/7.0 multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-7.0.list
sudo apt update && sudo apt install -y mongodb-org
sudo systemctl start mongod
```

```bash
# or, via Docker — no local install at all
docker run -d --name mongodb -p 27017:27017 mongo:7
```

Verify it's running:

```bash
mongosh
```

If this connects without error, you have a local instance on the default port, `27017`, and can connect with:

```
mongodb://localhost:27017/myapp
```

---

## Option 2: MongoDB Atlas (managed cloud)

1. Sign up at [mongodb.com/cloud/atlas](https://www.mongodb.com/cloud/atlas)
2. Create a project, then a cluster — choose the **M0 (free tier)**
3. Create a **database user** (username/password) under Database Access
4. Add your IP under Network Access (or `0.0.0.0/0` for "anywhere," fine for learning only)
5. Copy the connection string from the "Connect" button:

```
mongodb+srv://<username>:<password>@cluster0.abcde.mongodb.net/myapp?retryWrites=true&w=majority
```

`mongodb+srv://` automatically discovers every node in the cluster via DNS — this is the format you'll paste directly into Mongoose's `connect()` call in `03-setup/02-connecting-to-mongodb.md`.

---

## Local vs Atlas

|                    | Local                                  | Atlas                            |
| ------------------ | -------------------------------------- | -------------------------------- |
| Setup              | Install/manage yourself                | Sign up, click through a wizard  |
| Offline            | Yes                                    | No                               |
| Matches production | Only if production is also self-hosted | Yes, if production is also Atlas |

Either works fine for everything in this guide — swap the connection string and nothing about the Mongoose code changes.

## Quick summary

- Local install (native or Docker) gives you `mongodb://localhost:27017/...`
- Atlas needs a database user and a network access entry before any connection succeeds, and uses `mongodb+srv://...`
- Whichever you choose, the only thing that changes in later Mongoose examples is the connection string itself

## Next

**`02-shell-and-compass.md`** covers connecting to and browsing whichever instance you just set up, without writing any code yet.
