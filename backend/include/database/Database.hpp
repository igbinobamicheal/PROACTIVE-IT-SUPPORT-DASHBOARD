#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <mysqlx/xdevapi.h>
#include <memory>
#include <string>

class Database {
public:
    /**
     * Initializes the database connection pool using the global Configuration.
     */
    void initialize();

    /**
     * Obtains a Session from the connection pool.
     * The Session object returned is not thread-safe, but the retrieval itself is thread-safe.
     * When the Session goes out of scope, the connection is returned to the pool.
     */
    mysqlx::Session getSession();

    /**
     * Singleton instance access.
     */
    static Database& getInstance() {
        static Database instance;
        return instance;
    }

private:
    Database() = default;
    std::unique_ptr<mysqlx::Client> client;
};

#endif // DATABASE_HPP
