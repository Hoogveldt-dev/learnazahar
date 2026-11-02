add_executable(tests
    common/bit_field.cpp
    common/file_util.cpp
    common/param_package.cpp
    core/core_timing.cpp
    core/file_sys/path_parser.cpp
    core/hle/kernel/hle_ipc.cpp
    core/memory/memory.cpp
    core/memory/vm_manager.cpp
    precompiled_headers.h
    audio_core/hle/hle.cpp
    audio_core/hle/source.cpp
    audio_core/lle/lle.cpp
    audio_core/audio_fixures.h
    audio_core/decoder_tests.cpp
    video_core/pica_types.cpp
    video_core/shader.cpp
    audio_core/merryhime_3ds_audio/merry_audio/merry_audio.cpp
    audio_core/merryhime_3ds_audio/merry_audio/merry_audio.h
    audio_core/merryhime_3ds_audio/merry_audio/service_fixture.cpp
    audio_core/merryhime_3ds_audio/merry_audio/service_fixture.h
    audio_core/merryhime_3ds_audio/audio_test_biquad_filter.cpp
    core/woofer/bridge.cpp
)

# Woofer bridge (docs/woofer-bridge/PROTOCOL.md). The end-to-end test uses the game side
# of PokemonWoofer (core/woofer/game, a verbatim copy of its port/woofer_bridge.c and .h),
# built as a C library of its own (no C++ precompiled headers).
if (NOT WIN32)
    add_library(woofer_game_bridge STATIC EXCLUDE_FROM_ALL
        core/woofer/game/woofer_bridge.c
        core/woofer/game/woofer_bridge.h
    )
    target_include_directories(woofer_game_bridge PUBLIC core/woofer/game)
    target_sources(tests PRIVATE core/woofer/game_bridge.cpp)
    target_link_libraries(tests PRIVATE woofer_game_bridge json-headers)
endif()

# woofer_tests: the same Woofer tests without the emulator core, quick to build
# (cmake --build . --target woofer_tests). Used by .github/workflows/woofer.yml.
add_executable(woofer_tests EXCLUDE_FROM_ALL
    core/woofer/bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/core/woofer/bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/core/woofer/bridge.h
    ${CMAKE_SOURCE_DIR}/src/core/woofer/vocab.cpp
    ${CMAKE_SOURCE_DIR}/src/core/woofer/vocab.h
)
if (NOT WIN32)
    target_sources(woofer_tests PRIVATE core/woofer/game_bridge.cpp)
    target_link_libraries(woofer_tests PRIVATE woofer_game_bridge)
endif()
target_include_directories(woofer_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(woofer_tests PRIVATE citra_common catch2 json-headers Threads::Threads)
add_test(NAME woofer_tests COMMAND woofer_tests)

create_target_directory_groups(tests)

if (BSD STREQUAL "NetBSD")
    include(DisablePaxMprotect)
    disable_pax_mprotect(tests)
endif()

target_link_libraries(tests PRIVATE citra_common citra_core video_core audio_core)
target_link_libraries(tests PRIVATE ${PLATFORM_LIBRARIES} catch2 nihstro-headers Threads::Threads)

if (ENABLE_LIBRETRO)
    target_link_libraries(tests PRIVATE $<TARGET_OBJECTS:citra_libretro_common>)
endif()

add_test(NAME tests COMMAND tests)
if(NOT ANDROID AND (CMAKE_SYSTEM_NAME STREQUAL CMAKE_HOST_SYSTEM_NAME))
    catch_discover_tests(tests)
endif()


if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(tests PRIVATE precompiled_headers.h)
endif()

# Bundle in-place on MSVC so dependencies can be resolved by builds.
if (MSVC)
    include(BundleTarget)
    bundle_target_in_place(tests)
endif()
