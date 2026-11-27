# Linux Automation with Ansible: Architecture, Playbooks, and Idempotency

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Configuration Management, Ansible Architecture, Playbooks, Roles, and Baseline Hardening |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `02-shell/07-bash-scripting.md`, `04-networking/04-ssh.md`, `05-production/01-server-hardening.md` |
| **Target Audience** | DevOps Engineers, SREs, Systems Architects, Automation Engineers |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools / Files** | `ansible`, `ansible-playbook`, `ansible-galaxy`, `inventory.ini`, `ansible.cfg` |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Internal Working](#internal-working)
4. [Syntax & CLI Tools](#syntax--cli-tools)
5. [Progressive Practical Examples](#progressive-practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance, Security & Cost Implications](#performance-security--cost-implications)
9. [Comparison Table / Alternatives](#comparison-table--alternatives)
10. [Real-World Troubleshooting & Interview Q&A](#real-world-troubleshooting--interview-qa)
11. [Hands-On Practice Exercises](#hands-on-practice-exercises)
12. [Summary Cheat Sheet & Further Reading](#summary-cheat-sheet--further-reading)

---

## What & Why

Managing Linux servers by manually logging into individual machines over SSH and executing shell commands does not scale. Manual administration leads to:
- **Configuration Drift**: Servers that were supposedly identical diverge over time as engineers apply un-versioned hotfixes.
- **Snowflake Servers**: Fragile machines that nobody dares to reboot or replace because no one remembers the manual installation steps.
- **Lack of Auditability**: Zero source-controlled history of who changed system configurations or why.

While Bash scripts can automate repetitive tasks, Bash is fundamentally **imperative**: running `useradd alice` twice crashes with an error; running `echo "config" >> file.conf` ten times duplicates the line ten times.

**Ansible** is the industry standard for Linux configuration management. It is:
- **Agentless**: Requires zero daemon software installed on managed nodes; operates entirely over standard **OpenSSH** using Python.
- **Declarative & Idempotent**: You declare the desired end state (e.g., `state: present`, `state: started`). Executing an Ansible playbook once applies necessary changes; executing it 1,000 times produces `changed: 0` without altering the system.
- **Self-Documenting**: Written in human-readable YAML with Jinja2 templating.

---

## Core Concepts

### The Principle of Idempotency
An operation is **idempotent** if applying it multiple times produces the exact same result as applying it once, with zero unintended side effects.
- *Non-Idempotent (Bash)*: `echo "AllowUsers devops" >> /etc/ssh/sshd_config` (Appends line every time it runs).
- *Idempotent (Ansible)*:
  ```yaml
  - name: Ensure AllowUsers is configured
    ansible.builtin.lineinfile:
      path: /etc/ssh/sshd_config
      regexp: '^AllowUsers'
      line: 'AllowUsers devops'
      state: present
  ```
  If the line already exists, Ansible does nothing (`ok: [host]`). If missing, it adds it (`changed: [host]`).

### Inventory Management
Ansible targets nodes defined in an **Inventory** (INI or YAML format):
- Groups hosts logically (e.g. `[webservers]`, `[databases]`, `[production]`).
- Binds variables per host or per group (`group_vars/`, `host_vars/`).
- Supports dynamic inventory plugins (querying AWS EC2, GCP, Azure, or Kubernetes APIs automatically).

### Playbook Anatomy
- **Play**: Maps a group of managed hosts to a set of roles and tasks.
- **Task**: An execution unit invoking a specific Ansible module with parameters.
- **Handler**: A specialized task that only runs when explicitly triggered by a `notify` directive from another task (e.g., reloading Nginx only if the configuration file changed).
- **Facts**: System information automatically discovered from target nodes before execution (`ansible_facts`: OS distribution, kernel version, IP addresses, memory).

### Ansible Roles
As playbooks expand, logic is modularized into reusable **Roles** following a strict directory structure:
```text
my_role/
├── tasks/main.yml       # Core sequential task list
├── handlers/main.yml    # Service restart handlers
├── templates/           # Jinja2 templates (.j2)
├── files/               # Static files copied as-is
├── vars/main.yml        # High-priority role variables
├── defaults/main.yml    # Overridable default variables
└── meta/main.yml        # Role dependencies and metadata
```

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Control Node (Ansible Engine)                        |
|                                     ansible-playbook -i inv site.yml                    |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                   Execution Engine Lifecycle                            |
|   1. Parses YAML Playbook & evaluates Jinja2 variables                                 |
|   2. Connects to Managed Node via OpenSSH (ControlMaster multiplexing)                  |
|   3. Gathers System Facts (/proc, ip, systemd) via 'setup' module                       |
|   4. Synthesizes a standalone, self-contained Python script per task                    |
|   5. Copies the Python payload to: ~/.ansible/tmp/... on target node                    |
|   6. Executes the Python script on the managed node via unprivileged/sudo               |
|   7. Returns JSON result payload: {"changed": true, "rc": 0, ...}                       |
|   8. Removes temporary Python payload from remote disk immediately                      |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                    Managed Target Node (Linux)                          |
|                                    State Converged to Desired State                     |
+-----------------------------------------------------------------------------------------+
```

### Why Agentless Matters
Unlike legacy configuration agents (Puppet, Chef, SaltStack) that require persistent background daemons running as root on every server:
- Ansible introduces **0 persistent memory footprint** on target nodes.
- There are no client-side daemon certificates to expire, no agent software upgrades to manage, and no listening ports to secure beyond standard SSH (port 22).

---

## Syntax & CLI Tools

### Ad-Hoc Command Execution
Execute immediate, one-off commands across hundreds of servers without writing playbooks.

```bash
# Ping all servers in the inventory to verify connectivity
ansible all -i inventory.ini -m ping

# Check disk space on all webservers
ansible webservers -i inventory.ini -m command -a "df -h /"

# Restart nginx service with sudo privileges (-b / --become)
ansible webservers -i inventory.ini -b -m systemd -a "name=nginx state=restarted"

# Gather and display facts for a specific host
ansible srv-db-01 -i inventory.ini -m setup
```

### Running Playbooks
```bash
# Execute a playbook against inventory
ansible-playbook -i inventory.ini deploy-stack.yml

# Dry-run mode (--check): Previews changes without modifying any files on disk
ansible-playbook -i inventory.ini site.yml --check

# Show unified diff of changes that would be made (--diff)
ansible-playbook -i inventory.ini site.yml --check --diff

# Limit execution to a specific host or group (--limit)
ansible-playbook -i inventory.ini site.yml --limit "db-primary"

# Run only tasks tagged with a specific tag
ansible-playbook -i inventory.ini site.yml --tags "security,ntp"
```

---

## Progressive Practical Examples

### Example 1: Crafting an Inventory File (`inventory.ini`)
```ini
# inventory.ini

[control]
localhost ansible_connection=local

[webservers]
web-01.prod.internal ansible_host=10.0.1.10
web-02.prod.internal ansible_host=10.0.1.11

[databases]
db-01.prod.internal  ansible_host=10.0.2.50

# Metagroup combining multiple groups
[production:children]
webservers
databases

# Group variables applied to all production nodes
[production:vars]
ansible_user=ubuntu
ansible_ssh_private_key_file=~/.ssh/id_ed25519
ansible_python_interpreter=/usr/bin/python3
environment_tier=production
```

### Example 2: Idempotent Baseline Hardening Playbook
A production-grade playbook hardening Ubuntu and RHEL servers according to CIS Level 1 baselines.

```yaml
---
# harden-servers.yml
- name: Apply Enterprise Linux Security Baseline
  hosts: production
  become: yes
  gather_facts: yes

  vars:
    required_packages:
      - curl
      - htop
      - ufw
      - fail2ban
      - chrony
    unwanted_packages:
      - telnet
      - rsh-client
      - talk

  handlers:
    - name: Restart sshd
      ansible.builtin.systemd:
        name: "{{ 'ssh' if ansible_os_family == 'Debian' else 'sshd' }}"
        state: restarted

    - name: Reload sysctl
      ansible.builtin.command: sysctl --system

  tasks:
    - name: Update apt cache and remove unwanted legacy packages
      ansible.builtin.package:
        name: "{{ unwanted_packages }}"
        state: absent

    - name: Install mandatory security and telemetry packages
      ansible.builtin.package:
        name: "{{ required_packages }}"
        state: present

    - name: Enforce SSH configuration hardening
      ansible.builtin.lineinfile:
        path: /etc/ssh/sshd_config
        regexp: "{{ item.regexp }}"
        line: "{{ item.line }}"
        state: present
        validate: "/usr/sbin/sshd -t -f %s"
      loop:
        - { regexp: '^#?PermitRootLogin', line: 'PermitRootLogin no' }
        - { regexp: '^#?PasswordAuthentication', line: 'PasswordAuthentication no' }
        - { regexp: '^#?MaxAuthTries', line: 'MaxAuthTries 3' }
      notify: Restart sshd

    - name: Apply hardened kernel sysctl parameters
      ansible.builtin.copy:
        dest: /etc/sysctl.d/99-ansible-hardened.conf
        content: |
          kernel.randomize_va_space = 2
          fs.protected_symlinks = 1
          fs.protected_hardlinks = 1
          net.ipv4.tcp_syncookies = 1
          net.ipv4.conf.all.rp_filter = 1
        owner: root
        group: root
        mode: '0644'
      notify: Reload sysctl

    - name: Ensure Chrony time synchronization daemon is running and enabled
      ansible.builtin.systemd:
        name: chrony
        state: started
        enabled: yes
```

### Example 3: Dynamic Jinja2 Configuration Templating
Deploying an Nginx configuration file dynamically tailored to host facts (CPU core count, IP addresses).

```jinja2
{# templates/nginx.conf.j2 #}
# Generated automatically by Ansible - DO NOT EDIT MANUALLY
# Host: {{ ansible_fqdn }} | Cores: {{ ansible_processor_vcpus }}

user www-data;
worker_processes {{ ansible_processor_vcpus }};
pid /run/nginx.pid;

events {
    worker_connections 2048;
    multi_accept on;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    server_tokens off;
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;

    access_log /var/log/nginx/access.log;
    error_log /var/log/nginx/error.log warn;

    server {
        listen 80 default_server;
        server_name {{ ansible_fqdn }};
        root /var/www/html;

        location / {
            try_files $uri $uri/ =404;
        }
    }
}
```

```yaml
# In playbook tasks:
- name: Deploy dynamic Nginx configuration
  ansible.builtin.template:
    src: templates/nginx.conf.j2
    dest: /etc/nginx/nginx.conf
    owner: root
    group: root
    mode: '0644'
    validate: 'nginx -t -c %s'
  notify: Reload nginx
```

### Example 4: Creating Reusable Roles (`ansible-galaxy`)
Organize infrastructure logic into standardized roles.

```bash
# Initialize standard role skeleton
ansible-galaxy role init roles/node_exporter
```
**`roles/node_exporter/tasks/main.yml`**:
```yaml
---
- name: Create node_exporter system group
  ansible.builtin.group:
    name: node_exporter
    system: yes
    state: present

- name: Create node_exporter system user
  ansible.builtin.user:
    name: node_exporter
    group: node_exporter
    system: yes
    shell: /bin/false
    create_home: no

- name: Download and unpack node_exporter binary
  ansible.builtin.unarchive:
    src: "https://github.com/prometheus/node_exporter/releases/download/v{{ node_exporter_version }}/node_exporter-{{ node_exporter_version }}.linux-amd64.tar.gz"
    dest: /usr/local/bin/
    remote_src: yes
    extra_opts:
      - --strip-components=1
      - "node_exporter-{{ node_exporter_version }}.linux-amd64/node_exporter"
    owner: node_exporter
    group: node_exporter
    mode: '0755'

- name: Deploy systemd unit file
  ansible.builtin.template:
    src: node_exporter.service.j2
    dest: /etc/systemd/system/node_exporter.service
    mode: '0644'
  notify: Restart node_exporter

- name: Ensure node_exporter is enabled and started
  ansible.builtin.systemd:
    name: node_exporter
    state: started
    enabled: yes
    daemon_reload: yes
```

### Example 5: High-Performance Ansible Configuration (`ansible.cfg`)
Tune Ansible Control Node to manage hundreds of servers concurrently over SSH.

```ini
# ansible.cfg
[defaults]
inventory = inventory.ini
remote_user = ubuntu
host_key_checking = True
retry_files_enabled = False
forks = 50
gathering = smart
fact_caching = jsonfile
fact_caching_connection = /tmp/ansible_facts_cache
fact_caching_timeout = 86400
stdout_callback = yaml

[privilege_escalation]
become = True
become_method = sudo
become_user = root
become_ask_pass = False

[ssh_connection]
# Enable SSH pipelining to avoid copying temporary Python files
pipelining = True
# Enable SSH multiplexing for 5x-10x execution speedup
ssh_args = -o ControlMaster=auto -o ControlPersist=60m -o StrictHostKeyChecking=accept-new
```

---

## Best Practices

1. **Always Use Module-Specific Handlers for Service Restarts**: Never place a task that restarts a service directly in the main task sequence (`systemd: state=restarted`). If the configuration file was unchanged, restarting the service creates unnecessary application downtime. Use `notify: Restart nginx` on the configuration task so the handler triggers **only when changes occur**.
2. **Always Use `validate:` on Critical Configuration Files**: When modifying `/etc/sudoers`, `/etc/ssh/sshd_config`, or `/etc/nginx/nginx.conf`, always pass the `validate:` option:
   ```yaml
   validate: '/usr/sbin/sshd -t -f %s'
   validate: 'visudo -c -f %s'
   ```
   If a syntax error is introduced by a templating typo, Ansible aborts the file copy, preventing administrators from being locked out of remote servers.
3. **Enable SSH Pipelining in `ansible.cfg`**: Setting `pipelining = True` in `ansible.cfg` reduces the number of network roundtrips executed per task by piping Python scripts directly into remote Python interpreters over the SSH session rather than uploading files to `/tmp`. This speeds up playbook execution by **over 300%**.
4. **Never Store Plaintext Secrets in Playbooks**: Encrypt passwords, API keys, and private certificates using **Ansible Vault** (`ansible-vault encrypt_string` or `ansible-vault encrypt vars/secrets.yml`).
5. **Always Run `--check --diff` in CI/CD Pipelines**: Before merging infrastructure pull requests, run `ansible-playbook --check --diff` in your CI/CD runner. This previews the exact line-by-line diff of what will change on production servers without applying any changes.
6. **Prefer Built-In Declarative Modules Over `command` or `shell`**: Using `command: systemctl restart foo` breaks idempotency (Ansible reports `changed` every single run). Always use declarative modules: `ansible.builtin.systemd`, `ansible.builtin.copy`, `ansible.builtin.lineinfile`.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Using `shell` or `command` for package installations | Breaches idempotency; reports `changed` on every execution; fails to verify package status. | Use `ansible.builtin.package`, `apt`, or `dnf`. |
| Modifying `/etc/sudoers` without `validate: 'visudo -c -f %s'` | A syntax typo locks out Ansible's `become: sudo` escalation permanently. | Always include the `validate` parameter. |
| Forgetting `become: yes` on tasks requiring root | Fails with `Permission denied` on tasks modifying `/etc` or `/var`. | Specify `become: yes` at the play or task level. |
| Leaving `pipelining = False` on high-latency WAN links | Ansible spends 80% of its time copying ephemeral Python files to `/tmp` and deleting them. | Set `pipelining = True` in `ansible.cfg`. |
| Hardcoding package names across different distributions | `apache2` on Debian/Ubuntu vs `httpd` on RHEL fails with `Package not found`. | Use `ansible_os_family` conditional facts or `vars/{{ ansible_os_family }}.yml`. |
| Committing unencrypted passwords to Git repositories | Leaks production secrets. | Encrypt variables with `ansible-vault`. |

---

## Performance, Security & Cost Implications

### Performance
- **Forks Tuning**: By default, Ansible runs only **5 parallel forks** (`forks = 5`). On an inventory of 200 servers, tasks execute across 5 servers at a time. Increasing `forks = 50` in `ansible.cfg` executes tasks concurrently across 50 nodes simultaneously, cutting execution time from 10 minutes to under 60 seconds.
- **Fact Caching**: Gathering facts (`setup` module) on 500 servers takes 15–30 seconds. Enabling JSON or Redis fact caching (`gathering = smart`) caches host attributes, skipping fact collection on subsequent playbook runs.

### Security
- **Immutable Infrastructure Audit**: By running playbooks on a scheduled hourly cron via Ansible Tower / AWX, any unauthorized configuration changes made manually by rogue actors are automatically overwritten and reverted back to the approved Git baseline.

### Cost
- Standardizing server builds via Ansible eliminates human provisioning errors, cuts deployment times from hours to minutes, and prevents configuration drift, directly lowering operational maintenance expenditures.

---

## Comparison Table / Alternatives

| Feature / Tool | Ansible | Terraform | SaltStack | Puppet |
| :--- | :--- | :--- | :--- | :--- |
| **Primary Domain** | Configuration Management & App Deploy | Infrastructure Provisioning (VPCs, VMs) | High-Speed Configuration & Execution | Configuration Management |
| **Architecture** | **Agentless** (SSH + Python) | Agentless (Cloud APIs) | Agent-based (ZeroMQ) | Agent-based (Ruby) |
| **Language** | YAML + Jinja2 | HCL (HashiCorp Config Lang) | YAML + Python | Custom Ruby DSL |
| **State File** | **Stateless** (Queries live machine) | Stateful (`terraform.tfstate`) | Stateful master | Stateful catalog |
| **Best Used For** | Configuring OS, installing apps, hardening | Creating cloud resources (AWS/GCP/Azure)| Massive fleets (10,000+ nodes) | Legacy enterprise IT |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: What makes Ansible idempotent and why is idempotency essential?
<details>
<summary>View Solution</summary>

Ansible modules are designed around **desired state convergence**:
1. When a task executes, the module first inspects the target system's current state.
2. If the current state matches the desired specification (e.g., package `nginx` is already installed, or user `alice` already exists with UID 1001), the module takes zero action and returns `{"changed": false}`.
3. If the state differs, the module applies only the exact delta necessary to reach the desired state and returns `{"changed": true}`.

**Why it matters**:
Idempotency allows engineers to run playbooks repeatedly with confidence. You can execute an update playbook across 1,000 servers; if 995 servers are already up-to-date, Ansible updates only the 5 drifted nodes without disturbing active services on the remaining 995.
</details>

### Q2: What is the purpose of Handlers in Ansible and when do they execute?
<details>
<summary>View Solution</summary>

A **Handler** is a task that runs **only if notified by another task that reported a `changed` status**.
- Handlers do **not** run sequentially when notified.
- By default, all notified handlers run **once at the very end of the play**, regardless of how many tasks notified them.
- *Example*: If three separate configuration files are updated, each notifies `Restart nginx`. Nginx restarts **exactly once at the end**, rather than restarting three times mid-playbook.
</details>

### Q3: How do you handle operating system differences (e.g. Ubuntu vs RHEL) in a single playbook?
<details>
<summary>View Solution</summary>

Use Ansible's built-in discovery facts (`ansible_os_family` or `ansible_distribution`):
```yaml
- name: Install Apache web server
  ansible.builtin.package:
    name: "{{ 'apache2' if ansible_os_family == 'Debian' else 'httpd' }}"
    state: present

- name: Include OS-specific variables
  ansible.builtin.include_vars: "vars/{{ ansible_os_family }}.yml"
```
</details>

### Q4: Why does `command: systemctl restart nginx` violate Ansible best practices?
<details>
<summary>View Solution</summary>

1. **Breaches Idempotency**: Ansible cannot inspect the internal state of a raw shell command; it flags the task as `changed` on every run, corrupting automation reporting.
2. **Lack of Validation**: It does not verify service unit health or state cleanly.
3. **Correct Pattern**: Use the declarative `ansible.builtin.systemd` module:
   ```yaml
   ansible.builtin.systemd:
     name: nginx
     state: restarted
   ```
</details>

### Q5: What is the difference between `vars/main.yml` and `defaults/main.yml` in an Ansible role?
<details>
<summary>View Solution</summary>

Ansible has a strict **variable precedence hierarchy** (over 20 levels):
- **`defaults/main.yml`**: Has the **lowest precedence** in Ansible. It provides fallback values intended to be easily overridden by users in playbooks or group variables.
- **`vars/main.yml`**: Has very high precedence. It holds internal role variables that should **not** be overridden by external playbooks.
</details>

### Q6: How do you securely store and deploy database passwords in an Ansible repository?
<details>
<summary>View Solution</summary>

Use **Ansible Vault**:
1. Encrypt an individual secret string:
   ```bash
   ansible-vault encrypt_string 'SuperSecretDBPass' --name 'db_password'
   ```
   Paste the resulting encrypted YAML block into `group_vars/production.yml`.
2. Or encrypt an entire file:
   ```bash
   ansible-vault encrypt vars/secrets.yml
   ```
3. Run the playbook passing the vault password:
   ```bash
   ansible-playbook -i inventory.ini site.yml --ask-vault-pass
   # Or using a secure password file:
   ansible-playbook -i inventory.ini site.yml --vault-password-file ~/.vault_pass
   ```
</details>

### Q7: What does the `gather_facts: no` directive do and when should you use it?
<details>
<summary>View Solution</summary>

By default, every play begins with the `setup` module, which connects to every host and gathers hundreds of hardware, network, and OS facts.
- Setting `gather_facts: no` disables this discovery step.
- Use it when:
  1. Bootstrapping raw cloud VMs that do not yet have Python installed (`raw` module tasks).
  2. Running simple ad-hoc operational tasks where facts are unnecessary, speeding up playbook execution significantly.
</details>

### Q8: How can you preview the exact line-by-line configuration changes an Ansible playbook will make before running it on production servers?
<details>
<summary>View Solution</summary>

Run the playbook with `--check` and `--diff`:
```bash
ansible-playbook -i inventory.ini site.yml --check --diff
```
- `--check` (Dry Run): Tells modules to test changes against the system without modifying disk or services.
- `--diff`: Displays a unified patch diff (`---` before, `+++` after) showing the exact lines being added, removed, or altered in configuration files.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Ad-Hoc Host Inspection on Localhost
**Task**: Use Ansible's ad-hoc CLI to execute a ping and gather architecture facts against your local machine (`localhost`).
<details>
<summary>View Solution</summary>

```bash
# 1. Ping test
ansible localhost -m ping

# 2. Extract OS distribution facts
ansible localhost -m setup -a "filter=ansible_distribution*"
```
</details>

### Exercise 2: Writing and Testing an Idempotent File Touch Task
**Task**: Author a mini-playbook `/tmp/test-playbook.yml` that ensures a directory `/tmp/ansible-lab` exists with mode `0750`. Execute it twice to verify idempotency.
<details>
<summary>View Solution</summary>

```yaml
# /tmp/test-playbook.yml
---
- name: Test Idempotency
  hosts: localhost
  connection: local
  tasks:
    - name: Ensure test directory exists
      ansible.builtin.file:
        path: /tmp/ansible-lab
        state: directory
        mode: '0750'
```

```bash
# First run: reports 'changed: 1'
ansible-playbook /tmp/test-playbook.yml

# Second run: reports 'ok: 1, changed: 0' (Idempotent!)
ansible-playbook /tmp/test-playbook.yml

rm -rf /tmp/ansible-lab /tmp/test-playbook.yml
```
</details>

### Exercise 3: Validating Playbook Syntax and Dry-Run
**Task**: Create a playbook deploying a sample configuration file with `ansible.builtin.lineinfile`. Run `ansible-playbook --syntax-check` and `--check`.
<details>
<summary>View Solution</summary>

```bash
cat << 'EOF' > /tmp/validate-play.yml
---
- name: Validation Lab
  hosts: localhost
  connection: local
  tasks:
    - name: Add sentinel line to temporary file
      ansible.builtin.lineinfile:
        path: /tmp/sample.conf
        line: "SERVER_NAME=prod-node-01"
        create: yes
        mode: '0644'
EOF

# Syntax check
ansible-playbook /tmp/validate-play.yml --syntax-check

# Dry-run execution
ansible-playbook /tmp/validate-play.yml --check

rm -f /tmp/validate-play.yml /tmp/sample.conf
```
</details>

### Exercise 4: Testing Jinja2 Template Rendering
**Task**: Render a Jinja2 template converting a dictionary of key-values into an environment configuration file using Ansible.
<details>
<summary>View Solution</summary>

```yaml
# /tmp/template-lab.yml
---
- name: Template Lab
  hosts: localhost
  connection: local
  vars:
    app_env:
      DB_HOST: "10.0.1.50"
      DB_PORT: "5432"
      APP_MODE: "production"
  tasks:
    - name: Render environment file
      ansible.builtin.copy:
        dest: /tmp/app.env
        content: |
          {% for k, v in app_env.items() %}
          {{ k }}={{ v }}
          {% endfor %}
        mode: '0600'
```

```bash
ansible-playbook /tmp/template-lab.yml
cat /tmp/app.env
rm -f /tmp/template-lab.yml /tmp/app.env
```
</details>

### Exercise 5: Creating an Encrypted Secret with Ansible Vault
**Task**: Create an encrypted test variable using `ansible-vault encrypt_string`.
<details>
<summary>View Solution</summary>

```bash
# Create encrypted variable (prompts for vault password)
echo -n "SuperSecretValue123" | ansible-vault encrypt_string --name 'api_key'
```
</details>

### Exercise 6: Verifying SSH Pipelining Configuration
**Task**: Inspect your local or project `ansible.cfg` to confirm whether SSH pipelining is enabled.
<details>
<summary>View Solution</summary>

```bash
ansible-config dump | grep -i pipelining
# Should return: ANSIBLE_PIPELINING(default) = False or True
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Ping All Nodes** | `ansible all -i inv.ini -m ping` |
| **Run Playbook** | `ansible-playbook -i inv.ini site.yml` |
| **Dry-Run (Simulation)** | `ansible-playbook -i inv.ini site.yml --check` |
| **Show Unified Diff** | `ansible-playbook -i inv.ini site.yml --diff` |
| **Limit to Specific Node** | `ansible-playbook -i inv.ini site.yml -l "web-01"` |
| **Run Specific Tags** | `ansible-playbook -i inv.ini site.yml -t "security"` |
| **Gather Facts for Host** | `ansible <host> -i inv.ini -m setup` |
| **Encrypt with Vault** | `ansible-vault encrypt vars/secrets.yml` |
| **Run with Vault Prompt** | `ansible-playbook -i inv.ini site.yml --ask-vault-pass`|

### Further Reading
- [Ansible Official Documentation](https://docs.ansible.com/)
- [Ansible Community Builtin Modules Index](https://docs.ansible.com/ansible/latest/collections/ansible/builtin/index.html)
- [Ansible Galaxy: Community Roles & Collections](https://galaxy.ansible.com/)
- [Michael DeHaan: The Philosophy of Ansible](https://michaeldehaan.net/)
