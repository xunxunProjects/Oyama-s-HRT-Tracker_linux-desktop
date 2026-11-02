set(OYAMA_THIRD_PARTY "system" CACHE STRING
    "Third-party source: system or vendored")
set_property(CACHE OYAMA_THIRD_PARTY PROPERTY STRINGS system vendored)

if(NOT OYAMA_THIRD_PARTY MATCHES "^(system|vendored)$")
    message(FATAL_ERROR "OYAMA_THIRD_PARTY must be system or vendored")
endif()

if(OYAMA_THIRD_PARTY STREQUAL "vendored")
    set(_oyama_prefix "${CMAKE_SOURCE_DIR}/third_party/prefix")
    if(NOT EXISTS "${_oyama_prefix}/.stamp")
        message(FATAL_ERROR
            "Vendored third-party prefix not found at ${_oyama_prefix}. "
            "Run scripts/bootstrap-third-party.sh first, or configure with "
            "-DOYAMA_THIRD_PARTY=system.")
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${_oyama_prefix}")
    set(ENV{PKG_CONFIG_PATH}
        "${_oyama_prefix}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    message(STATUS "Third-party source: vendored (${_oyama_prefix})")
else()
    message(STATUS "Third-party source: system")
endif()
