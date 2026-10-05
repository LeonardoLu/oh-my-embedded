idf_component_register(
    SRCS "mosaico_diagnostics.c" "mosaico_diagnostics_core.c" "mosaico_diagnostics_tasks.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES usb_hs_console esp_timer esp_hw_support
)
