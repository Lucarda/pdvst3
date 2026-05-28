
cmake_minimum_required (VERSION 3.25.0)

option(GITHUB-CI "If we are running the github builds" OFF)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")

##### begin sdk

# Specify the minimum version of the target platform
if(NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS deployment target")
endif()

# Global options which can bet set on command line e.g.: cmake -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF ...
option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" ON)
option(SMTG_ENABLE_VSTGUI_SUPPORT "Enable VSTGUI Support" ON)
option(SMTG_ENABLE_WAYLAND_SUPPORT "Enable Wayland Support (Linux only)" OFF)

# -------------------------------------------------------------------------------
# Includes
# -------------------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(SMTG_VST3_SDK)

# -------------------------------------------------------------------------------
# SDK Project
# -------------------------------------------------------------------------------
project(vstsdk
    VERSION 3.8.0
    DESCRIPTION "Steinberg VST 3 Software Development Kit"
    HOMEPAGE_URL "https://www.steinberg.net"
)

smtg_setup_platform_toolset()
smtg_setup_symbol_visibility()

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Set the location of the VST 3 SDK
set(SDK_ROOT "${ROOT}")
set(public_sdk_SOURCE_DIR ${SDK_ROOT}/public.sdk)
set(pluginterfaces_SOURCE_DIR ${SDK_ROOT}/pluginterfaces)

if(SMTG_ENABLE_VSTGUI_SUPPORT)
    set(VSTGUI_ENABLE_WAYLAND_SUPPORT ${SMTG_ENABLE_WAYLAND_SUPPORT})
    smtg_enable_vstgui_support(VSTGUI_SOURCE_DIR "${ROOT}/vstgui4")
endif()

include_directories(${ROOT} ${SDK_ROOT})

# -------------------------------------------------------------------------------
# From Here this is optional...

# -------------------------------------------------------------------------------
# CORE AUDIO SDK, AAX SDK
# -------------------------------------------------------------------------------
setupCoreAudioSupport()
setupAaxSupport()
include(SMTG_FindJack)

# -------------------------------------------------------------------------------
# Projects
# -------------------------------------------------------------------------------
set(SDK_IDE_LIBS_FOLDER FOLDER "Libraries")

#---Add base libraries---------------------------
smtg_create_lib_base_target()
smtg_create_pluginterfaces_target()
smtg_create_public_sdk_common_target()
smtg_create_public_sdk_target()
smtg_create_public_sdk_hosting_target()

set(VST_SDK TRUE) # used for pluginterfaces and public.sdk modules which provides only a subset of them for VST-SDK
add_subdirectory(public.sdk/source/vst/interappaudio)

# Add hosting examples, it includes the validator (must be done before any plug-ins to support running the validator after building)
set(SDK_IDE_HOSTING_EXAMPLES_FOLDER FOLDER "Hosting-Examples")
add_subdirectory(public.sdk/samples/vst-hosting)

# Add utilities
set(SDK_IDE_UTILITIES_FOLDER FOLDER "Utilities")
add_subdirectory(public.sdk/samples/vst-utilities)

# -------------------------------------------------------------------------------
# IDE sorting
# -------------------------------------------------------------------------------
include(SMTG_CustomModuleTarget)

set_property(
    TARGET
        sdk
        sdk_common
        sdk_hosting
        base
        pluginterfaces
        cmake_modules
        cmake_VST_modules
    PROPERTY 
        ${SDK_IDE_LIBS_FOLDER}
)


add_compile_definitions(VSTGUI_ENABLE_XML_PARSER=1)

if(SMTG_ENABLE_VSTGUI_SUPPORT)
    set_property(
        TARGET
            vstgui
            vstgui_support
            vstgui_uidescription
        PROPERTY 
            ${SDK_IDE_LIBS_FOLDER}
    )
 
    if(TARGET vstgui_standalone)
        set_target_properties(vstgui_standalone
            PROPERTIES
                ${SDK_IDE_LIBS_FOLDER}
        )
    endif()
endif()



##### end sdk

#------pdvst3------------------------------------------

# remove files from build. we replace them in our sources.
file(READ "cmake/modules/SMTG_VST3_SDK.cmake" FILE_CONTENTS)
string(REPLACE "\${SDK_ROOT}/public.sdk/source/main/pluginfactory.cpp" "" FILE_CONTENTS ${FILE_CONTENTS})
string(REPLACE "\${SDK_ROOT}/public.sdk/source/main/pluginfactory.h" "" FILE_CONTENTS ${FILE_CONTENTS})
file(WRITE "cmake/modules/SMTG_VST3_SDK.cmake" ${FILE_CONTENTS})

project(pdvst3
    VERSION 1.0.0.0
    DESCRIPTION "pdvst3 VST 3 Plug-in"
)

smtg_add_vst3plugin(pdvst3
    source/pdvst3processor.h
    source/pdvst3processor.cpp
    source/pdvst3controller.h
    source/pdvst3controller.cpp
    source/pdvst3parsefile.cpp
    source/pdvst3pluginfactory.cpp
    source/pdvst3pluginfactory.h
    source/pdvst3cids.h
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(pdvst3
        PRIVATE
            sdk
            vstgui_support
            -lpthread
            -lc
            -lrt
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    target_link_libraries(pdvst3
        PRIVATE
            sdk
            vstgui_support
            -lpthread
            -lc
    )
endif()

if(SMTG_WIN)
    target_link_libraries(pdvst3
        PRIVATE
            sdk
            vstgui_support
    )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND GITHUB-CI)
    add_compile_definitions(-D_FORTIFY_SOURCE=0)
endif()

add_dependencies(pdvst3 pdvst3scheduler)

#set folders variables
set(FOLDER_MAIN ${CMAKE_CURRENT_BINARY_DIR}/VST3/${CMAKE_BUILD_TYPE}/pdvst3.vst3)
set(FOLDER_RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/VST3/${CMAKE_BUILD_TYPE}/pdvst3.vst3/Contents/Resources)

# copy files before linking
add_custom_command(
    TARGET pdvst3
    PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/source/config.txt
    ${FOLDER_MAIN}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/source/Pd_example.pd
    ${FOLDER_MAIN}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/README.md
    ${FOLDER_MAIN}
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
add_custom_command(
    TARGET pdvst3
    PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E rename
        ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/libpdvst3scheduler.so
        ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/pdvst3scheduler.pd_linux
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/pdvst3scheduler.pd_linux
        ${FOLDER_RESOURCES}
)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
add_custom_command(
    TARGET pdvst3
    PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E rename
        ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/libpdvst3scheduler.dylib
        ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/pdvst3scheduler.pd_darwin
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}/pdvst3scheduler.pd_darwin
        ${FOLDER_RESOURCES}
)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND GITHUB-CI)
add_custom_command(
    TARGET pdvst3
    PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/bin/Release/pdvst3scheduler.dll
        ${FOLDER_RESOURCES}
)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT GITHUB-CI)
add_custom_command(
    TARGET pdvst3
    PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/bin/pdvst3scheduler.dll
        ${FOLDER_RESOURCES}
)
endif()

# remove Windows folder icon to make clear it's a folder
if(SMTG_WIN)
add_custom_command(
    TARGET pdvst3
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove
            ${FOLDER_MAIN}/PlugIn.ico
    COMMAND ${CMAKE_COMMAND} -E remove
            ${FOLDER_MAIN}/desktop.ini
)
endif()

smtg_target_configure_version_file(pdvst3)

if(SMTG_MAC)
    smtg_target_set_bundle(pdvst3
        BUNDLE_IDENTIFIER net.steinberg.hello-world
        COMPANY_NAME "Steinberg Media Technologies GmbH"
    )
    smtg_target_set_debug_executable(pdvst3
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(pdvst3 PRIVATE
        resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT pdvst3)

        smtg_target_set_debug_executable(pdvst3
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
endif(SMTG_MAC)

# scheduler
add_subdirectory(source/scheduler)

# print variables
if(0)
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()
endif()

if(0)
    smtg_target_dump_plugin_package_variables(pdvst3)
endif()

