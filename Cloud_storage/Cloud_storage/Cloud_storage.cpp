#include <iostream>
#include "DataBase.h"
#include "httplib.h"
#include "json.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <functional>
using json = nlohmann::json;
using namespace std;



// Функция для получения текущей даты/времени
string getCurrentDateTime() {
    auto now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return string(buffer);
}

// Функция для получения даты через N дней
string getFutureDate(int days) {
    auto now = time(nullptr);
    now += days * 24 * 60 * 60;

    tm localTime;
    localtime_s(&localTime, &now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &localTime);
    return string(buffer);
}

// Простая функция хэширования (в реальном проекте используйте bcrypt)
string hashPassword(const string& password) {
    hash<string> hasher;
    return to_string(hasher(password));
}

// Вспомогательная функция для отправки JSON ответов
void sendJsonResponse(httplib::Response& res, int status, const json& data) {
    res.set_content(data.dump(4), "application/json");
    res.status = status;
}

// Вспомогательная функция для отправки ошибок
void sendError(httplib::Response& res, int status, const string& message) {
    json j = { {"error", message} };
    sendJsonResponse(res, status, j);
}

// Вспомогательная функция для успешного ответа
void sendSuccess(httplib::Response& res, const json& data = json::object()) {
    json j = { {"success", true} };
    if (!data.empty()) {
        j["data"] = data;
    }
    sendJsonResponse(res, 200, j);
}

int main() {
    try {
        // Инициализация базы данных
        DataBase& db = DataBase::getInstance();
        db.open("secrets.db");
        httplib::Server svr;

        // Включение CORS для работы с веб-интерфейсом
        svr.set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
            });

        // Обработка OPTIONS запросов (для CORS)
        svr.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
            res.status = 200;
            });

        // Корневой эндпоинт - информация о сервере
        svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
            json j = {
                {"service", "Secrets Management Server"},
                {"version", "1.0.0"},
                {"description", "API для управления пользователями и секретами"},
                {"endpoints", {
                    {"POST   /api/users", "Создать нового пользователя"},
                    {"GET    /api/users", "Получить всех пользователей"},
                    {"GET    /api/users/:id", "Получить пользователя по ID"},
                    {"POST   /api/auth/login", "Аутентификация пользователя"},
                    {"POST   /api/secrets", "Создать новый секрет"},
                    {"GET    /api/secrets", "Получить все секреты"},
                    {"GET    /api/secrets/:id", "Получить секрет по ID"},
                    {"GET    /api/users/:id/secrets", "Получить секреты пользователя"},
                    {"PUT    /api/secrets/:id", "Обновить секрет"},
                    {"DELETE /api/secrets/:id", "Удалить секрет"},
                    {"GET    /api/search/secrets", "Поиск секретов"},
                    {"GET    /api/stats", "Статистика системы"}
                }}
            };
            sendJsonResponse(res, 200, j);
            });
        // Создать нового пользователя
        svr.Post("/api/users", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                if (req.body.empty()) {
                    sendError(res, 400, "Тело запроса пустое");
                    return;
                }

                auto j = json::parse(req.body);

                // Проверяем обязательные поля
                if (!j.contains("username") || !j.contains("email") || !j.contains("password")) {
                    sendError(res, 400, "Отсутствуют обязательные поля: username, email, password");
                    return;
                }

                string username = j["username"].get<string>();
                string email = j["email"].get<string>();
                string password = j["password"].get<string>();
                string role = j.value("role", "user");

                // Валидация данных
                if (username.empty() || email.empty() || password.empty()) {
                    sendError(res, 400, "Поля не могут быть пустыми");
                    return;
                }

                // Проверяем, существует ли пользователь
                if (db.userExists(username)) {
                    sendError(res, 409, "Пользователь с таким именем уже существует");
                    return;
                }

                // Хэшируем пароль
                string password_hash = hashPassword(password);

                // Создаем пользователя
                User newUser(username, email, password_hash, role);
                int userId = db.addUser(newUser);

                json response = {
                    {"message", "Пользователь успешно создан"},
                    {"user_id", userId},
                    {"username", username},
                    {"email", email},
                    {"role", role}
                };

                sendSuccess(res, response);

            }
            catch (const json::exception& e) {
                sendError(res, 400, string("Ошибка разбора JSON: ") + e.what());
            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            catch (const exception& e) {
                sendError(res, 500, string("Внутренняя ошибка: ") + e.what());
            }
            });

        // Получить всех пользователей
        svr.Get("/api/users", [&](const httplib::Request&, httplib::Response& res) {
            try {
                vector<User> users = db.getAllUsers();

                json usersArray = json::array();
                for (const auto& user : users) {
                    usersArray.push_back({
                        {"id", user.id},
                        {"username", user.username},
                        {"email", user.email},
                        {"role", user.role}
                        });
                }

                sendSuccess(res, { {"users", usersArray}, {"count", users.size()} });

            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            });

        // Получить пользователя по ID
        svr.Get(R"(/api/users/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                int userId = stoi(req.matches[1]);
                User user = db.getUserById(userId);

                json userData = {
                    {"id", user.id},
                    {"username", user.username},
                    {"email", user.email},
                    {"role", user.role}
                };

                sendSuccess(res, userData);

            }
            catch (const DatabaseException& e) {
                sendError(res, 404, "Пользователь не найден");
            }
            catch (...) {
                sendError(res, 400, "Неверный ID пользователя");
            }
            });
        // Аутентификация пользователя
        svr.Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = json::parse(req.body);

                if (!j.contains("username") || !j.contains("password")) {
                    sendError(res, 400, "Отсутствуют обязательные поля: username, password");
                    return;
                }

                string username = j["username"].get<string>();
                string password = j["password"].get<string>();

                // Хэшируем пароль для проверки
                string password_hash = hashPassword(password);

                // Аутентифицируем пользователя
                bool authenticated = db.authenticate(username, password_hash);

                if (authenticated) {
                    // Получаем информацию о пользователе
                    User user = db.getUserByUsername(username);

                    json response = {
                        {"message", "Аутентификация успешна"},
                        {"user", {
                            {"id", user.id},
                            {"username", user.username},
                            {"email", user.email},
                            {"role", user.role}
                        }},
                        {"token", "demo_token_" + to_string(user.id)} // В реальном проекте используйте JWT
                    };

                    sendSuccess(res, response);
                }
                else {
                    sendError(res, 401, "Неверное имя пользователя или пароль");
                }

            }
            catch (const json::exception& e) {
                sendError(res, 400, string("Ошибка разбора JSON: ") + e.what());
            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            });
        // Создать новый секрет
        svr.Post("/api/secrets", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                auto j = json::parse(req.body);

                // Проверяем обязательные поля
                if (!j.contains("owner_id") || !j.contains("secret") || !j.contains("secret_type")) {
                    sendError(res, 400, "Отсутствуют обязательные поля: owner_id, secret, secret_type");
                    return;
                }

                int owner_id = j["owner_id"].get<int>();
                string secret = j["secret"].get<string>();
                string secret_type = j["secret_type"].get<string>();
                string description = j.value("description", "");

                // Автоматически определяем даты
                string created_at = getCurrentDateTime();
                string expires_at = j.value("expires_at", "");

                // Если не указана дата истечения, ставим 90 дней по умолчанию
                if (expires_at.empty()) {
                    int expires_in_days = j.value("expires_in_days", 90);
                    expires_at = getFutureDate(expires_in_days);
                }

                // Создаем секрет
                Secret newSecret(owner_id, secret, created_at, expires_at, secret_type);
                int secretId = db.addSecret(newSecret);

                json response = {
                    {"message", "Секрет успешно создан"},
                    {"secret_id", secretId},
                    {"owner_id", owner_id},
                    {"secret_type", secret_type},
                    {"created_at", created_at},
                    {"expires_at", expires_at},
                    {"description", description}
                };

                sendSuccess(res, response);

            }
            catch (const json::exception& e) {
                sendError(res, 400, string("Ошибка разбора JSON: ") + e.what());
            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            });

        // Получить все секреты
        svr.Get("/api/secrets", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                vector<Secret> secrets = db.getAllSecretsWithOwners();

                json secretsArray = json::array();
                for (const auto& secret : secrets) {
                    json secretData = {
                        {"id", secret.id},
                        {"owner_id", secret.owner_id},
                        {"secret", secret.secret},
                        {"secret_type", secret.secret_type},
                        {"created_at", secret.created_at},
                        {"expires_at", secret.expires_at}
                    };

                    // Добавляем информацию о владельце, если есть
                    if (secret.owner) {
                        secretData["owner"] = {
                            {"username", secret.owner->username},
                            {"email", secret.owner->email},
                            {"role", secret.owner->role}
                        };
                    }

                    secretsArray.push_back(secretData);
                }

                sendSuccess(res, { {"secrets", secretsArray}, {"count", secrets.size()} });

            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            });

        // Получить секрет по ID
        svr.Get(R"(/api/secrets/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                int secretId = stoi(req.matches[1]);
                Secret secret = db.getSecretById(secretId);

                json secretData = {
                    {"id", secret.id},
                    {"owner_id", secret.owner_id},
                    {"secret", secret.secret},
                    {"secret_type", secret.secret_type},
                    {"created_at", secret.created_at},
                    {"expires_at", secret.expires_at}
                };

                // Добавляем информацию о владельце, если есть
                if (secret.owner) {
                    secretData["owner"] = {
                        {"username", secret.owner->username},
                        {"email", secret.owner->email},
                        {"role", secret.owner->role}
                    };
                }

                sendSuccess(res, secretData);

            }
            catch (const DatabaseException& e) {
                sendError(res, 404, "Секрет не найден");
            }
            catch (...) {
                sendError(res, 400, "Неверный ID секрета");
            }
            });

        // Получить секреты пользователя
        svr.Get(R"(/api/users/(\d+)/secrets)", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                int userId = stoi(req.matches[1]);
                vector<Secret> secrets = db.getSecretsByUser(userId);

                json secretsArray = json::array();
                for (const auto& secret : secrets) {
                    secretsArray.push_back({
                        {"id", secret.id},
                        {"owner_id", secret.owner_id},
                        {"secret", secret.secret},
                        {"secret_type", secret.secret_type},
                        {"created_at", secret.created_at},
                        {"expires_at", secret.expires_at}
                        });
                }

                json response = {
                    {"user_id", userId},
                    {"secrets", secretsArray},
                    {"count", secrets.size()}
                };

                sendSuccess(res, response);

            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            catch (...) {
                sendError(res, 400, "Неверный ID пользователя");
            }
            });

        // Обновить секрет
        svr.Put(R"(/api/secrets/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                int secretId = stoi(req.matches[1]);
                auto j = json::parse(req.body);

                // Проверяем существование секрета
                if (!db.secretExists(secretId)) {
                    sendError(res, 404, "Секрет не найден");
                    return;
                }

                // Получаем текущий секрет
                Secret currentSecret = db.getSecretById(secretId);

                // Обновляем поля, если они предоставлены
                int owner_id = j.value("owner_id", currentSecret.owner_id);
                string secret = j.value("secret", currentSecret.secret);
                string secret_type = j.value("secret_type", currentSecret.secret_type);
                string expires_at = j.value("expires_at", currentSecret.expires_at);
                string created_at = getCurrentDateTime();

                // Обновляем секрет
                Secret updatedSecret(owner_id, secret, created_at, expires_at, secret_type);
                bool success = db.updateSecret(secretId, updatedSecret);

                if (success) {
                    json response = {
                        {"message", "Секрет успешно обновлен"},
                        {"secret_id", secretId},
                        {"updated_at", created_at}
                    };

                    sendSuccess(res, response);
                }
                else {
                    sendError(res, 500, "Не удалось обновить секрет");
                }

            }
            catch (const json::exception& e) {
                sendError(res, 400, string("Ошибка разбора JSON: ") + e.what());
            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            catch (...) {
                sendError(res, 400, "Неверный ID секрета");
            }
            });

        // Удалить секрет
        svr.Delete(R"(/api/secrets/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                int secretId = stoi(req.matches[1]);

                // Проверяем существование секрета
                if (!db.secretExists(secretId)) {
                    sendError(res, 404, "Секрет не найден");
                    return;
                }

                bool success = db.deleteSecret(secretId);

                if (success) {
                    json response = {
                        {"message", "Секрет успешно удален"},
                        {"secret_id", secretId}
                    };

                    sendSuccess(res, response);
                }
                else {
                    sendError(res, 500, "Не удалось удалить секрет");
                }

            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            catch (...) {
                sendError(res, 400, "Неверный ID секрета");
            }
            });

        // ==================== ПОИСК ====================

        // Поиск секретов
        svr.Get("/api/search/secrets", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                string query = req.get_param_value("q");

                if (query.empty()) {
                    sendError(res, 400, "Параметр поиска 'q' обязателен");
                    return;
                }

                vector<Secret> secrets = db.searchSecrets(query);

                json secretsArray = json::array();
                for (const auto& secret : secrets) {
                    secretsArray.push_back({
                        {"id", secret.id},
                        {"owner_id", secret.owner_id},
                        {"secret", secret.secret},
                        {"secret_type", secret.secret_type},
                        {"created_at", secret.created_at},
                        {"expires_at", secret.expires_at}
                        });
                }

                json response = {
                    {"query", query},
                    {"secrets", secretsArray},
                    {"count", secrets.size()}
                };

                sendSuccess(res, response);

            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            });
        // Получить статистику
        svr.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
            try {
                auto stats = db.getStatistics();

                json response = {
                    {"total_secrets", stats.totalSecrets},
                    {"unique_users", stats.uniqueUsers},
                    {"newest_user", stats.newestUser},
                    {"oldest_user", stats.oldestUser},
                    {"server_time", getCurrentDateTime()},
                    {"sqlite_version", db.getSQLiteVersion()}
                };

                sendSuccess(res, response);

            }
            catch (const DatabaseException& e) {
                sendError(res, 500, string("Ошибка базы данных: ") + e.what());
            }
            });

        // Запуск сервера
        cout << "Сервер запущен на http://localhost:8080" << endl;
        cout << "\nДоступные эндпоинты:" << endl;
        cout << "  GET    /                    - информация о сервере" << endl;
        cout << "  POST   /api/users           - создать пользователя" << endl;
        cout << "  GET    /api/users           - все пользователи" << endl;
        cout << "  GET    /api/users/:id       - пользователь по ID" << endl;
        cout << "  POST   /api/auth/login      - аутентификация" << endl;
        cout << "  POST   /api/secrets         - создать секрет" << endl;
        cout << "  GET    /api/secrets         - все секреты" << endl;
        cout << "  GET    /api/secrets/:id     - секрет по ID" << endl;
        cout << "  GET    /api/users/:id/secrets - секреты пользователя" << endl;
        cout << "  PUT    /api/secrets/:id     - обновить секрет" << endl;
        cout << "  DELETE /api/secrets/:id     - удалить секрет" << endl;
        cout << "  GET    /api/search/secrets  - поиск секретов" << endl;
        cout << "  GET    /api/stats           - статистика" << endl;
        cout << "\nПримеры использования с curl:" << endl;
        cout << "  Создать пользователя:" << endl;
        cout << "    curl -X POST http://localhost:8080/api/users \\" << endl;
        cout << "      -H \"Content-Type: application/json\" \\" << endl;
        cout << "      -d '{\"username\":\"test\",\"email\":\"test@test.com\",\"password\":\"123\"}'" << endl;
        cout << "\n  Создать секрет:" << endl;
        cout << "    curl -X POST http://localhost:8080/api/secrets \\" << endl;
        cout << "      -H \"Content-Type: application/json\" \\" << endl;
        cout << "      -d '{\"owner_id\":1,\"secret\":\"mypassword\",\"secret_type\":\"password\"}'" << endl;
        cout << "\nДля остановки сервера нажмите Ctrl+C\n" << endl;

        svr.listen("0.0.0.0", 8080);

        db.close();

    }
    catch (const DatabaseException& e) {
        cerr << "❌ Ошибка базы данных: " << e.what() << endl;
        return 1;
    }
    catch (const exception& e) {
        cerr << "❌ Ошибка: " << e.what() << endl;
        return 1;
    }

    return 0;
}