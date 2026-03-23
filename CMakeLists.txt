cmake_minimum_required(VERSION 3.20)
project(lunara_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(lunara_core
    src/engine.cpp
    src/interpreter.cpp
    src/lexer.cpp
    src/parser.cpp
    src/vm.cpp
)

target_include_directories(lunara_core PUBLIC include)

add_executable(lunara
    src/main.cpp
)

target_link_libraries(lunara PRIVATE lunara_core)

add_library(lunara_shared SHARED
    src/lunara_c.cpp
)

target_compile_definitions(lunara_shared PRIVATE LUNARA_C_EXPORTS)
target_link_libraries(lunara_shared PRIVATE lunara_core)
set_target_properties(lunara_shared PROPERTIES OUTPUT_NAME lunara_embed)

install(TARGETS lunara lunara_shared
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)

install(FILES
    README.md
    LUNARA_GUIDE.md
    DESTINATION docs
)

install(DIRECTORY examples
    DESTINATION .
)

install(DIRECTORY python
    DESTINATION .
    PATTERN "__pycache__" EXCLUDE
)

install(DIRECTORY website
    DESTINATION .
)

install(FILES
    include/lunara/lunara_c.h
    DESTINATION include/lunara
)

if(MSVC)
    target_compile_definitions(lunara_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(lunara PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(lunara_shared PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(lunara_core PRIVATE /W4 /permissive-)
    target_compile_options(lunara PRIVATE /W4 /permissive-)
    target_compile_options(lunara_shared PRIVATE /W4 /permissive-)
else()
    target_compile_options(lunara_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(lunara PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(lunara_shared PRIVATE -Wall -Wextra -Wpedantic)
endif()

