idf_component_register(
                    SRC_DIRS "." 
                        "./ws2812b"
                        "./wifi-smart-config"
                        "./tcp-server"
                        "./mqtt-server"
                        "./json-handle"
                    INCLUDE_DIRS "."  
                                "./tcp-server"
                                "./wifi-smart-config"
                                "./ws2812b"
                                "./mqtt-server"
                                "./json-handle"
                    REQUIRES  driver nvs_flash esp_wifi mqtt json spiffs 
                    )


