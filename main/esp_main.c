// ESP_PLATFORM application C stub for p44utils style C++ main program

#include <stdlib.h>

#include "wifi_init.h"

extern int main(int argc, char **argv);

int app_main()
{
  wifi_init();
  int ret = main(0, (void *)0);
  exit(ret);
}
