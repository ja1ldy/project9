/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */
#ifndef P9_X_DISPLAY_H
#define P9_X_DISPLAY_H

#include <sys/socket.h>

#define X11_DIR "/tmp/.X11-unix"
#define X11_PATH X11_DIR "/X0"
#define X11_BIN "/usr/X11/bin/"

// this port exposes display zero on a local unix socket
static inline struct sockaddr_un
display_address(void)
{
  return (struct sockaddr_un){ .sun_family = AF_UNIX, .sun_path = X11_PATH };
}
#endif
