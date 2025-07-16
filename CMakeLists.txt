idf_component_register(
    SRCS sht30.c
    INCLUDE_DIRS .
    REQUIRES driver freertos log  esp_timer
)