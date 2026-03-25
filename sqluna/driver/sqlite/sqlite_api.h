#pragma once

#include <cstdint>

namespace sqluna::driver::sqlite {

struct sqlite3;
struct sqlite3_stmt;
using sqlite3_destructor_type = void (*)(void*);

constexpr int kSqliteOk = 0;
constexpr int kSqliteRow = 100;
constexpr int kSqliteDone = 101;
constexpr int kSqliteInteger = 1;
constexpr int kSqliteFloat = 2;
constexpr int kSqliteText = 3;
constexpr int kSqliteBlob = 4;
constexpr int kSqliteNull = 5;
constexpr int kSqliteOpenReadWrite = 0x00000002;
constexpr int kSqliteOpenCreate = 0x00000004;
constexpr int kSqliteOpenFullMutex = 0x00010000;

class SQLiteApi {
  public:
    using OpenV2Fn = int (*)(const char*, sqlite3**, int, const char*);
    using CloseV2Fn = int (*)(sqlite3*);
    using PrepareV2Fn = int (*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
    using StepFn = int (*)(sqlite3_stmt*);
    using FinalizeFn = int (*)(sqlite3_stmt*);
    using BindNullFn = int (*)(sqlite3_stmt*, int);
    using BindInt64Fn = int (*)(sqlite3_stmt*, int, std::int64_t);
    using BindDoubleFn = int (*)(sqlite3_stmt*, int, double);
    using BindTextFn = int (*)(sqlite3_stmt*, int, const char*, int, sqlite3_destructor_type);
    using ColumnCountFn = int (*)(sqlite3_stmt*);
    using ColumnNameFn = const char* (*)(sqlite3_stmt*, int);
    using ColumnTypeFn = int (*)(sqlite3_stmt*, int);
    using ColumnInt64Fn = std::int64_t (*)(sqlite3_stmt*, int);
    using ColumnDoubleFn = double (*)(sqlite3_stmt*, int);
    using ColumnTextFn = const unsigned char* (*)(sqlite3_stmt*, int);
    using ErrMsgFn = const char* (*)(sqlite3*);
    using BusyTimeoutFn = int (*)(sqlite3*, int);
    using LastInsertRowIdFn = std::int64_t (*)(sqlite3*);

    static SQLiteApi& instance();

    OpenV2Fn open_v2 = nullptr;
    CloseV2Fn close_v2 = nullptr;
    PrepareV2Fn prepare_v2 = nullptr;
    StepFn step = nullptr;
    FinalizeFn finalize = nullptr;
    BindNullFn bind_null = nullptr;
    BindInt64Fn bind_int64 = nullptr;
    BindDoubleFn bind_double = nullptr;
    BindTextFn bind_text = nullptr;
    ColumnCountFn column_count = nullptr;
    ColumnNameFn column_name = nullptr;
    ColumnTypeFn column_type = nullptr;
    ColumnInt64Fn column_int64 = nullptr;
    ColumnDoubleFn column_double = nullptr;
    ColumnTextFn column_text = nullptr;
    ErrMsgFn errmsg = nullptr;
    BusyTimeoutFn busy_timeout = nullptr;
    LastInsertRowIdFn last_insert_rowid = nullptr;

  private:
    SQLiteApi();

    void* library_handle_ = nullptr;
};

}  // namespace sqluna::driver::sqlite
