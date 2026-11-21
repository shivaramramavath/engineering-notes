# POSIX API Quick Reference (`06-posix-api`)

## 1. Process Lifecycle (`<unistd.h>`, `<sys/wait.h>`)
```c
pid_t pid = fork();
if (pid == 0) {
    /* Child */
    char *args[] = {"ls", "-la", NULL};
    execvp("ls", args);
    _exit(127);
} else {
    /* Parent */
    int status;
    waitpid(pid, &status, 0);
}
```

## 2. Low-Level I/O (`<fcntl.h>`, `<unistd.h>`)
- `int fd = open("path", O_RDWR | O_CREAT | O_TRUNC, 0644);`
- `ssize_t bytes_read = read(fd, buffer, count);`
- `ssize_t bytes_written = write(fd, buffer, count);`
- `off_t offset = lseek(fd, 0, SEEK_SET);`
- `dup2(old_fd, new_fd);`
- `close(fd);`

## 3. Berkeley Sockets (`<sys/socket.h>`, `<netinet/in.h>`)
```c
int sfd = socket(AF_INET, SOCK_STREAM, 0);
int opt = 1;
setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(8080),
    .sin_addr.s_addr = INADDR_ANY
};
bind(sfd, (struct sockaddr*)&addr, sizeof(addr));
listen(sfd, 128);
int cfd = accept(sfd, NULL, NULL);
```

## 4. POSIX Threads (`<pthread.h>`)
- `pthread_create(&tid, NULL, worker_func, arg);`
- `pthread_join(tid, &ret_val);`
- `pthread_mutex_init(&lock, NULL);`
- `pthread_mutex_lock(&lock);` / `pthread_mutex_unlock(&lock);`
- `pthread_cond_wait(&cond, &lock);` / `pthread_cond_signal(&cond);`
