# 基本操作序列与实现对照

下面对应“基本操作序列图”的编号步骤，给出实现代码位置。

| 步骤 | 交互 | 实现 |
| --- | --- | --- |
| 1 | 用户输入离线账户信息 | `LoginDialog`（用户名/密码/本地账户选择） |
| 2–3 | 登录界面 → 账户管理器：验证本地账户，返回结果 | `AuthService::login`（`UserRepository::findByUsername` + `rules::hashPassword` 校验） |
| 4 | 显示登录成功或失败 | `LoginDialog::setStatus` / `main.cpp` 打开主界面 |
| 5 | 用户进入主界面 | `MainWindow`（含 `AppContext::setCurrentUser`） |
| 6–7 | 主界面 → 查询管理器：查询字典 / 查询单词 | `DictionaryPage::onSearch` → `DictionaryService::search` |
| 8–9 | 查询管理器 → 单词表：读取单词数据 / 返回单词详情 | `WordRepository::search` / `findBySpelling` |
| 10–11 | 返回查询结果 / 显示单词详情 | `DictionaryPage` 表格 / `WordDialog` |
| 12–13 | 查看每日词表 / 生成每日词表 | `DailyListPage::onGenerate` → `DailyListService::generate` |
| 14–15 | 每日词表管理器 → 单词表：读取单词 / 返回单词 | `WordBookRepository::wordIdsOfBook` + `WordRepository::findByIds` |
| 16–17 | 每日词表管理器 → 错题本：读取错题 / 返回错题 | `WrongBookRepository::wordIdsOfBook` |
| 18–19 | 返回每日词表 / 显示每日词表 | `DailyListPage::loadList` |
| 20–21 | 开始做题 / 生成题表 | `QuizPage::beginQuiz` ← `QuestionService::generatePaperFromWords` |
| 22–23 | 题目生成器 → 单词表：读取单词 / 返回所需单词 | `WordRepository::randomByBook`（干扰项词池）+ `findByIds` |
| 24–25 | 题目生成器 → 错题本：读取错题 / 返回所需错题 | `WrongBookRepository`（`WrongOnly`/`Mixed` 模式词源） |
| 26–27 | 返回题表 / 显示题目 | `QuizPage::showQuestion` |
| 28 | 提交答案 | `QuizPage::onSubmit` → `QuestionService::submitAnswer` |
| 29–31 | 更新错题本 / 加入错题 / 返回更新结果 | `WrongBookRepository::recordWrong`（自动管理） |
| 32–33 | 返回结果 / 显示结果 | `QuestionService::finishPaper` → `ResultPage::showResult` |
| 34–35 | 查看或编辑错题本 / 访问错题本 | `WrongBookPage` |
| 36–38 | 读取或修改错题本 / 返回结果 | `WrongBookRepository::listEntries / updateEntry` |
| 39 | 显示更新后的错题本 | `WrongBookPage::refresh` |
| 40–41 | 手动同步 / 执行手动同步 | `SyncPage::onExport/onImport` → `SyncService` |
| 42–43 | 导出错题本 / 导出个人设置 | `SyncService::exportTo`（调用 `SettingService::toJson`） |
| 44–45 | 返回同步结果 / 显示同步完成 | `SyncPage` 提示 + `SyncLogRepository::add` |
| 46–47 | 登出 / 登出处理 | `MainWindow::onLogout`（`AppContext::clearCurrentUser`） |
| 49 | 显示登录界面 | `main.cpp`：登出后重新显示 `LoginDialog` |

## 关键时序说明

### 登录 → 主界面
```
LoginDialog → AuthService::login → UserRepository::findByUsername
                                  → rules::hashPassword 校验
                                  → UserRepository::updateLastLogin
                                  → ensureUserScaffold（设置/单词本/错题本）
main.cpp → AppContext::setCurrentUser → MainWindow
```

### 生成每日词表 → 做题 → 交卷
```
DailyListPage::onGenerate → DailyListService::generate
   ├─ WordBookRepository::wordIdsOfBook          （单词表词源）
   └─ WrongBookRepository::wordIdsOfBook          （错题本词源）
QuizPage::beginQuiz ← QuestionService::generatePaperFromWords
   → QuestionStrategyRegistry::generate（策略分发）
   → QuestionRepository::insert + PaperRepository::addEntry
QuizPage::onSubmit → QuestionService::submitAnswer
   ├─ rules::checkAnswer
   ├─ WordBookRepository::updateEntryProgress     （熟悉度/复习安排）
   └─ WrongBookRepository::recordWrong            （错题自动管理）
ResultPage ← QuestionService::finishPaper → rules::summarize
```

### 手动同步
```
SyncPage::onExport → SyncService::exportTo
   ├─ WrongBookRepository::listEntries  → 序列化为 JSON
   └─ SettingService::toJson            → 写入文件
   → SyncLogRepository::add（记录日志）
SyncPage::onImport → SyncService::importFrom(path, policy)
   ├─ 逐条比对本地/远端错题（更新较新者 / 本地优先 / 导入优先 / 合并）
   ├─ 设置冲突处理
   └─ SyncLogRepository::add（记录冲突数与结果）
```
