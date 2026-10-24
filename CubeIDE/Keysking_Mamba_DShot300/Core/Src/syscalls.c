/* Minimal Newlib system-call stubs used by snprintf in this bare-metal app. */
#include <sys/stat.h>

int _close(int file)
{
  (void)file;
  return -1;
}

int _fstat(int file, struct stat *status)
{
  (void)file;
  status->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int file)
{
  (void)file;
  return 1;
}

int _lseek(int file, int offset, int whence)
{
  (void)file;
  (void)offset;
  (void)whence;
  return 0;
}

int _read(int file, char *buffer, int length)
{
  (void)file;
  (void)buffer;
  (void)length;
  return 0;
}

int _write(int file, char *buffer, int length)
{
  (void)file;
  (void)buffer;
  return length;
}
