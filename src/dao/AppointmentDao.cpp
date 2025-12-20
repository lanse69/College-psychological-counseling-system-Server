#include "AppointmentDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QDateTime>

#include "DBManager.h"
#include "core/ProtocolDefs.h"
#include "ScheduleDao.h"

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

    // 检查学生的时间冲突
    // 查询该学生在同一日期、同一时段，是否有状态不为 3 (已取消) 的预约
    QString checkStudentSql = "SELECT id FROM appointments WHERE student_id = ? AND date = ? AND time_slot = ? AND status != 3";
    query.prepare(checkStudentSql);
    query.addBindValue(studentId);
    query.addBindValue(date);
    query.addBindValue(timeSlot);

    if (!query.exec()) {
        db.rollback();
        errorMsg = "查询学生时间冲突失败: " + query.lastError().text();
        return false;
    }

    if (query.next()) {
        db.rollback();
        // 提示信息尽量具体，告知用户冲突的时间段
        errorMsg = QString("您在 %1 的 %2 时段已经有其他预约，无法重复预约。")
                       .arg(date)
                       .arg(GetTimeSlotText(timeSlot)); 
        return false;
    }

    // 检查预约表冲突 (appointments)
    // status != 3 表示未取消的预约都算占用
    QString checkDoctorSql = "SELECT id FROM appointments WHERE doctor_id = ? AND date = ? AND time_slot = ? AND status != 3";
    query.prepare(checkDoctorSql);
    query.addBindValue(doctorId);
    query.addBindValue(date);
    query.addBindValue(timeSlot);

    if (!query.exec()) {
        db.rollback();
        errorMsg = "检查医生预约冲突失败: " + query.lastError().text();
        return false;
    }

    if (query.next()) {
        db.rollback();
        errorMsg = "手慢了，该医生的此时间段刚被抢走";
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

    // 执行插入预约 (Status设为 1: 已确认)
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
    QString sql = R"(
        SELECT 
            a.id, a.student_id, a.doctor_id, a.date, a.time_slot, a.status, a.create_time, 
            a.modify_request_json, 
            u.real_name as doctor_name, 
            d.specialized_field,
            c.report_content, 
            c.result_tags
        FROM appointments a 
        JOIN users u ON a.doctor_id = u.id 
        JOIN doctor_info d ON u.id = d.user_id 
        LEFT JOIN consultation_records c ON a.id = c.appt_id
        WHERE a.student_id = ? 
        ORDER BY a.date DESC, a.time_slot DESC
    )";

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
        appointment["report"] = query.value("report_content").toString();
        appointment["resultTags"] = query.value("result_tags").toString();

        // 解析修改请求详情
        QByteArray reqBytes = query.value("modify_request_json").toByteArray();
        if (!reqBytes.isEmpty()) {
            QJsonObject reqObj = QJsonDocument::fromJson(reqBytes).object();
            appointment["pendingDate"] = reqObj["date"].toString();
            appointment["pendingSlot"] = reqObj["timeSlot"].toInt();
        }

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

        // 解析修改请求详情
        QByteArray reqBytes = query.value("modify_request_json").toByteArray();
        if (!reqBytes.isEmpty()) {
            QJsonObject reqObj = QJsonDocument::fromJson(reqBytes).object();
            appointment["pendingDate"] = reqObj["date"].toString();
            appointment["pendingSlot"] = reqObj["timeSlot"].toInt();
        }

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

bool AppointmentDao::modifyAppointmentDirect(QSqlDatabase db, int studentId, int appointmentId, const QString &newDate, int newSlot, QString &errorMsg)
{
    if (!db.isValid() || !db.isOpen()) {
        errorMsg = "数据库未连接";
        return false;
    }

    // 开启事务
    if (!db.transaction()) {
        errorMsg = "事务启动失败";
        return false;
    }

    QSqlQuery query(db);

    // 查询原预约信息 (锁定行，防止并发修改)
    // 只有状态为 0(待确认) 或 1(已确认) 的才可以修改
    QString oldSql = "SELECT doctor_id, date, time_slot, status FROM appointments WHERE id = ? AND student_id = ? FOR UPDATE";
    query.prepare(oldSql);
    query.addBindValue(appointmentId);
    query.addBindValue(studentId);

    if (!query.exec() || !query.next()) {
        db.rollback();
        errorMsg = "预约不存在、不属于您或已被删除";
        return false;
    }

    int doctorId = query.value("doctor_id").toInt();
    QString oldDate = query.value("date").toString();
    int oldSlot = query.value("time_slot").toInt();
    int status = query.value("status").toInt();

    if (status >= 2) { // 2=完成, 3=取消
        db.rollback();
        errorMsg = "该预约已完成或已取消，无法修改";
        return false;
    }

    // 如果新旧时间一致，直接返回成功
    if (oldDate == newDate && oldSlot == newSlot) {
        db.rollback(); // 不需要提交更改
        return true;
    }

    // 检查学生在新时间段是否有冲突
    QString checkStuSql = "SELECT id FROM appointments WHERE student_id = ? AND date = ? AND time_slot = ? AND status != 3 AND id != ?";
    query.prepare(checkStuSql);
    query.addBindValue(studentId);
    query.addBindValue(newDate);
    query.addBindValue(newSlot);
    query.addBindValue(appointmentId);
    if (query.exec() && query.next()) {
        db.rollback();
        errorMsg = "您在新的时间段已有其他预约";
        return false;
    }

    // 检查医生在新时间段是否空闲
    // 获取新日期的排班掩码
    int doctorScheduleFlags = ScheduleDao::getScheduleFlag(db, doctorId, QDate::fromString(newDate, Qt::ISODate));
    
    // 检查第 newSlot 位是否被占用 (1表示忙)
    if ((doctorScheduleFlags >> newSlot) & 1) {
        db.rollback();
        errorMsg = "医生在该新时间段已无空闲";
        return false;
    }

    // 释放旧的排班 (将 oldSlot 位置 0)
    QString releaseOldSql = "UPDATE schedules SET time_slot_flags = time_slot_flags & ~(1 << ?) WHERE doctor_id = ? AND date = ?";
    query.prepare(releaseOldSql);
    query.addBindValue(oldSlot);
    query.addBindValue(doctorId);
    query.addBindValue(oldDate);
    if (!query.exec()) {
        db.rollback();
        errorMsg = "释放旧排班失败";
        return false;
    }

    // 占用新的排班 (将 newSlot 位置 1)
    // 确保 schedule 记录存在 (如果是新的一天可能没有记录)
    ScheduleDao::initSchedule(db, doctorId, QDate::fromString(newDate, Qt::ISODate));

    QString occupyNewSql = "UPDATE schedules SET time_slot_flags = time_slot_flags | (1 << ?) WHERE doctor_id = ? AND date = ?";
    query.prepare(occupyNewSql);
    query.addBindValue(newSlot);
    query.addBindValue(doctorId);
    query.addBindValue(newDate);
    if (!query.exec()) {
        db.rollback();
        errorMsg = "占用新排班失败";
        return false;
    }

    // 更新预约记录
    QString updateApptSql = "UPDATE appointments SET date = ?, time_slot = ?, status = 0 WHERE id = ?";
    query.prepare(updateApptSql);
    query.addBindValue(newDate);
    query.addBindValue(newSlot);
    query.addBindValue(appointmentId);
    
    if (!query.exec()) {
        db.rollback();
        errorMsg = "更新预约记录失败: " + query.lastError().text();
        return false;
    }

    // 提交事务
    if (!db.commit()) {
        db.rollback();
        errorMsg = "提交事务失败";
        return false;
    }

    return true;
}

bool AppointmentDao::createModifyRequest(QSqlDatabase db, int appointmentId, int doctorId, const QString &newDate, int newSlot, QString &errorMsg)
{
    if (!db.isOpen()) { errorMsg = "数据库未连接"; return false; }
    
    // 检查医生该时段是否空闲
    // 软检查，不锁定，没真正修改
    int flags = ScheduleDao::getScheduleFlag(db, doctorId, QDate::fromString(newDate, Qt::ISODate));
    if ((flags >> newSlot) & 1) {
        errorMsg = "您的排班表中该时段已忙碌，无法发起修改";
        return false;
    }

    QJsonObject reqObj;
    reqObj["date"] = newDate;
    reqObj["timeSlot"] = newSlot;
    QString jsonStr = QJsonDocument(reqObj).toJson(QJsonDocument::Compact);

    // 更新预约：Status -> 4, JSON -> 写入
    // 只能修改状态为 0 (待确认) 或 1 (已确认) 的预约
    QSqlQuery query(db);
    query.prepare("UPDATE appointments SET status = 4, modify_request_json = :json WHERE id = :id AND doctor_id = :did AND status IN (0, 1)");
    query.bindValue(":json", jsonStr);
    query.bindValue(":id", appointmentId);
    query.bindValue(":did", doctorId);

    if (!query.exec()) {
        errorMsg = "发起修改请求失败: " + query.lastError().text();
        return false;
    }
    
    if (query.numRowsAffected() == 0) {
        errorMsg = "预约不存在、状态不正确或无权操作";
        return false;
    }

    return true;
}

bool AppointmentDao::resolveModifyRequest(QSqlDatabase db, int appointmentId, int studentId, bool accept, QString &errorMsg)
{
    if (!db.transaction()) { errorMsg = "事务启动失败"; return false; }

    QSqlQuery query(db);
    
    // 获取预约详情及请求详情 (锁行)
    query.prepare("SELECT doctor_id, date, time_slot, modify_request_json FROM appointments WHERE id = ? AND student_id = ? AND status = 4 FOR UPDATE");
    query.addBindValue(appointmentId);
    query.addBindValue(studentId);
    
    if (!query.exec() || !query.next()) {
        db.rollback();
        errorMsg = "预约状态异常，可能已处理";
        return false;
    }

    int doctorId = query.value("doctor_id").toInt();
    QString oldDate = query.value("date").toString();
    int oldSlot = query.value("time_slot").toInt();
    
    QByteArray jsonBytes = query.value("modify_request_json").toByteArray();
    if (jsonBytes.isEmpty()) {
        db.rollback();
        errorMsg = "找不到变更请求数据";
        return false;
    }
    QJsonObject reqObj = QJsonDocument::fromJson(jsonBytes).object();
    QString newDate = reqObj["date"].toString();
    int newSlot = reqObj["timeSlot"].toInt();

    if (!accept) {
        // 拒绝
        // 恢复状态为 1 (已确认)，清空 JSON
        query.prepare("UPDATE appointments SET status = 1, modify_request_json = NULL WHERE id = ?");
        query.addBindValue(appointmentId);
        if (!query.exec()) {
            db.rollback();
            errorMsg = "还原状态失败";
            return false;
        }
    } else {
        // 同意
        
        // 释放旧排班
        if (!ScheduleDao::releaseSlot(db, doctorId, QDate::fromString(oldDate, Qt::ISODate), oldSlot)) {
            db.rollback();
            errorMsg = "释放原排班失败";
            return false;
        }
        
        // 占用新排班
        if (!ScheduleDao::isSlotAvailable(db, doctorId, QDate::fromString(newDate, Qt::ISODate), newSlot)) {
            db.rollback();
            errorMsg = "医生的该新时段已被抢占，修改失败";
            return false;
        }
        
        if (!ScheduleDao::occupySlot(db, doctorId, QDate::fromString(newDate, Qt::ISODate), newSlot)) {
            db.rollback();
            errorMsg = "占用新排班失败";
            return false;
        }

        // 更新预约
        query.prepare("UPDATE appointments SET date = ?, time_slot = ?, status = 1, modify_request_json = NULL WHERE id = ?");
        query.addBindValue(newDate);
        query.addBindValue(newSlot);
        query.addBindValue(appointmentId);
        if (!query.exec()) {
            db.rollback();
            errorMsg = "更新预约失败";
            return false;
        }
    }

    if (!db.commit()) {
        db.rollback();
        errorMsg = "提交事务失败";
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

    // 开启事务，保证状态更新和排班释放的一致性
    if (!db.transaction()) { 
        errorMsg = "事务启动失败"; 
        return false; 
    }

    QSqlQuery query(db);

    // 查询预约信息 (日期和时段)
    query.prepare("SELECT date, time_slot, status FROM appointments WHERE id = ? AND doctor_id = ? FOR UPDATE");
    query.addBindValue(appointmentId);
    query.addBindValue(doctorId);

    if (!query.exec() || !query.next()) {
        db.rollback();
        errorMsg = "预约不存在或无权操作";
        return false;
    }
    
    QDate date = query.value("date").toDate();
    int timeSlot = query.value("time_slot").toInt();
    int currentStatus = query.value("status").toInt();

    if (currentStatus != 0) {
        db.rollback();
        errorMsg = "预约状态不允许拒绝 (非待确认状态)";
        return false;
    }

    // 更新状态为 3 (已取消)
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE appointments SET status = 3 WHERE id = ?");
    updateQuery.addBindValue(appointmentId);
    if (!updateQuery.exec()) {
        db.rollback();
        errorMsg = "更新状态失败";
        return false;
    }

    // 释放排班 (将对应位置 0)
    // AND NOT (1 << slot)
    QSqlQuery releaseQuery(db);
    releaseQuery.prepare("UPDATE schedules SET time_slot_flags = time_slot_flags & ~(1 << ?) WHERE doctor_id = ? AND date = ?");
    releaseQuery.addBindValue(timeSlot);
    releaseQuery.addBindValue(doctorId);
    releaseQuery.addBindValue(date);
    
    if (!releaseQuery.exec()) {
        db.rollback();
        errorMsg = "释放排班失败";
        return false;
    }

    return db.commit();
}

bool AppointmentDao::completeConsultation(QSqlDatabase db, int appointmentId, int doctorId, QString &errorMsg)
{
    if (!db.isValid()) {
        errorMsg = "数据库连接无效";
        return false;
    }

    if (!db.transaction()) { errorMsg = "事务启动失败"; return false; }

    QSqlQuery query(db);

    // 查询信息
    query.prepare("SELECT date, time_slot, status FROM appointments WHERE id = ? AND doctor_id = ? FOR UPDATE");
    query.addBindValue(appointmentId);
    query.addBindValue(doctorId);

    if (!query.exec() || !query.next()) {
        db.rollback();
        errorMsg = "预约不存在或无权操作";
        return false;
    }
    
    QDate date = query.value("date").toDate();
    int timeSlot = query.value("time_slot").toInt();
    int currentStatus = query.value("status").toInt();

    if (currentStatus != 1) {
        db.rollback();
        errorMsg = "只有已确认的预约才能完成";
        return false;
    }

    // 更新状态为 2 (已完成)
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE appointments SET status = 2 WHERE id = ?");
    updateQuery.addBindValue(appointmentId);
    if (!updateQuery.exec()) {
        db.rollback();
        errorMsg = "更新状态失败";
        return false;
    }

    // 释放排班
    QSqlQuery releaseQuery(db);
    releaseQuery.prepare("UPDATE schedules SET time_slot_flags = time_slot_flags & ~(1 << ?) WHERE doctor_id = ? AND date = ?");
    releaseQuery.addBindValue(timeSlot);
    releaseQuery.addBindValue(doctorId);
    releaseQuery.addBindValue(date);
    
    if (!releaseQuery.exec()) {
        db.rollback();
        errorMsg = "释放排班失败";
        return false;
    }

    return db.commit();
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

        QByteArray ansBytes = query.value("answers_json").toByteArray();
        if (!ansBytes.isEmpty()) {
            item["surveyAnswers"] = QJsonDocument::fromJson(ansBytes).array();
        } else {
            item["surveyAnswers"] = QJsonArray(); // 空数组
        }

        QString summary;

        if (!tags.isEmpty()) {
            // 优先使用结果标签
            summary = tags;
        } else if (!report.isEmpty()) {
            // 没标签，截取报告内容的前 20 个字
            summary = report.left(20) + (report.length()>20?"...":"");
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

bool AppointmentDao::saveReport(QSqlDatabase db, int appointmentId, int doctorId, 
                                const QString &content, const QString &tags, QString &errorMsg)
{
    if (!db.isValid() || !db.isOpen()) {
        errorMsg = "数据库连接无效";
        return false;
    }

    QSqlQuery query(db);

    // 校验该预约是否属于该医生
    query.prepare("SELECT id FROM appointments WHERE id = ? AND doctor_id = ?");
    query.addBindValue(appointmentId);
    query.addBindValue(doctorId);
    if (!query.exec() || !query.next()) {
        errorMsg = "预约不存在或无权操作";
        return false;
    }
    // 删除已有报告（如果存在）
    QSqlQuery delQuery(db);
    delQuery.prepare("DELETE FROM consultation_records WHERE appt_id = ?");
    delQuery.addBindValue(appointmentId);
    delQuery.exec();

    // 插入新报告
    QSqlQuery insertQuery(db);
    insertQuery.prepare(R"(
        INSERT INTO consultation_records (appt_id, doctor_id, report_content, result_tags, create_time)
        VALUES (?, ?, ?, ?, ?)
    )");
    insertQuery.addBindValue(appointmentId);
    insertQuery.addBindValue(doctorId);
    insertQuery.addBindValue(content);
    insertQuery.addBindValue(tags);
    insertQuery.addBindValue(QDateTime::currentDateTime());

    if (!insertQuery.exec()) {
        errorMsg = "保存报告失败: " + insertQuery.lastError().text();
        return false;
    }

    return true;
}

bool AppointmentDao::deleteCancelledAppointment(QSqlDatabase db, int appointmentId, int operatorId, QString &errorMsg)
{
    if (!db.isOpen()) {
        errorMsg = "数据库未连接";
        return false;
    }

    QSqlQuery query(db);

    // 安全校验：
    //    - 预约ID匹配
    //    - 状态必须是 3 (已取消)
    //    - 操作者必须是该预约的学生 OR 医生
    QString sql = R"(
        DELETE FROM appointments 
        WHERE id = :id 
          AND status = 3 
          AND (student_id = :uid OR doctor_id = :uid)
    )";

    query.prepare(sql);
    query.bindValue(":id", appointmentId);
    query.bindValue(":uid", operatorId);

    if (!query.exec()) {
        errorMsg = "删除失败: " + query.lastError().text();
        return false;
    }

    // numRowsAffected() 返回受影响行数
    // 如果为 0，说明条件不满足（比如不是已取消状态，或者不是该用户的预约）
    if (query.numRowsAffected() == 0) {
        errorMsg = "删除失败：记录不存在、状态未取消或无权操作";
        return false;
    }

    return true;
}