#pragma once

#include <QString>
#include <QJsonArray>
#include <QSqlDatabase>

struct UserInfo
{
    int id = -1;
    QString username;
    int role = 0;
    QString realName;

    /**
     * @brief 检查用户信息是否有效
     * @return bool 如果id不为-1则返回true，表示用户信息有效
     */
    bool isValid() const { return id != -1; }

    /**
     * @brief 检查是否为学生
     * @return bool
     */
    bool isStudent() const { return role == 1; }

    /**
     * @brief 检查是否为医生
     * @return bool 
     */
    bool isDoctor() const { return role == 2; }

    /**
     * @brief 检查是否为管理员
     * @return bool 
     */
    bool isAdmin() const { return role == 3; }
};

class UserDao
{
public:
    /**
     * @brief 验证用户登录，返回用户信息
     * @param username 用户名
     * @param passwordHash 密码哈希值
     * @return UserInfo 用户信息，失败时id为-1
     */
    static UserInfo validateUser(QSqlDatabase db, const QString& username, const QString& passwordHash);

    /**
     * @brief 添加基础用户
     * @param username 用户名
     * @param passwordHash 密码哈希值
     * @param role 用户角色
     * @param realName 真实姓名
     * @return int 新用户的ID，失败返回-1
     */
    static int addUser(QSqlDatabase db, const QString& username,
                       const QString& passwordHash, int role,
                       const QString& realName);

    /**
     * @brief 添加医生详细信息
     * @param userId 用户ID
     * @param intro 医生简介
     * @param specializedField 专长领域
     * @return bool 操作是否成功
     */
    static bool addDoctorInfo(QSqlDatabase db, int userId, const QString& intro, const QString& specializedField);

    /**
     * @brief 删除用户
     * @param userId 用户ID
     * @return bool 操作是否成功
     */
    static bool deleteUser(QSqlDatabase db, int userId);

    /**
     * @brief 检查用户名是否存在
     * @param username 用户名
     * @return bool 用户名是否存在
     */
    static bool isUsernameExist(QSqlDatabase db, const QString& username);

    /**
     * @brief 获取用户角色
     * @param userId 用户ID
     * @return int 用户角色，-1表示用户不存在
     */
    static int getUserRole(QSqlDatabase db, int userId);

    /**
     * @brief 更新基础信息
     * @param userId 用户ID
     * @param realName 真实姓名
     * @param passwordHash 密码哈希值，为空时不修改密码
     * @return bool 操作是否成功
     */
    static bool updateBasicInfo(QSqlDatabase db, int userId,
                                const QString& realName,
                                const QString& passwordHash = "");

    /**
     * @brief 更新医生详细信息
     * @param userId 用户ID
     * @param intro 医生简介
     * @param spec 专长领域
     * @return bool 操作是否成功
     */
    static bool updateDoctorInfo(QSqlDatabase db, int userId, const QString& intro, const QString& spec);

    /**
     * @brief 查询所有用户
     * @param excludeId 要排除的用户ID，默认为-1（不排除）
     * @return QJsonArray 用户列表JSON数组
     */
    static QJsonArray getAllUsers(QSqlDatabase db, int excludeId = -1);

    /**
     * @brief 获取医生列表（包含医生详细信息）
     * @return QJsonArray 医生列表数组
     */
    static QJsonArray getDoctorList(QSqlDatabase db);

    /**
     * @brief 获取单个医生详细信息
     * @param doctorId 医生ID
     * @return QJsonObject 医生详细信息对象
     */
    static QJsonObject getDoctorDetail(QSqlDatabase db, int doctorId);

    /**
     * @brief 检查医生是否存在
     * @param doctorId 医生ID
     * @return bool 医生是否存在
     */
    static bool isDoctorExist(QSqlDatabase db, int doctorId);
};
