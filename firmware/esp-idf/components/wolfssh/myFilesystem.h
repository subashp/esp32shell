#pragma once

// wolfSSH's SFTP port hook. ESP-IDF LittleFS exposes the standard VFS/POSIX
// file interface, so this adapter intentionally contains no filesystem state
// or alternate path traversal logic; the SFTP root is set to /littlefs.
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define WFILE FILE
#define WFOPEN(fs, f, fn, m) ((*(f) = fopen((fn), (m))) == NULL)
#define WFCLOSE(fs, f) fclose((f))
#define WFREAD(fs, b, s, a, f) fread((b), (s), (a), (f))
#define WFWRITE(fs, b, s, a, f) fwrite((b), (s), (a), (f))
#define WFSEEK(fs, f, o, w) fseek((f), (o), (w))
#define WFTELL(fs, f) ftell((f))
#define WREWIND(fs, f) rewind((f))
#define WSEEK_END SEEK_END
#define WBADFILE NULL
#define WFFLUSH fflush
#define WSETTIME(fs, f, a, m) 0
#define WFSETTIME(fs, f, a, m) 0
#define WCHMOD(fs, f, m) chmod((f), (m))
#define WFCHMOD(fs, f, m) fchmod((f), (m))

#define WSTAT_T struct stat
#define WRMDIR(fs, d) rmdir((d))
#define WMKDIR(fs, d, m) mkdir((d), (m))
#define WSTAT(fs, p, b) stat((p), (b))
#define WLSTAT(fs, p, b) lstat((p), (b))
#define WREMOVE(fs, d) remove((d))
#define WRENAME(fs, o, n) rename((o), (n))
#define WGETCWD(fs, r, sz) getcwd((r), (sz))
#define WS_DELIM '/'

#define WFD int
#define WOLFSSH_O_RDWR O_RDWR
#define WOLFSSH_O_RDONLY O_RDONLY
#define WOLFSSH_O_WRONLY O_WRONLY
#define WOLFSSH_O_APPEND O_APPEND
#define WOLFSSH_O_CREAT O_CREAT
#define WOLFSSH_O_TRUNC O_TRUNC
#define WOLFSSH_O_EXCL O_EXCL
#define WOPEN(fs, f, m, p) open((f), (m), (p))
#define WCLOSE(fs, fd) close((fd))
static inline int wolfssh_pwrite(int fd, unsigned char* buf, unsigned int size,
                                 const unsigned int* offset) {
  return (int)pwrite(fd, buf, size, offset[0]);
}
static inline int wolfssh_pread(int fd, unsigned char* buf, unsigned int size,
                                 const unsigned int* offset) {
  return (int)pread(fd, buf, size, offset[0]);
}
#define wPwrite wolfssh_pwrite
#define wPread wolfssh_pread
#define WPWRITE(fs, fd, b, s, o) wolfssh_pwrite((fd), (b), (s), (o))
#define WPREAD(fs, fd, b, s, o) wolfssh_pread((fd), (b), (s), (o))

#define WDIR DIR*
#define WOPENDIR(fs, h, c, d) ((*(c) = opendir((d))) == NULL)
#define WCLOSEDIR(fs, d) closedir(*(d))
#define WREADDIR(fs, d) readdir(*(d))
#define WREWINDDIR(fs, d) rewinddir(*(d))
