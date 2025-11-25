#include "DBManager.h"
#include <QCryptographicHash>

DBManager& DBManager::instance() {
    static DBManager instance;
    return instance;
}

DBManager::DBManager() {}

DBManager::~DBManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

QSqlDatabase DBManager::getDatabase() const {
    return m_db;
}

bool DBManager::connectToDatabase() {
    // 获取配置
    const DBConfig &config = ConfigManager::instance().db();
    
    m_dbName = config.dbName;
    
    if (!QSqlDatabase::isDriverAvailable("QMYSQL")) {
        qCritical() << "Error: QMYSQL driver not loaded!";
        return false;
    }

    // 连接 MySQL 服务 (不指定数据库名)
    {
        const QString tempConnName = "TempInitConnection";  
        // 确保没有残留的连接  
        if (QSqlDatabase::contains(tempConnName)) {  
            QSqlDatabase::removeDatabase(tempConnName);  
        }  
  
        QSqlDatabase tempDb = QSqlDatabase::addDatabase("QMYSQL", tempConnName);  
        tempDb.setHostName(config.host);  
        tempDb.setPort(config.port);  
        tempDb.setUserName(config.username);  
        tempDb.setPassword(config.password);  
  
        if (tempDb.open()) {  
            QSqlQuery query(tempDb);  
            QString createSql = QString("CREATE DATABASE IF NOT EXISTS %1 "  
                                      "CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci")  
                                      .arg(config.dbName);  
            query.exec(createSql);  
            tempDb.close();  
        } else {  
            qCritical() << "Failed to connect to MySQL server to init DB:" << tempDb.lastError().text();  
            return false;  
        }
    }
    // 移除临时连接名，防止干扰
    QSqlDatabase::removeDatabase("TempInitConnection");

    // 连接到具体的数据库
    const QString mainConnName = "PsyServerMainConnection"; 
    if (QSqlDatabase::contains(mainConnName)) {  
        m_db = QSqlDatabase::database(mainConnName);  
    } else {  
        m_db = QSqlDatabase::addDatabase("QMYSQL", mainConnName);  
    }
    m_db.setHostName(config.host);
    m_db.setPort(config.port);
    m_db.setUserName(config.username);
    m_db.setPassword(config.password);
    m_db.setDatabaseName(config.dbName);
    
    if (!m_db.open()) {
        qCritical() << "Step D Failed: Could not open database" << config.dbName;
        return false;
    }

    qDebug() << "Step D Success: Connected to" << config.dbName;
    return true;
}

bool DBManager::initTables() {
    bool success = true;

    // 1. 用户表 (users)
    // role: 1=Student, 2=Doctor, 3=Admin
    success &= createTable("users", R"(
        CREATE TABLE IF NOT EXISTS users (
            id INT AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(50) NOT NULL UNIQUE,
            password VARCHAR(255) NOT NULL,
            role INT NOT NULL,
            real_name VARCHAR(50),
            gender VARCHAR(10),
            age INT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        ) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
    )");

    // 2. 医生信息表 (doctor_info)
    // 扩展 users 表，存储医生特有信息
    success &= createTable("doctor_info", R"(
        CREATE TABLE IF NOT EXISTS doctor_info (
            user_id INT PRIMARY KEY,
            intro TEXT,
            specialized_field VARCHAR(255),
            avatar_path VARCHAR(255),
            FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
        ) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
    )");

    // 3. 医生排班表 (schedules)
    // time_slot_flags: 使用位掩码或简单的 0/1 字符串表示一天中哪些时间段有空
    // 比如: INT 类型，二进制 00001111 表示前4个时间段空闲
    success &= createTable("schedules", R"(
        CREATE TABLE IF NOT EXISTS schedules (
            id INT AUTO_INCREMENT PRIMARY KEY,
            doctor_id INT NOT NULL,
            date DATE NOT NULL,
            time_slot_flags INT DEFAULT 0,
            status INT DEFAULT 1, 
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE,
            UNIQUE KEY unique_schedule (doctor_id, date)
        ) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
    )");

    // 4. 预约记录表 (appointments)
    // status: 0=Pending, 1=Confirmed, 2=Completed, 3=Cancelled
    success &= createTable("appointments", R"(
        CREATE TABLE IF NOT EXISTS appointments (
            id INT AUTO_INCREMENT PRIMARY KEY,
            student_id INT NOT NULL,
            doctor_id INT NOT NULL,
            date DATE NOT NULL,
            time_slot INT NOT NULL,
            status INT DEFAULT 0,
            create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            modify_request_json JSON,
            FOREIGN KEY (student_id) REFERENCES users(id) ON DELETE CASCADE,
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE
        ) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
    )");

    // 5. 问卷模板表 (surveys)
    // content_json: 存储题目数组 eg：[{"q":"最近睡眠如何?", "options":["好","坏"]}]
    success &= createTable("surveys", R"(
        CREATE TABLE IF NOT EXISTS surveys (
            id INT AUTO_INCREMENT PRIMARY KEY,
            doctor_id INT NOT NULL,
            title VARCHAR(100) NOT NULL,
            content_json JSON NOT NULL,
            create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE
        ) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
    )");

    // 6. 问卷回答表 (survey_answers)
    // answers_json: 存储学生提交的答案
    success &= createTable("survey_answers", R"(
        CREATE TABLE IF NOT EXISTS survey_answers (
            id INT AUTO_INCREMENT PRIMARY KEY,
            appt_id INT NOT NULL,
            student_id INT NOT NULL,
            answers_json JSON NOT NULL,
            submit_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (appt_id) REFERENCES appointments(id) ON DELETE CASCADE,
            FOREIGN KEY (student_id) REFERENCES users(id) ON DELETE CASCADE
        ) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
    )");

    // 7. 咨询记录/报告表 (consultation_records)
    // tags: 用于统计 "最常遇到的心理问题"
    success &= createTable("consultation_records", R"(
        CREATE TABLE IF NOT EXISTS consultation_records (
            id INT AUTO_INCREMENT PRIMARY KEY,
            appt_id INT NOT NULL,
            doctor_id INT NOT NULL,
            report_content TEXT,
            result_tags VARCHAR(255),
            create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (appt_id) REFERENCES appointments(id) ON DELETE CASCADE,
            FOREIGN KEY (doctor_id) REFERENCES users(id) ON DELETE CASCADE
        ) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
    )");

    // 初始化默认数据
    if (success) {
        seedDefaultAdmin();
    }

    return success;
}

bool DBManager::createTable(const QString &tableName, const QString &sql) {
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        qCritical() << "Failed to create table" << tableName << ":" << query.lastError().text();
        return false;
    }
    // qDebug() << "Table checked/created:" << tableName;
    return true;
}

void DBManager::seedDefaultAdmin() {
    QSqlQuery query(m_db);
    // 检查是否已存在 Admin
    query.exec("SELECT count(*) FROM users WHERE role = 3"); // 3 = ADMIN
    if (query.next() && query.value(0).toInt() > 0) {
        return; // 已存在，跳过
    }

    // 插入默认管理员: admin / 123456
    qDebug() << "Seeding default admin account...";
    query.prepare("INSERT INTO users (username, password, role, real_name) VALUES (:u, :p, :r, :n)");
    query.bindValue(":u", "admin");
    QString hashedPassword = QString(QCryptographicHash::hash("123456", QCryptographicHash::Sha256).toHex());
    query.bindValue(":p", hashedPassword);
    query.bindValue(":r", 3);
    query.bindValue(":n", "System Admin");
    
    if (!query.exec()) {
        qWarning() << "Failed to seed admin:" << query.lastError().text();
    } else {
        qDebug() << "Default Admin created. User: admin, Pass: 123456";
    }
}
