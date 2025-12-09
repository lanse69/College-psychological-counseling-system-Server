#include "ScheduleDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

#include "core/ProtocolDefs.h" 

bool ScheduleDao::initSchedule(QSqlDatabase db, int doctorId, QDate date) {
    QSqlQuery query(db);

    query.prepare(R"(
        INSERT INTO schedules (doctor_id, date, time_slot_flags)
        VALUES (:did, :date, 0)
        ON CONFLICT (doctor_id, date) DO NOTHING
    )");
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (!query.exec()) {
        qCritical() << "错误: 初始化排班失败 " << query.lastError().text();
        return false;
    }
    return true;
}

int ScheduleDao::getScheduleFlag(QSqlDatabase db, int doctorId, QDate date) {
    QSqlQuery query(db);
    query.prepare("SELECT time_slot_flags FROM schedules WHERE doctor_id = :did AND date = :date");
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    // 无记录则视为默认空闲（0）
    return 0;
}

QMap<QString, int> ScheduleDao::getScheduleRange(QSqlDatabase db, int doctorId, QDate startDate, QDate endDate) {
    QMap<QString, int> resultMap;
    QSqlQuery query(db);

    query.prepare(R"(
        SELECT to_char(date, 'YYYY-MM-DD'), time_slot_flags
        FROM schedules
        WHERE doctor_id = :did AND date >= :start AND date <= :end
    )");
    query.bindValue(":did", doctorId);
    query.bindValue(":start", startDate);
    query.bindValue(":end", endDate);

    if (query.exec()) {
        while (query.next()) {
            resultMap.insert(query.value(0).toString(), query.value(1).toInt());
        }
    } else {
        qWarning() << "警告: 获取排班范围失败 " << query.lastError().text();
    }
    return resultMap;
}

bool ScheduleDao::isSlotAvailable(QSqlDatabase db, int doctorId, QDate date, int slotIndex) {
    // 严格边界检查
    if (slotIndex < 0 || slotIndex > MAX_TIME_SLOT_INDEX) {
        qWarning() << "isSlotAvailable: 索引越界 " << slotIndex;
        return false; // 越界视为不可用
    }

    int flags = getScheduleFlag(db, doctorId, date);
    
    // 检查对应位是否为 0 (0表示空闲)
    // (flags >> slotIndex) & 1 取出第 slotIndex 位的值
    bool isBusy = (flags >> slotIndex) & 1;
    
    return !isBusy;
}

bool ScheduleDao::occupySlot(QSqlDatabase db, int doctorId, QDate date, int slotIndex) {
    // 严格边界检查
    if (slotIndex < 0 || slotIndex > MAX_TIME_SLOT_INDEX) {
        qCritical() << "occupySlot: 索引越界 " << slotIndex;
        return false;
    }

    // 确保记录存在
    if (!initSchedule(db, doctorId, date)) return false;

    QSqlQuery query(db);
    // 利用位运算 OR (|) 将对应位置 1
    query.prepare(R"(
        UPDATE schedules
        SET time_slot_flags = time_slot_flags | (1 << :slot)
        WHERE doctor_id = :did AND date = :date
    )");

    query.bindValue(":slot", slotIndex);
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (!query.exec()) {
        qCritical() << "错误: 占用时间段失败 " << query.lastError().text();
        return false;
    }
    return true;
}

bool ScheduleDao::releaseSlot(QSqlDatabase db, int doctorId, QDate date, int slotIndex) {
    // 严格边界检查
    if (slotIndex < 0 || slotIndex > MAX_TIME_SLOT_INDEX) {
        qCritical() << "releaseSlot: 索引越界 " << slotIndex;
        return false;
    }

    QSqlQuery query(db);
    // 利用位运算 AND NOT (& ~) 将对应位置 0
    query.prepare(R"(
        UPDATE schedules
        SET time_slot_flags = time_slot_flags & ~(1 << :slot)
        WHERE doctor_id = :did AND date = :date
    )");

    query.bindValue(":slot", slotIndex);
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (!query.exec()) {
        qCritical() << "错误: 释放时间段失败 " << query.lastError().text();
        return false;
    }
    return true;
}

bool ScheduleDao::updateScheduleMask(QSqlDatabase db, int doctorId, QDate date, int newMask) {
    if (!initSchedule(db, doctorId, date)) return false;

    if (newMask > 127) {
        qWarning() << "警告: 排班掩码 " << newMask << " 超出 7 个时段的范围 (0-127)";
    }

    QSqlQuery query(db);
    query.prepare("UPDATE schedules SET time_slot_flags = :mask WHERE doctor_id = :did AND date = :date");
    query.bindValue(":mask", newMask);
    query.bindValue(":did", doctorId);
    query.bindValue(":date", date);

    if (!query.exec()) {
        qCritical() << "错误: 更新排班掩码失败 " << query.lastError().text();
        return false;
    }
    return true;
}