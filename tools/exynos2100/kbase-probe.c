/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Pro-Termux-arch
 * Version handshake only. No SET_FLAGS, allocation or job submission.
 * Values/layout follow src/panfrost/lib/kmod/kbase_kmod.c (MIT, upstream).
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct version_check { uint16_t major, minor; };
#define JM_VERSION_CHECK _IOWR(0x80, 0, struct version_check)

int main(void)
{
   const struct version_check requests[] = {{0, 0}, {11, 31}, {11, 99}};
   int accepted = 0;
   for (unsigned i = 0; i < sizeof(requests)/sizeof(requests[0]); i++) {
      int fd = open("/dev/mali0", O_RDWR | O_CLOEXEC);
      if (fd < 0) { perror("open /dev/mali0"); return 1; }
      struct version_check v = requests[i];
      errno = 0;
      int r = ioctl(fd, JM_VERSION_CHECK, &v), err = errno;
      printf("JM requested=%u.%u result=%d returned=%u.%u errno=%d (%s)\n",
             requests[i].major, requests[i].minor, r, v.major, v.minor, err, strerror(err));
      if (i == 1) accepted = r == 0 && v.major == 11;
      close(fd);
   }
   puts("Version handshakes are not a kernel DDK release identifier.");
   return accepted ? 0 : 1;
}
