# pdvst3

## compiling

base dependencies: cmake, git, c++ compiler (MSVC on Windows)

### dependencies for Linux:

    sudo apt-get install cmake gcc "libstdc++6" libx11-xcb-dev libxcb-util-dev libxcb-cursor-dev libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libfontconfig1-dev libcairo2-dev libgtkmm-3.0-dev libsqlite3-dev libxcb-keysyms1-dev

clone repository:

    git clone <repo>

    cd to <repo>

init submodules:

    git submodule update --init --recursive

make build folder and cd to it:

    mkdir build && cd build


linux:

    cmake ../ -DCMAKE_BUILD_TYPE:STRING=release -DSMTG_CREATE_MODULE_INFO=off

    make

win:

    cmake ../ -DCMAKE_BUILD_TYPE:STRING=release -DSMTG_CREATE_MODULE_INFO=off -DSMTG_USE_STATIC_CRT:BOOL=ON

    cmake --build .

mac:

    cmake -DCMAKE_BUILD_TYPE:STRING=release -DSMTG_CREATE_MODULE_INFO=off -DSMTG_DISABLE_CODE_SIGNING=on ../

    cmake --build . --config Release


### vst3 validator

on normal builds the vst3 validator is runned. for this to succeed you must
have Pure Data available and correctly declared in source/config.txt. if for
whatever reason you need to just build the plugin without running its test
add this command at the end of the cmake command:

    -DSMTG_RUN_VST_VALIDATOR=off
