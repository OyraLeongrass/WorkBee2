#pragma once
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include "sqlite3.h"
#include <iostream>
using namespace std;
struct User {
    int id_user;
    string username;
    string password_hash;
    string role;
    bool is_active;
    string created_at;

    User() : id_user(0), is_active(true) {}
};

struct Secret {
    int id_secrets;
    int owner_id;
    string secret_value;
    string created_at;
    string expires_at;
    string secret_type;

    Secret() : id_secrets(0), owner_id(0) {}
};

struct AuditLog {
    int id_audit_logs;
    int user_id;
    string action;
    string object_type;
    int object_id;
    string created_at;

    AuditLog() : id_audit_logs(0), user_id(0), object_id(0) {}
};




class DatabaseException : public runtime_error {
public:
    DatabaseException(const string& message) : runtime_error(message) {}
};

class DataBase {
private:
    sqlite3* db;
    string dbPath;
    DataBase() : db(nullptr) {}

public:
    // Singleton - получение единственного экземпляра
    static DataBase& getInstance() {
        static DataBase instance;
        return instance;
    }

    ~DataBase() {
        close();
                }

    // Открытие базы данных
    bool open(const string& path) {
        dbPath = path;
        int rc = sqlite3_open(path.c_str(), &db);

        if (rc != SQLITE_OK) {
            throw DatabaseException("Не удалось открыть базу данных: " +
                string(sqlite3_errmsg(db)));
        }
        executeSQL("PRAGMA foreign_keys = ON;");
        dropTables();
        createTablesUsers();
        createTablesSecrets();
        cout << "База данных открыта: " << path << endl;
        return true;
    }

    // Закрытие базы данных
    void close() {
        if (db) {
            sqlite3_close(db);
            db = nullptr;
            cout << "База данных закрыта" << endl;
        }
    }

    // Создание таблиц
    void createTablesUsers() {
        string sql =
            "CREATE TABLE IF NOT EXISTS users ("
            "id_user INTEGER PRIMARY KEY AUTOINCREMENT,"
            "username VARCHAR(100) NOT NULL UNIQUE,"
            "password_hash VARCHAR(255) NOT NULL,"
            "role VARCHAR(100) NOT NULL,"
            "is_active BOOLEAN NOT NULL DEFAULT 1,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ");";
        executeSQL(sql);
    }
    void createTablesSecrets() {
        string sql =
            "CREATE TABLE IF NOT EXISTS secrets ("
            "id_secrets INTEGER PRIMARY KEY AUTOINCREMENT,"
            "owner_id INTEGER NOT NULL,"
            "secret_value TEXT NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "expires_at TIMESTAMP,"
            "secret_type VARCHAR(50) NOT NULL,"
            "FOREIGN KEY(owner_id) REFERENCES users(id_user) ON DELETE CASCADE"
            ");";
        executeSQL(sql);
    }
    void createTablesAuditLogs() {
        string sql =
            "CREATE TABLE IF NOT EXISTS audit_logs ("
            "id_audit_logs INTEGER PRIMARY KEY AUTOINCREMENT,"
            "user_id INTEGER NOT NULL,"
            "action VARCHAR(100) NOT NULL,"
            "object_type VARCHAR(50) NOT NULL,"
            "object_id INTEGER,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "FOREIGN KEY(user_id) REFERENCES users(id_user)"
            ");";
        executeSQL(sql);
    }

    // Добовление нового пользователя
    int addUser(const User& user) {
        if (userExists(user.username)) {
            throw DatabaseException("Пользователь с таким именем уже существует");
        }
        string sql =
            "INSERT INTO users (username, password_hash, role, is_active) "
            "VALUES (?, ?, ?, ?);";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, user.password_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, user.role.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, user.is_active);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return (int)sqlite3_last_insert_rowid(db);
    }
    // Получение пользователя по ID
    User getUserById(int id) {
        string sql =
            "SELECT id_user, username, password_hash, role, is_active "
            "FROM users WHERE id_user = ?;";

        sqlite3_stmt* stmt = nullptr;
        User u;

        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            u.id_user = sqlite3_column_int(stmt, 0);
            u.username = (const char*)sqlite3_column_text(stmt, 1);
            u.password_hash = (const char*)sqlite3_column_text(stmt, 2);
            u.role = (const char*)sqlite3_column_text(stmt, 3);
            u.is_active = sqlite3_column_int(stmt, 4);
        }
        else {
            sqlite3_finalize(stmt);
            throw DatabaseException("Пользователь не найден");
        }

        sqlite3_finalize(stmt);
        return u;
    }
    // Получение пользователя по имени
    User getUserByUsername(const string& username) {
        string sql =
            "SELECT id_user, username, password_hash, role, is_active "
            "FROM users WHERE username = ?;";

        sqlite3_stmt* stmt = nullptr;
        User user;

        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
            throw DatabaseException(sqlite3_errmsg(db));

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user.id_user = sqlite3_column_int(stmt, 0);
            user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            user.password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            user.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            user.is_active = sqlite3_column_int(stmt, 4);
        }
        else {
            sqlite3_finalize(stmt);
            throw DatabaseException("Пользователь не найден");
        }

        sqlite3_finalize(stmt);
        return user;
    }
    // Получение всех пользователей
    vector<User> getAllUsers() {
        string sql =
            "SELECT id_user, username, password_hash, role, is_active "
            "FROM users ORDER BY username;";

        vector<User> users;
        sqlite3_stmt* stmt = nullptr;

        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
            throw DatabaseException(sqlite3_errmsg(db));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            User user;
            user.id_user = sqlite3_column_int(stmt, 0);
            user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            user.password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            user.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            user.is_active = sqlite3_column_int(stmt, 4);
            users.push_back(user);
        }

        sqlite3_finalize(stmt);
        return users;
    }
    // Проверка существования пользователя
    bool userExists(const string& username) {
        string sql = "SELECT COUNT(*) FROM users WHERE username = ?;";
        sqlite3_stmt* stmt = nullptr;

        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);

        sqlite3_finalize(stmt);
        return count > 0;
    }



    // Добавление нового секрета
    int addSecret(const Secret& secret) {
        if (!userExists(getUserById(secret.owner_id).username)) {
            throw DatabaseException("Владелец секрета не существует");
        }
        string sql =
            "INSERT INTO secrets (owner_id, secret_value, expires_at, secret_type) "
            "VALUES (?, ?, ?, ?);";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

        sqlite3_bind_int(stmt, 1, secret.owner_id);
        sqlite3_bind_text(stmt, 2, secret.secret_value.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, secret.expires_at.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, secret.secret_type.c_str(), -1, SQLITE_TRANSIENT);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return (int)sqlite3_last_insert_rowid(db);
    }
    //Проверка существования секрета
    bool secretExists(int secretId) {
        string sql = "SELECT COUNT(*) FROM secrets WHERE id_secrets = ?;";
        sqlite3_stmt* stmt = nullptr;

        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, secretId);

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);

        sqlite3_finalize(stmt);
        return count > 0;
    }
    // Получение секрета по ID
    Secret getSecretById(int secretId) {
        if (!secretExists(secretId)) {
            throw DatabaseException("Секрет не существует");
        }
        string sql =
            "SELECT id_secrets, owner_id, secret_value, created_at, expires_at, secret_type "
            "FROM secrets WHERE id_secrets = ?;";

        sqlite3_stmt* stmt = nullptr;
        Secret s;

        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
            throw DatabaseException(sqlite3_errmsg(db));

        sqlite3_bind_int(stmt, 1, secretId);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            s.id_secrets = sqlite3_column_int(stmt, 0);
            s.owner_id = sqlite3_column_int(stmt, 1);
            s.secret_value = (const char*)sqlite3_column_text(stmt, 2);
            s.created_at = (const char*)sqlite3_column_text(stmt, 3);
            s.expires_at = (const char*)sqlite3_column_text(stmt, 4);
            s.secret_type = (const char*)sqlite3_column_text(stmt, 5);
        }
        else {
            sqlite3_finalize(stmt);
            throw DatabaseException("Секрет не найден");
        }

        sqlite3_finalize(stmt);
        return s;
    }
    // Получение секрета по пользователю
    vector<Secret> getSecretsByUser(int userId) {
        vector<Secret> list;
        const char* sql =
            "SELECT id_secrets, owner_id, secret_value, created_at, expires_at, secret_type "
            "FROM secrets WHERE owner_id = ?;";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, userId);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Secret s;
            s.id_secrets = sqlite3_column_int(stmt, 0);
            s.owner_id = sqlite3_column_int(stmt, 1);
            s.secret_value = (const char*)sqlite3_column_text(stmt, 2);
            s.created_at = (const char*)sqlite3_column_text(stmt, 3);
            s.expires_at = (const char*)sqlite3_column_text(stmt, 4);
            s.secret_type = (const char*)sqlite3_column_text(stmt, 5);
            list.push_back(s);
        }

        sqlite3_finalize(stmt);
        return list;
    }
    // Получение всех секретов
    vector<Secret> getAllSecrets() {
        string sql =
            "SELECT id_secrets, owner_id, secret_value, created_at, expires_at, secret_type "
            "FROM secrets ORDER BY created_at DESC;";

        vector<Secret> secrets;
        sqlite3_stmt* stmt = nullptr;

        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
            throw DatabaseException(sqlite3_errmsg(db));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Secret s;
            s.id_secrets = sqlite3_column_int(stmt, 0);
            s.owner_id = sqlite3_column_int(stmt, 1);
            s.secret_value = (const char*)sqlite3_column_text(stmt, 2);
            s.created_at = (const char*)sqlite3_column_text(stmt, 3);
            s.expires_at = (const char*)sqlite3_column_text(stmt, 4);
            s.secret_type = (const char*)sqlite3_column_text(stmt, 5);
            secrets.push_back(s);
        }

        sqlite3_finalize(stmt);
        return secrets;
    }
    //Обновление секрета
    bool updateSecret(int secretId, const Secret& s) {
        if (!secretExists(secretId)) {
            throw DatabaseException("Нельзя обновить несуществующий секрет");
        }
        string sql =
            "UPDATE secrets SET secret_value = ?, expires_at = ?, secret_type = ? "
            "WHERE id_secrets = ?;";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK)
            throw DatabaseException("Ошибка подготовки запроса");

        sqlite3_bind_text(stmt, 1, s.secret_value.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, s.expires_at.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, s.secret_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, secretId);

        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }
    //Поиск секретов
    vector<Secret> searchSecrets(const string& pattern) {
        string sql =
            "SELECT id_secrets, owner_id, secret_value, created_at, expires_at, secret_type "
            "FROM secrets WHERE secret_value LIKE ? OR secret_type LIKE ?;";

        vector<Secret> list;
        sqlite3_stmt* stmt = nullptr;
        string p = "%" + pattern + "%";

        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, p.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, p.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Secret s;
            s.id_secrets = sqlite3_column_int(stmt, 0);
            s.owner_id = sqlite3_column_int(stmt, 1);
            s.secret_value = (const char*)sqlite3_column_text(stmt, 2);
            s.created_at = (const char*)sqlite3_column_text(stmt, 3);
            s.expires_at = (const char*)sqlite3_column_text(stmt, 4);
            s.secret_type = (const char*)sqlite3_column_text(stmt, 5);
            list.push_back(s);
        }

        sqlite3_finalize(stmt);
        return list;
    }

    // Удаление секрета по ID
    bool deleteSecret(int secretId) {
        string sql = "DELETE FROM tbl_secret WHERE id = ?;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            sqlite3_bind_int(stmt, 1, secretId);

            rc = sqlite3_step(stmt);
            bool success = (rc == SQLITE_DONE);

            if (success) {
                cout << "Секрет ID " << secretId << " удален" << endl;
            }
            else {
                cerr << "Ошибка удаления секрета" << endl;
            }

            sqlite3_finalize(stmt);
            return success;

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }


    // Логирование действий
    void addAuditLog(int userId, const string& action,
        const string& objectType, int objectId) {
        string sql =
            "INSERT INTO audit_logs (user_id, action, object_type, object_id) "
            "VALUES (?, ?, ?, ?);";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

        sqlite3_bind_int(stmt, 1, userId);
        sqlite3_bind_text(stmt, 2, action.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, objectType.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, objectId);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    // Аутентификация (проверка пользователя и пароля)
    bool authenticate(const string& username, const string& password_hash) {
        string sql =
            "SELECT COUNT(*) FROM users "
            "WHERE username = ? AND password_hash = ? AND is_active = 1;";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);

        int count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);

        sqlite3_finalize(stmt);
        return count > 0;
    }


    void dropTables() {
        try {
            executeSQL("DROP TABLE IF EXISTS secret;");
            executeSQL("DROP TABLE IF EXISTS users;");
            executeSQL("DROP TABLE IF EXISTS audit_logs;");
        }
        catch (const DatabaseException& e) { cerr << "Ошибка удаления таблиц: " << e.what() << endl; }
    }
   

   


   

    // Получение статистики
    struct Statistics {
        int totalActions;
        int uniqueUsers;
        string lastActiveUser;
        string firstActiveUser;

        void print() const {
            cout << "\n=== СТАТИСТИКА ===" << endl;
            cout << "Всего действий: " << totalActions << endl;
            cout << "Уникальных пользователей: " << uniqueUsers << endl;
            cout << "Последний активный пользователь: " << lastActiveUser << endl;
            cout << "Первый активный пользователь: " << firstActiveUser << endl;
        }
    }; 
    Statistics getStatistics() {
        Statistics stats;

        // Общее количество действий
        stats.totalActions = executeScalar<int>(
            "SELECT COUNT(*) FROM audit_logs;"
        );

        // Количество уникальных пользователей
        stats.uniqueUsers = executeScalar<int>(
            "SELECT COUNT(DISTINCT user_id) FROM audit_logs;"
        );

        // Последний активный пользователь
        stats.lastActiveUser = executeScalar<string>(
            "SELECT u.username FROM audit_logs a "
            "JOIN users u ON a.user_id = u.id_user "
            "ORDER BY a.created_at DESC LIMIT 1;"
        );

        // Первый активный пользователь
        stats.firstActiveUser = executeScalar<string>(
            "SELECT u.username FROM audit_logs a "
            "JOIN users u ON a.user_id = u.id_user "
            "ORDER BY a.created_at ASC LIMIT 1;"
        );

        return stats;
    }



    // Получение версии SQLite
    static string getSQLiteVersion() {
        return sqlite3_libversion();
    }

    // Очистка всей таблицы
    bool clearAllSecrets() {
        string sql = "DELETE FROM secret;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            rc = sqlite3_step(stmt);
            bool success = (rc == SQLITE_DONE);

            if (success) {
                cout << "Все секреты удалены" << endl;
            }

            sqlite3_finalize(stmt);
            return success;

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }
    bool clearAllUsers() {
        string sql = "DELETE FROM users;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            rc = sqlite3_step(stmt);
            bool success = (rc == SQLITE_DONE);

            if (success) {
                cout << "Все пользователи удалены" << endl;
            }

            sqlite3_finalize(stmt);
            return success;

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }

private:
    // Вспомогательные методы

    void executeSQL(const string& sql) {
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);

        if (rc != SQLITE_OK) {
            string error = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            throw DatabaseException("Ошибка выполнения SQL: " + error);
        }
    }

    void executeSQLWithParam(const string& sql, const string& param) {
        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            sqlite3_bind_text(stmt, 1, param.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }

    template<typename T>
    T executeScalar(const string& sql) {
        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            T result{};
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                if constexpr (is_same_v<T, int>) {
                    result = sqlite3_column_int(stmt, 0);
                }
                else if constexpr (is_same_v<T, string>) {
                    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    result = text ? text : "";
                }
            }

            sqlite3_finalize(stmt);
            return result;

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }
};