# Check ESP-IDF >= 5.0
if(IDF_VERSION_MAJOR GREATER_EQUAL 5)
    set(SRCS
        led_strip_new.c
        led_strip_encoder.c
    )
else()
    set(SRCS led_strip.c)
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS .
    REQUIRES driver log color esp_idf_lib_helpers
)
