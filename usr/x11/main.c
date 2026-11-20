/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#include "tinyx.h"
#include "demo.h"
#include "display.h"

int uptime(void);
static volatile sig_atomic_t stopping;
static int socket_mode;

struct framebuffer {
  unsigned char *video;
  unsigned char *shadow;
  size_t stride;
  unsigned width, height;
};

struct queue {
  size_t used, end;
  unsigned char data[4096];
};

static void
interrupt(int sig)
{
  stopping = sig;
}

static uint32_t
milliseconds(void *ctx)
{
  return (uint32_t)uptime() * (1000 / P9_HZ);
}

static void
log_error(void *ctx, tinyx_log_level level, const char *message)
{
  if(level != TINYX_LOG_AUDIT)
    fputs(message, stderr);
}

static int
nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  return flags < 0 ? -1 : fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static pid_t
spawn(char **command, int check, int *receive, int *send)
{
  int input[2], output[2];
  if(pipe(input) < 0)
    return -1;
  if(pipe(output) < 0){
    close(input[0]);
    close(input[1]);
    return -1;
  }
  pid_t pid = fork();
  if(!pid){
    close(input[1]);
    close(output[0]);
    if(dup2(input[0], 0) < 0 || dup2(output[1], 1) < 0)
      _exit(126);
    if(input[0] != 0)
      close(input[0]);
    if(output[1] != 1)
      close(output[1]);
    if(!command)
      _exit(x11demo(check));
    execve(command[0], command, environ);
    perror(command[0]);
    _exit(127);
  }
  close(input[0]);
  close(output[1]);
  *receive = output[0];
  *send = input[1];
  return pid;
}

static unsigned
keycode(unsigned key, int extended)
{
  if(!extended){
    if(key >= 1 && key <= 0x53)
      return key + 8;
    return key == 0x57 ? 95 : key == 0x58 ? 96 : 0;
  }
  static const unsigned char arrows[13] = {110,111,112,0,113,0,114,0,115,116,117,118,119};
  if(key >= 0x47 && key <= 0x53)
    return arrows[key - 0x47];
  switch(key){
  case 0x1c: return 104;
  case 0x1d: return 105;
  case 0x35: return 106;
  case 0x38: return 108;
  case 0x5b: return 133;
  case 0x5c: return 134;
  case 0x5d: return 135;
  default: return 0;
  }
}

static void
input(tinyx_server *server, int fd, int keyboard)
{
  static int extended, pause_bytes;
  static unsigned modifiers;
  static unsigned buttons;
  static const unsigned button_code[] = {1, 3, 2};
  struct input_event events[24];
  ssize_t got = read(fd, events, sizeof(events));
  for(size_t i = 0; got > 0 && i < (size_t)got / sizeof(*events); ++i){
    const struct input_event *event = &events[i];
    if(keyboard && event->type == INPUT_KEY){
      unsigned raw = event->code;
      if(pause_bytes){
        --pause_bytes;
        continue;
      }
      if(raw == 0xe1){
        pause_bytes = 5;
        extended = 0;
        continue;
      }
      if(raw == 0xe0){
        extended = 1;
        continue;
      }
      unsigned code = keycode(raw & 0x7f, extended);
      extended = 0;
      unsigned bit = code == 37 || code == 105 ? 1U : code == 64 || code == 108 ? 2U : 0U;
      if(raw & 0x80)
        modifiers &= ~bit;
      else
        modifiers |= bit;
      if(code == 9 && !(raw & 0x80) && (!socket_mode || modifiers == 3)){
        stopping = SIGINT;
        return;
      }
      if(code)
        tinyx_key(server, code, !(raw & 0x80));
    } else if(!keyboard && event->type == INPUT_RELATIVE){
      if(event->x || event->y)
        tinyx_pointer_motion_relative(server, event->x, event->y);
      for(unsigned j = 0; j < sizeof(button_code)/sizeof(*button_code); j++){
        if((buttons ^ event->buttons) & (1U << j))
          tinyx_pointer_button(server, button_code[j], !!(event->buttons & (1U << j)));
      }
      buttons = event->buttons;
    }
  }
}

static int
present(tinyx_server *server, const struct framebuffer *fb)
{
  if(!fb->video)
    return 0;

  size_t count;
  if(tinyx_server_take_damage(server, NULL, 0, &count) != TINYX_OK)
    return -1;
  if(!count)
    return 0;

  tinyx_damage_rect rects[256];
  size_t capacity = sizeof(rects) / sizeof(*rects);
  if(count < capacity)
    capacity = count;
  if(tinyx_server_take_damage(server, rects, capacity, &count) != TINYX_OK)
    return -1;
  for(size_t i = 0; i < count; i++){
    int64_t left = rects[i].x, top = rects[i].y;
    int64_t right = left + rects[i].width, bottom = top + rects[i].height;
    if(left < 0)
      left = 0;
    if(top < 0)
      top = 0;
    if(right > fb->width)
      right = fb->width;
    if(bottom > fb->height)
      bottom = fb->height;
    if(left >= right || top >= bottom)
      continue;

    size_t start = (size_t)left * 4;
    size_t len = (size_t)(right - left) * 4;
    size_t offset = (size_t)top * fb->stride + start;
    size_t rows = bottom - top;
    if(len == fb->stride){
      memcpy(fb->video + offset, fb->shadow + offset, rows * len);
    } else {
      for(; rows; rows--, offset += fb->stride)
        memcpy(fb->video + offset, fb->shadow + offset, len);
    }
  }
  return 0;
}

static int
submit(tinyx_client *client, struct queue *incoming)
{
  if(incoming->used == incoming->end)
    return 0;
  size_t sent = 0;
  tinyx_status result = tinyx_client_send(client, incoming->data + incoming->used,
                                           incoming->end - incoming->used, &sent);
  if(result != TINYX_OK && result != TINYX_ERROR_WOULD_BLOCK)
    return -1;
  incoming->used += sent;
  return 0;
}

static int
wait_ms(const tinyx_step_result *step, int pending)
{
  if(step->immediate_work || (pending && step->requests_processed))
    return 0;
  return step->next_timeout_ms < 20 ? step->next_timeout_ms : 20;
}

static int
pump(tinyx_server *server, tinyx_client *client, pid_t *child,
     int rx, int tx, int keyboard, int pointer, struct queue *queues,
     const struct framebuffer *fb)
{
  struct queue *incoming = &queues[0], *outgoing = &queues[1];
  int max = rx > tx ? rx : tx;
  if(keyboard > max)
    max = keyboard;
  if(pointer > max)
    max = pointer;
  int done = 0;
  while(!stopping){
    int status;
    int waited = waitpid(*child, &status, WNOHANG);
    if(waited == *child){
      *child = -1;
      return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
    if(waited < 0 && errno != EINTR)
      return 1;

    if(submit(client, incoming) < 0)
      return 1;
    tinyx_step_result step;
    if(tinyx_server_step(server, 32, &step) != TINYX_OK || present(server, fb) < 0)
      return 1;
    if(outgoing->used == outgoing->end){
      outgoing->used = outgoing->end = 0;
      tinyx_status result = tinyx_client_receive(client, outgoing->data, sizeof(outgoing->data),
                                                &outgoing->end);
      if(result != TINYX_OK && result != TINYX_ERROR_WOULD_BLOCK && result != TINYX_ERROR_CLOSED)
        return 1;
    }

    fd_set reading, writing;
    FD_ZERO(&reading);
    FD_ZERO(&writing);
    if(!done && incoming->used == incoming->end)
      FD_SET(rx, &reading);
    if(outgoing->used < outgoing->end)
      FD_SET(tx, &writing);
    if(keyboard >= 0)
      FD_SET(keyboard, &reading);
    if(pointer >= 0)
      FD_SET(pointer, &reading);
    struct timeval timeout = { .tv_usec = 1000 * wait_ms(&step, incoming->used < incoming->end) };
    int ready = select(max + 1, &reading, &writing, NULL, &timeout);
    if(ready < 0 && errno != EINTR)
      return 1;
    if(ready <= 0)
      continue;
    if(FD_ISSET(rx, &reading)){
      ssize_t n = read(rx, incoming->data, sizeof(incoming->data));
      if(n > 0){
        incoming->used = 0;
        incoming->end = n;
      } else if(n == 0){
        done = 1;
        tinyx_client_shutdown_send(client);
      } else if(errno != EAGAIN && errno != EINTR){
        return 1;
      }
    }
    if(FD_ISSET(tx, &writing)){
      ssize_t n = write(tx, outgoing->data + outgoing->used, outgoing->end - outgoing->used);
      if(n > 0)
        outgoing->used += n;
      else if(n < 0 && errno != EAGAIN && errno != EINTR){
        // the client may be exiting while its final events are pending
        if(errno != EPIPE)
          return 1;
        outgoing->used = outgoing->end;
      }
    }
    if(FD_ISSET(keyboard, &reading))
      input(server, keyboard, 1);
    if(FD_ISSET(pointer, &reading))
      input(server, pointer, 0);
  }
  return 0;
}

enum { client_limit = 8 };
struct link {
  int fd;
  tinyx_client *client;
  struct queue rx, tx;
};

static int
listener(void)
{
  struct stat directory;
  if(stat(X11_DIR, &directory) < 0 || !S_ISDIR(directory.st_mode) ||
     directory.st_uid != 0 || (directory.st_mode & 022)){
    errno = EACCES;
    return -1;
  }
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd < 0)
    return -1;
  struct sockaddr_un address = display_address();
  mode_t previous = umask(0077);
  int bound = bind(fd, (struct sockaddr*)&address, sizeof(address));
  umask(previous);
  if(bound < 0 || listen(fd, client_limit) < 0 || nonblock(fd) < 0){
    int saved = errno;
    close(fd);
    if(bound == 0)
      unlink(X11_PATH);
    errno = saved;
    return -1;
  }
  return fd;
}

static void
drop(struct link *c)
{
  tinyx_client_destroy(c->client);
  close(c->fd);
  memset(c, 0, sizeof(*c));
  c->fd = -1;
}

static int
collect(struct link *c)
{
  if(c->tx.used == c->tx.end){
    c->tx.used = c->tx.end = 0;
    tinyx_status status = tinyx_client_receive(c->client, c->tx.data, sizeof(c->tx.data), &c->tx.end);
    if(status != TINYX_OK && status != TINYX_ERROR_WOULD_BLOCK)
      return -1;
  }
  return 0;
}

static int
transfer(struct link *c, short events)
{
  if(events & (POLLERR | POLLNVAL))
    return -1;
  if((events & (POLLIN | POLLHUP)) && c->rx.used == c->rx.end){
    ssize_t n = read(c->fd, c->rx.data, sizeof(c->rx.data));
    if(n > 0){
      c->rx.used = 0;
      c->rx.end = n;
    } else if(n == 0 || (errno != EAGAIN && errno != EINTR)){
      return -1;
    }
  }
  if(events & POLLOUT){
    ssize_t n = write(c->fd, c->tx.data + c->tx.used, c->tx.end - c->tx.used);
    if(n > 0)
      c->tx.used += n;
    else if(n < 0 && errno != EAGAIN && errno != EINTR)
      return -1;
  }
  return 0;
}

static int
multiplex(tinyx_server *server, int fd, int keyboard, int pointer,
          const struct framebuffer *fb)
{
  struct link *peers = calloc(client_limit, sizeof(*peers));
  if(!peers)
    return 1;
  for(int i = 0; i < client_limit; i++)
    peers[i].fd = -1;
  int result = 0;
  while(!stopping){
    for(int i = 0; i < client_limit; i++)
      if(peers[i].fd >= 0 && submit(peers[i].client, &peers[i].rx) < 0)
        drop(&peers[i]);
    tinyx_step_result step;
    if(tinyx_server_step(server, 32, &step) != TINYX_OK || step.generation_finished || present(server, fb) < 0){
      result = 1;
      break;
    }
    struct pollfd wait[3 + client_limit] = {{fd, POLLIN, 0},
      {keyboard, POLLIN, 0}, {pointer, POLLIN, 0}};
    int pending = 0;
    for(int i = 0; i < client_limit; i++){
      struct link *c = &peers[i];
      if(c->fd >= 0){
        if(collect(c) < 0)
          drop(c);
        else
          pending |= c->rx.used < c->rx.end;
      }
      wait[i+3].fd = c->fd;
      wait[i+3].events = (c->rx.used == c->rx.end ? POLLIN : 0) |
                         (c->tx.used < c->tx.end ? POLLOUT : 0);
    }
    int ready = poll(wait, 3 + client_limit, wait_ms(&step, pending));
    if(ready < 0 && errno != EINTR){
      result = 1;
      break;
    }
    if(ready <= 0)
      continue;
    for(int i = 0; i < client_limit; i++)
      if(peers[i].fd >= 0 && transfer(&peers[i], wait[i+3].revents) < 0)
        drop(&peers[i]);
    if(wait[0].revents & POLLIN){
      int new = accept(fd, NULL, NULL);
      if(new >= 0){
        int i = 0;
        while(i < client_limit && peers[i].fd >= 0)
          i++;
        if(i == client_limit || nonblock(new) < 0 ||
           tinyx_client_open(server, NULL, &peers[i].client) != TINYX_OK)
          close(new);
        else
          peers[i].fd = new;
      }
    }
    if(wait[1].revents & POLLIN)
      input(server, keyboard, 1);
    if(wait[2].revents & POLLIN)
      input(server, pointer, 0);
  }
  for(int i = 0; i < client_limit; i++)
    if(peers[i].fd >= 0)
      drop(&peers[i]);
  free(peers);
  return result;
}

int
main(int argc, char **argv)
{
  int check = argc == 2 && !strcmp(argv[1], "--check");
  socket_mode = argc == 2 && !strcmp(argv[1], ":0");
  if(argc > 1 && !check && !socket_mode && argv[1][0] != '/'){
    fputs("usage: tinyx [--check | :0 | /path/to/x11-client [args ...]]\n", stderr);
    return 2;
  }
  if(signal(SIGINT, interrupt) == SIG_ERR || signal(SIGTERM, interrupt) == SIG_ERR ||
     signal(SIGTSTP, SIG_IGN) == SIG_ERR)
    return 1;

  int rx = -1, tx = -1, keyboard = -1, pointer = -1, fb = -1, result = 1;
  int socketfd = -1;
  pid_t child = -1;
  if(socket_mode)
    socketfd = listener();
  else
    child = spawn(!check && argc > 1 ? argv + 1 : NULL, check, &rx, &tx);
  void *pixels = MAP_FAILED;
  size_t bytes = 0;
  struct framebuffer target = {0};
  struct queue *queues = NULL;
  tinyx_server *server = NULL;
  tinyx_client *client = NULL;
  tinyx_screen_config screen;
  tinyx_screen_config_init(&screen);
  screen.framebuffer.width = 320;
  screen.framebuffer.height = 200;
  if(socket_mode && socketfd < 0){
    perror("tinyx: cannot listen on " X11_PATH);
    goto cleanup;
  }
  if(!socket_mode && (child < 0 || nonblock(rx) < 0 || nonblock(tx) < 0)){
    perror("tinyx: cannot start the client transport");
    goto cleanup;
  }
  if(!check){
    struct fbinfo info;
    int mode;
    fb = open("/dev/fb0", O_RDWR);
    keyboard = open("/dev/keyboard", O_RDONLY | O_NONBLOCK);
    pointer = open("/dev/mouse", O_RDONLY | O_NONBLOCK);
    if(pointer < 0)
      pointer = open("/dev/touchpad", O_RDONLY | O_NONBLOCK);
    if(fb < 0 || keyboard < 0 || ioctl(fb, FBIOGETINFO, &info) < 0 ||
       info.bits_per_pixel != 32 || !info.width || !info.height ||
       info.width > info.stride / 4 || info.height > SIZE_MAX / info.stride || ioctl(keyboard, KBDIOGETMODE, &mode) < 0 ||
       mode != KBD_MODE_SET1){
      fputs("tinyx: requires a 32 bit framebuffer and a set 1 keyboard\n", stderr);
      goto cleanup;
    }
    struct input_event stale[24];
    for(int i = 0; i < 16 && read(keyboard, stale, sizeof(stale)) > 0; i++){}
    if(pointer >= 0)
      for(int i = 0; i < 16 && read(pointer, stale, sizeof(stale)) > 0; i++){}
    bytes = (size_t)info.stride * info.height;
    pixels = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if(pixels == MAP_FAILED){
      perror("tinyx: cannot map /dev/fb0");
      goto cleanup;
    }
    target.shadow = calloc(1, bytes);
    if(!target.shadow){
      perror("tinyx: cannot allocate the shadow framebuffer");
      goto cleanup;
    }
    target.video = pixels;
    target.stride = info.stride;
    target.width = info.width;
    target.height = info.height;
    screen.framebuffer.width = info.width;
    screen.framebuffer.height = info.height;
    screen.framebuffer.stride_bytes = info.stride;
    screen.framebuffer.pixels = target.shadow;
    screen.framebuffer.pixels_size = bytes;
  }

  tinyx_config config;
  tinyx_error failure;
  tinyx_config_init(&config);
  config.initial_screen = &screen;
  config.host.monotonic_time_ms = milliseconds;
  config.host.log = log_error;
  if(tinyx_server_create(&config, &server, &failure) != TINYX_OK){
    fprintf(stderr, "tinyx: %s\n", failure.message);
    goto cleanup;
  }
  if(socket_mode){
    result = multiplex(server, socketfd, keyboard, pointer, &target);
    goto cleanup;
  }
  if(tinyx_client_open(server, NULL, &client) != TINYX_OK){
    fputs("tinyx: cannot open the X11 stream\n", stderr);
    goto cleanup;
  }
  queues = calloc(2, sizeof(*queues));
  if(!queues){
    perror("tinyx: cannot allocate transport buffers");
    goto cleanup;
  }
  result = pump(server, client, &child, rx, tx, keyboard, pointer, queues, &target);
  if(result)
    fputs("tinyx: X11 session failed\n", stderr);
  else if(check && !stopping)
    puts("Tiny-X X11 rendering check passed");

cleanup:
  if(socketfd >= 0){
    close(socketfd);
    unlink(X11_PATH);
  }
  free(queues);
  if(rx >= 0)
    close(rx);
  if(tx >= 0)
    close(tx);
  if(child > 0){
    kill(child, SIGKILL);
    while(waitpid(child, NULL, 0) < 0 && errno == EINTR){}
  }
  if(client)
    tinyx_client_destroy(client);
  if(server)
    tinyx_server_destroy(server);
  if(pixels != MAP_FAILED)
    munmap(pixels, bytes);
  free(target.shadow);
  if(keyboard >= 0)
    close(keyboard);
  if(pointer >= 0)
    close(pointer);
  if(fb >= 0)
    close(fb);
  return result;
}
