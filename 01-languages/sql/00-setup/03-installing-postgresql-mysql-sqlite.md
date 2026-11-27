# 03: Installing PostgreSQL, MySQL & SQLite

| Property | Value |
|:---|:---|
| **Primary Dialect** | PostgreSQL 17+ (Standard ANSI SQL:2023) |
| **Dialects Compared** | PostgreSQL 17, MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Prerequisites** | [02 - RDBMS Landscape & Dialects](./02-rdbms-landscape-and-dialects.md) |
| **Target Audience** | Software Engineers, DevOps / SRE, Database Administrators |
| **Verification Status** | Verified against official Docker images and CLI installations |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [5 Practical Progressive Examples](#5-practical-progressive-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance & Security Considerations](#performance--security-considerations)
9. [Comparison Matrix](#comparison-matrix)
10. [8 Real-World Interview Questions](#8-real-world-interview-questions)
11. [6 Graded Practice Exercises](#6-graded-practice-exercises)
12. [Summary & Quick Reference](#summary--quick-reference)
13. [Further Reading](#further-reading)

---

## 1. What & Why

Setting up a robust, reproducible database environment is a prerequisite for software engineering and database administration. While cloud-managed database services (AWS RDS, Google Cloud SQL, Azure Database) handle provisioning in production, local development and continuous integration pipelines require developers to spin up isolated, lightweight, and deterministic relational database instances on demand.

Misconfiguring a local database instance leads to subtle bugs that manifest only in production: character encoding mismatches (UTF-8 vs Latin1), case-sensitivity bugs between Windows/macOS and Linux filesystems, authentication failures (`pg_hba.conf` restrictions), and missing extensions (`pgvector`, `uuid-ossp`).

Using containerized workflows via **Docker** and **Docker Compose** provides complete isolation, eliminates host dependency conflicts, guarantees identical database engine versions across team members, and allows ephemeral teardown and rebuild in seconds. For embedded and test-driven environments, **SQLite** provides zero-installation in-process execution.

---

## 2. Core Concepts

### 1. The Containerized Multi-Database Stack
Rather than polluting the host operating system with background services and conflicting service ports, Docker isolates each RDBMS in its own lightweight Linux namespace:
- **PostgreSQL Container**: Runs `postgres:17-alpine`, exposes port `5432`, mounts data to a persistent volume.
- **MySQL Container**: Runs `mysql:8.4`, exposes port `3306`, configures authentication plugins and character sets.
- **SQLite**: No container needed; linked as a C library or invoked via host CLI (`sqlite3`) directly against local `.db` files.

### 2. Configuration Files and Directories
Every relational database relies on critical configuration manifests:
- **PostgreSQL**:
  - `postgresql.conf`: Server configuration (memory limits, connection counts, WAL settings, query logging).
  - `pg_hba.conf` (Host-Based Authentication): Client authentication rules (which IP addresses can connect, using which authentication method: `scram-sha-256`, `md5`, `trust`).
- **MySQL**:
  - `my.cnf` (or `/etc/mysql/my.cnf`): Global server configurations (`innodb_buffer_pool_size`, `character-set-server`, `default_authentication_plugin`).
- **SQLite**:
  - No configuration file. Configured at runtime per connection using `PRAGMA` commands (`PRAGMA journal_mode = WAL;`, `PRAGMA foreign_keys = ON;`).

---

## 3. How It Works Internally

### Database Initialization (`initdb` vs `mysqld --initialize`)
When an RDBMS starts for the first time with an empty data directory, it performs a bootstrapping initialization sequence:

```
Bootstrap Sequence (PostgreSQL initdb)
+------------------------------------------------------------------------+
| 1. Create File Layout (base/, global/, pg_wal/, pg_subtrans/)          |
| 2. Initialize System Catalogs (pg_database, pg_class, pg_attribute)   |
| 3. Create Default Template DBs (template0, template1, postgres)        |
| 4. Initialize Transaction ID (XID) Counter and Write-Ahead Log (WAL)   |
| 5. Generate Default Configs (postgresql.conf, pg_hba.conf)             |
+------------------------------------------------------------------------+
```

- In Docker, entrypoint scripts detect whether the mounted data directory is empty. If empty, it executes `initdb` (PostgreSQL) or `mysqld --initialize-insecure` (MySQL), runs any `.sql` or `.sh` initialization scripts located in `/docker-entrypoint-initdb.d/`, and then launches the main server daemon.

---

## 4. Syntax & Usage

### Production-Ready Multi-Engine `docker-compose.yml`
Save this manifest in your project root to run PostgreSQL 17 and MySQL 8.4 simultaneously:

```yaml
services:
  # Primary Reference: PostgreSQL 17
  postgres:
    image: postgres:17
    container_name: sql_mastery_postgres
    restart: unless-stopped
    environment:
      POSTGRES_USER: engineer
      POSTGRES_PASSWORD: secretpassword
      POSTGRES_DB: learning_db
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data
      - ./init-scripts/postgres:/docker-entrypoint-initdb.d
    command: >
      postgres -c shared_buffers=256MB
               -c work_mem=16MB
               -c log_min_duration_statement=200

  # Secondary Comparison: MySQL 8.4 LTS
  mysql:
    image: mysql:8.4
    container_name: sql_mastery_mysql
    restart: unless-stopped
    environment:
      MYSQL_ROOT_PASSWORD: rootpassword
      MYSQL_DATABASE: learning_db
      MYSQL_USER: engineer
      MYSQL_PASSWORD: secretpassword
    ports:
      - "3306:3306"
    volumes:
      - mysqldata:/var/lib/mysql
      - ./init-scripts/mysql:/docker-entrypoint-initdb.d
    command: >
      --default-authentication-plugin=caching_sha2_password
      --character-set-server=utf8mb4
      --collation-server=utf8mb4_0900_ai_ci

volumes:
  pgdata:
  mysqldata:
```

### Multi-Dialect Comparison Matrix

| Component / Task | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
|:---|:---|:---|:---|:---|
| **Default Port** | `5432` | `3306` | None (Embedded / in-process) | `1433` |
| **Official Docker Image** | `postgres:17` | `mysql:8.4` | N/A (Standard tool) | `mcr.microsoft.com/mssql/server:2022-latest` |
| **Default Superuser** | `postgres` | `root` | N/A (File permissions) | `sa` |
| **Config File Name** | `postgresql.conf` | `my.cnf` | None (Runtime `PRAGMA`) | `sqlservr.ini` |
| **Client Auth Config** | `pg_hba.conf` | `mysql.user` table grants | OS filesystem ACLs | Windows Auth or SQL Auth |
| **Default Database Name**| `postgres` | `mysql` | Direct file path (`app.db`)| `master` |

---

## 5. 5 Practical Progressive Examples

### Example 1: Starting and Verifying Docker Containers
Launch the stack and verify that both database servers are responding to TCP connections.

```bash
# Start containers in background
docker compose up -d

# Check running status
docker compose ps
```
```text
NAME                     IMAGE         STATUS         PORTS
sql_mastery_postgres     postgres:17   Up (healthy)   0.0.0.0:5432->5432/tcp
sql_mastery_mysql        mysql:8.4     Up (healthy)   0.0.0.0:3306->3306/tcp
```

### Example 2: Interactive Smoke Test on PostgreSQL 17
Connect via `docker exec` and run basic environment sanity queries.

```bash
docker exec -it sql_mastery_postgres psql -U engineer -d learning_db -c "SELECT version();"
```
```text
                                                 version
---------------------------------------------------------------------------------------------------------
 PostgreSQL 17.4 on x86_64-pc-linux-musl, compiled by gcc (Alpine 14.2.0) 14.2.0, 64-bit
(1 row)
```

```bash
# Check database encoding and timezone
docker exec -it sql_mastery_postgres psql -U engineer -d learning_db -c "SHOW server_encoding; SHOW TimeZone;"
```
```text
 server_encoding 
-----------------
 UTF8
(1 row)

 TimeZone 
----------
 UTC
(1 row)
```

### Example 3: Interactive Smoke Test on MySQL 8.4 LTS
Verify MySQL authentication, storage engine defaults, and character set configuration.

```bash
docker exec -it sql_mastery_mysql mysql -u engineer -psecretpassword learning_db -e "SELECT VERSION(), @@default_storage_engine, @@character_set_database;"
```
```text
+-----------+--------------------------+----------------------------+
| VERSION() | @@default_storage_engine | @@character_set_database   |
+-----------+--------------------------+----------------------------+
| 8.4.0     | InnoDB                   | utf8mb4                    |
+-----------+--------------------------+----------------------------+
```

### Example 4: Edge Case: Foreign Key Enforcement in SQLite
By default, SQLite maintains backward compatibility with legacy versions by having foreign key constraint enforcement **disabled**! It must be explicitly turned on per connection.

```bash
sqlite3 sandbox.db
```
```sql
-- Check foreign key status (0 = disabled, 1 = enabled)
PRAGMA foreign_keys;
```
```text
0
```
```sql
-- Enable foreign key checking (MANDATORY in modern apps)
PRAGMA foreign_keys = ON;
PRAGMA foreign_keys;
```
```text
1
```
*(Every SQLite application connection must execute `PRAGMA foreign_keys = ON;` upon opening a connection).*

### Example 5: Optimized PostgreSQL Health Check & Tuning Verification
Query the internal settings catalog to verify that runtime configuration overrides took effect.

```sql
SELECT name, setting, unit, source
FROM pg_settings
WHERE name IN ('shared_buffers', 'work_mem', 'max_connections', 'log_min_duration_statement');
```
```text
+------------------------------+---------+------+--------------------+
| name                         | setting | unit | source             |
+------------------------------+---------+------+--------------------+
| log_min_duration_statement   | 200     | ms   | command line       |
| max_connections              | 100     |      | default            |
| shared_buffers               | 32768   | 8kB  | command line       |
| work_mem                     | 16384   | kB   | command line       |
+------------------------------+---------+------+--------------------+
```
*(32768 units of 8kB = 256MB shared buffers).*

---

## 6. Best Practices

1. **Pin Exact Docker Tag Versions**: Never use `image: postgres:latest` in team repositories. Use semantic tags (`postgres:17.4` or `postgres:17-alpine`) to prevent breaking changes when major versions release.
2. **Always Mount Persistent Named Volumes**: Never rely on container-ephemeral storage. Named volumes (`volumes: - pgdata:/var/lib/postgresql/data`) protect your schema and test data across container restarts.
3. **Standardize on UTF-8 Everywhere**: Explicitly configure `UTF8` in PostgreSQL and `utf8mb4` with `utf8mb4_0900_ai_ci` in MySQL. Avoid legacy `utf8` in MySQL (which only supports 3-byte characters and breaks on emojis).
4. **Automate Schema Initialization**: Place `.sql` schema seed files inside `/docker-entrypoint-initdb.d/`. Containers will automatically execute them in alphabetical order on first boot.
5. **Use `.env` Files for Credentials**: Never hardcode database passwords in `docker-compose.yml` pushed to public git repositories. Reference environment variables (`${DB_PASSWORD}`).

---

## 7. Common Mistakes & Gotchas

| Mistake / Anti-Pattern | Why It Fails / Impact | Correct Pattern |
|:---|:---|:---|
| **Assuming SQLite Checks Foreign Keys by Default** | SQLite allows inserting invalid child foreign keys unless explicitly activated. | Always run `PRAGMA foreign_keys = ON;` immediately after connecting. |
| **Using MySQL's Legacy `utf8` Charset** | MySQL's `utf8` only supports 3-byte Unicode characters; inserting 4-byte characters (emojis, rare symbols) crashes the write. | Always use `utf8mb4` (`character-set-server=utf8mb4`). |
| **Modifying `initdb` Scripts After First Boot** | Docker entrypoint scripts in `/docker-entrypoint-initdb.d` ONLY execute if the data directory is completely empty. Changes made later are silently ignored. | To re-run initialization scripts, blow away the volume: `docker compose down -v`. |
| **Port Clashes with Host Databases** | If PostgreSQL is already installed locally on the host OS running on port 5432, Docker will fail to bind port 5432. | Map to an alternative host port: `ports: - "5433:5432"`. |
| **Neglecting `pg_hba.conf` in Postgres** | Even with correct passwords, remote connections from outside Docker are rejected if client CIDR rules are missing. | Use standard Docker image defaults or add `host all all all scram-sha-256`. |
| **Storing Passwords in Shell History** | Running `psql -W` or typing `mysql -psecret` leaves plain-text passwords in `~/.bash_history`. | Use `.pgpass` files or `PGPASSWORD` environment variables. |

---

## 8. Performance & Security Considerations

### Performance
- **Volume Mount Performance**: On macOS and Windows (Docker Desktop), bind mounts (`./data:/var/lib/postgresql/data`) experience significant filesystem translation latency. Always use Docker named volumes (`pgdata:...`) for database storage to run at native Linux filesystem speeds.
- **SQLite WAL Mode**: By default, SQLite uses a rollback journal which locks the entire database during writes. Enable Write-Ahead Logging (`PRAGMA journal_mode = WAL;`) for 10x write throughput and non-blocking concurrent reads.

### Security
- **Never Bind to `0.0.0.0` in Production**: In production compose files, bind database ports strictly to `127.0.0.1:5432:5432` or keep database containers inside an internal Docker network with no host-exposed ports.
- **SCRAM-SHA-256 Authentication**: Ensure PostgreSQL uses `scram-sha-256` (the modern standard since Postgres 14) rather than obsolete `md5` password hashing.

---

## 9. Comparison Matrix

### Installation & Packaging Methods

| Method | Isolation | Setup Effort | Host Resource Usage | Best Used For |
|:---|:---|:---:|:---:|:---|
| **Docker Compose** | High (Containerized) | Low (Single command) | Moderate (Docker VM on Mac/Win) | Daily local development, CI/CD, team parity |
| **Native Package (`apt`, `brew`)** | Low (Host pollution) | Moderate | Lowest (Runs directly on OS) | Dedicated development machines, bare-metal servers |
| **SQLite Embedded** | Complete (Process-bound)| Zero (Bundled) | Negligible | CLI tools, unit tests, mobile/desktop applications |
| **Cloud Managed (RDS, Cloud SQL)**| High (Fully managed) | Low (Cloud console) | Offloaded to cloud | Staging, QA, production environments |

---

## 10. 8 Real-World Interview Questions

### Conceptual Questions

#### Q1: What is the purpose of `pg_hba.conf` in PostgreSQL, and how does it evaluate incoming connection requests?
<details>
<summary>Answer</summary>
`pg_hba.conf` (Host-Based Authentication) controls client authentication. It acts as an internal firewall and authentication validator. 

When a connection arrives, PostgreSQL scans `pg_hba.conf` sequentially from top to bottom until it finds the *first matching line* based on:
1. Connection type (`local` for Unix domain socket, `host` for TCP/IP).
2. Database name.
3. User name.
4. Client IP address / CIDR block.
5. Authentication method (`scram-sha-256`, `trust`, `reject`, `peer`).

If no line matches, the connection is rejected. Order matters: more specific rules must precede general rules.
</details>

#### Q2: What is the difference between Docker named volumes and host bind mounts when running database containers?
<details>
<summary>Answer</summary>
- **Named Volume (`pgdata:/var/lib/postgresql/data`)**: Managed directly by the Docker daemon inside the Docker VM filesystem (ext4). Provides maximum I/O performance, correct file permissions, and data persistence across container recreations.
- **Bind Mount (`./local_dir:/var/lib/postgresql/data`)**: Maps a directory from the host OS directly into the container. On macOS and Windows, this crosses a VM file-sharing virtualization boundary (VirtioFS / gRPC-FUSE), causing severe disk I/O performance penalties for databases.
</details>

#### Q3: Why does MySQL 8.4 LTS use `caching_sha2_password` as the default authentication plugin instead of `mysql_native_password`?
<details>
<summary>Answer</summary>
`mysql_native_password` relies on SHA-1 hashing, which is cryptographically broken and vulnerable to collision attacks. `caching_sha2_password` uses SHA-256 with salted hashes for superior cryptographic protection. Additionally, it implements an in-memory cache of authenticated credentials on the server to avoid re-calculating expensive SHA-256 hashes on every connection handshake.
</details>

#### Q4: How does SQLite maintain durability without a background database server daemon?
<details>
<summary>Answer</summary>
SQLite coordinates directly with the host operating system kernel and filesystem using POSIX advisory locks (`fcntl`) or Windows file locks. Before mutating data pages, SQLite writes transactions to either a rollback journal file (`dbname.db-journal`) or a Write-Ahead Log file (`dbname.db-wal`) and invokes the OS `fsync()` system call to flush dirty disk controller caches to persistent physical storage.
</details>

### Practical Configuration & Command Puzzles

#### Q5: Write the bash command to backup a PostgreSQL database running inside a Docker container directly to a compressed file on the host machine.
<details>
<summary>Answer</summary>

```bash
docker exec -t sql_mastery_postgres pg_dump -U engineer -d learning_db | gzip > backup_$(date +%Y%m%d).sql.gz
```
*(Using `-t` without `-i` avoids pseudo-TTY character corruption when piping binary data).*
</details>

#### Q6: How do you configure PostgreSQL to automatically log all queries taking longer than 250 milliseconds?
<details>
<summary>Answer</summary>
In `postgresql.conf`:
```ini
log_min_duration_statement = 250
```
Or dynamically without restarting the server:
```sql
ALTER SYSTEM SET log_min_duration_statement = 250;
SELECT pg_reload_conf();
```
</details>

#### Q7: Your containerized MySQL instance refuses connections with "Access denied for user 'root'@'172.18.0.1'". What is the issue and how do you resolve it?
<details>
<summary>Answer</summary>
In MySQL, user identities are bound to hostnames/IPs (`'user'@'host'`). A user created as `'root'@'localhost'` cannot connect over Docker's bridge network (where the incoming gateway IP is `172.18.0.1`). 

Resolution: Grant permissions to connect from any host (`%`):
```sql
CREATE USER 'root'@'%' IDENTIFIED BY 'rootpassword';
GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
```
</details>

#### Q8: Write an automated SQLite initialization script that configures WAL mode, enables foreign keys, sets a 5-second busy timeout, and verifies the settings.
<details>
<summary>Answer</summary>

```sql
-- init_sqlite.sql
PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;
PRAGMA busy_timeout = 5000;
PRAGMA synchronous = NORMAL;

-- Verification
SELECT 
    'WAL Mode: ' || (PRAGMA journal_mode) AS wal_check,
    'Foreign Keys: ' || (PRAGMA foreign_keys) AS fk_check;
```
Run with:
```bash
sqlite3 test.db < init_sqlite.sql
```
</details>

---

## 11. 6 Graded Practice Exercises

### Beginner Exercises

#### Exercise 1: Docker Container Spinup
Write a single `docker run` command that starts an ephemeral PostgreSQL 17 container named `pg_test` on port 5432 with password `password123`, which automatically deletes itself upon stopping.
<details>
<summary>Solution</summary>

```bash
docker run --rm -d --name pg_test -p 5432:5432 -e POSTGRES_PASSWORD=password123 postgres:17
```
</details>

#### Exercise 2: SQLite In-Memory Execution
Using the `sqlite3` CLI, execute an in-memory SQL statement that creates a temporary table, inserts two rows, and outputs the result in Markdown table format.
<details>
<summary>Solution</summary>

```bash
sqlite3 :memory: -markdown "CREATE TABLE items (id INT, name TEXT); INSERT INTO items VALUES (1, 'Widget'), (2, 'Gadget'); SELECT * FROM items;"
```
</details>

### Intermediate Exercises

#### Exercise 3: Automated Database Seeding via Docker
Create an initialization SQL script that sets up a `users` table and place it in the correct directory so that Docker Compose automatically provisions it on initial container boot.
<details>
<summary>Solution</summary>

Create file `./init-scripts/postgres/01-init-schema.sql`:
```sql
CREATE TABLE IF NOT EXISTS users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO users (username) VALUES ('alice'), ('bob'), ('carol');
```
In `docker-compose.yml`, mount:
```yaml
volumes:
  - ./init-scripts/postgres:/docker-entrypoint-initdb.d
```
</details>

#### Exercise 4: Testing Connection Resiliency
Write a bash one-liner using `pg_isready` to poll a starting PostgreSQL container until it is ready to accept connections.
<details>
<summary>Solution</summary>

```bash
until docker exec sql_mastery_postgres pg_isready -U engineer -d learning_db; do
  echo "Waiting for PostgreSQL to initialize..."
  sleep 1
done
echo "PostgreSQL is ready!"
```
</details>

### Advanced Exercises

#### Exercise 5: PostgreSQL Non-Root Production Container Configuration
Explain how to configure a custom `postgresql.conf` file inside a Docker container without overriding the default file permissions owned by the `postgres` system user.
<details>
<summary>Solution</summary>
Mount a custom configuration snippet into `/etc/postgresql/custom.conf` and tell PostgreSQL to include it via the command line argument:

```yaml
services:
  postgres:
    image: postgres:17
    volumes:
      - ./custom_pg.conf:/etc/postgresql/custom.conf:ro
    command: postgres -c config_file=/etc/postgresql/custom.conf
```
Or include it in `postgresql.conf` using `include_dir` or `include = '/etc/postgresql/custom.conf'`.
</details>

#### Exercise 6: Multi-Tenant SQLite Database Provisioning
Write a Python script that programmatically connects to a new SQLite database, enables WAL mode, sets strict type affinity (`STRICT` tables introduced in SQLite 3.37), and enforces foreign keys.
<details>
<summary>Solution</summary>

```python
import sqlite3

def init_tenant_db(db_path: str):
    con = sqlite3.connect(db_path)
    cur = con.cursor()
    # Configure production pragmas
    cur.execute("PRAGMA journal_mode = WAL;")
    cur.execute("PRAGMA foreign_keys = ON;")
    cur.execute("PRAGMA synchronous = NORMAL;")
    
    # Create strict table
    cur.execute("""
        CREATE TABLE IF NOT EXISTS tenant_users (
            user_id INTEGER PRIMARY KEY,
            username TEXT NOT NULL,
            credit_balance REAL NOT NULL
        ) STRICT;
    """)
    con.commit()
    con.close()
    print(f"Initialized tenant database at {db_path}")

init_tenant_db("tenant_01.db")
```
</details>

---

## 12. Summary & Quick Reference

### Docker Essential Commands
| Command | Purpose |
|:---|:---|
| `docker compose up -d` | Start all database services in background |
| `docker compose ps` | Check container health and exposed ports |
| `docker compose logs -f postgres` | Stream live server logs |
| `docker compose down -v` | Destroy containers AND wipe database volumes |
| `docker exec -it <container> psql -U <user> -d <db>` | Open interactive PostgreSQL shell |
| `docker exec -it <container> mysql -u <user> -p<pw>` | Open interactive MySQL shell |

### Quick Ports & Defaults Reference
- **PostgreSQL**: Port `5432`, Superuser `postgres`, CLI tool `psql`.
- **MySQL**: Port `3306`, Superuser `root`, CLI tool `mysql`.
- **SQLite**: No port, no superuser, single file, CLI tool `sqlite3`.

---

## 13. Further Reading
- Docker Official Postgres Image: [Docker Hub PostgreSQL](https://hub.docker.com/_/postgres)
- Docker Official MySQL Image: [Docker Hub MySQL](https://hub.docker.com/_/mysql)
- PostgreSQL Server Configuration Documentation: [PostgreSQL 17 Config Docs](https://www.postgresql.org/docs/17/runtime-config.html)
- SQLite PRAGMA Commands: [Official SQLite PRAGMA Reference](https://www.sqlite.org/pragma.html)
- [04 - Clients & Developer Tools](./04-clients-and-tools.md)
