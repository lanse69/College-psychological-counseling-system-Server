#include "UserDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QJsonObject>

#include "DBManager.h"

UserInfo UserDao::validateUser(const QString& username, const QString& passwordHash) {
    UserInfo user;
    QSqlDatabase db = DBManager::instance().getMainDatabase();
    if (!db.isOpen()) return user;

    QSqlQuery query(db);
    // 数据库存的是 Hash 后的密码
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

bool UserDao::isUsernameExist(const QString& username) {
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("SELECT count(*) FROM users WHERE username = :u");
    query.bindValue(":u", username);
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

int UserDao::addUser(const QString& username, const QString& passwordHash, int role, const QString& realName) {
    QSqlDatabase db = DBManager::instance().getMainDatabase();
    QSqlQuery query(db);

    query.prepare("INSERT INTO users (username, password, role, real_name) "
                  "VALUES (:u, :p, :r, :n) RETURNING id");
    query.bindValue(":u", username);
    query.bindValue(":p", passwordHash);
    query.bindValue(":r", role);
    query.bindValue(":n", realName);

    if (query.exec()) {
        if (query.next()) {
            return query.value(0).toInt();
        }
    } else {
        qCritical() << "Add User Failed:" << query.lastError().text();
    }
    return -1;
}

bool UserDao::addDoctorInfo(int userId, const QString& intro, const QString& specializedField) {
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("INSERT INTO doctor_info (user_id, intro, specialized_field) VALUES (:id, :intro, :spec)");
    query.bindValue(":id", userId);
    query.bindValue(":intro", intro);
    query.bindValue(":spec", specializedField);

    if (!query.exec()) {
        qCritical() << "Add Doctor Info Failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool UserDao::deleteUser(int userId) {
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("DELETE FROM users WHERE id = :id");
    query.bindValue(":id", userId);

    if (!query.exec()) {
        qCritical() << "Delete User Failed:" << query.lastError().text();
        return false;
    }
    return true;
}

int UserDao::getUserRole(int userId) {
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("SELECT role FROM users WHERE id = :id");
    query.bindValue(":id", userId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return -1; // Not found
}

bool UserDao::updateBasicInfo(int userId, const QString& realName, const QString& passwordHash) {
    QSqlQuery query(DBManager::instance().getMainDatabase());

    QString sql = "UPDATE users SET real_name = :n";
    if (!passwordHash.isEmpty()) {
        sql += ", password = :p";
    }
    sql += " WHERE id = :id";

    query.prepare(sql);
    query.bindValue(":n", realName);
    query.bindValue(":id", userId);
    if (!passwordHash.isEmpty()) {
        query.bindValue(":p", passwordHash);
    }

    if (!query.exec()) {
        qCritical() << "Update User Basic Info Failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool UserDao::updateDoctorInfo(int userId, const QString& intro, const QString& spec) {
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("UPDATE doctor_info SET intro = :intro, specialized_field = :spec WHERE user_id = :id");
    query.bindValue(":intro", intro);
    query.bindValue(":spec", spec);
    query.bindValue(":id", userId);

    if (!query.exec()) {
        qCritical() << "Update Doctor Info Failed:" << query.lastError().text();
        return false;
    }
    return true;
}

QJsonArray UserDao::getAllUsers(int excludeId) {
    QJsonArray list;
    QSqlQuery query(DBManager::instance().getMainDatabase());

    query.prepare("SELECT id, username, real_name, role FROM users WHERE id != :eid ORDER BY id ASC");
    query.bindValue(":eid", excludeId);

    if (query.exec()) {
        while (query.next()) {
            QJsonObject obj;
            obj["id"] = query.value("id").toInt();
            obj["username"] = query.value("username").toString();
            obj["realName"] = query.value("real_name").toString();
            obj["role"] = query.value("role").toInt();
            list.append(obj);
        }
    } else {
        qWarning() << "Get All Users Failed:" << query.lastError().text();
    }
    return list;
}
