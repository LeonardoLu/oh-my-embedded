cmake_minimum_required(VERSION 3.20)
project(imu_native_regression LANGUAGES C CXX)

set(ui "${IMU_UPSTREAM}/components/mosaic_ui")
add_subdirectory("${ESP_GSP_COMPONENT_DIR}/tools/sim_bridge" sim_bridge)
gsp_add_backend(imu_native
    SOURCES platform.c "${ui}/apps/imu/imu_liquid.c"
        "${ui}/apps/imu/liquidduck/flip.cpp"
        "${ui}/common/mosaic_app_catalog.c"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
        "${CMAKE_CURRENT_SOURCE_DIR}/../settings_native/stubs"
        "${ui}/include" "${ui}/common" "${ui}/runtime" "${ui}/core"
        "${ui}/apps/imu"
    PROFILE "${ui}/common/mosaic_rgb565_auto.yaml"
    SCENES "${ui}/apps/imu/scene/imu_480.json")

foreach(target imu_native imu_native_module)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/imu_native_assets/scene0")
    target_compile_definitions(${target} PRIVATE
        IMU_APP_SOURCE="${ui}/apps/imu/imu_app.c")
endforeach()
