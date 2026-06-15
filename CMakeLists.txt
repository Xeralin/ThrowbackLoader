cmake_minimum_required(VERSION 3.20)
project(ThrowbackLoader LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR
        "This build cross-compiles to Windows. Configure with:\n"
        "  -DCMAKE_TOOLCHAIN_FILE=toolchain-x86_64-w64-mingw32.cmake")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_link_options(-static -static-libgcc -static-libstdc++)
add_link_options($<$<NOT:$<CONFIG:Debug>>:-s>)
add_link_options(-Wl,--no-insert-timestamp)

add_compile_definitions(_WINDOWS _USRDLL)

set(TBL_DEPS_INC "${CMAKE_BINARY_DIR}/third_party")
function(tbl_fetch_header url dest)
    if(NOT EXISTS "${dest}")
        file(DOWNLOAD "${url}" "${dest}" TLS_VERIFY ON STATUS _st)
        list(GET _st 0 _code)
        if(NOT _code EQUAL 0)
            list(GET _st 1 _msg)
            file(REMOVE "${dest}")
            message(FATAL_ERROR "Download failed (${url}): ${_msg}")
        endif()
    endif()
endfunction()
tbl_fetch_header(
    "https://raw.githubusercontent.com/marzer/tomlplusplus/v3.4.0/toml.hpp"
    "${TBL_DEPS_INC}/toml++/toml.hpp")

add_library(tbl_core STATIC
    Core/Config.cpp
    Core/Core.cpp
    Core/Logger.cpp
    Core/Utils.cpp)
target_include_directories(tbl_core PUBLIC Core "${TBL_DEPS_INC}")

function(tbl_add_dll target out_name)
    add_library(${target} SHARED ${ARGN})
    target_link_libraries(${target} PRIVATE tbl_core)
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${out_name}"
        SUFFIX ".dll")
endfunction()

tbl_add_dll(uplay_r1 uplay_r1_loader64
    UplayR1/DllMain.cpp
    UplayR1/uplay/Ach.cpp
    UplayR1/uplay/Avatar.cpp
    UplayR1/uplay/Chat.cpp
    UplayR1/uplay/Friends.cpp
    UplayR1/uplay/Installer.cpp
    UplayR1/uplay/Options.cpp
    UplayR1/uplay/Overlay.cpp
    UplayR1/uplay/Party.cpp
    UplayR1/uplay/Presence.cpp
    UplayR1/uplay/Save.cpp
    UplayR1/uplay/Store.cpp
    UplayR1/uplay/Uplay.cpp
    UplayR1/uplay/User.cpp
    UplayR1/uplay/Win.cpp)
target_compile_definitions(uplay_r1 PRIVATE UPLAYR1_EXPORTS)
add_custom_command(TARGET uplay_r1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:uplay_r1>"
        "$<TARGET_FILE_DIR:uplay_r1>/upc_r1_loader64.dll")

tbl_add_dll(uplay_r2 upc_r2_loader64
    UplayR2/DllMain.cpp
    UplayR2/uplay/Achievement.cpp
    UplayR2/uplay/Friend.cpp
    UplayR2/uplay/InstallChunks.cpp
    UplayR2/uplay/Multiplayer.cpp
    UplayR2/uplay/Overlay.cpp
    UplayR2/uplay/Product.cpp
    UplayR2/uplay/StorageFile.cpp
    UplayR2/uplay/Store.cpp
    UplayR2/uplay/Uplay.cpp)
target_compile_definitions(uplay_r2 PRIVATE UPLAYR2_EXPORTS)

tbl_add_dll(defaultargs defaultargs
    DefaultArgs/DllMain.cpp
    DefaultArgs/exports.def)
target_compile_definitions(defaultargs PRIVATE DEFAULTARGS_EXPORTS)

add_executable(launch_r6s WIN32 Launcher/LaunchR6S.cpp)
set_target_properties(launch_r6s PROPERTIES OUTPUT_NAME "LaunchR6S")
target_include_directories(launch_r6s PRIVATE Core "${TBL_DEPS_INC}")
target_link_libraries(launch_r6s PRIVATE user32 shell32)
