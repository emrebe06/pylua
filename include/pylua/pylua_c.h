#pragma once

#if defined(_WIN32)
#if defined(PYLUA_C_EXPORTS)
#define PYLUA_API __declspec(dllexport)
#else
#define PYLUA_API __declspec(dllimport)
#endif
#else
#define PYLUA_API
#endif

extern "C" {

PYLUA_API const char* pylua_run_file(const char* script_path, const char* backend, int* exit_code);
PYLUA_API const char* pylua_run_source(const char* source, const char* virtual_path, const char* backend, int* exit_code);
PYLUA_API void pylua_string_free(const char* value);

}
