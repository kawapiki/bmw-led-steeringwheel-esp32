idf_component_register(SRCS "wheel_io.c" "wheel_led.c" "rpm_led.c" INCLUDE_DIRS "include" "../../../../boards/wheel_cvs8161" REQUIRES contracts wheel_core esp_driver_gpio esp_driver_rmt)
