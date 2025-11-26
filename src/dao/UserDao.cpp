#include "UserDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

#include "DBManager.h"

UserInfo UserDao::validateUser(const QString& username, const QString& passwordHash) {
    UserInfo user;
    QSqlDatabase db = DBManager::instance().getMainDatabase();
    if (!db.isOpen()) return user;

    QSqlQuery query(db);
    // 注意：数据库存的是 Hash 后的密码
    query.prepare("SELECT id, role, real_name FROM users WHERE username = :u AND password = :p");
    query.bindValue(":u", username);
    query.bindValue(":p", passwordHash);

    if (query.exec()) {
        if (query.next()) {
            user.id = query.value("id").toInt();
            user.username = username;
            user.role = query.value("role").toInt();
            user.realName = query.value("real_name").toString();
        }
    } else {
        qCritical() << "Login query failed:" << query.lastError().text();
    }
    return user;
}