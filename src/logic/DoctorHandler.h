#pragma once

#include <QObject>
#include <QJsonObject>

#include "network/ClientSocket.h"

/**
 * @brief 医生相关业务处理器
 * 
 * 负责处理所有与医生相关的请求，包括：
 * - 获取医生列表
 * - 获取医生详细信息
 * - 医生信息管理
 */
class DoctorHandler
{
public:
    /**
     * @brief 处理获取医生列表请求
     * @param sender 请求发送者（ClientSocket）
     * @param request 请求JSON对象
     */
    static void handleGetDoctorList(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 处理获取医生详情请求
     * @param sender 请求发送者（ClientSocket）
     * @param request 请求JSON对象
     */
    static void handleGetDoctorDetail(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 处理学生端获取医生列表请求
     * @param sender 请求发送者（ClientSocket）
     * @param request 请求JSON对象
     */
    static void handleStudentGetDoctorList(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 处理医生获取预约列表请求
     * @param sender 请求发送者（ClientSocket）
     * @param request 请求JSON对象
     */
    static void handleGetAppointments(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 处理医生确认预约请求
     * @param sender 请求发送者（ClientSocket）
     * @param request 请求JSON对象
     */
    static void handleConfirmAppointment(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 处理医生拒绝预约请求
     * @param sender 请求发送者（ClientSocket）
     * @param request 请求JSON对象
     */
    static void handleRejectAppointment(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 处理医生提交咨询报告请求
     * @param sender 请求发送者（ClientSocket）
     * @param request 请求JSON对象
     */
    static void handleSubmitReport(ClientSocket* sender, const QJsonObject& request);
    static void handleCompleteConsultation(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 处理医生获取预约学生列表请求
     * @param sender 请求发送者（ClientSocket）
     * @param request 请求JSON对象
     */
    static void handleGetPatients(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 处理获取预约学生历史记录请求
     */
    static void handleGetPatientHistory(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 获取排班信息
     */
    static void handleGetSchedule(ClientSocket* sender, const QJsonObject& request);

    /**
     * @brief 更新排班信息
     */
    static void handleUpdateSchedule(ClientSocket* sender, const QJsonObject& request);


    static void handleDeleteBooking(ClientSocket* sender, const QJsonObject& request);

private:
    /**
     * @brief 发送成功响应
     * @param sender 客户端连接
     * @param cmd 命令类型
     * @param data 响应数据
     * @param msg 响应消息
     */
    static void sendSuccessResponse(ClientSocket* sender, int cmd, const QJsonValue& data, const QString& msg = "操作成功");

    /**
     * @brief 发送错误响应
     * @param sender 客户端连接
     * @param cmd 命令类型
     * @param code 错误码
     * @param msg 错误消息
     */
    static void sendErrorResponse(ClientSocket* sender, int cmd, int code, const QString& msg);
};
