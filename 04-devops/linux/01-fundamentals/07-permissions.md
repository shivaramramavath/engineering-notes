# Permissions

Every file has an owner, a group, and a set of permission bits that decide who can read, write, or execute it. Most "Permission denied" errors, broken deploys, and security holes come from getting this wrong, so it pays to know exactly what each bit does.

Prerequisites: [Filesystem and Navigation](./02-filesystem-and-navigation.md) and [Users and Groups](./04-users-and-groups.md).

## Reading permissions

```text
-rwxr-xr--  1 alice dev  1200 Oct  4 15:30 deploy.sh
│└┬┘└┬┘└┬┘
│ │  │  └─ others (everyone else):  r--
│ │  └──── group  (dev):            r-x
│ └─────── owner  (alice):          rwx
└───────── type: - file, d directory, l symlink
```

The kernel picks **one** class and applies only that: if you are the owner, owner bits apply (even if group bits are more generous). Else if you are in the file's group, group bits apply. Otherwise, other bits apply.

### What r, w, x mean

The bits mean different things on files and directories. The directory meanings trip people up.

| Bit | On a file | On a directory |
|---|---|---|
| `r` | read contents | list names (`ls`) |
| `w` | modify contents | create, delete, rename entries inside |
| `x` | execute as a program | enter it (`cd`) and access items by name |

Consequences worth remembering:

- To reach `/a/b/c.txt` you need `x` on **every directory** in the path (`/`, `/a`, `/a/b`), not only on the file. A very common cause of "denied" on a file that looks fine.
- **Deleting a file needs `w` on its directory, not on the file.** A read-only file in a writable directory can be deleted.
- `r` without `x` on a directory lets you list names but not open anything inside. `x` without `r` lets you open files if you know the name.
- `root` bypasses `rwx` checks (it still can't execute a file that has no `x` bit at all).

## Numeric (octal) form

Each class is a sum: `r=4`, `w=2`, `x=1`.

| Octal | Symbolic | Typical use |
|---|---|---|
| `755` | `rwxr-xr-x` | programs, directories |
| `644` | `rw-r--r--` | regular files |
| `600` | `rw-------` | private files (SSH keys) |
| `700` | `rwx------` | private directories (`~/.ssh`) |
| `664` / `775` | group-writable | shared project files / dirs |

## Changing permissions: `chmod`

```bash
chmod 644 notes.md             # octal
chmod u+x deploy.sh            # add execute for the owner
chmod g+w,o-rwx shared.txt     # add group write, remove all from others
chmod a-w config.yml           # remove write for all (a = ugo)
chmod -R u=rwX,g=rX,o= app/    # recursive, sensible for mixed trees
```

Symbolic form is `[ugoa][+-=][rwxXst]`. The capital `X` sets execute **only on directories** (and on files that already have execute for someone), which makes it the right choice for recursive changes.

```bash
chmod -R 755 app/     # makes every FILE executable too; usually wrong
chmod -R u=rwX,go=rX app/   # directories get x, plain files don't
```

## Changing ownership: `chown` and `chgrp`

Only root can change a file's owner. The owner can change the group to one they belong to.

```bash
sudo chown alice file.txt
sudo chown alice:dev file.txt        # owner and group
sudo chown -R www-data:www-data /var/www/site
sudo chown :dev file.txt             # group only (or: chgrp dev file.txt)
```

Run `ls -ld /path` to check the directory itself, and `id` to check your own groups.

## Default permissions and `umask`

New files start from a base mode and have bits removed by the **umask**:

- Base for files is `666` (no execute by default), for directories `777`.
- Result = base with the umask bits cleared.

```text
umask 022  →  files 644, directories 755
umask 002  →  files 664, directories 775   (group-writable)
umask 077  →  files 600, directories 700   (private)
```

```bash
umask          # show current
umask 027      # set for this shell
```

Defaults vary by distro and setup (Ubuntu commonly uses `002` for regular users with per-user groups), so check with `umask` rather than assuming. To make a service create private files, set `UMask=` in its systemd unit ([systemd](../03-system/03-systemd-and-services.md)).

## Special bits: setuid, setgid, sticky

Three extra bits live in a fourth leading octal digit (`4`, `2`, `1`).

| Bit | On a file | On a directory | Seen as |
|---|---|---|---|
| **setuid** (4) | runs with the file **owner's** privileges | ignored | `s` in owner `x` slot |
| **setgid** (2) | runs with the file **group's** privileges | new files inherit the directory's group | `s` in group `x` slot |
| **sticky** (1) | ignored | only a file's owner (or dir owner/root) can delete or rename it | `t` in other `x` slot |

```bash
ls -l /usr/bin/passwd   # -rwsr-xr-x  → setuid root; lets normal users update /etc/shadow
ls -ld /tmp             # drwxrwxrwt  → sticky: everyone writes, only owners delete
```

The most practical one is **setgid on a shared directory**, so everything created inside belongs to the team group:

```bash
sudo mkdir /srv/project
sudo chown root:dev /srv/project
sudo chmod 2775 /srv/project     # setgid + rwxrwxr-x
```

Combine with `umask 002` and members of `dev` can collaborate without fighting over ownership.

A capital `S` or `T` in `ls` output means the special bit is set but the underlying `x` is not, which is usually a mistake.

Setuid binaries are a classic privilege-escalation target. Audit them occasionally:

```bash
find / -xdev -perm -4000 -type f 2>/dev/null
```

Setuid is honored on compiled binaries but ignored for shell scripts on Linux. Don't try to give a script setuid.

## ACLs (a first look)

Classic permissions allow one owner and one group. When you need "also let user bob read this" without changing ownership, use POSIX ACLs. They need the `acl` package and a filesystem with ACL support (ext4 and XFS have it by default).

```bash
setfacl -m u:bob:r file.txt         # give bob read
setfacl -m g:auditors:rx /srv/data  # give a group read+execute
setfacl -d -m g:dev:rwx /srv/project   # default ACL: inherited by new files
getfacl file.txt                    # show ACLs
setfacl -x u:bob file.txt           # remove bob's entry
```

A `+` at the end of the `ls -l` permission string (`-rw-r--r--+`) means an ACL is present. With ACLs, the group column in `ls -l` shows the ACL **mask**, which can hide what's really granted. Always use `getfacl` when a `+` is shown.

## Debugging "Permission denied"

Work from the path outward:

```bash
id                                  # who am I, which groups?
namei -l /var/www/site/index.html   # permissions of every component in the path
ls -ld /var/www/site                # the directory itself
getfacl /var/www/site/index.html    # ACLs, if there's a "+"
```

`namei -l` is the fastest way to find a directory in the path missing `x`. If permissions look right but access still fails, consider:

- You were added to a group but haven't re-logged-in ([users and groups](./04-users-and-groups.md)).
- The service runs as a different user than you think (`ps -o user,cmd -p <pid>`).
- A mandatory access control layer (SELinux or AppArmor) is blocking it; see [server hardening](../05-production/01-server-hardening.md).
- The filesystem is mounted read-only or with `noexec` ([storage](../03-system/05-storage-and-filesystems.md)).

## Common mistakes

- **`chmod 777` as a fix.** It makes the file writable by everyone and hides the real cause. Find who needs access and grant just that.
- **`chmod -R 755` on a whole tree**, making data files executable. Use `X`, or `find -type d` / `-type f` separately.
- **Group-writable but wrong group**, so "everyone has access" except the process that needs it.
- **Private keys too open.** SSH refuses keys with loose permissions; use `chmod 600 ~/.ssh/id_ed25519` and `700 ~/.ssh`.
- **Forgetting directory `x`** when sharing a file deep in a tree.
- Believing `rm` needs write permission on the file. It needs `w` on the parent directory.

## Quick Summary

- Three classes (owner, group, others) times three bits (r, w, x); the first matching class wins.
- For directories: `r` lists, `w` changes entries, `x` lets you enter; you need `x` along the whole path.
- `chmod` changes modes (use `X` for recursion), `chown` changes owner/group, `umask` sets defaults.
- setgid on a shared directory keeps group ownership consistent; sticky protects `/tmp`; avoid setuid unless you must.
- ACLs add per-user/group exceptions; `getfacl` shows the truth when `ls -l` ends in `+`.
- Debug with `id`, `namei -l`, `ls -ld`, and `getfacl`.

**Next:** [Shell Basics](../02-shell/01-shell-basics.md)
