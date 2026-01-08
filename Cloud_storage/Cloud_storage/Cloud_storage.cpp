#include <iostream>
#include "DataBase.h"
#include "httplib.h"
#include "json.hpp"
using json = nlohmann::json;
using namespace std;



int main() {
    cout << "Тестирование базы данных секретов\n" << endl;
    try {
        DataBase& db = DataBase::getInstance();
        db.open("secrets.db");
        httplib::Server svr;

        // Добавляем тестовые данные
        Secret secret1("1", "cloud_key", "2026-01-08", "2026-02-01", "personal");
        Secret secret2("2", "light_token", "2026-01-08", "2026-03-01", "work");
        Secret secret3("3", "star_pass", "2026-01-08", "2026-04-01", "system");
        Secret secret4("4", "shadow_code", "2026-01-08", "2026-05-01", "test");
        Secret secret5("5", "spirit_lock", "2026-01-08", "2026-06-01", "archive");

        db.addSecret(secret1);
        db.addSecret(secret2);
        db.addSecret(secret3);
        db.addSecret(secret4);
        db.addSecret(secret5);

        // Эндпоинт: список всех секретов
        svr.Get("/secrets", [&](const httplib::Request&, httplib::Response& res) {
            auto secrets = db.getAllSecrets();
            json j;
            for (const auto& s : secrets) {
                j.push_back({
                    {"id", s.id},
                    {"owner_id", s.owner_id},
                    {"secret", s.secret},
                    {"created_at", s.created_at},
                    {"expires_at", s.expires_at},
                    {"secret_type", s.secret_type}
                    });
            }
            res.set_content(j.dump(4), "application/json");
            });

        // Эндпоинт: получить секрет по ID
        svr.Get(R"(/secret/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
            int id = stoi(req.matches[1]);
            try {
                Secret s = db.getSecretById(id);
                json j = {
                    {"id", s.id},
                    {"owner_id", s.owner_id},
                    {"secret", s.secret},
                    {"created_at", s.created_at},
                    {"expires_at", s.expires_at},
                    {"secret_type", s.secret_type}
                };
                res.set_content(j.dump(4), "application/json");
            }
            catch (const DatabaseException& e) {
                res.status = 404;
                res.set_content(string("Ошибка: ") + e.what(), "text/plain");
            }
            });

        // Эндпоинт: добавить секрет (POST)
        svr.Post("/secret", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = json::parse(req.body);
                Secret s(
                    j["owner_id"].get<string>(),
                    j["secret"].get<string>(),
                    j["created_at"].get<string>(),
                    j["expires_at"].get<string>(),
                    j["secret_type"].get<string>()
                );
                int newId = db.addSecret(s);
                res.set_content("Секрет добавлен, ID: " + to_string(newId), "text/plain");
            }
            catch (const exception& e) {
                res.status = 400;
                res.set_content(string("Ошибка добавления: ") + e.what(), "text/plain");
            }
            });

        cout << "Сервер запущен на http://localhost:8080\n";
        svr.listen("0.0.0.0", 8080);

        db.clearAllSecrets();
        db.close();
    }
    catch (const DatabaseException& e) {
        cerr << "Ошибка базы данных: " << e.what() << endl;
    }
    catch (const exception& e) {
        cerr << "Общая ошибка: " << e.what() << endl;
    }
    cin.get();
    return 0;
}
