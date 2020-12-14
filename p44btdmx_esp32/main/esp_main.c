// ESP_PLATFORM application C stub for p44utils style C++ main program

#include <stdlib.h>

#if CONFIG_P44_WIFI_SUPPORT
#include "wifi_init.h"
#endif

extern int main(int argc, char **argv);

int app_main()
{
  #if CONFIG_P44_WIFI_SUPPORT
  wifi_init();
  #endif
  int ret = main(0, (void *)0);
  exit(ret);
}
