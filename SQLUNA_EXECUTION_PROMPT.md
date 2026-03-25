# SQLUna Execution Prompt

Before coding, create a detailed execution plan.
Break the system into modules and explain responsibilities.
Do NOT write code yet.

Then use this implementation brief:

- Build SQLUna as a native C++ ORM engine for Lunara.
- Target a production-grade architecture, not a toy example.
- Follow this modular structure:
  - `sqluna/core/connection`
  - `sqluna/core/pool`
  - `sqluna/core/config`
  - `sqluna/query/builder`
  - `sqluna/query/ast`
  - `sqluna/query/compiler`
  - `sqluna/orm/model`
  - `sqluna/orm/mapper`
  - `sqluna/orm/schema`
  - `sqluna/security/sanitizer`
  - `sqluna/security/encryption`
  - `sqluna/driver/sqlite`
  - `sqluna/utils/logger`
  - `sqluna/utils/error`
- Required features:
  - connection pooling
  - config struct
  - SQLite driver as first target
  - AST-based query builder
  - ORM mapper for C++ structs
  - prepared statements only
  - minimal migrations (`create table`, `add column`)
  - field-level encryption interface
- Performance priorities:
  - correctness first
  - avoid unnecessary heap allocations
  - use move semantics
  - reduce string copies where possible
  - leave room for a future lock-free pool
- Constraints:
  - no external ORM libraries
  - no heavy dependencies
  - no fake placeholder code
  - do not skip core components
- Done when all of these work:
  - create SQLite connection
  - create table via migration
  - insert data
  - query data through builder
  - map result into a struct
- Build target:
  - C++17 or newer
  - simple compile path
