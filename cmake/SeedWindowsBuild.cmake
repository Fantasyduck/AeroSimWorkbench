# Reuse dependency settings without copying the previous build's source binding.
set(seed_cache "$ENV{AEROSIM_WINDOWS_SEED_CACHE}")
if(NOT EXISTS "${seed_cache}")
    return()
endif()

file(STRINGS "${seed_cache}" cache_entries
    REGEX "^(AEROSIM_|Qt5|Qwt|VTK|SARibbon|SARIBBON|Eigen3|EIGEN3|CMAKE_BUILD_TYPE|CMAKE_PREFIX_PATH)")
foreach(entry IN LISTS cache_entries)
    if(entry MATCHES "^([^:]+):(BOOL|PATH|FILEPATH|STRING|UNINITIALIZED)=(.*)$")
        set(cache_name "${CMAKE_MATCH_1}")
        set(cache_type "${CMAKE_MATCH_2}")
        set(cache_value "${CMAKE_MATCH_3}")
        if(cache_value STREQUAL "" OR cache_value MATCHES "-NOTFOUND$")
            continue()
        endif()
        if(cache_type STREQUAL "UNINITIALIZED")
            set(cache_type STRING)
        endif()
        if(NOT DEFINED ${cache_name} OR "${${cache_name}}" STREQUAL ""
           OR "${${cache_name}}" MATCHES "-NOTFOUND$")
            set("${cache_name}" "${cache_value}" CACHE "${cache_type}"
                "Setting from the previous Windows build" FORCE)
        endif()
    endif()
endforeach()
