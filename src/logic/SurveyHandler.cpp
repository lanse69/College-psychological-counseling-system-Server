#include "SurveyHandler.h"

#include <QDebug>
#include <QJsonArray>

#include "dao/QuestionnaireDao.h"
#include "core/ProtocolDefs.h"
#include "core/AsyncExecutor.h"
#include "network/ClientSocket.h"

void SurveyHandler::handleGetSurveyContent(ClientSocket* sender, const QJsonObject& request)
{
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int apptId = data[JsonKeys::APPOINTMENT_ID].toInt();

    AsyncExecutor::run(sender,
        [apptId](QSqlDatabase db) -> QPair<bool, QJsonObject> {
            QuestionnaireDao dao;
            QString errorMsg;
            QJsonObject survey = dao.getSurveyByAppointment(db, apptId, errorMsg);
            
            if (survey.isEmpty()) return {false, QJsonObject{{"error", errorMsg}}};
            return {true, survey};
        },
        [sender, request](QPair<bool, QJsonObject> result) {
            QJsonObject response;
            response[JsonKeys::CMD] = request[JsonKeys::CMD];
            
            if (result.first) {
                response[JsonKeys::CODE] = StatusCode::SUCCESS;
                response[JsonKeys::MSG] = "获取问卷成功";
                response[JsonKeys::DATA] = result.second;
            } else {
                response[JsonKeys::CODE] = StatusCode::NOT_FOUND;
                response[JsonKeys::MSG] = result.second["error"].toString();
            }
            sender->sendJson(response);
        }
    );
}

void SurveyHandler::handleStudentSubmitSurvey(ClientSocket* sender, const QJsonObject& request)
{
    int studentId = sender->userId();
    QJsonObject data = request[JsonKeys::DATA].toObject();
    int apptId = data[JsonKeys::APPOINTMENT_ID].toInt();
    QJsonArray answers = data["answers"].toArray();

    if (studentId <= 0 || apptId <= 0 || answers.isEmpty()) {
        QJsonObject resp;
        resp[JsonKeys::CMD] = request[JsonKeys::CMD];
        resp[JsonKeys::CODE] = StatusCode::BAD_REQUEST;
        resp[JsonKeys::MSG] = "提交数据不完整";
        sender->sendJson(resp);
        return;
    }

    AsyncExecutor::run(sender,
        [apptId, studentId, answers](QSqlDatabase db) -> QPair<bool, QString> {
            QuestionnaireDao dao;
            QString errorMsg;
            bool success = dao.saveAnswers(db, apptId, studentId, answers, errorMsg);
            return {success, success ? "提交成功" : errorMsg};
        },
        [sender, request](QPair<bool, QString> result) {
            QJsonObject response;
            response[JsonKeys::CMD] = request[JsonKeys::CMD];
            response[JsonKeys::CODE] = result.first ? StatusCode::SUCCESS : StatusCode::INTERNAL_ERROR;
            response[JsonKeys::MSG] = result.second;
            sender->sendJson(response);
        }
    );
}

void SurveyHandler::handleDoctorGetSurvey(ClientSocket* sender, const QJsonObject& request)
{
    int doctorId = sender->userId(); // 必须是医生
    
    AsyncExecutor::run(sender,
        [doctorId](QSqlDatabase db) -> QPair<bool, QJsonObject> {
            QuestionnaireDao dao;
            QString errorMsg;
            QJsonObject survey = dao.getDoctorSurvey(db, doctorId, errorMsg);
            return {true, survey};
        },
        [sender, request](QPair<bool, QJsonObject> result) {
            QJsonObject response;
            response[JsonKeys::CMD] = request[JsonKeys::CMD];
            response[JsonKeys::CODE] = StatusCode::SUCCESS; 
            response[JsonKeys::MSG] = "获取成功";
            response[JsonKeys::DATA] = result.second;
            sender->sendJson(response);
        }
    );
}

void SurveyHandler::handleDoctorSaveSurvey(ClientSocket* sender, const QJsonObject& request)
{
    int doctorId = sender->userId();
    QJsonObject data = request[JsonKeys::DATA].toObject();
    QString title = data["title"].toString();
    QJsonArray questions = data["questions"].toArray();

    if (title.isEmpty() || questions.isEmpty()) {
        // 发送参数错误响应
        return;
    }

    AsyncExecutor::run(sender,
        [doctorId, title, questions](QSqlDatabase db) -> QPair<bool, QString> {
            QuestionnaireDao dao;
            QString errorMsg;
            bool success = dao.saveSurveyTemplate(db, doctorId, title, questions, errorMsg);
            return {success, success ? "问卷保存成功" : errorMsg};
        },
        [sender, request](QPair<bool, QString> result) {
            QJsonObject response;
            response[JsonKeys::CMD] = request[JsonKeys::CMD];
            response[JsonKeys::CODE] = result.first ? StatusCode::SUCCESS : StatusCode::INTERNAL_ERROR;
            response[JsonKeys::MSG] = result.second;
            sender->sendJson(response);
        }
    );
}