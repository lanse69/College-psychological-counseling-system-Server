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
     * @brief 获取医生的预约学生列表
     * 
     * @param doctorId 医生ID
     * @param errorMsg 错误信息输出
     * @return QJsonArray 预约学生列表
     */
    QJsonArray getDoctorPatients(QSqlDatabase db, int doctorId, QString &errorMsg);

    // 保存咨询报告
    bool saveReport(QSqlDatabase db, int appointmentId, int doctorId, const QString &content, const QString &tags, QString &errorMsg);

    // 删除已取消的预约
    bool deleteCancelledAppointment(QSqlDatabase db, int appointmentId, int operatorId, QString &errorMsg);

    /**
     * @brief 学生直接修改预约
     * @return true 修改成功, false 失败(errorMsg包含原因)
     */
    bool modifyAppointmentDirect(QSqlDatabase db, int studentId, int appointmentId, const QString &newDate, int newSlot, QString &errorMsg);

    // 医生发起修改请求 (仅更新状态和JSON，不改实际日期)
    bool createModifyRequest(QSqlDatabase db, int appointmentId, int doctorId, const QString &newDate, int newSlot, QString &errorMsg);

    // 学生处理修改请求 (同意=true, 拒绝=false)
    bool resolveModifyRequest(QSqlDatabase db, int appointmentId, int studentId, bool accept, QString &errorMsg);
};
