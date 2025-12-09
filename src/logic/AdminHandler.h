#pragma once

#include <QObject>
#include <QJsonObject>

class ClientSocket;

class AdminHandler : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 添加新用户 (CMD: ADMIN_ADD_USER)
     */
    static void handleAddUser(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 删除用户 (CMD: ADMIN_DEL_USER)
     */
    static void handleDeleteUser(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 修改用户信息 (CMD: UPDATE_USER_INFO by Admin)
     */
    static void handleUpdateUserInfo(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 获取统计数据 (CMD: GET_STATISTICS)
     */
    static void handleGetStatistics(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 获取用户列表 (CMD: ADMIN_GET_USER_LIST)
     */
    static void handleGetUserList(ClientSocket* sender, const QJsonObject& request);

private:
    AdminHandler() = default;
};