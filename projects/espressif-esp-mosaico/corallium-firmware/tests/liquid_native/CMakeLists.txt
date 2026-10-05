cmake_minimum_required(VERSION 3.20)
project(liquid_native_regression LANGUAGES C CXX)

set(ui "${LIQUID_UPSTREAM}/components/mosaic_ui")
add_subdirectory("${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge" sim_bridge)
gsp_add_backend(liquid_native
    SOURCES platform.c "${ui}/apps/liquid/liquid_engine.c"
        "${ui}/apps/liquid/liquidduck/flip.cpp"
        "${ui}/common/mosaic_app_catalog.c"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../settings_native/stubs"
        "${ui}/include" "${ui}/common" "${ui}/runtime"
        "${ui}/apps/liquid"
    PROFILE "${ui}/common/mosaic_rgb565_auto.yaml"
    SCENES "${ui}/apps/liquid/scene/liquid_480.json")

foreach(target liquid_native liquid_native_module)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/liquid_native_assets/scene0")
    target_compile_definitions(${target} PRIVATE
        LIQUID_APP_SOURCE="${ui}/apps/liquid/liquid_app.c")
endforeach()
