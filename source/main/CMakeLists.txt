idf_component_register(
    SRCS "main.c" "quotes_model.c" "quotes_ui.c" "quotes_pack.c" "quotes_network.c" "quotes_context.c" "../assets/fonts/quote_font_20.c" "../assets/fonts/quote_font_14.c" "../assets/fonts/quote_font_18.c" "../assets/fonts/quote_font_28.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client esp-tls esp_wifi esp_event esp_netif bsp esp_timer nvs_flash json esp_driver_usb_serial_jtag
)
if("$ENV{QUOTES_SHARE_BUILD}" STREQUAL "1")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE QUOTES_NO_BUILTIN=1)
endif()
