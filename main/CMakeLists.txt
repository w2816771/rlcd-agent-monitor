idf_component_register(SRCS "main.cpp" "monitor_ui.cpp" "monitor_model.cpp" "monitor_ble.cpp"
                            "board_sensors.cpp" "audio_meter.cpp" "debug_log.cpp" "rlcd_font_bold_14.c" "rlcd_font_bold_18.c"
                            "rlcd_font_bold_26.c" "rlcd_font_bold_40.c" "rlcd_font_bold_72.c"
                            "rlcd_font_bold_104.c"
                       INCLUDE_DIRS "."
                       REQUIRES rlcd lvgl esp_timer driver esp_adc esp_driver_i2c esp_driver_i2s
                                esp_codec_dev bt nvs_flash json)
