# This directory structure is being created by `scripts/install-deps.sh`
# and is used to inject all the dependencies the operating system's
# package manager did not provide (not found or too old version).

# {{{ helper: subproject_version(<subproject-name> <result-variable>)
#
# Extract version of a sub-project, which was previously included with add_subdirectory().
function(subproject_version subproject_name VERSION_VAR)
    # Read CMakeLists.txt for subproject and extract project() call(s) from it.
    file(STRINGS "${${subproject_name}_SOURCE_DIR}/CMakeLists.txt" project_calls REGEX "[ \t]*project\\(")
    # For every project() call try to extract its VERSION option
    foreach(project_call ${project_calls})
        string(REGEX MATCH "VERSION[ ]+\"?([^ \")]+)" version_param "${project_call}")
        if(version_param)
            set(version_value "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(version_value)
        set(${VERSION_VAR} "${version_value}" PARENT_SCOPE)
    else()
        message("WARNING: Cannot extract version for subproject '${subproject_name}'")
    endif()
endfunction(subproject_version)
# }}}

set(ContourThirdParties_SRCDIR ${PROJECT_SOURCE_DIR}/_deps/sources)
if(EXISTS "${ContourThirdParties_SRCDIR}/CMakeLists.txt")
    message(STATUS "Embedding 3rdparty libraries: ${ContourThirdParties_SRCDIR}")
    add_subdirectory(${ContourThirdParties_SRCDIR})
else()
    message(STATUS "No 3rdparty libraries found at ${ContourThirdParties_SRCDIR}")
endif()

macro(HandleThirdparty _TARGET _CPM_TARGET)
    message(STATUS "=== Handling ${_TARGET}")
    if(TARGET ${_TARGET})
        set(THIRDPARTY_BUILTIN_${_TARGET} "embedded")
    else()
        find_package(${_TARGET})

        if(${_TARGET}_FOUND)
            set(THIRDPARTY_BUILTIN_${_TARGET} "system package")
        else()
            message(STATUS "====== Using CPM to add ${_TARGET}")
            CPMAddPackage(${_CPM_TARGET})
            set(THIRDPARTY_BUILTIN_${_TARGET} "embedded (CPM)")
        endif()
    endif()
endmacro()


message(STATUS "==============================================================================")
message(STATUS "    Contour ThirdParties: ${ContourThirdParties}")

# Start including packages

set(LIBUNICODE_MINIMAL_VERSION "0.4.0")
set(BOXED_CPP_MINIMAL_VERSION "1.2.2")
set(TERMBENCH_PRO_COMMIT_HASH "96c6bb7897af4110d1b99a93b982f8ec10e71183")
set(FMT_VERSION "10.0.0")
set(CATCH_VERSION "3.4.0")
set(RANGE_V3_VERSION "0.12.0")
set(YAML_CPP_VERSION "0.8.0")
set(HARFBUZZ_VERSION "8.4.0")

HandleThirdparty(fmt "gh:fmtlib/fmt#${FMT_VERSION}")
HandleThirdparty(range-v3 "gh:ericniebler/range-v3#${RANGE_V3_VERSION}")
HandleThirdparty(yaml-cpp "gh:jbeder/yaml-cpp#${YAML_CPP_VERSION}")
HandleThirdparty(harfbuzz "gh:harfbuzz/harfbuzz#${HARFBUZZ_VERSION}")
HandleThirdparty(Freetype "https://download.savannah.gnu.org/releases/freetype/freetype-2.10.0.tar.gz")
HandleThirdparty(Catch2 "gh:catchorg/Catch2@${CATCH_VERSION}")
HandleThirdparty(libunicode "gh:contour-terminal/libunicode#v${LIBUNICODE_MINIMAL_VERSION}")
HandleThirdparty(termbench "gh:contour-terminal/termbench-pro#${TERMBENCH_PRO_COMMIT_HASH}")
HandleThirdparty(boxed_cpp "gh:contour-terminal/boxed-cpp#v${BOXED_CPP_MINIMAL_VERSION}")

set(BOXED_CPP_MINIMAL_VERSION "1.2.2")
subproject_version(boxed-cpp boxed_cpp_version)
if(NOT DEFINED boxed_cpp_version OR boxed_cpp_version VERSION_LESS BOXED_CPP_MINIMAL_VERSION)
    message(FATAL_ERROR "Embedded boxed-cpp version must be at least ${BOXED_CPP_MINIMAL_VERSION}, but found ${boxed_cpp_version}")
endif()

set(LIBUNICODE_MINIMAL_VERSION "0.4.0")
subproject_version(libunicode libunicode_version)
if(NOT DEFINED libunicode_version OR libunicode_version VERSION_LESS LIBUNICODE_MINIMAL_VERSION)
    message(FATAL_ERROR "Embedded libunicode version must be at least ${LIBUNICODE_MINIMAL_VERSION}, but found ${libunicode_version}")
endif()

if(TARGET GSL)
    set(THIRDPARTY_BUILTIN_GSL "embedded")
else()
    set(THIRDPARTY_BUILTIN_GSL "system package")
    if (WIN32)
        # On Windows we use vcpkg and there the name is different
        find_package(Microsoft.GSL CONFIG REQUIRED)
        #target_link_libraries(main PRIVATE Microsoft.GSL::GSL)
    else()
        find_package(Microsoft.GSL REQUIRED)
    endif()
endif()

macro(ContourThirdPartiesSummary2)
    message(STATUS "==============================================================================")
    message(STATUS "    Contour ThirdParties")
    message(STATUS "------------------------------------------------------------------------------")
    message(STATUS "Catch2              ${THIRDPARTY_BUILTIN_Catch2}")
    message(STATUS "GSL                 ${THIRDPARTY_BUILTIN_GSL}")
    message(STATUS "fmt                 ${THIRDPARTY_BUILTIN_fmt}")
    message(STATUS "freetype            ${THIRDPARTY_BUILTIN_Freetype}")
    message(STATUS "harfbuzz            ${THIRDPARTY_BUILTIN_harfbuzz}")
    message(STATUS "range-v3            ${THIRDPARTY_BUILTIN_range-v3}")
    message(STATUS "yaml-cpp            ${THIRDPARTY_BUILTIN_yaml-cpp}")
    message(STATUS "termbench-pro       ${THIRDPARTY_BUILTIN_termbench}")
    message(STATUS "libunicode          ${THIRDPARTY_BUILTIN_libunicode}")
    message(STATUS "boxed-cpp           ${THIRDPARTY_BUILTIN_boxed_cpp}")
    message(STATUS "------------------------------------------------------------------------------")
endmacro()
