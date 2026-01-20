#include <iostream>
#include "DataBase.h"
#include "httplib.h"
#include "json.hpp"
#include <ctime>
#include <functional>
using json = nlohmann::json;
using namespace std;

/* ===== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ===== */

string getCurrentDateTime() {
    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return buffer;
}

string hashPassword(const string& password) {
    hash<string> hasher;
    return to_string(hasher(password));
}

void sendJson(httplib::Response& res, int status, const json& data) {
    res.status = status;
    res.set_content(data.dump(4), "application/json");
}

void sendError(httplib::Response& res, int status, const string& msg) {
    sendJson(res, status, { {"error", msg} });
}

void sendSuccess(httplib::Response& res, const json& data = {}) {
    json j = { {"success", true} };
    if (!data.empty()) j["data"] = data;
    sendJson(res, 200, j);
}

/* ===== MAIN ===== */

int main() {
    try {
        DataBase& db = DataBase::getInstance();
        db.open("secrets.db");

        httplib::Server svr;

        svr.set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE"},
            {"Access-Control-Allow-Headers", "Content-Type"}
            });

        /* ===== USERS ===== */

        svr.Post("/api/users", [&](const httplib::Request& req, httplib::Response& res) {
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
                sendSuccess(res, { {"user_id", id} });
            }
            catch (const exception& e) {
                sendError(res, 500, e.what());
            }
            });

        svr.Get("/api/users", [&](const httplib::Request&, httplib::Response& res) {
            auto users = db.getAllUsers();
            json arr = json::array();

            for (auto& u : users) {
                arr.push_back({
                    {"id_user", u.id_user},
                    {"username", u.username},
                    {"role", u.role},
                    {"is_active", u.is_active}
                    });
            }

            sendSuccess(res, { {"users", arr} });
            });

        /* ===== AUTH ===== */

        svr.Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
            auto j = json::parse(req.body);

            string username = j.value("username", "");
            string password = j.value("password", "");

            if (db.authenticate(username, hashPassword(password))) {
                sendSuccess(res, { {"message", "Успешный вход"} });
            }
            else {
                sendError(res, 401, "Неверные данные");
            }
            });

        /* ===== SECRETS ===== */

        svr.Post("/api/secrets", [&](const httplib::Request& req, httplib::Response& res) {
            auto j = json::parse(req.body);

            Secret s;
            s.owner_id = j["owner_id"];
            s.secret_value = j["secret"];
            s.secret_type = j["secret_type"];
            s.expires_at = j.value("expires_at", "");

            int id = db.addSecret(s);
            sendSuccess(res, { {"secret_id", id} });
            });

        svr.Get("/api/secrets", [&](const httplib::Request&, httplib::Response& res) {
            auto secrets = db.getAllSecrets();
            json arr = json::array();

            for (auto& s : secrets) {
                arr.push_back({
                    {"id_secrets", s.id_secrets},
                    {"owner_id", s.owner_id},
                    {"type", s.secret_type},
                    {"created_at", s.created_at},
                    {"expires_at", s.expires_at}
                    });
            }

            sendSuccess(res, { {"secrets", arr} });
            });

        svr.Get(R"(/api/secrets/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
            int id = stoi(req.matches[1]);
            Secret s = db.getSecretById(id);

            sendSuccess(res, {
                {"id_secrets", s.id_secrets},
                {"owner_id", s.owner_id},
                {"secret", s.secret_value},
                {"type", s.secret_type}
                });
            });

        svr.Delete(R"(/api/secrets/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
            int id = stoi(req.matches[1]);
            db.deleteSecret(id);
            sendSuccess(res, { {"deleted", id} });
            });

        /* ===== START ===== */

        cout << "✅ Сервер запущен: http://localhost:8080\n";
        svr.listen("0.0.0.0", 8080);
        db.close();
    }
    catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << endl;
    }
}
