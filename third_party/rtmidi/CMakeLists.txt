# RtMidi 6.0.0 (Gary P. Scavone, MIT-style licence -- see LICENSE in this
# directory), taken as carried by AetherSDR at d58e2b8a, i.e. upstream
# 6.0.0 plus AetherSDR's one fix 5769f480 ("Fix CoreMIDI init crash during
# MIDI auto-connect", #1949: the `throw()` specifier removed from
# getCoreMidiClientSingleton, and one error text corrected).
#
# Consumed by src/core/midi/MidiKeyInput.{h,cpp} (2026-11-17, HaliKey
# MIDI). See README-LONGPATH.md and docs/attribution/RTMIDI-PROVENANCE.md.
#
# STATIC (not OBJECT): tests link LongpathObjs, and an OBJECT library does
# not carry a second OBJECT library's objects along transitively -- the
# same reason as third_party/ggmorse.

add_library(rtmidi STATIC RtMidi.cpp)
target_include_directories(rtmidi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rtmidi PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The backend is chosen by a define, and it is PUBLIC: RtMidi.h's class
# layout is backend-conditional in places, so every includer must see the
# same one (as in AetherSDR's CMakeLists.txt).
if(APPLE)
    target_compile_definitions(rtmidi PUBLIC __MACOSX_CORE__)
    target_link_libraries(rtmidi PUBLIC
        "-framework CoreMIDI" "-framework CoreAudio"
        "-framework CoreFoundation" "-framework IOKit")
elseif(WIN32)
    target_compile_definitions(rtmidi PUBLIC __WINDOWS_MM__)
    target_link_libraries(rtmidi PUBLIC winmm)
else()
    target_compile_definitions(rtmidi PUBLIC __LINUX_ALSA__)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(rtmidi_alsa REQUIRED IMPORTED_TARGET alsa)
    target_link_libraries(rtmidi PUBLIC PkgConfig::rtmidi_alsa)
endif()

# Upstream code, upstream warnings: keep them out of the Longpath build log.
if(MSVC)
    target_compile_options(rtmidi PRIVATE /w)
else()
    target_compile_options(rtmidi PRIVATE -w)
endif()
