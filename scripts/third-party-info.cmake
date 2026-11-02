# Usage: cmake -DNAME=qt6 [-DVERSIONS=path] -P scripts/third-party-info.cmake
if(NOT DEFINED NAME)
    message(FATAL_ERROR "NAME is required")
endif()

if(DEFINED VERSIONS)
    include("${VERSIONS}")
else()
    include("${CMAKE_CURRENT_LIST_DIR}/../third_party/versions.cmake")
endif()

string(TOUPPER "${NAME}" _name)
foreach(_field VERSION URL SHA256 PATCHES)
    if(DEFINED OYAMA_${_name}_${_field})
        message("${_field}=${OYAMA_${_name}_${_field}}")
    endif()
endforeach()
