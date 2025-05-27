
####################



cmake_minimum_required (VERSION 3.25.0)

# Specify the minimum version of the target platform
if(NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS deployment target")
endif()

# Global options which can bet set on command line e.g.: cmake -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF ...
option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)
option(SMTG_ENABLE_VSTGUI_SUPPORT "Enable VSTGUI Support" OFF)
option(SMTG_CREATE_MODULE_INFO "Create the moduleinfo.json file" OFF)



#-------------------------------------------------------------------------------
# Includes
#-------------------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(SMTG_VST3_SDK)

#-------------------------------------------------------------------------------
# SDK Project
#-------------------------------------------------------------------------------


project(vstsdk
    VERSION 3.7.13
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
    smtg_enable_vstgui_support(VSTGUI_SOURCE_DIR "${ROOT}/vstgui4")
endif()

include_directories(${ROOT} ${SDK_ROOT})

#-------------------------------------------------------------------------------
# Projects
#-------------------------------------------------------------------------------
set(SDK_IDE_LIBS_FOLDER FOLDER "Libraries")

#---Add base libraries---------------------------
set(VST_SDK TRUE) # used for pluginterfaces and public.sdk modules which provides only a subset of them for VST-SDK
add_subdirectory(pluginterfaces)
add_subdirectory(base)
add_subdirectory(public.sdk)
add_subdirectory(public.sdk/source/vst/interappaudio)


# Add hosting examples, it includes the validator (must be done before any plug-ins to support running the validator after building)
set(SDK_IDE_HOSTING_EXAMPLES_FOLDER FOLDER "Hosting-Examples")
add_subdirectory(public.sdk/samples/vst-hosting)

# Add utilities
set(SDK_IDE_UTILITIES_FOLDER FOLDER "Utilities")
add_subdirectory(public.sdk/samples/vst-utilities)



######################

# remove files from build. we replace them in our sources.

file(READ "public.sdk/CMakeLists.txt" FILE_CONTENTS)
string(REPLACE "source/main/pluginfactory.cpp" "" FILE_CONTENTS ${FILE_CONTENTS})
string(REPLACE "source/main/pluginfactory.h" "" FILE_CONTENTS ${FILE_CONTENTS})
file(WRITE "public.sdk/CMakeLists.txt" ${FILE_CONTENTS})




project(pdvst3
    VERSION 1.0.0.0
    DESCRIPTION "pdvst3 VST 3 Plug-in"
)


smtg_add_vst3plugin(pdvst3
    #source/version.h
    #source/pdvst3cids.h
    source/pdvst3processor.h
    source/pdvst3processor.cpp
    source/pdvst3controller.h
    source/pdvst3controller.cpp
    #source/pdvst3entry.cpp
    #resource/pdvst3editor.uidesc
    source/pdvst3parsefile.cpp
    source/pdvst3pluginfactory.cpp
	source/pdvst3pluginfactory.h
)

target_link_libraries(pdvst3
    PRIVATE
        sdk
)



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

