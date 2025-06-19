# pdvst3

### compiling

linux:

`cmake ../ -DCMAKE_BUILD_TYPE:STRING=release -DSMTG_CREATE_MODULE_INFO=off`

`make`

win:

`cmake ../ -DCMAKE_BUILD_TYPE:STRING=release -DSMTG_CREATE_MODULE_INFO=off -DSMTG_USE_STATIC_CRT:BOOL=ON`

`cmake --build .`

mac:

`cmake -DCMAKE_BUILD_TYPE:STRING=release -DSMTG_CREATE_MODULE_INFO=off -DSMTG_DISABLE_CODE_SIGNING=on ../`

`cmake --build . --config Release`



