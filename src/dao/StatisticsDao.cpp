#include "StatisticsDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QDebug>
#include <QDate>

#include "core/ProtocolDefs.h"

StatisticsDao::StatisticsDao(QObject *parent) : QObject(parent) {}

QJsonArray StatisticsDao::getConsultTrend(QSqlDatabase db, QString &errorMsg)
{
    QJsonArray result;
    if (!db.isOpen()) {
        errorMsg = "数据库未连接";
        return result;
    }

    QSqlQuery query(db);
    // 统计过去12个月，每月完成(status=2)的咨询数量
    QString sql = R"(
        SELECT TO_CHAR(date, 'YYYY-MM') as month_str, COUNT(*) as cnt
        FROM appointments
        WHERE status = 2 
          AND date > CURRENT_DATE - INTERVAL '12 months'
        GROUP BY month_str
        ORDER BY month_str ASC
    )";

    if (!query.exec(sql)) {
        errorMsg = "统计趋势失败: " + query.lastError().text();
        return result;
    }

    while (query.next()) {
        QJsonObject item;
        item["label"] = query.value("month_str").toString();
        item["value"] = query.value("cnt").toInt();
        result.append(item);
    }
    
    return result;
}

QJsonArray StatisticsDao::getCommonIssues(QSqlDatabase db, QString &errorMsg)
{
    QJsonArray result;
    if (!db.isOpen()) {
        errorMsg = "数据库未连接";
        return result;
    }

    QSqlQuery query(db);
    
    // 1. 获取 result_tags 不为空的记录
    // 2. string_to_array 按逗号分割
    // 3. unnest 将数组转为多行
    // 4. TRIM 去除空格
    // 5. GROUP BY 统计并排序
    QString sql = R"(
        SELECT TRIM(tag) as clean_tag, COUNT(*) as cnt
        FROM (
            SELECT unnest(string_to_array(result_tags, ',')) as tag
            FROM consultation_records
            WHERE result_tags IS NOT NULL AND result_tags != ''
        ) as tag_table
        GROUP BY clean_tag
        ORDER BY cnt DESC
        LIMIT 10
    )";

    if (!query.exec(sql)) {
        errorMsg = "统计热门问题失败: " + query.lastError().text();
        qWarning() << "尝试降级查询:" << errorMsg;
        return result;
    }

    while (query.next()) {
        QJsonObject item;
        item["label"] = query.value("clean_tag").toString();
        item["value"] = query.value("cnt").toInt();
        result.append(item);
    }

    return result;
}

QJsonArray StatisticsDao::getStudentGenderStats(QSqlDatabase db, QString &errorMsg)
{
    QJsonArray result;
    if (!db.isOpen()) { errorMsg = "数据库未连接"; return result; }

    QSqlQuery query(db);
    // 统计所有学生账号的性别分布
    // role = 1 是学生
    QString sql = R"(
        SELECT COALESCE(NULLIF(gender, ''), '未知') as g_label, COUNT(*) as cnt
        FROM users
        WHERE role = 1
        GROUP BY g_label
    )";

    if (!query.exec(sql)) {
        errorMsg = "统计性别失败: " + query.lastError().text();
        return result;
    }

    while (query.next()) {
        QJsonObject item;
        item["label"] = query.value("g_label").toString();
        item["value"] = query.value("cnt").toInt();
        result.append(item);
    }
    return result;
}

QJsonArray StatisticsDao::getTopDoctors(QSqlDatabase db, QString &errorMsg)
{
    QJsonArray result;
    if (!db.isOpen()) { errorMsg = "数据库未连接"; return result; }

    QSqlQuery query(db);
    // 统计完成咨询(status=2)数量最多的医生
    QString sql = R"(
        SELECT u.real_name, COUNT(a.id) as cnt
        FROM appointments a
        JOIN users u ON a.doctor_id = u.id
        WHERE a.status = 2
        GROUP BY u.real_name
        ORDER BY cnt DESC
        LIMIT 10
    )";

    if (!query.exec(sql)) {
        errorMsg = "统计热门医生失败: " + query.lastError().text();
        return result;
    }

    while (query.next()) {
        QJsonObject item;
        item["label"] = query.value("real_name").toString();
        item["value"] = query.value("cnt").toInt();
        result.append(item);
    }
    return result;
}

QJsonArray StatisticsDao::getPeakTimeSlots(QSqlDatabase db, QString &errorMsg)
{
    QJsonArray result;
    if (!db.isOpen()) { errorMsg = "数据库未连接"; return result; }

    QSqlQuery query(db);
    // 统计所有有效预约(已确认1 + 已完成2)的时段分布
    QString sql = R"(
        SELECT time_slot, COUNT(*) as cnt
        FROM appointments
        WHERE status IN (1, 2)
        GROUP BY time_slot
        ORDER BY time_slot ASC
    )";

    if (!query.exec(sql)) {
        errorMsg = "统计时段分布失败: " + query.lastError().text();
        return result;
    }

    while (query.next()) {
        int slotIndex = query.value("time_slot").toInt();
        QJsonObject item;
        // 将 0,1,2 转换为 "08:30-09:30"
        item["label"] = GetTimeSlotText(slotIndex); 
        item["value"] = query.value("cnt").toInt();
        result.append(item);
    }
    return result;
}