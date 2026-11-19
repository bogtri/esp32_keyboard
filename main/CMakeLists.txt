idf_component_register(SRC_DIRS "./src"
                    REQUIRES esp_hid
                    PRIV_REQUIRES esp_driver_gpio esp_driver_pcnt nvs_flash esp_pm
                    INCLUDE_DIRS "./include")
