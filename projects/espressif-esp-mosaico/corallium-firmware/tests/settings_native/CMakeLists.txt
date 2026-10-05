cmake_minimum_required(VERSION 3.20)
project(settings_native_regression LANGUAGES C)

set(ui "${SETTINGS_UPSTREAM}/components/mosaic_ui")
if(NOT SETTINGS_NATIVE_APP_SOURCE)
    set(SETTINGS_NATIVE_APP_SOURCE "${ui}/apps/settings/settings_app.c")
endif()
add_subdirectory("${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge" sim_bridge)
gsp_add_backend(settings_native
    SOURCES platform.c unavailable.c
        "${ui}/common/mosaic_settings_model.c"
        "${ui}/common/qrcodegen/qrcodegen.c"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
        "${ui}/include" "${ui}/common" "${ui}/runtime"
        "${ui}/common/qrcodegen" "${ui}/apps/settings"
    PROFILE "${ui}/common/mosaic_rgb565_auto.yaml"
    SCENES "${SETTINGS_NATIVE_SCENE}")

foreach(target settings_native settings_native_module)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/settings_native_assets/scene0")
    target_compile_definitions(${target} PRIVATE
        SETTINGS_APP_SOURCE="${SETTINGS_NATIVE_APP_SOURCE}")
endforeach()
