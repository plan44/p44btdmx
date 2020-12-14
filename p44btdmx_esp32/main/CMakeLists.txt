set(srcs
  "esp_main.c"
  "p44dmx_main.cpp"
  "esp_bt.cpp"
  "p44btdmx.cpp"
  "pwmlight.cpp"
  "p44lrglight.cpp")

set(incl
  ".")

if(CONFIG_P44_WIFI_SUPPORT)
    list(APPEND srcs
      "wifi_init.c")
endif()

idf_component_register(
  SRCS srcs
  INCLUDE_DIRS incl)

target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-reorder
  -frtti
  -isystem /Volumes/CaseSens/openwrt/build_dir/target-mipsel_24kc_musl/boost_1_71_0)
