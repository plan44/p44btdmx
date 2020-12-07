# p44utils on EPS32 - Test/Demo/Skeleton project

Note: this is Work In Progress at this time. The intention is that this will evolve to a useful starting point for
creating p44utils based EPS32 projects.

At this time, it is more a test application for proting p44utils features to ESP32

## how to add submodules for ESP32

    `git submodule add -b esp32 ssh://plan44@plan44.ch/home/plan44/sharedgit/p44utils.git components/p44utils`

And if we use p44lrgraphics:

    `git submodule add -b master ssh://plan44@plan44.ch/home/plan44/sharedgit/p44lrgraphics.git components/p44lrgraphics`




## IDF patches

- need to add C++ guard in `components/vfs/include/sys/ioctl.h` for v3.1.2, **but this is fixed in IDF master already**!
