set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_compile_options(${PROJECT_NAME} PUBLIC
    ${CPU_FLAGS}
    ${CC_SECURE}
    # Compiler flags specific to Debug build type
    $<$<CONFIG:Debug>:
      $<$<COMPILE_LANGUAGE:C>:
        -std=gnu11
        -fstack-usage
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -O0
        -g3
        -ggdb
      >
      $<$<COMPILE_LANGUAGE:ASM>:
        -x
        assembler-with-cpp
        -MMD
        -MP
      >
    >

    # Compiler flags specific to Release build type
    $<$<CONFIG:Release>:
      $<$<COMPILE_LANGUAGE:C>:
        -std=gnu11
        -fstack-usage
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Os
      >
      $<$<COMPILE_LANGUAGE:ASM>:
        -x
        assembler-with-cpp
        -MMD
        -MP
      >
    >
)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FatFs"
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
    APM32E10X_HD
    APM32E103_EVAL
    USE_STDPERIPH_DRIVER
    $<$<CONFIG:Debug>:DEBUG>
)

target_link_options(${PROJECT_NAME} PUBLIC
    ${CPU_FLAGS}
    ${CC_SECURE}
    -T${CMAKE_CURRENT_BINARY_DIR}/apm32e10x_flash.ld
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    --specs=nosys.specs
    -Wl,--start-group
    -lc
    -lm
    -Wl,--end-group
    -Wl,-z,max-page-size=8
    -Wl,--print-memory-usage
)

# Clean .map file
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/${PROJECT_NAME}.map")
