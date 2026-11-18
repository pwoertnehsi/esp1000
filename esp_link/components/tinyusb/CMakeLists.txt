idf_component_register(SRCS
    "src/tusb.c"
    "src/common/tusb_fifo.c"
    "src/host/usbh.c"
    "src/portable/synopsys/dwc2/dwc2_common.c"
    "src/portable/synopsys/dwc2/hcd_dwc2.c"
    INCLUDE_DIRS "src" "config"
    PRIV_REQUIRES esp_hw_support esp_timer esp_driver_gpio)
# With only an application class driver, TinyUSB's built-in driver table is
# empty. GCC 15 diagnoses the corresponding zero-iteration lookup loop.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-type-limits)
