cmake_minimum_required(VERSION 3.15)
project(fake_lora C)

# ---- Language standard: strict ISO C, no compiler-specific extensions -----
# Extensions off = no GNU-isms creeping in (statement expressions, typeof,
# non-standard attributes, etc.) that would silently break portability to a
# different compiler or architecture down the line.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(MCU "atmega328p")
set(F_CPU "8000000UL")
set(PROGRAMMER "usbasp")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(HEX_OUTPUT_PATH
    "/mnt/d/DevaBDM/Embedded Systems/Proteus/LandSlideReceiver"
    CACHE PATH "Directory to place generated HEX file")
set(HEX_FILE "${HEX_OUTPUT_PATH}/${PROJECT_NAME}.hex")

# ---- Build type: Debug by default -------------------------------------
# Switch to Release by hand when you're done developing — either edit the line
# below to 'Release', or reconfigure with -DCMAKE_BUILD_TYPE=Release.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      Debug
      CACHE STRING "Build type: Debug or Release" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")

# ---- Warnings: maximum, treated as errors ----------------------------------
# NOTE: -Wconversion / -Wsign-conversion can get noisy on register-heavy MCU
# code, where 8-bit values get implicitly promoted to int. If they're more noise
# than signal for your code, comment them out — everything else here is worth
# leaving on.
add_compile_options(
  -mmcu=${MCU}
  -Wall
  -Wextra
  -Wpedantic
  -Werror
  -Wshadow
  -Wdouble-promotion
  -Wformat=2
  -Wundef
  # -Wconversion -Wsign-conversion
  -Wcast-align
  -Wcast-qual
  -Wlogical-op
  -Wredundant-decls
  # -Wmissing-declarations -Wmissing-prototypes
  -Wstrict-prototypes
  -Wold-style-definition
  -Wnested-externs
  -Wpointer-arith
  -Wwrite-strings
  -ffunction-sections
  -fdata-sections)

# ---- Debug vs Release: optimization level, debug symbols, assert control --
# -Og keeps optimizations debugger-friendly; -Os in Release favors the small
# code size that matters on flash-constrained MCUs. DEBUG/NDEBUG gate any
# assert()/debug-only code in your sources.
add_compile_options("$<$<CONFIG:Debug>:-Og>" "$<$<CONFIG:Debug>:-g3>"
                    "$<$<CONFIG:Release>:-Os>")
add_compile_definitions("$<$<CONFIG:Debug>:DEBUG>"
                        "$<$<CONFIG:Release>:NDEBUG>")

add_definitions(-DF_CPU=${F_CPU})
add_link_options(-mmcu=${MCU} -Wl,--gc-sections)

add_executable(${PROJECT_NAME} main.c)

# ---- AVR-specific: HEX generation + flashing -------------------------------
# Swap this section when retargeting to another bare-metal architecture — other
# families use different objcopy output formats and flashing tools (e.g.
# arm-none-eabi-objcopy + openocd/st-flash for ARM Cortex-M).
add_custom_command(
  OUTPUT ${HEX_FILE}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${HEX_OUTPUT_PATH}
  COMMAND avr-objcopy -O ihex -j .text -j .data $<TARGET_FILE:${PROJECT_NAME}>
          ${HEX_FILE}
  DEPENDS ${PROJECT_NAME}
  COMMENT "Generating HEX file at ${HEX_FILE}")
add_custom_target(hex ALL DEPENDS ${HEX_FILE})
add_custom_target(
  flash
  COMMAND avrdude -c ${PROGRAMMER} -p ${MCU} -U flash:w:${HEX_FILE}:i
  DEPENDS hex
  COMMENT "Flashing HEX file to ${MCU}...")
