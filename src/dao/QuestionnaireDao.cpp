#include "QuestionnaireDao.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QDebug>
#include <QDateTime>

QuestionnaireDao::QuestionnaireDao(QObject *parent) : QObject(parent) {}

QJsonObject QuestionnaireDao::getSurveyByAppointment(QSqlDatabase db, int appointmentId, QString &errorMsg)
{
    QJsonObject result;
    if (!db.isOpen()) {
        errorMsg = "数据库未连接";
        return result;
    }

    QSqlQuery query(db);
    
    // 通过预约ID找到医生ID
    query.prepare("SELECT doctor_id FROM appointments WHERE id = ?");
    query.addBindValue(appointmentId);
    
    if (!query.exec() || !query.next()) {
        errorMsg = "找不到该预约记录";
        return result;
    }
    int doctorId = query.value(0).toInt();

    // 查找该医生的问卷
    query.prepare("SELECT title, content_json FROM surveys WHERE doctor_id = ? ORDER BY create_time DESC LIMIT 1");
    query.addBindValue(doctorId);

    if (query.exec() && query.next()) {
        result["title"] = query.value("title").toString();
        // Postgres JSONB 转为 ByteArray 后解析
        QByteArray jsonBytes = query.value("content_json").toByteArray();
        result["questions"] = QJsonDocument::fromJson(jsonBytes).array();
    } else {
        // 如果没有问卷，生成默认并重试
        ensureDefaultSurvey(db, doctorId);
        if (query.exec() && query.next()) {
            result["title"] = query.value("title").toString();
            QByteArray jsonBytes = query.value("content_json").toByteArray();
            result["questions"] = QJsonDocument::fromJson(jsonBytes).array();
        } else {
            errorMsg = "获取问卷失败";
            return result;
        }
    }

    // 查询是否有历史填写的答案
    QSqlQuery ansQuery(db);
    ansQuery.prepare("SELECT answers_json FROM survey_answers WHERE appt_id = ?");
    ansQuery.addBindValue(appointmentId);
    
    if (ansQuery.exec() && ansQuery.next()) {
        QByteArray ansBytes = ansQuery.value("answers_json").toByteArray();
        QJsonArray existingAnswers = QJsonDocument::fromJson(ansBytes).array();
        // 将历史答案放入返回的 JSON 中
        result["existingAnswers"] = existingAnswers;
    }

    return result;
}

bool QuestionnaireDao::saveAnswers(QSqlDatabase db, int appointmentId, int studentId, const QJsonArray &answers, QString &errorMsg)
{
    if (!db.isOpen()) {
        errorMsg = "数据库未连接";
        return false;
    }

    QJsonDocument doc(answers);
    QString jsonStr = doc.toJson(QJsonDocument::Compact);

    QSqlQuery delQuery(db);
    delQuery.prepare("DELETE FROM survey_answers WHERE appt_id = ?");
    delQuery.addBindValue(appointmentId);
    delQuery.exec();

    QSqlQuery query(db); 

    query.prepare("INSERT INTO survey_answers (appt_id, student_id, answers_json, submit_time) "
                  "VALUES (?, ?, ?, ?)");
    query.addBindValue(appointmentId);
    query.addBindValue(studentId);
    query.addBindValue(jsonStr);
    query.addBindValue(QDateTime::currentDateTime());

    if (!query.exec()) {
        errorMsg = "保存答案失败: " + query.lastError().text();
        return false;
    }

    return true;
}

void QuestionnaireDao::ensureDefaultSurvey(QSqlDatabase db, int doctorId)
{
    // 创建一个默认的 SDS 抑郁自评量表模版
    QJsonArray questions;
    
    auto addQ = [&](const QString& text) {
        QJsonObject q;
        q["question"] = text;
        q["type"] = "single";
        QJsonArray opts = {"从不", "偶尔", "经常", "总是"};
        q["options"] = opts;
        questions.append(q);
    };

    addQ("我感到情绪低落，郁郁寡欢");
    addQ("我觉得一天之中早晨最好");
    addQ("我一阵阵哭出来或觉得想哭");
    addQ("我晚上睡眠不好");
    addQ("我吃得跟平常一样多");
    addQ("我与异性亲密接触时和以往一样感到愉快");
    addQ("我发觉我的体重在下降");
    addQ("我有便秘的苦恼");
    addQ("心跳比平时快");
    addQ("我无缘无故地感到疲乏");

    QJsonDocument doc(questions);
    QString jsonContent = doc.toJson(QJsonDocument::Compact);

    QSqlQuery query(db);
    query.prepare("INSERT INTO surveys (doctor_id, title, content_json) VALUES (?, ?, ?)");
    query.addBindValue(doctorId);
    query.addBindValue("SDS 抑郁自评量表 (系统默认)");
    query.addBindValue(jsonContent);
    query.exec();
}

QJsonObject QuestionnaireDao::getDoctorSurvey(QSqlDatabase db, int doctorId, QString &errorMsg)
{
    QJsonObject result;
    if (!db.isOpen()) {
        errorMsg = "数据库未连接";
        return result;
    }

    QSqlQuery query(db);
    // 获取最新的一份
    query.prepare("SELECT title, content_json FROM surveys WHERE doctor_id = ? ORDER BY create_time DESC LIMIT 1");
    query.addBindValue(doctorId);

    if (query.exec() && query.next()) {
        result["title"] = query.value("title").toString();
        QByteArray jsonBytes = query.value("content_json").toByteArray();
        result["questions"] = QJsonDocument::fromJson(jsonBytes).array();
    }
    return result;
}

bool QuestionnaireDao::saveSurveyTemplate(QSqlDatabase db, int doctorId, const QString &title, const QJsonArray &questions, QString &errorMsg)
{
    if (!db.isOpen()) {
        errorMsg = "数据库未连接";
        return false;
    }

    QJsonDocument doc(questions);
    QString jsonStr = doc.toJson(QJsonDocument::Compact);

    QSqlQuery query(db);
    query.prepare("INSERT INTO surveys (doctor_id, title, content_json, create_time) VALUES (?, ?, ?, ?)");
    query.addBindValue(doctorId);
    query.addBindValue(title);
    query.addBindValue(jsonStr);
    query.addBindValue(QDateTime::currentDateTime());

    if (!query.exec()) {
        errorMsg = "保存问卷失败: " + query.lastError().text();
        return false;
    }
    return true;
}