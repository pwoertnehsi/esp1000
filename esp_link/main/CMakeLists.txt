idf_component_register(SRCS "main.c" "phy_recovery.c" "host_wakeup.c" "native_scan.c" "native_profile.c" "usb_router.c" "local_tcp.c" "network_page.c" "../../common/lab_gateway.c" INCLUDE_DIRS "../../common"
    PRIV_REQUIRES lwip tinyusb esp_driver_gpio esp_driver_uart esp_timer esp_hw_support
    esp_wifi esp_event esp_netif nvs_flash esp_http_client mbedtls)
