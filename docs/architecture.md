# 体系结构设计

系统采用经典的分层（Layered）架构，自上而下为：表示层 → 应用服务层 → 领域层 → 数据访问层；
基础设施与辅助功能作为横切能力贯穿各层。

```
┌──────────────────────────────────────────────────────────────┐
│ 表示层 (ui)                                                    │
│  LoginDialog · MainWindow · HomePage · WordBookPage            │
│  WrongBookPage · DailyListPage · QuizPage · ResultPage         │
│  DictionaryPage · SettingsPage · SyncPage · WordDialog         │
└───────────────┬──────────────────────────────────────────────┘
                │ 调用（依赖接口 Navigator / AppContext）
┌───────────────▼──────────────────────────────────────────────┐
│ 应用服务层 (service)                                           │
│  AuthService · WordService · WordBookService · WrongBookService│
│  QuestionService · DailyListService · DictionaryService        │
│  SettingService · SyncService                                  │
└───────────────┬──────────────────────────────────────────────┘
                │
┌───────────────▼──────────────┐   ┌───────────────────────────┐
│ 领域层 (domain)               │   │ 题目生成 (generator)       │
│  实体 Entities · 枚举 Types   │   │  IQuestionStrategy 及实现  │
│  规则 Rules（校验/评分/复习）  │   │  DistractorGenerator       │
└───────────────┬──────────────┘   └───────────────────────────┘
                │
┌───────────────▼──────────────────────────────────────────────┐
│ 数据访问层 (data)                                              │
│  SqliteDb · Database(建表/迁移) · Repository/DAO               │
│  UserRepository · SettingRepository · WordRepository           │
│  WordBookRepository · WrongBookRepository · QuestionRepository │
│  PaperRepository · DailyListRepository · SyncLogRepository     │
└───────────────┬──────────────────────────────────────────────┘
                │
        SQLite (vocab.db) · 本地文件 (audio / sync / JSON)
```

横切能力（`src/core`）：`AppContext`（依赖装配）、`Config`（配置管理）、`Logger`（日志）、
`Paths`（路径管理）、`AudioPlayer`（音频）、`Result/Status`（统一错误返回，替代异常穿透）。

---

## 一、各层职责

### 1. 表示层（ui）
- 只负责界面呈现与用户交互，**不直接访问数据库**。
- 通过 `AppContext` 获取服务，通过 `Navigator` 接口请求页面跳转。
- `MainWindow` 实现 `Navigator`：`navigateTo / startQuiz / showResult`。
- `Page` 为所有页面基类，提供 `pageId()` 与 `refresh()`。

### 2. 应用服务层（service）
- 编排用例，组合仓储与领域规则，返回 `Result<T>` / `Status`。
- 事务边界、跨聚合业务（如“交卷后同时更新单词本与错题本”）在此层完成。

### 3. 领域层（domain）
- `Entities.h`：用户、设置、单词、单词本/条目、错题本/条目、题目、题表/条目、每日词表/条目、同步日志等实体。
- `Rules.h`：纯逻辑规则——答案校验、拼写容错、评分、复习间隔、熟悉度更新、错题优先级/状态、账户校验与口令哈希。

### 4. 题目生成（generator）
- `IQuestionStrategy` 策略接口 + 四个实现（EnToCn / CnToEn / SimilarMatch / Cloze）。
- `QuestionStrategyRegistry` 负责按题型分发，并在题型不可用时回退。
- `DistractorGenerator` 负责干扰项生成（释义、拼写、形近词）与选项打乱。

### 5. 数据访问层（data）
- `SqliteDb`/`Stmt`：SQLite C API 的薄封装，预编译语句、参数绑定、事务。
- `Database`：单例，负责打开数据库、建表（幂等）、开启外键与 WAL。
- 各 `Repository`：以聚合为单位的读写，输出领域实体（JSON 字段自动序列化/反序列化）。

### 6. 基础设施（core）
- `Paths`：统一管理数据目录（便携式：可执行文件同级 `data`）。
- `Config`：应用级配置（主题等）；`Logger`：文件 + 控制台日志。
- `Result/Status`：统一错误传递，服务层不抛出异常。

---

## 二、关键设计决策

1. **依赖倒置**：服务层依赖仓储（数据访问），表示层依赖服务；页面通过 `Navigator` 抽象与主窗口解耦。
2. **服务返回 Result**：避免异常穿透 UI，UI 统一以消息框呈现 `error()`。
3. **策略模式**：题型生成可扩展——新增题型只需实现 `IQuestionStrategy` 并注册。
4. **便携式数据目录**：数据与程序同目录，便于打包分发与离线使用。
5. **事务批量导入**：词表导入在单事务内完成，8535 词条约 1.3 秒。
6. **重复词条合并**：同一 `headWord` 的多条记录（不同义项）在导入时合并，避免信息丢失。
