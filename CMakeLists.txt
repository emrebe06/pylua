cmake_minimum_required(VERSION 3.20)
project(lunara_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(lunara_core
    src/analyzer.cpp
    src/engine.cpp
    src/interpreter.cpp
    src/lexer.cpp
    src/parser.cpp
    src/vm.cpp
)

target_include_directories(lunara_core PUBLIC include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lunara_core PUBLIC sqluna_core)

add_executable(lunara
    src/main.cpp
)

target_link_libraries(lunara PRIVATE lunara_core)

add_library(sqluna_core
    sqluna/core/config/config.cpp
    sqluna/core/connection/connection.cpp
    sqluna/core/pool/connection_pool.cpp
    sqluna/query/compiler/sql_compiler.cpp
    sqluna/query/builder/query_builder.cpp
    sqluna/orm/schema/migration.cpp
    sqluna/security/sanitizer/input_sanitizer.cpp
    sqluna/security/encryption/field_encryption.cpp
    sqluna/driver/sqlite/sqlite_api.cpp
    sqluna/driver/sqlite/sqlite_connection.cpp
    sqluna/utils/logger/logger.cpp
)

target_include_directories(sqluna_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sqluna_demo
    sqluna/demo/main.cpp
)

target_link_libraries(sqluna_demo PRIVATE sqluna_core)

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
    target_link_libraries(sqluna_core PUBLIC ${CMAKE_DL_LIBS})
    target_compile_options(sqluna_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(sqluna_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MSVC)
    target_compile_definitions(sqluna_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(sqluna_demo PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(sqluna_core PRIVATE /W4 /permissive-)
    target_compile_options(sqluna_demo PRIVATE /W4 /permissive-)
endif()

