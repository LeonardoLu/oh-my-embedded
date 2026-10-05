cmake_minimum_required(VERSION 3.20)
project(works_native_regression LANGUAGES C CXX)
set(ui "${WORKS_UPSTREAM}/components/mosaic_ui")
add_subdirectory("${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge" sim_bridge)
gsp_add_backend(works_native
    SOURCES platform.c "${ui}/common/mosaic_app_catalog.c"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
        "${CMAKE_CURRENT_SOURCE_DIR}/../settings_native/stubs"
        "${ui}/include" "${ui}/common" "${ui}/runtime" "${ui}/apps/liquid"
    PROFILE "${ui}/common/mosaic_rgb565_auto.yaml"
    SCENES "${ui}/apps/works/scene/works_480.json")
foreach(target works_native works_native_module)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/works_native_assets/scene0")
    target_compile_definitions(${target} PRIVATE
        WORKS_APP_SOURCE="${ui}/apps/works/works_app.c")
endforeach()
