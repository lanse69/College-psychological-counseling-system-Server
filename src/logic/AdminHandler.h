#pragma once

#include <QObject>
#include <QJsonObject>

class ClientSocket;

class AdminHandler : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 添加新用户 (CMD: ADMIN_ADD_USER)
     * @param sender 操作者 (必须校验是否为管理员)
     * @param request 包含新用户的 username, password, role 等信息
     */
    static void handleAddUser(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 删除用户 (CMD: ADMIN_DEL_USER)
     * @param request 包含 targetUserId
     */
    static void handleDeleteUser(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 修改用户信息 (CMD: UPDATE_USER_INFO by Admin)
     * 管理员可以强制修改医生或学生的信息
     */
    static void handleUpdateUserInfo(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 获取统计数据 (CMD: GET_STATISTICS)
     * 
     * 注意：此函数的实现使用 QtConcurrent::run 
     * 和 DBManager::openThreadConnection 来避免阻塞主线程。
     * 
     * @param request 包含 statType
     */
    static void handleGetStatistics(ClientSocket* sender, const QJsonObject& request);

    static void handleGetUserList(ClientSocket* sender, const QJsonObject& request);

private:
    AdminHandler() = default;

    /**
     * @brief 辅助函数：校验发送者是否具有管理员权限
     */
    static bool checkAdminPermission(ClientSocket* sender);
};
