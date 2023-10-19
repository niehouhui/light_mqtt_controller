idf_component_register(
                    SRC_DIRS "." 
                        "./ws2812b"
                        "./wifi-smart-config"
                        "./tcp_server"
                        "./mqtt_server"
                    INCLUDE_DIRS "."  
                                "./tcp_server"
                                "./wifi-smart-config"
                                "./ws2812b"
                                "./mqtt_server"
                    REQUIRES  driver nvs_flash esp_wifi mqtt json spiffs
                    )


