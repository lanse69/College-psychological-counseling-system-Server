#pragma once

#include <QString>
#include <QJsonArray>

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

    // 添加基础用户 (返回新生成的 ID，失败返回 -1)
    static int addUser(const QString& username, const QString& passwordHash, int role, const QString& realName);

    // 添加医生详细信息
    static bool addDoctorInfo(int userId, const QString& intro, const QString& specializedField);

    // 删除用户
    static bool deleteUser(int userId);

    // 检查用户名是否存在
    static bool isUsernameExist(const QString& username);

    // 获取用户角色 (返回 -1 表示用户不存在)
    static int getUserRole(int userId);

    // 更新基础信息 (passwordHash 为空时不修改密码)
    static bool updateBasicInfo(int userId, const QString& realName, const QString& passwordHash = "");

    // 更新医生详细信息
    static bool updateDoctorInfo(int userId, const QString& intro, const QString& spec);

    // 查询所有用户
    static QJsonArray getAllUsers(int excludeId = -1);
};
