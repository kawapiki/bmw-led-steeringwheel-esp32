idf_component_register(SRCS "main.c" INCLUDE_DIRS "." "../../../boards/wheel_cvs8161"
    REQUIRES wheel_core esp_driver_gpio esp_timer esp_app_format esp_system)
