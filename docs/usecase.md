# 用例与实现对照

本文档将需求中的用例图/功能层次映射到具体实现代码，便于评审与答辩。

## 一、用例（用例图）对照

| 用例 | 说明 | 实现位置 |
| --- | --- | --- |
| 离线登录 | 校验本地账户口令 | `AuthService::login` / `LoginDialog::onLogin` |
| 新建账户 | 创建本地账户与默认数据 | `AuthService::createAccount` / `LoginDialog::onCreateAccount` |
| 选择本地账户 | 从已存账户中选择 | `LoginDialog::reloadAccounts` / `onAccountSelected` |
| 查询单词本 | 查询/筛选单词本条目 | `WordBookPage` + `WordBookService::listEntries` |
| 查看单词详情 | 展示释义/例句/同近义词等 | `WordDialog` |
| 更改个人设置 | 修改每日词数/题型/主题等 | `SettingsPage` + `SettingService::update` |
| 查看每日词表 | 查看词表与完成状态 | `DailyListPage` + `DailyListService::entries` |
| 生成每日词表 | 按模式生成 | `DailyListService::generate` |
| ↳ 混合错题库与单词表 | 模式 `Mixed` | `DailyListService::generate` |
| ↳ 仅从单词表选取 | 模式 `WordBookOnly` | 同上 |
| ↳ 仅从错题本选取 | 模式 `WrongOnly` | 同上 |
| 做题 | 逐题作答 | `QuizPage` |
| 生成题表 | 由词源生成题目集合 | `QuestionService::generatePaper` / `generatePaperFromWords` |
| ↳ 英文单词选中文释义 | 题型 `EnToCn` | `EnToCnStrategy` |
| ↳ 中文释义拼写英文单词 | 题型 `CnToEn` | `CnToEnStrategy` |
| ↳ 相似词匹配词义 | 题型 `SimilarMatch` | `SimilarMatchStrategy` |
| ↳ 选词填空 | 题型 `Cloze` | `ClozeStrategy` |
| 查看错题本 | 查询/筛选错题 | `WrongBookPage` + `WrongBookService::list` |
| 编辑错题本 | 修改备注/我的答案/优先级/状态 | `WrongBookPage::onEdit` + `WrongBookService::update` |
| 自动管理错题本 | 答错自动加入、答对累计 | `QuestionService::submitAnswer` |
| 手动同步 | 同步错题本与个人设置 | `SyncPage` + `SyncService` |
| ↳ 同步错题本 | 导出/导入错题条目 | `SyncService::exportTo / importFrom` |
| ↳ 同步个人设置 | 导出/导入设置 | `SettingService::toJson / fromJson` |
| 登出 | 返回登录界面 | `MainWindow::onLogout` → `main.cpp` 重新显示登录框 |

## 二、功能层次（七层）对照

- **第一层 表示层与用户交互**：`src/ui/**`（登录、主界面导航、各功能页、详情/编辑对话框）。
- **第二层 应用服务层**：`src/service/**` 的九个服务。
- **第三层 领域层与核心规则**：`src/domain/Entities.h`（实体）、`Rules.h/.cpp`（规则）。
- **第四层 题目生成与练习**：`src/generator/**` + `QuestionService`。
- **第五层 同步功能**：`SyncService`（导出/导入/冲突处理/日志）。
- **第六层 数据访问与存储**：`src/data/**`（SQLite + Repository/DAO）。
- **第七层 基础设施与辅助**：`src/core/**`（配置、日志、异常、音频、路径）+ `deploy.ps1`。

## 三、实体属性对照（第三层）

| 实体 | 关键属性 | 代码 |
| --- | --- | --- |
| 单词 | 拼写、词性、音标、音频、中英释义、语法（同根/同近义/短语）、例句、来源 | `Word` |
| 题目 | 类型、题干、选项、答案、解析、难度、来源（所属单词） | `Question` |
| 题表/试卷 | 标题、生成模式、创建时间、完成时间、得分 | `Paper` |
| 作答明细 | 用户答案、是否正确、答题时间 | `PaperEntry` |
| 错题条目 | 错误次数、正确次数、最近错误时间、最近复习时间、优先级、状态 | `WrongEntry` |
| 单词本条目 | 熟悉度、复习次数、下次复习时间 | `WordBookEntry` |
| 每日词表条目 | 来源类型、顺序、是否完成 | `DailyEntry` |
| 用户 / 个人设置 | 账户、每日词数、题型偏好、主题、同步路径 | `User` / `Setting` |
| 同步日志 | 方向、文件、条目数、冲突数、状态、说明 | `SyncLog` |
