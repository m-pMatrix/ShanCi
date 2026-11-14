#pragma once
//
// AppContext.h - 应用上下文：装配数据访问层与服务层
//
#include "core/AudioPlayer.h"
#include "data/Database.h"
#include "data/Repositories.h"
#include "domain/Entities.h"
#include "service/Services.h"

#include <QObject>

#include <memory>

namespace vt {

class AppContext : public QObject {
    Q_OBJECT
public:
    static AppContext &instance();

    // 打开数据库并装配所有依赖
    bool init(const QString &dbPath);
    void shutdown();

    // 数据访问层
    Database &db() { return m_db; }
    UserRepository &users() { return *m_users; }
    SettingRepository &settings() { return *m_settings; }
    WordRepository &words() { return *m_words; }
    WordBookRepository &wordBooks() { return *m_bookRepo; }
    WrongBookRepository &wrongBooks() { return *m_wrongRepo; }
    QuestionRepository &questions() { return *m_questions; }
    PaperRepository &papers() { return *m_papers; }
    DailyListRepository &dailyLists() { return *m_daily; }
    SyncLogRepository &syncLogs() { return *m_syncLogs; }
    StudyPlanRepository &studyPlans() { return *m_plans; }

    // 应用服务层
    AuthService &auth() { return *m_auth; }
    SettingService &settingService() { return *m_settingService; }
    WordService &wordService() { return *m_wordService; }
    WordBookService &wordBookService() { return *m_wordBookService; }
    WrongBookService &wrongBookService() { return *m_wrongBookService; }
    QuestionService &questionService() { return *m_questionService; }
    DailyListService &dailyListService() { return *m_dailyService; }
    DictionaryService &dictionaryService() { return *m_dictionary; }
    SyncService &syncService() { return *m_sync; }
    AudioPlayer &audio() { return *m_audio; }

    // 当前登录用户
    qint64 currentUserId() const { return m_currentUserId; }
    void setCurrentUser(const User &u);
    void clearCurrentUser();
    Setting currentSetting();

signals:
    void currentUserChanged(qint64 userId);
    void dataChanged();

private:
    AppContext() = default;

    Database m_db;
    std::unique_ptr<UserRepository> m_users;
    std::unique_ptr<SettingRepository> m_settings;
    std::unique_ptr<WordRepository> m_words;
    std::unique_ptr<WordBookRepository> m_bookRepo;
    std::unique_ptr<WrongBookRepository> m_wrongRepo;
    std::unique_ptr<QuestionRepository> m_questions;
    std::unique_ptr<PaperRepository> m_papers;
    std::unique_ptr<DailyListRepository> m_daily;
    std::unique_ptr<SyncLogRepository> m_syncLogs;
    std::unique_ptr<StudyPlanRepository> m_plans;

    std::unique_ptr<AuthService> m_auth;
    std::unique_ptr<SettingService> m_settingService;
    std::unique_ptr<WordService> m_wordService;
    std::unique_ptr<WordBookService> m_wordBookService;
    std::unique_ptr<WrongBookService> m_wrongBookService;
    std::unique_ptr<QuestionService> m_questionService;
    std::unique_ptr<DailyListService> m_dailyService;
    std::unique_ptr<DictionaryService> m_dictionary;
    std::unique_ptr<SyncService> m_sync;
    std::unique_ptr<AudioPlayer> m_audio;

    qint64 m_currentUserId = 0;
};

} // namespace vt
