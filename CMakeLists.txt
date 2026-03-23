cmake_minimum_required(VERSION 3.20)
project(pylua_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(pylua_core
    src/engine.cpp
    src/interpreter.cpp
    src/lexer.cpp
    src/parser.cpp
    src/vm.cpp
)

target_include_directories(pylua_core PUBLIC include)

add_executable(pylua
    src/main.cpp
)

target_link_libraries(pylua PRIVATE pylua_core)

add_library(pylua_shared SHARED
    src/pylua_c.cpp
)

target_compile_definitions(pylua_shared PRIVATE PYLUA_C_EXPORTS)
target_link_libraries(pylua_shared PRIVATE pylua_core)
set_target_properties(pylua_shared PROPERTIES OUTPUT_NAME pylua_embed)

install(TARGETS pylua pylua_shared
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)

install(FILES
    README.md
    PYLUA_GUIDE.md
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
    include/pylua/pylua_c.h
    DESTINATION include/pylua
)

if(MSVC)
    target_compile_definitions(pylua_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(pylua PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(pylua_shared PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(pylua_core PRIVATE /W4 /permissive-)
    target_compile_options(pylua PRIVATE /W4 /permissive-)
    target_compile_options(pylua_shared PRIVATE /W4 /permissive-)
else()
    target_compile_options(pylua_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pylua PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pylua_shared PRIVATE -Wall -Wextra -Wpedantic)
endif()
