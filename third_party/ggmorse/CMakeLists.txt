# STATIC (not OBJECT): tests link LongpathObjs, and an OBJECT library
# does not carry a second OBJECT library's objects along transitively.
# ggmorse vendored at commit 8fb433d6cd6a71940f51b5724663ec0c75bf0b62
# Upstream: https://github.com/ggerganov/ggmorse
# License: MIT (see LICENSE in this directory)
#
# Only the library (include/ggmorse/ggmorse.h + src/*) is vendored, byte
# for byte; the upstream CMake, examples and tests are not. Consumed by
# src/core/CwDecoder.{h,cpp}. See README-LONGPATH.md and
# docs/attribution/GGMORSE-PROVENANCE.md.

add_library(ggmorse STATIC
    src/ggmorse.cpp
    src/resampler.cpp
)
target_include_directories(ggmorse
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(ggmorse PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Upstream code, upstream warnings: keep them out of the Longpath build log.
if(MSVC)
    target_compile_options(ggmorse PRIVATE /w)
else()
    target_compile_options(ggmorse PRIVATE -w)
endif()
