idf_component_register(
  SRCS
  "esp_main.c"
  "p44utils_main.cpp"
  "wifi_init.c"
  INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE
  -Wno-reorder
  -frtti
  -isystem /Volumes/CaseSens/openwrt/build_dir/target-mipsel_24kc_musl/boost_1_71_0)
