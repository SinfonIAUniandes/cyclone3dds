set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR armv6k)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT DEFINED ENV{DEVKITPRO} OR NOT DEFINED ENV{DEVKITARM})
  message(FATAL_ERROR "DEVKITPRO and DEVKITARM must be set for the Nintendo 3DS toolchain")
endif()

set(DEVKITPRO "$ENV{DEVKITPRO}")
set(DEVKITARM "$ENV{DEVKITARM}")
set(CMAKE_C_COMPILER "${DEVKITARM}/bin/arm-none-eabi-gcc")
set(CMAKE_AR "${DEVKITARM}/bin/arm-none-eabi-ar")
set(CMAKE_RANLIB "${DEVKITARM}/bin/arm-none-eabi-ranlib")

set(NINTENDO_3DS_ARCH_FLAGS "-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations")
set(CMAKE_C_FLAGS_INIT "${NINTENDO_3DS_ARCH_FLAGS} -D__3DS__ -I${DEVKITPRO}/libctru/include")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${NINTENDO_3DS_ARCH_FLAGS} -specs=3dsx.specs -L${DEVKITPRO}/libctru/lib -lctru")

set(CMAKE_FIND_ROOT_PATH "${DEVKITPRO}/libctru")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
