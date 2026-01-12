#pragma once
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include "sqlite3.h"
#include <iostream>
using namespace std;
struct User {
    int id;
    string username;
    string email;
    string password;
    string role;

    User() : id(0) {}

    User(const string& uname, const string& mail,
        const string& pwd_hash, const string& r = "user")
        : id(0), username(uname), email(mail),
        password(pwd_hash), role(r) {}

    void print() const {
        cout << "ID: " << id
            << ", Имя: " << username
            << ", Email: " << email
            << ", Роль: " << role
            << endl;
    }
};
struct Secret {
    int id;
    int owner_id;    
    string secret;      
    string created_at;   
    string expires_at;   
    string secret_type; 
    shared_ptr<User> owner;

    Secret() : id(0) {}

    Secret(const int& oid, const string& sec,
        const string& created, const string& expires,
        const string& type)
        : id(0), owner_id(oid), secret(sec),
        created_at(created), expires_at(expires),
        secret_type(type) {}

    void print() const {
        cout << "ID: " << id
            << ", Владелец: " << owner->username
            << ", Секрет: " << secret
            << ", Дата создания: " << created_at
            << ", Срок действия: " << expires_at
            << ", Тип: " << secret_type << endl;
    }
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
    void createTablesSecrets() {
        string sql =
            "CREATE TABLE IF NOT EXISTS tbl_secret ("
            "id_secret INTEGER PRIMARY KEY AUTOINCREMENT,"
            "owner_id INTEGER NOT NULL,"
            "secret TEXT NOT NULL,"
            "created_at TEXT NOT NULL,"
            "expires_at TEXT NOT NULL,"
            "secret_type TEXT NOT NULL,"
            "FOREIGN KEY (owner_id) REFERENCES tbl_users(id_user) ON DELETE CASCADE"
            ");";

        executeSQL(sql);
        executeSQL("CREATE INDEX IF NOT EXISTS idx_created_at ON tbl_secret(created_at)");
        cout << "Таблица секретов создана/проверена" << endl;
    }
    void createTablesUsers() {
        string sql = "CREATE TABLE IF NOT EXISTS tbl_users("
            "id_user INTEGER PRIMARY KEY AUTOINCREMENT,"
            "username TEXT NOT NULL,"
            "password TEXT NOT NULL,"
            "role TEXT NOT NULL"
            ");";
        executeSQL(sql);
        executeSQL("CREATE INDEX IF NOT EXISTS idx_user ON tbl_users(username);");
        cout << "Таблица пользователя создана/проверена" << endl;
    }

    // Добавление нового секрета
    int addSecret(const Secret& secret) {
        string sql =
            "INSERT INTO tbl_secret (owner_id, secret, created_at, expires_at, secret_type) "
            "VALUES (?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) throw DatabaseException(sqlite3_errmsg(db));
            sqlite3_bind_int(stmt, 1, secret.owner_id);
            sqlite3_bind_text(stmt, 2, secret.secret.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, secret.created_at.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, secret.expires_at.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, secret.secret_type.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) throw DatabaseException(sqlite3_errmsg(db));

            int newId = static_cast<int>(sqlite3_last_insert_rowid(db));
            sqlite3_finalize(stmt);
            return newId;
        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }



    // Получение секрета по ID
    Secret getSecretById(int secretId) {
        string sql =
            "SELECT s.id, s.owner_id, s.secret, s.created_at, s.expires_at, "
            "s.secret_type, s.version, s.description, "
            "u.username, u.email, u.role "
            "FROM tbl_secret s "
            "JOIN tbl_users u ON s.owner_id = u.id "
            "WHERE s.id = ? and id = ?;";
        sqlite3_stmt* stmt = nullptr;
        Secret s;
        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK)
                throw DatabaseException(sqlite3_errmsg(db)); sqlite3_bind_int(stmt, 1, secretId);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                s.id = sqlite3_column_int(stmt, 0);
                s.owner_id = reinterpret_cast<const int>(sqlite3_column_text(stmt, 1));
                s.secret = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                s.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                s.expires_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                s.secret_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            }
            else {
                throw DatabaseException("Секрет не найден");
            }
            sqlite3_finalize(stmt);
            return s;
        }
        catch (...) {
            if (stmt)  sqlite3_finalize(stmt);
            throw;
        }
    }
    
    vector<Secret> getSecretsByUser(int userId) {
        string sql =
            "SELECT id, owner_id, secret, created_at, expires_at, "
            "secret_type, version, description "
            "FROM tbl_secret WHERE owner_id = ? "
            "ORDER BY created_at DESC;";

        vector<Secret> secrets;
        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) throw DatabaseException(sqlite3_errmsg(db));

            sqlite3_bind_int(stmt, 1, userId);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Secret secret;
                secret.id = sqlite3_column_int(stmt, 0);
                secret.owner_id = sqlite3_column_int(stmt, 1);
                secret.secret = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                secret.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                secret.expires_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                secret.secret_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                secrets.push_back(secret);
            }

            sqlite3_finalize(stmt);
            return secrets;
        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }

    // Получение всех секретов
    vector<Secret> getAllSecretsWithOwners() {
        string sql =
            "SELECT s.id, s.owner_id, s.secret, s.created_at, s.expires_at, "
            "s.secret_type, s.version, s.description, "
            "u.username, u.email, u.role "
            "FROM tbl_secret s "
            "JOIN tbl_users u ON s.owner_id = u.id "
            "ORDER BY s.created_at DESC;";

        vector<Secret> secrets;
        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) throw DatabaseException(sqlite3_errmsg(db));

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Secret secret;
                secret.id = sqlite3_column_int(stmt, 0);
                secret.secret = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                secret.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                secret.expires_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                secret.secret_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
               
                // Создаем объект пользователя
                secret.owner = make_shared<User>();
                secret.owner->username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
                secrets.push_back(secret);
            }

            sqlite3_finalize(stmt);
            return secrets;
        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }


    // Обновление секрета
    bool updateSecret(int secretId, const Secret& updatedSecret) {
        string sql = 
            "UPDATE tbl_secret SET owner_id = ?, secret = ?, created_at = ?, expires_at = ?, secret_type = ? "
            "WHERE id = ?;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            sqlite3_bind_int(stmt, 1, updatedSecret.owner_id); 
            sqlite3_bind_text(stmt, 2, updatedSecret.secret.c_str(), -1, SQLITE_TRANSIENT); 
            sqlite3_bind_text(stmt, 3, updatedSecret.created_at.c_str(), -1, SQLITE_TRANSIENT); 
            sqlite3_bind_text(stmt, 4, updatedSecret.expires_at.c_str(), -1, SQLITE_TRANSIENT); 
            sqlite3_bind_text(stmt, 5, updatedSecret.secret_type.c_str(), -1, SQLITE_TRANSIENT); 
            sqlite3_bind_int(stmt, 6, secretId);

            rc = sqlite3_step(stmt);
            bool success = (rc == SQLITE_DONE);

            if (success) {
                cout << "Секрет ID " << secretId << " обновлен" << endl;
            }
            else {
                cerr << "Ошибка обновления секрета" << endl;
            }

            sqlite3_finalize(stmt);
            return success;

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
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
    void dropTables() {
        try {
            executeSQL("DROP TABLE IF EXISTS tbl_secret;");
            executeSQL("DROP TABLE IF EXISTS tbl_users;");
            executeSQL("DROP INDEX IF EXISTS idx_created_at;");
            executeSQL("DROP INDEX IF EXISTS idx_username;");
            cout << "Индексы idx_created_at и idx_username удалены" << endl;
            cout << "Таблицы tbl_secret и tbl_userd удалены" << endl;
        }
        catch (const DatabaseException& e) { cerr << "Ошибка удаления таблиц: " << e.what() << endl; }
    }
   

    // Проверка существования пользователя
    bool userExists(const string& username) {
        string sql = "SELECT COUNT(*) FROM tbl_users WHERE username = ?;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса: " +
                    string(sqlite3_errmsg(db)));
            }

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

            int count = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }

            sqlite3_finalize(stmt);
            return count > 0;
        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }


    // Проверка существования секрета
    bool secretExists(int secretId) {
        string sql = "SELECT COUNT(*) FROM tbl_secret WHERE id = ?;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            sqlite3_bind_int(stmt, 1, secretId);

            int count = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }

            sqlite3_finalize(stmt);
            return count > 0;

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }

    // Аутентификация (проверка пользователя и пароля)
    bool authenticate(const string& username, const string& password) {
        string sql =
            "SELECT COUNT(*) FROM tbl_users WHERE username = ? AND password = ?;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

            int count = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }

            sqlite3_finalize(stmt);
            return count > 0;

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }

    // Поиск секретов по шаблону
    vector<Secret> searchSecrets(const string& pattern) {
        string sql =
            "SELECT id, owner_id, secret, created_at, expires_at, secret_type "
            "FROM tbl_secret WHERE owner_id LIKE ? OR secret LIKE ? OR secret_type LIKE ? "
            "ORDER BY created_at;";

        vector<Secret> secrets;
        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса: " +
                    string(sqlite3_errmsg(db)));
            }

            string searchPattern = "%" + pattern + "%";
            sqlite3_bind_text(stmt, 1, searchPattern.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, searchPattern.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, searchPattern.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Secret s;
                s.id = sqlite3_column_int(stmt, 0);
                s.owner_id = reinterpret_cast<const int>(sqlite3_column_text(stmt, 1));
                s.secret = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                s.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                s.expires_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                s.secret_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));

                secrets.push_back(s);
            }

            sqlite3_finalize(stmt);
            return secrets;
        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }


    // Получение статистики
    struct Statistics {
        int totalSecrets;
        int uniqueUsers;
        string newestUser;
        string oldestUser;

        void print() const {
            cout << "\n=== СТАТИСТИКА ===" << endl;
            cout << "Всего секретов: " << totalSecrets << endl;
            cout << "Уникальных пользователей: " << uniqueUsers << endl;
            cout << "Новый пользователь: " << newestUser << endl;
            cout << "Старый пользователь: " << oldestUser << endl;
        }
    };

    Statistics getStatistics() {
        Statistics stats;
        string sql = "SELECT COUNT(*) FROM tbl_secret;";
        stats.totalSecrets = executeScalar<int>(sql);
        sql = "SELECT COUNT(DISTINCT owner_id) FROM tbl_secret;";
        stats.uniqueUsers = executeScalar<int>(sql);
        sql = "SELECT owner_id FROM tbl_secret ORDER BY created_at DESC LIMIT 1;";
        stats.newestUser = executeScalar<string>(sql);
        sql = "SELECT owner_id FROM tbl_secret ORDER BY created_at ASC LIMIT 1;";
        stats.oldestUser = executeScalar<string>(sql);

        return stats;
    }


    // Получение версии SQLite
    static string getSQLiteVersion() {
        return sqlite3_libversion();
    }

    // Очистка всей таблицы
    bool clearAllSecrets() {
        string sql = "DELETE FROM tbl_secret;";

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
        string sql = "DELETE FROM tbl_users;";

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
    // Добавление нового пользователя
    int addUser(const User& user) {
        string sql =
            "INSERT INTO tbl_users (username, password, role) "
            "VALUES (?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;
        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) throw DatabaseException(sqlite3_errmsg(db));

            sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, user.password.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, user.role.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) throw DatabaseException(sqlite3_errmsg(db));

            int newId = static_cast<int>(sqlite3_last_insert_rowid(db));
            sqlite3_finalize(stmt);
            return newId;
        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }
    // Получение пользователя по имени
    User getUserByUsername(const string& username) {
        string sql =
            "SELECT id_user, username, password, role "
            "FROM tbl_users WHERE username = ?;";

        sqlite3_stmt* stmt = nullptr;
        User user;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) throw DatabaseException(sqlite3_errmsg(db));

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                user.id = sqlite3_column_int(stmt, 0);
                user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                user.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                user.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            }
            else {
                throw DatabaseException("Пользователь с именем '" + username + "' не найден");
            }

            sqlite3_finalize(stmt);
            return user;
        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }
    // Получение всех пользователей
    vector<User> getAllUsers() {
        string sql =
            "SELECT id_user, username, password, role "
            "FROM tbl_users ORDER BY username;";

        vector<User> users;
        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) throw DatabaseException(sqlite3_errmsg(db));

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                User user;
                user.id = sqlite3_column_int(stmt, 0);
                user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                user.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                user.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

                users.push_back(user);
            }

            sqlite3_finalize(stmt);
            return users;
        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }

    // Получение пользователя по ID
    User getUserById(int userId) {
        string sql =
            "SELECT id_user, username, password, role "
            "FROM tbl_users WHERE id_user = ?;";

        sqlite3_stmt* stmt = nullptr;
        User user;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) throw DatabaseException(sqlite3_errmsg(db));

            sqlite3_bind_int(stmt, 1, userId);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                user.id = sqlite3_column_int(stmt, 0);
                user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                user.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                user.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            }
            else {
                throw DatabaseException("Пользователь не найден");
            }

            sqlite3_finalize(stmt);
            return user;
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