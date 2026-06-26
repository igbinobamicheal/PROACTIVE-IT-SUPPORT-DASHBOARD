#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>

class Database {
public:
    /**
     * Initializes the database connection pool using the global Configuration.
     */
    void initialize();

    /**
     * Custom deleter for returning a connection to the pool.
     */
    struct ConnectionDeleter {
        void operator()(sql::Connection* conn) const;
    };
    using ConnectionPtr = std::unique_ptr<sql::Connection, ConnectionDeleter>;

    /**
     * Obtains a Connection from the pool.
     */
    ConnectionPtr getConnection();

    /**
     * Singleton instance access.
     */
    static Database& getInstance() {
        static Database instance;
        return instance;
    }

private:
    Database() = default;
    ~Database();

    void returnConnection(sql::Connection* conn);

    sql::Driver* driver = nullptr;
    std::queue<sql::Connection*> connectionPool;
    std::mutex poolMutex;
    std::condition_variable poolCv;
    int maxPoolSize = 10;
    int currentPoolSize = 0;

    std::string dbUrl;
    std::string dbUser;
    std::string dbPassword;
    std::string dbSchema;
};

#endif // DATABASE_HPP
