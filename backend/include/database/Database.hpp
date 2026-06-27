#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <pqxx/pqxx>
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
        void operator()(pqxx::connection* conn) const;
    };
    using ConnectionPtr = std::unique_ptr<pqxx::connection, ConnectionDeleter>;

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

    void returnConnection(pqxx::connection* conn);
    void prepareConnection(pqxx::connection& conn);

    std::queue<pqxx::connection*> connectionPool;
    std::mutex poolMutex;
    std::condition_variable poolCv;
    int maxPoolSize = 10;
    int currentPoolSize = 0;

    std::string dbUrlStr;
};

#endif // DATABASE_HPP
