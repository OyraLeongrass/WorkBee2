#pragma once
#include "httplib.h"
#include "DataBase.h"
#include <iostream>
#include "json.hpp"
#include <ctime>
#include <functional>
using json = nlohmann::json;
using namespace std;

class SecretServer {
private:
    httplib::Server server;
    DataBase& db;

public:
    SecretServer() : db(DataBase::getInstance()) {}

    /* ===== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===== */
    static string getCurrentDateTime() {
        time_t now = time(nullptr);
        tm localTime;
        localtime_s(&localTime, &now);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
        return buffer;
    }

    static string hashPassword(const string& password) {
        hash<string> hasher;
        return to_string(hasher(password));
    }

    static void sendJson(httplib::Response& res, int status, const json& data) {
        res.status = status;
        res.set_content(data.dump(4), "application/json");
    }

    static void sendError(httplib::Response& res, int status, const string& msg) {
        sendJson(res, status, { {"error", msg} });
    }

    static void sendSuccess(httplib::Response& res, const json& data = {}) {
        json j = { {"success", true} };
        if (!data.empty()) j["data"] = data;
        sendJson(res, 200, j);
    }
    /* ===== АУТЕНТИФИКАЦИЯ С ВОЗВРАТОМ РОЛИ ===== */
    string authenticateAndGetRole(const string& username, const string& password) {
        try {
            if (db.authenticate(username, hashPassword(password))) {
                User user = db.getUserByUsername(username);
                return user.role;
            }
            else {
                return "";
            }
        }
        catch (const DatabaseException& e) {
            cerr << "Ошибка аутентификации: " << e.what() << endl;
            return "";
        }
    }
    vector<Secret> getSecretsByRole(const string& username, const string& password) {
        string role = authenticateAndGetRole(username, password);
        if (role.empty()) throw runtime_error("Неверные данные");

        if (role == "admin") {
            return db.getAllSecrets();
        }
        else {
            User user = db.getUserByUsername(username);
            return db.getSecretsByUser(user.id_user);
        }
    }

    /* ===== ПОЛУЧЕНИЕ ПОЛЬЗОВАТЕЛЕЙ С УЧЕТОМ РОЛИ ===== */
    vector<User> getUsersByRole(const string& username, const string& password) {
        string role = authenticateAndGetRole(username, password);
        if (role.empty()) throw runtime_error("Неверные данные");

        if (role == "admin") {
            return db.getAllUsers();
        }
        else {
            User user = db.getUserByUsername(username);
            return { user };
        }
    }

    /* ===== ПОЛУЧЕНИЕ АУДИТ-ЛОГОВ С УЧЕТОМ РОЛИ ===== */
    vector<AuditLog> getAuditLogsByRole(const string& username, const string& password) {
        string role = authenticateAndGetRole(username, password);
        if (role.empty()) throw runtime_error("Неверные данные");

        if (role == "admin") {
            return db.getAuditLogs();
        }
        else {
            User user = db.getUserByUsername(username);
            auto allLogs = db.getAuditLogs();
            vector<AuditLog> filtered;
            for (auto& log : allLogs) {
                if (log.user_id == user.id_user) filtered.push_back(log);
            }
            return filtered;
        }
    }


    /* ===== ИНИЦИАЛИЗАЦИЯ СЕРВЕРА ===== */
    void initRoutes() {
        server.set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type"}
            });

        server.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("", "text/plain");
            });
        server.Post("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = json::parse(req.body);
                if (!j.contains("username") || !j.contains("password")) {
                    sendError(res, 400, "username и password обязательны");
                    return;
                }
                User user;
                user.username = j["username"];
                user.password_hash = hashPassword(j["password"]);
                user.role = j.value("role", "user");
                user.is_active = true;
                int id = db.addUser(user);
                db.addAuditLog(id, "Добавлен пользователь", "user", id);
                sendSuccess(res, { {"user_id", id} });
            }
            catch (const exception& e) {
                sendError(res, 500, e.what());
            }
            });
        server.Get("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = json::parse(req.body);
                string username = j.value("username", "");
                string password = j.value("password", "");
                auto users = getUsersByRole(username, password);
                json arr = json::array();
                for (auto& u : users) {
                    arr.push_back({
                        {"id_user", u.id_user},
                        {"username", u.username},
                        {"role", u.role},
                        {"is_active", u.is_active},
                        {"created_at", u.created_at}
                        });
                }
                sendSuccess(res, { {"users", arr} });
            }
            catch (const exception& e) {
                sendError(res, 401, e.what());
            }
            });
        server.Post("/api/auth/login", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = json::parse(req.body);
                string username = j.value("username", "");
                string password = j.value("password", "");
                string role = authenticateAndGetRole(username, password);
                if (!role.empty()) {
                    sendSuccess(res, { {"message", "Успешный вход"}, {"role", role} });
                }
                else {
                    sendError(res, 401, "Неверные данные");
                }
            }
            catch (const exception& e) {
                sendError(res, 500, e.what());
            }
            });
        server.Post("/api/secrets", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = json::parse(req.body);
                Secret s;
                s.owner_id = j["owner_id"];
                s.secret_value = j["secret_value"];
                s.secret_type = j["secret_type"];
                int expires_in_days = j.value("expires_in_days", 0);
                if (expires_in_days > 0) {
                    time_t now = time(nullptr);
                    now += expires_in_days * 24 * 3600;
                    char buffer[20];
                    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
                    s.expires_at = buffer;
                }
                int id = db.addSecret(s);
                db.addAuditLog(s.owner_id, "Добавлен секрет", "secret", id);
                sendSuccess(res, { {"secret_id", id} });
            }
            catch (const exception& e) {
                sendError(res, 500, e.what());
            }
            });
        server.Get("/api/secrets", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = json::parse(req.body);
                string username = j.value("username", "");
                string password = j.value("password", "");
                auto secrets = getSecretsByRole(username, password);
                json arr = json::array();
                for (auto& s : secrets) {
                    arr.push_back({
                        {"id_secrets", s.id_secrets},
                        {"owner_id", s.owner_id},
                        {"secret_value", s.secret_value},
                        {"secret_type", s.secret_type},
                        {"created_at", s.created_at},
                        {"expires_at", s.expires_at}
                        });
                }
                sendSuccess(res, { {"secrets", arr} });
            }
            catch (const exception& e) {
                sendError(res, 401, e.what());
            }
            });
        server.Get(R"(/api/secrets/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            int id = stoi(req.matches[1]);
            Secret s = db.getSecretById(id);
            sendSuccess(res, {
                {"id_secrets", s.id_secrets},
                {"owner_id", s.owner_id},
                {"secret_value", s.secret_value},
                {"secret_type", s.secret_type},
                {"created_at", s.created_at},
                {"expires_at", s.expires_at}
                });
            });
        server.Delete(R"(/api/secrets/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            int id = stoi(req.matches[1]);
            db.deleteSecret(id);
            db.addAuditLog(0, "Удален секрет", "secret", id);
            sendSuccess(res, { {"deleted", id} });
            });
        server.Put(R"(/api/secrets/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            int secretId = stoi(req.matches[1]);
            auto j = json::parse(req.body);
            Secret s;
            s.secret_value = j["secret_value"];
            s.secret_type = j["secret_type"];
            int expires_in_days = j.value("expires_in_days", 0);
            if (expires_in_days > 0) {
                time_t now = time(nullptr);
                now += expires_in_days * 24 * 3600;
                char buffer[20];
                strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
                s.expires_at = buffer;
            }
            bool success = db.updateSecret(secretId, s);
            db.addAuditLog(0, "Обновлен секрет", "secret", secretId);
            sendSuccess(res, { {"success", success} });
            });
        server.Get("/api/audit_logs", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = json::parse(req.body);
                string username = j.value("username", "");
                string password = j.value("password", "");

                auto logs = getAuditLogsByRole(username, password);
                json arr = json::array();
                for (auto& log : logs) {
                    arr.push_back({
                        {"id_audit_logs", log.id_audit_logs},
                        {"user_id", log.user_id},
                        {"action", log.action},
                        {"object_type", log.object_type},
                        {"object_id", log.object_id},
                        {"created_at", log.created_at}
                        });
                }
                sendSuccess(res, { {"audit_logs", arr} });
            }
            catch (const exception& e) {
                sendError(res, 401, e.what());
            }
            });

        server.Get("/api/statistics", [this](const httplib::Request&, httplib::Response& res) {
            auto stats = db.getStatistics();
            sendSuccess(res, {
                {"total_actions", stats.totalActions},
                {"unique_users", stats.uniqueUsers},
                {"first_active_user", stats.firstActiveUser},
                {"last_active_user", stats.lastActiveUser}
                });
            });
    }

    void run(const string& dbPath, int port = 8080) {
        db.open(dbPath);
        initRoutes();
        cout << "Сервер запущен на порту " << port << endl;
        server.listen("0.0.0.0", port);
        db.close();
    }
};

