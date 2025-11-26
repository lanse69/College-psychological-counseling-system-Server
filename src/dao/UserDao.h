#pragma once

#include <QString>

struct UserInfo {
    int id = -1;
    QString username;
    int role = 0;
    QString realName;
    bool isValid() const { return id != -1; }
};

class UserDao {
public:
    // 验证用户登录，返回用户信息
    static UserInfo validateUser(const QString& username, const QString& passwordHash);
};