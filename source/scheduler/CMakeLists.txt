cmake_minimum_required (VERSION 3.25.0)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")


project(pdvst3scheduler C)

# Specify the DLL source files
add_library(pdvst3scheduler SHARED
    vstschedlib.c
    cross-helper.c
)

target_include_directories(pdvst3scheduler PUBLIC
    pd-sources
    ../
)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_FLAGS "-DPD_WATCHDOG=1")
    target_link_libraries(pdvst3scheduler PUBLIC
        -lpthread
        -lc
        -lrt
)

elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_link_libraries(pdvst3scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pd-sources/pd.lib
)

elseif(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
	set(CMAKE_C_FLAGS "-DPD_WATCHDOG=1")
    target_link_libraries(pdvst3scheduler PRIVATE
        "-Wl,-undefined,suppress"
        -flat_namespace
        -lpthread
        -lc  
)

endif()
