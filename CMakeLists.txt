cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(params_manager)

# Includi la directory src e la directory build (per param_defs.hpp)
target_include_directories(app PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})

# Includi la libreria MAVLink
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Aggiungi i file sorgente
target_sources(app PRIVATE
    src/main.cpp
    src/param_manager.cpp
    src/mavlink_handler.cpp
)

# Genera i file dei parametri da YAML
find_package(Python3 REQUIRED)

set(YAML_SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/src/param_schema.yaml)
set(GENERATOR_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_params.py)
set(PARAM_DEFS_HPP ${CMAKE_CURRENT_BINARY_DIR}/param_defs.hpp)
set(PARAM_DEFS_CPP ${CMAKE_CURRENT_BINARY_DIR}/param_defs.cpp)

add_custom_command(
    OUTPUT ${PARAM_DEFS_HPP} ${PARAM_DEFS_CPP}
    COMMAND ${Python3_EXECUTABLE} ${GENERATOR_SCRIPT} ${YAML_SCHEMA} ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${YAML_SCHEMA} ${GENERATOR_SCRIPT}
    COMMENT "Generazione codice parametri da YAML"
)

target_sources(app PRIVATE ${PARAM_DEFS_CPP})