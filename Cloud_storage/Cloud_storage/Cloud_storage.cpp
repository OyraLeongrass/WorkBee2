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
        Secret secret1("zephyr", "cloud_key", "wind123");
        Secret secret2("luminara", "light_token", "shine456");
        Secret secret3("orion", "star_pass", "galaxy789");
        Secret secret4("nyx", "shadow_code", "dark321");
        Secret secret5("aether", "spirit_lock", "pure654");

        int id1 = db.addSecret(secret1);
        int id2 = db.addSecret(secret2);
        int id3 = db.addSecret(secret3);
        int id4 = db.addSecret(secret4);
        int id5 = db.addSecret(secret5);

        cout << "\n3. Список всех секретов" << endl;
        for (const auto& s : db.getAllSecrets()) s.print();

        cout << "\n4. Получение секрета по пользователю 'luminara'" << endl;
        db.getSecretByUser("luminara").print();

        cout << "\n5. Получение секрета по ID" << endl;
        db.getSecretById(id1).print();

        cout << "\n6. Обновление секрета" << endl;
        Secret updated("zephyr", "new_cloud_key", "new_wind");
        db.updateSecret(id1, updated);
        db.getSecretById(id1).print();

        cout << "\n7. Поиск секретов по шаблону 'key'" << endl;
        for (const auto& s : db.searchSecrets("key")) s.print();

        cout << "\n8. Проверка существования пользователя 'orion'" << endl;
        cout << (db.userExists("orion") ? "ДА" : "НЕТ") << endl;

        cout << "\n9. Проверка существования секрета ID " << id2 << endl;
        cout << (db.secretExists(id2) ? "ДА" : "НЕТ") << endl;

        cout << "\n10. Аутентификация luminara" << endl;
        cout << (db.authenticate("luminara", "shine456") ? "УСПЕШНО" : "ОШИБКА") << endl;

        cout << "\n11. Статистика" << endl;
        db.getStatistics().print();

        cout << "\n12. Удаление секрета по ID (orion)" << endl;
        db.deleteSecret(id3);

        cout << "\n13. Удаление секрета по пользователю 'nyx'" << endl;
        db.deleteSecretByUser("nyx");

        cout << "\n14. Список после удаления" << endl;
        for (const auto& s : db.getAllSecrets()) s.print();

        cout << "\n15. Очистка всей таблицы" << endl;
        db.clearAllSecrets();
        cout << "\nСписок после удаления" << endl;
        for (const auto& s : db.getAllSecrets()) s.print();

        cout << "\n16. Закрытие базы данных" << endl;
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
