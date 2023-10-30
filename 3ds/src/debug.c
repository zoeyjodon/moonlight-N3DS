#ifdef DEBUG
#include <sys/iosupport.h>
#include <whb/proc.h>
#include <whb/crash.h>
#include <whb/log.h>
#include <whb/log_udp.h>

#include <stdlib.h>
#include <stdio.h>

static ssize_t n3ds_log_write(struct _reent* r, void* fd, const char* ptr, size_t len) {
  WHBLogWritef("%*.*s", len, len, ptr);
  return len;
}
static const devoptab_t dotab_stdout = {
  .name = "stdout_whb",
  .write_r = n3ds_log_write,
};

void __wrap_abort() {
  WHBLogPrint("Abort called! Forcing stack trace.");
  *(unsigned int*)(0xDEADC0DE) = 0xBABECAFE;
  for(;;) {}
}

void Debug_Init() {
  WHBInitCrashHandler();
  WHBLogUdpInit();
  devoptab_list[STD_OUT] = &dotab_stdout;
  devoptab_list[STD_ERR] = &dotab_stdout;
}
#endif
