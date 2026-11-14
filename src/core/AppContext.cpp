#include "core/AppContext.h"

#include "core/Logger.h"

namespace vt {

AppContext &AppContext::instance() {
    static AppContext ctx;
    return ctx;
}

bool AppContext::init(const QString &dbPath) {
    if (!m_db.open(dbPath)) {
        Logger::error(QStringLiteral("App"), QStringLiteral("数据库打开失败: %1").arg(dbPath));
        return false;
    }

    m_users = std::make_unique<UserRepository>(m_db);
    m_settings = std::make_unique<SettingRepository>(m_db);
    m_words = std::make_unique<WordRepository>(m_db);
    m_bookRepo = std::make_unique<WordBookRepository>(m_db);
    m_wrongRepo = std::make_unique<WrongBookRepository>(m_db);
    m_questions = std::make_unique<QuestionRepository>(m_db);
    m_papers = std::make_unique<PaperRepository>(m_db);
    m_daily = std::make_unique<DailyListRepository>(m_db);
    m_syncLogs = std::make_unique<SyncLogRepository>(m_db);
    m_plans = std::make_unique<StudyPlanRepository>(m_db);

    m_auth = std::make_unique<AuthService>(*m_users, *m_settings, *m_bookRepo, *m_wrongRepo);
    m_settingService = std::make_unique<SettingService>(*m_settings);
    m_wordService = std::make_unique<WordService>(*m_words, m_db);
    m_wordBookService = std::make_unique<WordBookService>(*m_bookRepo, *m_auth, m_db);
    m_wrongBookService = std::make_unique<WrongBookService>(*m_wrongRepo);
    m_questionService = std::make_unique<QuestionService>(*m_questions, *m_papers, *m_words, *m_bookRepo,
                                                          *m_wrongRepo, *m_settings, *m_plans);
    m_dailyService = std::make_unique<DailyListService>(*m_daily, *m_plans, *m_bookRepo, *m_wrongRepo,
                                                        *m_words, *m_settings);
    m_dictionary = std::make_unique<DictionaryService>(*m_wordService, *m_wordBookService, *m_wrongBookService);
    m_sync = std::make_unique<SyncService>(*m_wrongRepo, *m_settings, *m_syncLogs, *m_users);
    m_audio = std::make_unique<AudioPlayer>();

    Logger::info(QStringLiteral("App"), QStringLiteral("应用上下文初始化完成"));
    return true;
}

void AppContext::shutdown() {
    if (m_audio) m_audio->stop();
    m_db.close();
}

void AppContext::setCurrentUser(const User &u) {
    m_currentUserId = u.id;
    // 应用该用户的音频设置
    const Setting s = m_settings->load(u.id);
    if (m_audio) m_audio->setEnabled(s.audioEnabled);
    emit currentUserChanged(u.id);
    emit dataChanged();
}

void AppContext::clearCurrentUser() {
    m_currentUserId = 0;
    emit currentUserChanged(0);
}

Setting AppContext::currentSetting() {
    if (m_currentUserId <= 0) return Setting{};
    return m_settings->load(m_currentUserId);
}

} // namespace vt
