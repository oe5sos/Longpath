# SGP4/SDP4 reference propagator by David Vallado (Vallado, Crawford, Hujsak,
# Kelso, "Revisiting Spacetrack Report #3", AIAA 2006-6753), vendored from
# the python-sgp4 project at commit bf25b00ccf8cf0770a8e5ba458134156f1c70812
# (https://github.com/brandon-rhodes/python-sgp4, extension/SGP4.{cpp,h}).
# License: MIT (see LICENSE.txt in this directory); Vallado's own terms for
# the reference code are "provided freely, without warranty". Details in
# docs/attribution/SGP4-PROVENANCE.md.
#
# Files are verbatim (CRLF line endings preserved so the upstream hashes
# match). Longpath talks to it only through src/core/sat/SatelliteTracker.

add_library(sgp4 STATIC SGP4.cpp)
target_include_directories(sgp4 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(sgp4 PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(sgp4 PRIVATE
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-sign-compare
        -Wno-shadow
        -Wno-deprecated-declarations
        -Wno-format
    )
endif()
if(MSVC)
    target_compile_definitions(sgp4 PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
