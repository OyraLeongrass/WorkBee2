#include <iostream>
#include "DataBase.h"
#include <Windows.h>
using namespace std;

void demonstrateDatabase() {
      try {
        DataBase& db = DataBase::getInstance();
        cout << "1. Открытие базы данных" << endl;
        db.open("secrets.db");

        cout << "Версия SQLite: " << DataBase::getSQLiteVersion() << endl;
        cout << "\n2. Добавление секретов" << endl;
        Secret secret1("1", "cloud_key", "2026-01-08", "2026-02-01", "personal");
        Secret secret2("2", "light_token", "2026-01-08", "2026-03-01", "work");
        Secret secret3("3", "star_pass", "2026-01-08", "2026-04-01", "system");
        Secret secret4("4", "shadow_code", "2026-01-08", "2026-05-01", "test");
        Secret secret5("5", "spirit_lock", "2026-01-08", "2026-06-01", "archive");

        int id1 = db.addSecret(secret1);
        int id2 = db.addSecret(secret2);
        int id3 = db.addSecret(secret3);
        int id4 = db.addSecret(secret4);
        int id5 = db.addSecret(secret5);

        cout << "\n3. Список всех секретов" << endl;
        for (const auto& s : db.getAllSecrets()) s.print();

        cout << "\n4. Получение секрета по ID" << endl;
        db.getSecretById(id1).print();

        cout << "\n5. Обновление секрета" << endl;
        Secret updated("1", "new_cloud_key", "2026-01-09", "2026-02-15", "personal");
        db.updateSecret(id1, updated);
        db.getSecretById(id1).print();

        cout << "\n6. Поиск секретов по шаблону 'key'" << endl;
        for (const auto& s : db.searchSecrets("key")) s.print();

        cout << "\n7. Проверка существования пользователя с owner_id = '3'" << endl;
        cout << (db.userExists("3") ? "ДА" : "НЕТ") << endl;

        cout << "\n8. Проверка существования секрета ID " << id2 << endl;
        cout << (db.secretExists(id2) ? "ДА" : "НЕТ") << endl;

        cout << "\n9. Статистика" << endl;
        db.getStatistics().print();

        cout << "\n10. Удаление секрета по ID (star_pass)" << endl;
        db.deleteSecret(id3);

        cout << "\n11. Список после удаления" << endl;
        for (const auto& s : db.getAllSecrets()) s.print();

        cout << "\n12. Очистка всей таблицы" << endl;
        db.clearAllSecrets();
        cout << "\nСписок после удаления" << endl;
        for (const auto& s : db.getAllSecrets()) s.print();

        cout << "\n13. Закрытие базы данных" << endl;
        db.close();
    }
    catch (const DatabaseException& e) {
        cerr << "Ошибка базы данных: " << e.what() << endl;
    }
    catch (const exception& e) {
        cerr << "Общая ошибка: " << e.what() << endl;
    }
}


int main() {
    cout << "Тестирование базы данных секретов\n" << endl;
    demonstrateDatabase();
    cout << "\nНажмите Enter для выхода...";
    cin.get();
    return 0;
}
