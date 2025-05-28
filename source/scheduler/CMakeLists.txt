cmake_minimum_required (VERSION 3.25.0)

project(pdvst3scheduler)



# Specify the DLL source files
add_library(pdvst3scheduler SHARED
	vstschedlib.c
	cross-helper.c
)

target_include_directories(pdvst3scheduler PUBLIC 
	pd-sources 
	../
)

set(CMAKE_C_FLAGS "-DPD_WATCHDOG=1")

target_link_libraries(pdvst3scheduler PUBLIC
	-lpthread 
	-lc 
	-lrt
)


# set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
