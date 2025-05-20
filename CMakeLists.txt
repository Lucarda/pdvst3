cmake_minimum_required(VERSION 3.14.0)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

project(pdvst3
    VERSION 1.0.0.0
    DESCRIPTION "pdvst3 VST 3 Plug-in"
)

add_subdirectory(external)

smtg_enable_vst3_sdk()

smtg_add_vst3plugin(pdvst3
    source/version.h
    source/pdvst3cids.h
    source/pdvst3processor.h
    source/pdvst3processor.cpp
    source/pdvst3controller.h
    source/pdvst3controller.cpp
    source/pdvst3entry.cpp
    resource/pdvst3editor.uidesc
    source/parsefile.cpp
)

target_link_libraries(pdvst3
    PRIVATE
        sdk
		-ldl
		-lc
)

smtg_target_add_plugin_resources(pdvst3
    RESOURCES

)

smtg_target_add_plugin_snapshots (pdvst3
    RESOURCES

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
