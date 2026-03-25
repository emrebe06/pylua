#include "sqluna/core/pool/connection_pool.h"

#include "sqluna/utils/error/error.h"

namespace sqluna::core {

PooledConnection::PooledConnection(ConnectionPool* pool, std::shared_ptr<Connection> connection)
    : pool_(pool), connection_(std::move(connection)) {}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : pool_(other.pool_), connection_(std::move(other.connection_)) {
    other.pool_ = nullptr;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    release();
    pool_ = other.pool_;
    connection_ = std::move(other.connection_);
    other.pool_ = nullptr;
    return *this;
}

PooledConnection::~PooledConnection() { release(); }

Connection& PooledConnection::get() const { return *connection_; }

Connection* PooledConnection::operator->() const { return connection_.get(); }

PooledConnection::operator bool() const { return static_cast<bool>(connection_); }

void PooledConnection::release() {
    if (pool_ != nullptr && connection_) {
        pool_->release(std::move(connection_));
    }
    pool_ = nullptr;
}

ConnectionPool::ConnectionPool(std::size_t max_size, Factory factory) : max_size_(max_size), factory_(std::move(factory)) {}

PooledConnection ConnectionPool::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!available_.empty()) {
        auto connection = std::move(available_.back());
        available_.pop_back();
        return PooledConnection(this, std::move(connection));
    }

    if (created_ >= max_size_) {
        throw utils::error::DriverError("connection pool exhausted");
    }

    ++created_;
    return PooledConnection(this, factory_());
}

void ConnectionPool::release(std::shared_ptr<Connection> connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    available_.push_back(std::move(connection));
}

}  // namespace sqluna::core
