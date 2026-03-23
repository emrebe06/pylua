#pragma once

#if defined(_WIN32)
#if defined(LUNARA_C_EXPORTS)
#define LUNARA_API __declspec(dllexport)
#else
#define LUNARA_API __declspec(dllimport)
#endif
#else
#define LUNARA_API
#endif

extern "C" {

LUNARA_API const char* lunara_run_file(const char* script_path, const char* backend, int* exit_code);
LUNARA_API const char* lunara_run_source(const char* source, const char* virtual_path, const char* backend, int* exit_code);
LUNARA_API void lunara_string_free(const char* value);

}

