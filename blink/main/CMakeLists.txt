idf_component_register(SRCS "blink_example_main.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_ledc esp_timer
                       INCLUDE_DIRS "")