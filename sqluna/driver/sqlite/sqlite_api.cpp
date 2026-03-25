#include "sqluna/driver/sqlite/sqlite_api.h"

#include <array>
#include <string>

#include "sqluna/utils/error/error.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace sqluna::driver::sqlite {

namespace {

void* load_library_handle() {
#if defined(_WIN32)
    constexpr std::array<const char*, 2> kCandidates = {"winsqlite3.dll", "sqlite3.dll"};
    for (const char* candidate : kCandidates) {
        if (HMODULE handle = LoadLibraryA(candidate)) {
            return handle;
        }
    }
#else
    constexpr std::array<const char*, 3> kCandidates = {"libsqlite3.so.0", "libsqlite3.so", "libsqlite3.dylib"};
    for (const char* candidate : kCandidates) {
        if (void* handle = dlopen(candidate, RTLD_NOW)) {
            return handle;
        }
    }
#endif
    throw utils::error::DriverError("unable to load a SQLite runtime library");
}

template <typename Fn>
Fn load_symbol(void* library_handle, const char* name) {
#if defined(_WIN32)
    auto* raw = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library_handle), name));
#else
    auto* raw = dlsym(library_handle, name);
#endif
    if (raw == nullptr) {
        throw utils::error::DriverError(std::string("missing SQLite symbol: ") + name);
    }
    return reinterpret_cast<Fn>(raw);
}

}  // namespace

SQLiteApi& SQLiteApi::instance() {
    static SQLiteApi api;
    return api;
}

SQLiteApi::SQLiteApi() : library_handle_(load_library_handle()) {
    open_v2 = load_symbol<OpenV2Fn>(library_handle_, "sqlite3_open_v2");
    close_v2 = load_symbol<CloseV2Fn>(library_handle_, "sqlite3_close_v2");
    prepare_v2 = load_symbol<PrepareV2Fn>(library_handle_, "sqlite3_prepare_v2");
    step = load_symbol<StepFn>(library_handle_, "sqlite3_step");
    finalize = load_symbol<FinalizeFn>(library_handle_, "sqlite3_finalize");
    bind_null = load_symbol<BindNullFn>(library_handle_, "sqlite3_bind_null");
    bind_int64 = load_symbol<BindInt64Fn>(library_handle_, "sqlite3_bind_int64");
    bind_double = load_symbol<BindDoubleFn>(library_handle_, "sqlite3_bind_double");
    bind_text = load_symbol<BindTextFn>(library_handle_, "sqlite3_bind_text");
    column_count = load_symbol<ColumnCountFn>(library_handle_, "sqlite3_column_count");
    column_name = load_symbol<ColumnNameFn>(library_handle_, "sqlite3_column_name");
    column_type = load_symbol<ColumnTypeFn>(library_handle_, "sqlite3_column_type");
    column_int64 = load_symbol<ColumnInt64Fn>(library_handle_, "sqlite3_column_int64");
    column_double = load_symbol<ColumnDoubleFn>(library_handle_, "sqlite3_column_double");
    column_text = load_symbol<ColumnTextFn>(library_handle_, "sqlite3_column_text");
    errmsg = load_symbol<ErrMsgFn>(library_handle_, "sqlite3_errmsg");
    busy_timeout = load_symbol<BusyTimeoutFn>(library_handle_, "sqlite3_busy_timeout");
    last_insert_rowid = load_symbol<LastInsertRowIdFn>(library_handle_, "sqlite3_last_insert_rowid");
}

}  // namespace sqluna::driver::sqlite
