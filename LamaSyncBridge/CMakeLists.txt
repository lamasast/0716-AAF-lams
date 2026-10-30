cmake_minimum_required(VERSION 3.21)
project(LamaSyncAudIO LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(LAMASYNC_BUILD_AUDIO "Build the AudIO engine and Windows serial bridge" ON)

include(CTest)

add_library(LamaSyncProtocol INTERFACE)
target_include_directories(LamaSyncProtocol INTERFACE src)

add_executable(LamaSyncProtocolTest tests/protocol_test.cpp)
target_link_libraries(LamaSyncProtocolTest PRIVATE LamaSyncProtocol)
add_test(NAME LamaSyncProtocolTest COMMAND LamaSyncProtocolTest)

if(LAMASYNC_BUILD_AUDIO)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../AudIO/CMakeLists.txt")
        message(FATAL_ERROR "Expected sibling ../AudIO from the IGNITE repository")
    endif()

    set(AUDIO_BUILD_EXAMPLES OFF CACHE BOOL "Skip original AudIO demos" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../AudIO" "${CMAKE_CURRENT_BINARY_DIR}/AudIO")

    add_executable(LamaSyncAudIO src/main.cpp)
    target_link_libraries(LamaSyncAudIO PRIVATE LamaSyncProtocol AudIO_Core)

    add_executable(LamaSyncAudioRenderTest tests/audio_render_test.cpp)
    target_link_libraries(LamaSyncAudioRenderTest PRIVATE AudIO_Core)
    add_test(
        NAME LamaSyncAudioRenderTest
        COMMAND LamaSyncAudioRenderTest
                --output "${CMAKE_CURRENT_BINARY_DIR}/audio-test/lamasync-220hz-48k.wav"
    )

    if(WIN32)
        set(LAMASYNC_WINDOWS_AUDIO_LIBS
            winmm dsound dxguid ole32 user32 advapi32 version imm32 setupapi
        )
        target_link_libraries(LamaSyncAudIO PRIVATE ${LAMASYNC_WINDOWS_AUDIO_LIBS})
        target_link_libraries(LamaSyncAudioRenderTest PRIVATE ${LAMASYNC_WINDOWS_AUDIO_LIBS})
    endif()

    if(MSVC)
        target_compile_options(LamaSyncAudIO PRIVATE /W4 /permissive-)
        target_compile_options(LamaSyncAudioRenderTest PRIVATE /W4 /permissive-)
    endif()
endif()

if(MSVC)
    target_compile_options(LamaSyncProtocolTest PRIVATE /W4 /permissive-)
endif()
