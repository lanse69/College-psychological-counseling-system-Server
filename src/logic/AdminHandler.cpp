#include "AdminHandler.h"

#include <QtConcurrent>
#include <QJsonArray>

#include "dao/DBManager.h"
#include "network/ClientSocket.h"
#include "core/ProtocolDefs.h"

void AdminHandler::handleGetStatistics(ClientSocket* client, const QJsonObject& req) {
    // 获取 Socket 指针
    QPointer<ClientSocket> safeClient(client); 

    // 获取查询参数
    QJsonObject data = req[JsonKeys::DATA].toObject();
    QString statType = data["type"].toString();

    // 启动子线程
    QtConcurrent::run([safeClient, statType]() {
        // 获取独立数据库连接
        QString connName;
        QSqlDatabase db = DBManager::instance().openThreadConnection(connName);
        
        QJsonArray resultMap; // 存储查询结果
        
        if (db.isOpen()) {
            QSqlQuery query(db);
            
            // 根据类型进行查询
            if (statType == "consult_trend") {
                // 统计每天/每月的咨询人数]
                QString sql = R"(
                    SELECT to_char(date, 'YYYY-MM') as month, COUNT(*) 
                    FROM appointments 
                    WHERE status = 2
                    GROUP BY month 
                    ORDER BY month DESC LIMIT 12
                )";
                if (query.exec(sql)) {
                    while(query.next()) {
                        QJsonObject item;
                        item["label"] = query.value(0).toString();
                        item["value"] = query.value(1).toInt();
                        resultMap.append(item);
                    }
                }
            }
        } else if (statType == "common_issues") {
            // 统计咨询记录中的 tag
            // TODO: 改为 string_to_array + unnest
            QString sql = R"(
                SELECT result_tags, COUNT(*) 
                FROM consultation_records 
                GROUP BY result_tags 
                ORDER BY count DESC LIMIT 10
            )";
            if (query.exec(sql)) {
                while(query.next()) {
                    QJsonObject item;
                    item["label"] = query.value(0).toString(); // tag
                    item["value"] = query.value(1).toInt();
                    resultMap.append(item);
                }
            }
        }

        // 关闭独立连接
        DBManager::instance().closeThreadConnection(connName);

        // 将结果发送回主线程
        if (safeClient) {
            // 构建回包
            QJsonObject response;
            response[JsonKeys::CMD] = (int)CmdType::GET_STATISTICS;
            response[JsonKeys::CODE] = 200;
            response[JsonKeys::DATA] = resultMap;

            // 回到创建 client 的线程 (即主线程) 执行 sendJson
            QMetaObject::invokeMethod(safeClient, [safeClient, response](){
                safeClient->sendJson(response);
            });
        }
    });
}