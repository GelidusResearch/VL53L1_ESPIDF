idf_component_register(SRCS
  "VL53L1X_api.c"
  "VL53L1X_calibration.c"
  "vl53l1_platform.c"
  "i2c_platform_esp.c"
  "gpio_platform_esp.c"
  REQUIRES mbedtls log driver esp_timer freertos)

