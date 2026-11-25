# Getting Started with Docker

Everything you need to go from "Docker isn't installed" to "I ran my first container."

---

## 1. Install Docker

### Windows / macOS

Install **Docker Desktop** from [docker.com](https://www.docker.com/products/docker-desktop/). It bundles the Docker daemon, CLI, and a GUI, and runs a lightweight Linux VM under the hood (Docker containers are Linux containers, even on Windows/macOS).

On Windows, Docker Desktop will prompt you to enable **WSL 2** (Windows Subsystem for Linux) if it isn't already — accept this, it's the recommended backend and is noticeably faster than the older Hyper-V backend.

### Linux

Install Docker Engine directly (no Desktop app needed):

```bash
# Debian/Ubuntu
curl -fsSL https://get.docker.com | sh

# Then add your user to the docker group so you don't need sudo every time
sudo usermod -aG docker $USER
```

Log out and back in (or restart) for the group change to take effect.

### Verify the install

```bash
docker --version
```

```
Docker version 27.x.x, build xxxxxxx
```

---

## 2. Confirm the daemon is running

Docker's CLI is just a client — it talks to a background service called the **Docker daemon**. If the daemon isn't running, every command fails with a connection error.

```bash
docker info
```

If this prints detailed system info, you're good. If it errors with something like `Cannot connect to the Docker daemon`, start Docker Desktop (or, on Linux, `sudo systemctl start docker`) and try again.

---

## 3. Run your first container

```bash
docker run hello-world
```

```
Unable to find image 'hello-world:latest' locally
latest: Pulling from library/hello-world
...
Hello from Docker!
This message shows that your installation appears to be working correctly.
```

What just happened, step by step:

1. Docker looked for the `hello-world` image locally and didn't find it
2. It pulled the image from **Docker Hub** (the default public registry)
3. It created and started a **container** from that image
4. The container ran, printed a message, and exited

---

## 4. A slightly more real example

```bash
docker run -d -p 8080:80 --name my-nginx nginx
```

- `-d` — detached, runs in the background
- `-p 8080:80` — maps port 8080 on your machine to port 80 in the container
- `--name my-nginx` — gives the container a memorable name instead of a random one

Visit `http://localhost:8080` — you should see the Nginx welcome page.

```bash
docker ps                 # confirm it's running
docker stop my-nginx       # stop it when done
docker rm my-nginx          # remove it
```

(These are covered properly in `02_docker_commands`.)

---

## You're set up when you can:

- [ ] Run `docker --version` and see a version number
- [ ] Run `docker info` without a connection error
- [ ] Run `docker run hello-world` and see the success message
- [ ] Run `docker run -d -p 8080:80 nginx` and see the welcome page at `localhost:8080`

## Next

Head to **`01_docker_fundamentals`** to understand what an image and a container actually are, and how they relate — before learning more commands.
