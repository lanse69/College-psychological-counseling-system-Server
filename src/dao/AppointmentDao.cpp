#include "AppointmentDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QDateTime>

#include "DBManager.h"

AppointmentDao::AppointmentDao(QObject *parent) : QObject(parent) {}

bool AppointmentDao::createAppointment(QSqlDatabase db, int studentId, int doctorId, const QString &date, int timeSlot, QString &errorMsg) {
    if (!db.isValid() || !db.isOpen()) {
        errorMsg = "数据库未连接";
        return false;
    }

    // 开启事务
    if (!db.transaction()) {
        errorMsg = "启动事务失败: " + db.lastError().text();
        return false;
    }

    QSqlQuery query(db);

    // 检查预约表冲突 (appointments)
    // status != 3 表示未取消的预约都算占用
    QString checkSql = "SELECT id FROM appointments WHERE doctor_id = ? AND date = ? AND time_slot = ? AND status != 3";
    query.prepare(checkSql);
    query.addBindValue(doctorId);
    query.addBindValue(date);
    query.addBindValue(timeSlot);

    if (!query.exec()) {
        db.rollback();
        errorMsg = "检查预约冲突失败: " + query.lastError().text();
        return false;
    }

    if (query.next()) {
        db.rollback();
        errorMsg = "手慢了，该时间段刚被抢走";
        return false;
    }

    // 检查排班表冲突 (schedules) 并 确保排班记录存在
    QString initScheduleSql = "INSERT INTO schedules (doctor_id, date, time_slot_flags) VALUES (?, ?, 0) ON CONFLICT (doctor_id, date) DO NOTHING";
    query.prepare(initScheduleSql);
    query.addBindValue(doctorId);
    query.addBindValue(date);
    if (!query.exec()) {
        db.rollback();
        errorMsg = "初始化排班失败: " + query.lastError().text();
        return false;
    }

    // 检查掩码位是否被占用
    QString checkMaskSql = "SELECT time_slot_flags FROM schedules WHERE doctor_id = ? AND date = ?";
    query.prepare(checkMaskSql);
    query.addBindValue(doctorId);
    query.addBindValue(date);
    if (query.exec() && query.next()) {
        int flags = query.value(0).toInt();
        if ((flags >> timeSlot) & 1) {
            db.rollback();
            errorMsg = "医生该时段已设置为忙碌/休息";
            return false;
        }
    }

    // 执行插入预约 (Status 直接设为 1: 已确认)
    QString insertSql = "INSERT INTO appointments (student_id, doctor_id, date, time_slot, status, create_time) "
                        "VALUES (?, ?, ?, ?, ?, ?)";
    query.prepare(insertSql);
    query.addBindValue(studentId);
    query.addBindValue(doctorId);
    query.addBindValue(date);
    query.addBindValue(timeSlot);
    query.addBindValue(1);
    query.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!query.exec()) {
        db.rollback();
        errorMsg = "创建预约失败: " + query.lastError().text();
        return false;
    }

    // 更新排班表掩码 (标记该位置为忙碌)
    // 使用位运算 OR: time_slot_flags | (1 << timeSlot)
    QString updateMaskSql = "UPDATE schedules SET time_slot_flags = time_slot_flags | (1 << ?) WHERE doctor_id = ? AND date = ?";
    query.prepare(updateMaskSql);
    query.addBindValue(timeSlot);
    query.addBindValue(doctorId);
    query.addBindValue(date);

    if (!query.exec()) {
        db.rollback();
        errorMsg = "更新排班状态失败: " + query.lastError().text();
        return false;
    }

    // 提交事务
    if (!db.commit()) {
        db.rollback();
        errorMsg = "提交事务失败: " + db.lastError().text();
        return false;
    }

    return true;
}

QJsonArray AppointmentDao::getStudentAppointments(QSqlDatabase db, int studentId, QString &errorMsg)
{
    QJsonArray result;

    if (!db.isValid()) {
        errorMsg = "数据库连接无效";
        return result;
    }

    if (!db.isOpen()) {
        if (!db.open()) {
            errorMsg = "数据库连接失败: " + db.lastError().text();
            return result;
        }
    }

    QSqlQuery query(db);
    QString sql
        = "SELECT a.id, a.student_id, a.doctor_id, a.date, a.time_slot, a.status, a.create_time, "
          "a.modify_request_json, u.real_name as doctor_name, d.specialized_field FROM "
          "appointments a JOIN users u ON a.doctor_id = u.id JOIN doctor_info d ON u.id = "
          "d.user_id WHERE a.student_id = ? ORDER BY a.date DESC, a.time_slot DESC";

    if (!query.prepare(sql)) {
        errorMsg = "SQL准备失败: " + query.lastError().text();
        return result;
    }

    query.addBindValue(studentId);

    if (!query.exec()) {
        errorMsg = "获取预约列表失败: " + query.lastError().text();
        return result;
    }

    while (query.next()) {
        QJsonObject appointment;
        appointment["id"] = query.value("id").toInt();
        appointment["studentId"] = query.value("student_id").toInt();
        appointment["doctorId"] = query.value("doctor_id").toInt();
        appointment["appointmentDate"] = query.value("date").toString();
        appointment["timeSlot"] = query.value("time_slot").toInt();
        appointment["status"] = query.value("status").toInt();
        appointment["doctorName"] = query.value("doctor_name").toString();
        appointment["specializedField"] = query.value("specialized_field").toString();
        appointment["createdAt"] = query.value("create_time").toString();

        result.append(appointment);
    }

    query.finish(); // 显式结束查询

    return result;
}

QJsonArray AppointmentDao::getDoctorAppointments(QSqlDatabase db, int doctorId, QString &errorMsg)
{
    QJsonArray result;

    if (!db.isValid()) {
        errorMsg = "数据库连接无效";
        return result;
    }

    if (!db.isOpen()) {
        if (!db.open()) {
            errorMsg = "数据库连接失败: " + db.lastError().text();
            return result;
        }
    }

    QSqlQuery query(db);
    QString sql
        = "SELECT a.id, a.student_id, a.doctor_id, a.date, a.time_slot, a.status, a.create_time, "
          "a.modify_request_json, u.real_name as student_name FROM appointments a JOIN users u ON "
          "a.student_id = u.id WHERE a.doctor_id = ? ORDER BY a.date DESC, a.time_slot DESC";

    if (!query.prepare(sql)) {
        errorMsg = "SQL准备失败: " + query.lastError().text();
        return result;
    }

    query.addBindValue(doctorId);

    if (!query.exec()) {
        errorMsg = "获取预约列表失败: " + query.lastError().text();
        qWarning() << errorMsg;
        return result;
    }

    while (query.next()) {
        QJsonObject appointment;
        appointment["id"] = query.value("id").toInt();
        appointment["studentId"] = query.value("student_id").toInt();
        appointment["doctorId"] = query.value("doctor_id").toInt();
        appointment["appointmentDate"] = query.value("date").toString();
        appointment["timeSlot"] = query.value("time_slot").toInt();
        appointment["status"] = query.value("status").toInt();
        appointment["studentName"] = query.value("student_name").toString();
        appointment["createdAt"] = query.value("create_time").toString();

        result.append(appointment);
    }

    return result;
}


bool AppointmentDao::cancelAppointment(QSqlDatabase db, int appointmentId, QString &errorMsg)
{
    if (!db.isValid() || !db.isOpen()) {
        errorMsg = "数据库未连接";
        return false;
    }

    if (!db.transaction()) {
        errorMsg = "事务启动失败";
        return false;
    }

    QSqlQuery query(db);

    // 先查询该预约的信息 (doctor_id, date, time_slot)，释放排班
    QString querySql = "SELECT doctor_id, date, time_slot FROM appointments WHERE id = ?";
    query.prepare(querySql);
    query.addBindValue(appointmentId);
    
    int doctorId = 0;
    QString dateStr;
    int timeSlot = 0;

    if (query.exec() && query.next()) {
        doctorId = query.value("doctor_id").toInt();
        dateStr = query.value("date").toString();
        timeSlot = query.value("time_slot").toInt();
    } else {
        db.rollback();
        errorMsg = "预约不存在";
        return false;
    }

    // 更新预约状态为 3 (已取消)
    QString updateSql = "UPDATE appointments SET status = 3 WHERE id = ?";
    query.prepare(updateSql);
    query.addBindValue(appointmentId);

    if (!query.exec()) {
        db.rollback();
        errorMsg = "取消预约失败: " + query.lastError().text();
        return false;
    }

    // 释放排班表 (Schedules) 的掩码
    // 使用位运算 AND NOT: time_slot_flags & ~(1 << timeSlot)
    QString releaseSql = "UPDATE schedules SET time_slot_flags = time_slot_flags & ~(1 << ?) WHERE doctor_id = ? AND date = ?";
    query.prepare(releaseSql);
    query.addBindValue(timeSlot);
    query.addBindValue(doctorId);
    query.addBindValue(dateStr);

    if (!query.exec()) {
        db.rollback();
        errorMsg = "释放排班失败: " + query.lastError().text();
        return false;
    }

    if (!db.commit()) {
        db.rollback();
        errorMsg = "事务提交失败";
        return false;
    }

    return true;
}

bool AppointmentDao::updateAppointmentStatus(QSqlDatabase db, int appointmentId, int status, QString &errorMsg)
{
    if (!db.isValid()) {
        errorMsg = "数据库连接无效";
        return false;
    }

    QSqlQuery query(db);
    QString sql = "UPDATE appointments SET status = ? WHERE id = ?";
    query.prepare(sql);
    query.addBindValue(status);
    query.addBindValue(appointmentId);

    if (!query.exec()) {
        errorMsg = "更新预约状态失败: " + query.lastError().text();
        return false;
    }

    return true;
}

bool AppointmentDao::isTimeSlotAvailable(QSqlDatabase db, int doctorId, const QString &date, int timeSlot, QString &errorMsg)
{
    if (!db.isValid()) {
        errorMsg = "数据库连接无效";
        return false;
    }

    QSqlQuery query(db);
    QString sql = "SELECT id FROM appointments WHERE doctor_id = ? AND date = ? AND time_slot = ? "
                  "AND status != 3";
    query.prepare(sql);
    query.addBindValue(doctorId);
    query.addBindValue(date);
    query.addBindValue(timeSlot);

    if (!query.exec()) {
        errorMsg = "检查时间段可用性失败: " + query.lastError().text();
        return false;
    }

    return !query.next(); // 如果没有记录，则时间段可用
}

QJsonArray AppointmentDao::getUserInfo(QSqlDatabase db, int userId, QString &errorMsg)
{
    QJsonArray result;

    if (!db.isValid()) {
        errorMsg = "数据库连接无效";
        return result;
    }

    if (!db.isOpen()) {
        if (!db.open()) {
            errorMsg = "数据库连接失败: " + db.lastError().text();
            return result;
        }
    }

    QSqlQuery query(db);
    QString sql = "SELECT id, username, role, real_name FROM users WHERE id = ?";

    if (!query.prepare(sql)) {
        errorMsg = "SQL准备失败: " + query.lastError().text();
        return result;
    }

    query.addBindValue(userId);

    if (!query.exec()) {
        errorMsg = "查询用户信息失败: " + query.lastError().text();
        return result;
    }

    if (query.next()) {
        QJsonObject user;
        user["id"] = query.value("id").toInt();
        user["username"] = query.value("username").toString();
        user["role"] = query.value("role").toInt();
        user["realName"] = query.value("real_name").toString();
        result.append(user);
    }

    query.finish(); // 显式结束查询

    return result;
}

bool AppointmentDao::confirmAppointment(QSqlDatabase db, int appointmentId, int doctorId, QString &errorMsg)
{
    if (!db.isValid()) {
        errorMsg = "数据库连接无效";
        return false;
    }

    // 先检查预约是否存在且属于该医生
    QSqlQuery checkQuery(db);
    QString checkSql = "SELECT id, status FROM appointments WHERE id = ? AND doctor_id = ?";
    checkQuery.prepare(checkSql);
    checkQuery.addBindValue(appointmentId);
    checkQuery.addBindValue(doctorId);

    if (!checkQuery.exec()) {
        errorMsg = "检查预约失败: " + checkQuery.lastError().text();
        return false;
    }

    if (!checkQuery.next()) {
        errorMsg = "预约不存在或无权限操作";
        return false;
    }

    int currentStatus = checkQuery.value("status").toInt();
    if (currentStatus != 0) { // 只有待确认状态才能确认
        errorMsg = "预约状态不允许确认";
        return false;
    }

    checkQuery.finish();

    // 更新预约状态为已确认（状态1）
    return updateAppointmentStatus(db, appointmentId, 1, errorMsg);
}

bool AppointmentDao::rejectAppointment(QSqlDatabase db, int appointmentId, int doctorId, QString &errorMsg)
{
    if (!db.isValid()) {
        errorMsg = "数据库连接无效";
        return false;
    }

    // 先检查预约是否存在且属于该医生
    QSqlQuery checkQuery(db);
    QString checkSql = "SELECT id, status FROM appointments WHERE id = ? AND doctor_id = ?";
    checkQuery.prepare(checkSql);
    checkQuery.addBindValue(appointmentId);
    checkQuery.addBindValue(doctorId);

    if (!checkQuery.exec()) {
        errorMsg = "检查预约失败: " + checkQuery.lastError().text();
        return false;
    }

    if (!checkQuery.next()) {
        errorMsg = "预约不存在或无权限操作";
        return false;
    }

    int currentStatus = checkQuery.value("status").toInt();
    if (currentStatus != 0) { // 只有待确认状态才能拒绝
        errorMsg = "预约状态不允许拒绝";
        return false;
    }

    checkQuery.finish();

    // 更新预约状态为已取消（状态3）
    return updateAppointmentStatus(db, appointmentId, 3, errorMsg);
}

bool AppointmentDao::completeConsultation(QSqlDatabase db, int appointmentId, int doctorId, QString &errorMsg)
{
    if (!db.isValid()) {
        errorMsg = "数据库连接无效";
        return false;
    }

    // 先检查预约是否存在且属于该医生
    QSqlQuery checkQuery(db);
    QString checkSql = "SELECT id, status FROM appointments WHERE id = ? AND doctor_id = ?";
    checkQuery.prepare(checkSql);
    checkQuery.addBindValue(appointmentId);
    checkQuery.addBindValue(doctorId);

    if (!checkQuery.exec()) {
        errorMsg = "检查预约失败: " + checkQuery.lastError().text();
        return false;
    }

    if (!checkQuery.next()) {
        errorMsg = "预约不存在或无权限操作";
        return false;
    }

    int currentStatus = checkQuery.value("status").toInt();
    // 只有已确认状态的预约才能完成咨询
    if (currentStatus != 1) {
        errorMsg = "只有已确认的预约才能完成咨询";
        return false;
    }

    checkQuery.finish();

    // 更新预约状态为已完成（状态2）
    return updateAppointmentStatus(db, appointmentId, 2, errorMsg);
}

QJsonArray AppointmentDao::getDoctorPatients(QSqlDatabase db, int doctorId, QString &errorMsg)
{
    QJsonArray result;

    if (!db.isOpen()) {
        const_cast<DBManager&>(DBManager::instance()).getMainDatabase().open();
    }

    if (!db.isOpen()) {
        errorMsg = "数据库未连接";
        return result;
    }

    QSqlQuery query(db);

    // - 关联 users 表获取真实姓名
    // - 使用 GROUP BY 去重学生ID
    // - 使用 COUNT 统计预约次数
    // - 使用 MAX 获取最近预约日期
    QString sql = R"(
        SELECT 
            u.id, 
            u.real_name, 
            COUNT(a.id) as appt_count, 
            MAX(a.date) as last_date 
        FROM appointments a 
        JOIN users u ON a.student_id = u.id 
        WHERE a.doctor_id = :did 
        GROUP BY u.id, u.real_name
        ORDER BY last_date DESC
    )";

    query.prepare(sql);
    query.bindValue(":did", doctorId);

    if (!query.exec()) {
        errorMsg = "查询患者列表失败: " + query.lastError().text();
        qWarning() << errorMsg;
        return result;
    }

    while (query.next()) {
        QJsonObject patient;
        patient["userId"] = query.value("id").toInt();
        patient["realName"] = query.value("real_name").toString();
        patient["appointmentCount"] = query.value("appt_count").toInt();
        
        // 处理日期格式，确保不返回空值
        QDate date = query.value("last_date").toDate();
        if (date.isValid()) {
            patient["lastAppointmentDate"] = date.toString(Qt::ISODate);
        } else {
            patient["lastAppointmentDate"] = "无记录";
        }

        result.append(patient);
    }

    return result;
}

QJsonArray AppointmentDao::getHistoryByDoctorAndStudent(QSqlDatabase db, int doctorId, int studentId, QString &errorMsg)
{
    QJsonArray result;

    // 确保连接有效
    if (!db.isOpen()) {
        const_cast<DBManager&>(DBManager::instance()).getMainDatabase().open();
    }

    if (!db.isOpen()) {
        errorMsg = "数据库未连接";
        return result;
    }

    QSqlQuery query(db);
    // 关联查询：Appointments (主) -> Users (学生名) -> ConsultationRecords (报告/标签)
    // 使用 LEFT JOIN，因为只有已完成的预约才会有记录
    QString sql = R"(
        SELECT 
            a.id, a.date, a.time_slot, a.status, a.create_time, 
            u.real_name as student_name,
            c.report_content, c.result_tags,
            sa.answers_json
        FROM appointments a
        JOIN users u ON a.student_id = u.id
        LEFT JOIN consultation_records c ON a.id = c.appt_id
        LEFT JOIN survey_answers sa ON a.id = sa.appt_id
        WHERE a.doctor_id = :did AND a.student_id = :sid
        ORDER BY a.date DESC, a.time_slot DESC
    )";

    query.prepare(sql);
    query.bindValue(":did", doctorId);
    query.bindValue(":sid", studentId);

    if (!query.exec()) {
        errorMsg = "查询历史记录失败: " + query.lastError().text();
        return result;
    }

    while (query.next()) {
        QJsonObject item;
        item["id"] = query.value("id").toInt();
        item["appointmentDate"] = query.value("date").toString();
        item["timeSlot"] = query.value("time_slot").toInt();
        item["status"] = query.value("status").toInt();

        QString tags = query.value("result_tags").toString();
        QString report = query.value("report_content").toString();
        QString summary;

        if (!tags.isEmpty()) {
            // 优先使用结果标签
            summary = tags;
        } else if (!report.isEmpty()) {
            // 没标签，截取报告内容的前 20 个字
            summary = report.left(20);
            if (report.length() > 20) {
                summary += "...";
            }
        } else {
            // 没有咨询记录
            summary = "常规咨询";
        }
        
        item["reason"] = summary;
        
        item["report"] = report;
        item["resultTags"] = tags;
        
        result.append(item);
    }
    
    return result;
}