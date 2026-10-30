idf_component_register(
    SRCS
        "adc.cpp"
        "battery.cpp"
        "button.cpp"
        "compass.cpp"
        "hmi.cpp"
        "ledc.cpp"
        "main.cpp"
        "mcpwm.cpp"
        "motion.cpp"
        "motor.cpp"
        "net.cpp"
        "nvs.cpp"
        "proximity.cpp"
        "wifi.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_adc
        esp_driver_gpio
        esp_driver_ledc
        esp_driver_mcpwm
        esp_driver_i2c
        esp_event
        esp_netif
        esp_timer
        esp_wifi
        nvs_flash
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_26)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(SOC_TABLE_HEADER "${CMAKE_BINARY_DIR}/generated/soc_table.hpp")
set(SOC_TABLE_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/generate_soc_table.py")

set(GAMMA_TABLE_HEADER "${CMAKE_BINARY_DIR}/generated/gamma_table.hpp")
set(GAMMA_TABLE_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/generate_gamma_table.py")

add_custom_command(
    OUTPUT  ${SOC_TABLE_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${SOC_TABLE_SCRIPT}
    --output ${SOC_TABLE_HEADER}
    --points 21
    --steepness 6.0
    DEPENDS ${SOC_TABLE_SCRIPT}
    COMMENT "Generating soc_table.hpp"
    VERBATIM
)

add_custom_command(
    OUTPUT  ${GAMMA_TABLE_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${GAMMA_TABLE_SCRIPT}
    --output ${GAMMA_TABLE_HEADER}
    --points 21
    --gamma 2.8
    DEPENDS ${GAMMA_TABLE_SCRIPT}
    COMMENT "Generating gamma_table.hpp"
    VERBATIM
)

add_custom_target(generate_soc_table DEPENDS ${SOC_TABLE_HEADER})
add_custom_target(generate_gamma_table DEPENDS ${GAMMA_TABLE_HEADER})

add_dependencies(${COMPONENT_LIB} generate_soc_table generate_gamma_table)

target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_BINARY_DIR}/generated")
