add_executable(elastic_mqtt_client
  main.c
  ntp_sync.c
)
target_include_directories(elastic_mqtt_client PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${PICO_LWIP_PATH}/src/include
  ${PICO_MBEDTLS_PATH}/include
)
target_link_libraries(elastic_mqtt_client PRIVATE
  hardware_adc
  hardware_rtc
  pico_cyw43_arch_lwip_threadsafe_background
  pico_lwip_mqtt
  pico_lwip_sntp
  pico_stdlib
)
pico_enable_stdio_usb(elastic_mqtt_client 1)
pico_enable_filesystem(elastic_mqtt_client AUTO_INIT TRUE)
pico_add_extra_outputs(elastic_mqtt_client)
