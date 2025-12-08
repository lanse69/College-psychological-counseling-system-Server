#include "UserDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>

#include "DBManager.h"

UserInfo UserDao::validateUser(
    const QString& username, const QString& passwordHash)
{
    UserInfo user;
    QSqlDatabase db = DBManager::instance().getMainDatabase();
    if (!db.isOpen()) {
        qWarning() << "数据库未连接，无法验证用户";
        return user;
    }

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
            qDebug() << "用户验证成功:" << username << "ID:" << user.id;
        } else {
            qDebug() << "用户验证失败，未找到匹配的用户:" << username;
        }
    } else {
        qCritical() << "登录查询失败:" << query.lastError().text();
    }
    return user;
}

bool UserDao::isUsernameExist(
    const QString& username)
{
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("SELECT count(*) FROM users WHERE username = :u");
    query.bindValue(":u", username);
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    qWarning() << "检查用户名存在性失败:" << query.lastError().text();
    return false;
}

int UserDao::addUser(
    const QString& username, const QString& passwordHash, int role, const QString& realName)
{
    QSqlDatabase db = DBManager::instance().getMainDatabase();
    QSqlQuery query(db);

    query.prepare("INSERT INTO users (username, password, role, real_name) " "VALUES (:u, :p, :r, "
                                                                             ":n) RETURNING id");
    query.bindValue(":u", username);
    query.bindValue(":p", passwordHash);
    query.bindValue(":r", role);
    query.bindValue(":n", realName);

    if (query.exec()) {
        if (query.next()) {
            int userId = query.value(0).toInt();
            qDebug() << "用户添加成功:" << username << "ID:" << userId;
            return userId;
        }
    } else {
        qCritical() << "添加用户失败:" << query.lastError().text();
    }
    return -1;
}

bool UserDao::addDoctorInfo(
    int userId, const QString& intro, const QString& specializedField)
{
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare(
        "INSERT INTO doctor_info (user_id, intro, specialized_field) VALUES (:id, :intro, :spec)");
    query.bindValue(":id", userId);
    query.bindValue(":intro", intro);
    query.bindValue(":spec", specializedField);

    if (!query.exec()) {
        qCritical() << "添加医生信息失败:" << query.lastError().text();
        return false;
    }
    qDebug() << "医生信息添加成功，用户ID:" << userId;
    return true;
}

bool UserDao::deleteUser(
    int userId)
{
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("DELETE FROM users WHERE id = :id");
    query.bindValue(":id", userId);

    if (!query.exec()) {
        qCritical() << "删除用户失败:" << query.lastError().text();
        return false;
    }
    qDebug() << "用户删除成功，ID:" << userId;
    return true;
}

int UserDao::getUserRole(
    int userId)
{
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("SELECT role FROM users WHERE id = :id");
    query.bindValue(":id", userId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qWarning() << "获取用户角色失败，用户ID:" << userId;
    return -1; // Not found
}

bool UserDao::updateBasicInfo(
    int userId, const QString& realName, const QString& passwordHash)
{
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
        qCritical() << "更新用户基本信息失败:" << query.lastError().text();
        return false;
    }
    qDebug() << "用户基本信息更新成功，ID:" << userId;
    return true;
}

bool UserDao::updateDoctorInfo(
    int userId, const QString& intro, const QString& spec)
{
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare(
        "UPDATE doctor_info SET intro = :intro, specialized_field = :spec WHERE user_id = :id");
    query.bindValue(":intro", intro);
    query.bindValue(":spec", spec);
    query.bindValue(":id", userId);

    if (!query.exec()) {
        qCritical() << "更新医生信息失败:" << query.lastError().text();
        return false;
    }
    qDebug() << "医生信息更新成功，用户ID:" << userId;
    return true;
}

QJsonArray UserDao::getAllUsers(
    int excludeId)
{
    QJsonArray list;
    QSqlQuery query(DBManager::instance().getMainDatabase());

    query.prepare(
        "SELECT id, username, real_name, role FROM users WHERE id != :eid ORDER BY id ASC");
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
        qDebug() << "获取所有用户成功，共" << list.size() << "个用户";
    } else {
        qWarning() << "获取所有用户失败:" << query.lastError().text();
    }
    return list;
}

QJsonArray UserDao::getDoctorList()
{
    QJsonArray list;
    QSqlDatabase db = DBManager::instance().getMainDatabase();
    
    // 确保连接有效
    if (!db.isOpen()) {
        const_cast<DBManager&>(DBManager::instance()).getMainDatabase().open();
    }

    if (!db.isOpen()) {
        qWarning() << "数据库未连接，无法获取医生列表";
        return list;
    }

    QSqlQuery query(db);

    QString sql = R"(
        SELECT u.id, u.real_name, u.username, d.intro, d.specialized_field 
        FROM users u 
        JOIN doctor_info d ON u.id = d.user_id 
        WHERE u.role = 2 
        ORDER BY u.id ASC
    )";

    if (query.exec(sql)) {
        while (query.next()) {
            QJsonObject obj;
            obj["id"] = query.value("id").toInt();
            obj["realName"] = query.value("real_name").toString();
            obj["username"] = query.value("username").toString();
            obj["intro"] = query.value("intro").toString();
            obj["specializedField"] = query.value("specialized_field").toString();
            list.append(obj);
        }
        qDebug() << "获取医生列表成功，共" << list.size() << "名医生";
    } else {
        qWarning() << "获取医生列表失败:" << query.lastError().text();
    }
    return list;
}

QJsonObject UserDao::getDoctorDetail(
    int doctorId)
{
    QJsonObject obj;
    QSqlDatabase db = DBManager::instance().getMainDatabase();
    if (!db.isOpen()) {
        qWarning() << "数据库未连接，无法获取医生详情";
        return obj;
    }

    QSqlQuery query(db);
    query
        .prepare("SELECT u.id, u.real_name, u.username, d.intro, d.specialized_field " "FROM users "
                                                                                       "u " "LEFT "
                                                                                            "JOIN "
                                                                                            "doctor"
                                                                                            "_info "
                                                                                            "d ON "
                                                                                            "u.id "
                                                                                            "= "
                                                                                            "d."
                                                                                            "user_"
                                                                                            "id " "WHERE u.id = :id AND u.role = 2");
    query.bindValue(":id", doctorId);

    if (query.exec() && query.next()) {
        obj["id"] = query.value("id").toInt();
        obj["realName"] = query.value("real_name").toString();
        obj["username"] = query.value("username").toString();
        obj["intro"] = query.value("intro").toString();
        obj["specializedField"] = query.value("specialized_field").toString();
        qDebug() << "获取医生详情成功:" << obj["realName"].toString();
    } else {
        qWarning() << "获取医生详情失败，ID:" << doctorId << "错误:" << query.lastError().text();
    }
    return obj;
}

bool UserDao::isDoctorExist(
    int doctorId)
{
    QSqlQuery query(DBManager::instance().getMainDatabase());
    query.prepare("SELECT COUNT(*) FROM users WHERE id = :id AND role = 2");
    query.bindValue(":id", doctorId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    qWarning() << "检查医生存在性失败:" << query.lastError().text();
    return false;
}
