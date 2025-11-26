#pragma once

#include <QObject>
#include <QJsonObject>

class ClientSocket;

class BookingHandler : public QObject {
    Q_OBJECT
public:
    // 学生发起预约
    static void handleCreateBooking(ClientSocket* sender, const QJsonObject& request);
    
    // 获取我的预约
    static void handleGetMyBookings(ClientSocket* sender, const QJsonObject& request);
    
    // 取消预约 (如果是医生取消，需要推送通知给学生)
    static void handleCancelBooking(ClientSocket* sender, const QJsonObject& request);

    // 学生修改预约 (服务端校验医生时间表，无需医生人工同意)
    static void handleModifyBookingDirect(ClientSocket* sender, const QJsonObject& request);

    // 医生发起修改请求 (服务端转发给学生)
    static void handleModifyRequest(ClientSocket* sender, const QJsonObject& request);

    // 学生回复修改请求 (同意/拒绝)
    static void handleModifyReply(ClientSocket* sender, const QJsonObject& request);

private:
    BookingHandler() = default;
};