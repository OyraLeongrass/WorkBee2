  #pragma once
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include "sqlite3.h"
#include <iostream>
#include <Windows.h>
using namespace std;

struct Secret {
    int id;
    string user;
    string secret;
    string password;

    Secret() : id(0) {}

    Secret(const string& uname, const string& sec, const string& pwd)
        : id(0), user(uname), secret(sec), password(pwd) {
    }

    void print() const {
        cout << "ID: " << id
            << ", Пользователь: " << user
            << ", Секрет: " << secret
            << ", Пароль: " << password << endl;
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
        createTables();
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
    void createTables() {
        string sql =
            "CREATE TABLE IF NOT EXISTS tbl_secret ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "user TEXT NOT NULL,"
            "secret TEXT NOT NULL,"
            "password TEXT NOT NULL"
            ");";

        executeSQL(sql);

        // Создаем индекс для быстрого поиска по пользователю
        executeSQL("CREATE INDEX IF NOT EXISTS idx_user ON tbl_secret(user);");

        cout << "Таблица создана/проверена" << endl;
    }


    // Добавление нового секрета
    int addSecret(const Secret& secret) {
        string sql =
            "INSERT INTO tbl_secret (user, secret, password) "
            "VALUES (?, ?, ?);";

        sqlite3_stmt* stmt = nullptr;

        try {
            // Подготавливаем запрос
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса: " +
                    string(sqlite3_errmsg(db)));
            }

            // Привязываем параметры
            sqlite3_bind_text(stmt, 1, secret.user.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, secret.secret.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, secret.password.c_str(), -1, SQLITE_TRANSIENT);

            // Выполняем запрос
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                throw DatabaseException("Ошибка добавления секрета: " +
                    string(sqlite3_errmsg(db)));
            }

            // Получаем ID добавленной записи
            int newId = static_cast<int>(sqlite3_last_insert_rowid(db));

            sqlite3_finalize(stmt);

            cout << "Секрет добавлен, ID: " << newId << endl;
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
            "SELECT id, user, secret, password "
            "FROM tbl_secret WHERE id = ?;";

        sqlite3_stmt* stmt = nullptr;
        Secret secretObj;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            sqlite3_bind_int(stmt, 1, secretId);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                secretObj.id = sqlite3_column_int(stmt, 0);
                secretObj.user = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                secretObj.secret = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                secretObj.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            }
            else {
                throw DatabaseException("Секрет с ID " + to_string(secretId) + " не найден");
            }

            sqlite3_finalize(stmt);
            return secretObj;

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }

    // Получение секрета по пользователю
    Secret getSecretByUser(const string& username) {
        string sql =
            "SELECT id, user, secret, password "
            "FROM tbl_secret WHERE user = ?;";

        sqlite3_stmt* stmt = nullptr;
        Secret secretObj;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                secretObj.id = sqlite3_column_int(stmt, 0);
                secretObj.user = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                secretObj.secret = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                secretObj.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            }
            else {
                throw DatabaseException("Секрет для пользователя '" + username + "' не найден");
            }

            sqlite3_finalize(stmt);
            return secretObj;

        }
        catch (...) {
            if (stmt) sqlite3_finalize(stmt);
            throw;
        }
    }

    // Получение всех секретов
    vector<Secret> getAllSecrets() {
        string sql =
            "SELECT id, user, secret, password "
            "FROM tbl_secret ORDER BY user;";

        vector<Secret> secrets;
        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Secret secretObj;
                secretObj.id = sqlite3_column_int(stmt, 0);
                secretObj.user = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                secretObj.secret = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                secretObj.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

                secrets.push_back(secretObj);
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
            "UPDATE tbl_secret SET user = ?, secret = ?, password = ? "
            "WHERE id = ?;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            sqlite3_bind_text(stmt, 1, updatedSecret.user.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, updatedSecret.secret.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, updatedSecret.password.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 4, secretId);

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

    // Удаление секрета по пользователю
    bool deleteSecretByUser(const string& username) {
        string sql = "DELETE FROM tbl_secret WHERE user = ?;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            bool success = (rc == SQLITE_DONE);

            if (success) {
                cout << "Секрет пользователя '" << username << "' удален" << endl;
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

    // Проверка существования пользователя
    bool userExists(const string& username) {
        string sql = "SELECT COUNT(*) FROM tbl_secret WHERE user = ?;";

        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
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
            "SELECT COUNT(*) FROM tbl_secret WHERE user = ? AND password = ?;";

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
            "SELECT id, user, secret, password "
            "FROM tbl_secret WHERE user LIKE ? OR secret LIKE ? "
            "ORDER BY user;";

        vector<Secret> secrets;
        sqlite3_stmt* stmt = nullptr;

        try {
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw DatabaseException("Ошибка подготовки запроса");
            }

            string searchPattern = "%" + pattern + "%";
            sqlite3_bind_text(stmt, 1, searchPattern.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, searchPattern.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Secret secretObj;
                secretObj.id = sqlite3_column_int(stmt, 0);
                secretObj.user = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                secretObj.secret = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                secretObj.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

                secrets.push_back(secretObj);
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

        sql = "SELECT COUNT(DISTINCT user) FROM tbl_secret;";
        stats.uniqueUsers = executeScalar<int>(sql);

        sql = "SELECT user FROM tbl_secret ORDER BY id DESC LIMIT 1;";
        stats.newestUser = executeScalar<string>(sql);

        sql = "SELECT user FROM tbl_secret ORDER BY id ASC LIMIT 1;";
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