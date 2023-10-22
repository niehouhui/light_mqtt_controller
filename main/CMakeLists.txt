idf_component_register(
                    SRC_DIRS "." 
                        "./ws2812b"
                        "./ws2812b/src"
                        "./wifi-smart-config"
                        "./tcp_server"
                        "./mqtt_server"
                        "./json_handle"
                    INCLUDE_DIRS "."  
                                "./tcp_server"
                                "./wifi-smart-config"
                                "./ws2812b"
                                "./ws2812b/include"
                                "./mqtt_server"
                                "./json_handle"
                    REQUIRES  driver nvs_flash esp_wifi mqtt json spiffs 
                    )


