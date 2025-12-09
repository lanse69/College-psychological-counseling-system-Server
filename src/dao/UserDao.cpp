#include "UserDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QRandomGenerator>

#include "DBManager.h"

// 生成随机盐 (16字节转Hex)
static QString generateSalt() {
    const int saltLength = 16;
    QByteArray saltData;
    saltData.resize(saltLength);
    // 使用 Qt6 全局随机生成器填满数据
    QRandomGenerator::global()->fillRange(
        reinterpret_cast<quint32*>(saltData.data()), 
        saltLength / sizeof(quint32)
    );
    return QString(saltData.toHex());
}

// 计算加盐哈希
static QString hashPassword(const QString& clientHash, const QString& salt) {
    // 组合规则：ClientHash + Salt
    QByteArray data = (clientHash + salt).toUtf8();
    return QString(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

UserInfo UserDao::validateUser(QSqlDatabase db, const QString& username, const QString& passwordHash)
{
    UserInfo user;

    if (!db.isOpen()) {
        qWarning() << "数据库未连接，无法验证用户";
        return user;
    }

    QSqlQuery query(db);
    
    query.prepare("SELECT id, role, real_name, password, salt FROM users WHERE username = :u");
    query.bindValue(":u", username);

    if (query.exec()) {
        if (query.next()) {
            QString dbHash = query.value("password").toString();
            QString dbSalt = query.value("salt").toString();

            QString checkHash = hashPassword(passwordHash, dbSalt);

            // 比对计算结果和数据库存储结果
            if (checkHash == dbHash) {
                user.id = query.value("id").toInt();
                user.username = username;
                user.role = query.value("role").toInt();
                user.realName = query.value("real_name").toString();
                qDebug() << "用户验证成功:" << username << "ID:" << user.id;
            } else {
                qDebug() << "密码错误:" << username;
            }
        } else {
            qDebug() << "用户不存在:" << username;
        }
    } else {
        qCritical() << "登录查询失败:" << query.lastError().text();
    }
    return user;
}

bool UserDao::isUsernameExist(QSqlDatabase db, const QString& username)
{
    QSqlQuery query(db);
    query.prepare("SELECT count(*) FROM users WHERE username = :u");
    query.bindValue(":u", username);
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    qWarning() << "检查用户名存在性失败:" << query.lastError().text();
    return false;
}

int UserDao::addUser(QSqlDatabase db, const QString& username, const QString& passwordHash, int role, const QString& realName)
{
    QSqlQuery query(db);

    QString salt = generateSalt();
    QString finalHash = hashPassword(passwordHash, salt);

    query.prepare("INSERT INTO users (username, password, salt, role, real_name) "
                  "VALUES (:u, :p, :s, :r, :n) RETURNING id");
    
    query.bindValue(":u", username);
    query.bindValue(":p", finalHash); // 存加盐后的哈希
    query.bindValue(":s", salt);      // 存盐
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

bool UserDao::addDoctorInfo(QSqlDatabase db, int userId, const QString& intro, const QString& specializedField)
{
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO doctor_info (user_id, intro, specialized_field) VALUES (:id, :intro, :spec)");
    query.bindValue(":id", userId);
    query.bindValue(":intro", intro);
    query.bindValue(":spec", specializedField);

    if (!query.exec()) {
        qCritical() << "添加医生信息失败:" << query.lastError().text();
        return false;
    }
    return true;
}

bool UserDao::deleteUser(QSqlDatabase db, int userId)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM users WHERE id = :id");
    query.bindValue(":id", userId);

    if (!query.exec()) {
        qCritical() << "删除用户失败:" << query.lastError().text();
        return false;
    }
    qDebug() << "用户删除成功，ID:" << userId;
    return true;
}

int UserDao::getUserRole(QSqlDatabase db, int userId)
{
    QSqlQuery query(db);
    query.prepare("SELECT role FROM users WHERE id = :id");
    query.bindValue(":id", userId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    qWarning() << "获取用户角色失败，用户ID:" << userId;
    return -1; // Not found
}

bool UserDao::updateBasicInfo(QSqlDatabase db, int userId, const QString& realName, const QString& passwordHash)
{
    QSqlQuery query(db);

    QString sql = "UPDATE users SET real_name = :n";
    
    QString salt, finalHash;
    
    if (!passwordHash.isEmpty()) {
        salt = generateSalt();
        finalHash = hashPassword(passwordHash, salt);
        
        sql += ", password = :p, salt = :s";
    }
    
    sql += " WHERE id = :id";

    query.prepare(sql);
    query.bindValue(":n", realName);
    query.bindValue(":id", userId);
    
    if (!passwordHash.isEmpty()) {
        query.bindValue(":p", finalHash);
        query.bindValue(":s", salt);
    }

    if (!query.exec()) {
        qCritical() << "更新用户基本信息失败:" << query.lastError().text();
        return false;
    }
    qDebug() << "用户基本信息更新成功，ID:" << userId;
    return true;
}

bool UserDao::updateDoctorInfo(QSqlDatabase db, int userId, const QString& intro, const QString& spec)
{
    QSqlQuery query(db);
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

QJsonArray UserDao::getAllUsers(QSqlDatabase db, int excludeId)
{
    QJsonArray list;
    QSqlQuery query(db);

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
        qWarning() << "获取所有用户失败:" << query.lastError().text();
    }
    return list;
}

QJsonArray UserDao::getDoctorList(QSqlDatabase db)
{
    QJsonArray list;
    
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
    } else {
        qWarning() << "获取医生列表失败:" << query.lastError().text();
    }
    return list;
}

QJsonObject UserDao::getDoctorDetail(QSqlDatabase db, int doctorId)
{
    QJsonObject obj;
    if (!db.isOpen()) {
        qWarning() << "数据库未连接，无法获取医生详情";
        return obj;
    }

    QSqlQuery query(db);
    query.prepare("SELECT u.id, u.real_name, u.username, d.intro, d.specialized_field FROM users u LEFT JOIN doctor_info d ON u.id = d.user_id " "WHERE u.id = :id AND u.role = 2");
    query.bindValue(":id", doctorId);

    if (query.exec() && query.next()) {
        obj["id"] = query.value("id").toInt();
        obj["realName"] = query.value("real_name").toString();
        obj["username"] = query.value("username").toString();
        obj["intro"] = query.value("intro").toString();
        obj["specializedField"] = query.value("specialized_field").toString();
    } else {
        qWarning() << "获取医生详情失败，ID:" << doctorId << "错误:" << query.lastError().text();
    }
    return obj;
}

bool UserDao::isDoctorExist(QSqlDatabase db, int doctorId)
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM users WHERE id = :id AND role = 2");
    query.bindValue(":id", doctorId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    qWarning() << "检查医生存在性失败:" << query.lastError().text();
    return false;
}