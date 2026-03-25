#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "sqluna/core/connection/connection.h"

namespace sqluna::core {

class ConnectionPool;

class PooledConnection {
  public:
    PooledConnection() = default;
    PooledConnection(ConnectionPool* pool, std::shared_ptr<Connection> connection);
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;
    PooledConnection(PooledConnection&& other) noexcept;
    PooledConnection& operator=(PooledConnection&& other) noexcept;
    ~PooledConnection();

    Connection& get() const;
    Connection* operator->() const;
    explicit operator bool() const;

  private:
    void release();

    ConnectionPool* pool_ = nullptr;
    std::shared_ptr<Connection> connection_;
};

class ConnectionPool {
  public:
    using Factory = std::function<std::shared_ptr<Connection>()>;

    ConnectionPool(std::size_t max_size, Factory factory);

    PooledConnection acquire();

  private:
    void release(std::shared_ptr<Connection> connection);

    friend class PooledConnection;

    std::size_t max_size_;
    std::size_t created_ = 0;
    Factory factory_;
    std::vector<std::shared_ptr<Connection>> available_;
    std::mutex mutex_;
};

}  // namespace sqluna::core
