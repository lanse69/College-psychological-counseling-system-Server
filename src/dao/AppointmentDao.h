#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>

class AppointmentDao : public QObject
{
    Q_OBJECT
public:
    explicit AppointmentDao(QObject *parent = nullptr);

    // 创建预约
    bool createAppointment(QSqlDatabase db, int studentId, int doctorId, const QString &date, int timeSlot, QString &errorMsg);

    // 获取学生的预约列表
    QJsonArray getStudentAppointments(QSqlDatabase db, int studentId, QString &errorMsg);

    // 获取医生的预约列表
    QJsonArray getDoctorAppointments(QSqlDatabase db, int doctorId, QString &errorMsg);

    // 取消预约
    bool cancelAppointment(QSqlDatabase db, int appointmentId, QString &errorMsg);

    // 更新预约状态
    bool updateAppointmentStatus(QSqlDatabase db, int appointmentId, int status, QString &errorMsg);

    // 确认预约（医生操作）
    bool confirmAppointment(QSqlDatabase db, int appointmentId, int doctorId, QString &errorMsg);

    // 拒绝预约（医生操作）
    bool rejectAppointment(QSqlDatabase db, int appointmentId, int doctorId, QString &errorMsg);

    // 完成咨询（医生操作）
    bool completeConsultation(QSqlDatabase db, int appointmentId, int doctorId, QString &errorMsg);

    // 检查时间段是否可用
    bool isTimeSlotAvailable(QSqlDatabase db, int doctorId, const QString &date, int timeSlot, QString &errorMsg);

    // 获取用户信息
    QJsonArray getUserInfo(QSqlDatabase db, int userId, QString &errorMsg);

    /**
     * @brief 获取特定医生和特定学生之间的预约历史
     * @param doctorId 医生ID
     * @param studentId 学生ID
     */
    QJsonArray getHistoryByDoctorAndStudent(QSqlDatabase db, int doctorId, int studentId, QString &errorMsg);

    /**
     * @brief 获取医生的预约学生列表（聚合查询）
     * 优化：直接在数据库层进行 GROUP BY 去重和统计，避免全量查询预约记录。
     * 
     * @param doctorId 医生ID
     * @param errorMsg 错误信息输出
     * @return QJsonArray 预约学生列表
     */
    QJsonArray getDoctorPatients(QSqlDatabase db, int doctorId, QString &errorMsg);
};
