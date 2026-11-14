# 数据库设计（SQLite）

数据库文件：`<可执行目录>/data/vocab.db`
初始化：`Database::createTables()`（幂等，`CREATE TABLE IF NOT EXISTS`），并启用 `PRAGMA foreign_keys=ON`、`journal_mode=WAL`。

## 表结构

### users —— 用户（离线账户）
| 字段 | 类型 | 说明 |
| --- | --- | --- |
| id | INTEGER PK | 主键 |
| username | TEXT UNIQUE | 用户名 |
| password_hash | TEXT | SHA-256(salt::password::vocab) |
| salt | TEXT | 随机盐 |
| display_name | TEXT | 昵称 |
| created_at / last_login_at | TEXT | ISO 时间 |

### settings —— 个人设置（与用户一对一）
| 字段 | 说明 |
| --- | --- |
| user_id | PK，外键 → users |
| daily_word_count | 每日词数 |
| question_types | 题型偏好（逗号分隔：EnToCn,CnToEn,SimilarMatch,Cloze） |
| theme | light / dark |
| sync_path | 同步目录 |
| font_size | 字号 |
| audio_enabled / show_phonetic / auto_add_wrong / strict_spelling | 布尔开关（0/1） |
| updated_at | 更新时间（同步冲突比较依据） |

### words —— 单词
| 字段 | 说明 |
| --- | --- |
| id | PK |
| book | CET4 / CET6 / CUSTOM |
| word_rank | 词表序号 |
| spelling | 拼写（唯一键之一，`UNIQUE(book, spelling)`） |
| phonetic_uk / phonetic_us | 英/美音标 |
| audio_uk / audio_us | 发音资源 |
| star | 重要程度 |
| pos / translation_cn / translation_en | 首条释义（便于查询与排序） |
| mem_method | 记忆方法 |
| source | 来源（真题年份等） |
| phrases_json | 短语列表 |
| sentences_json | 例句列表（含真题例句） |
| synos_json | 同近义词 |
| relwords_json | 同根词 |
| exam_json | 真题题目 |
| created_at | 创建时间 |

索引：`idx_words_spelling(spelling)`、`idx_words_book(book, word_rank)`。

### word_books / word_book_entries —— 单词本及条目
- `word_books`：id、user_id、name、description、created_at、updated_at。
- `word_book_entries`：id、word_book_id、word_id、familiarity(0–5)、review_count、next_review_at、source、added_at；`UNIQUE(word_book_id, word_id)`。

### wrong_books / wrong_entries —— 错题本及条目
- `wrong_books`：id、user_id（`UNIQUE(user_id)`，每用户一个默认错题本）、name、created_at。
- `wrong_entries`：id、wrong_book_id、word_id、question_type、wrong_count、correct_count、last_wrong_at、last_review_at、next_review_at、priority(LOW/NORMAL/HIGH)、status(NEW/REVIEWING/MASTERED)、my_answer、remark、updated_at；`UNIQUE(wrong_book_id, word_id)`。

### questions —— 题目
id、type、stem、options_json、answer、explanation、difficulty、word_id、created_at。

### papers / paper_entries —— 题表（试卷）与作答明细
- `papers`：id、user_id、title、mode、total_count、correct_count、score、created_at、finished_at。
- `paper_entries`：id、paper_id、question_id、seq、user_answer、is_correct、answer_time_ms、answered_at。

### daily_lists / daily_entries —— 每日词表与条目
- `daily_lists`：id、user_id、list_date、mode、total_count、done_count、created_at、completed_at；`UNIQUE(user_id, list_date)`。
- `daily_entries`：id、daily_list_id、word_id、source_type(WordBook/WrongBook/Manual)、seq、is_completed；`UNIQUE(daily_list_id, word_id)`。

### sync_logs —— 同步日志
id、user_id、direction(EXPORT/IMPORT)、file_path、item_count、conflict_count、status(SUCCESS/PARTIAL/FAILED)、message、created_at。

### meta —— 元数据
key/value（预留：版本、迁移标记等）。

---

## 外键与级联

除 `words`（全局词库）与 `questions` 外，所有业务表均通过外键关联 `users` 或父表，
并配置 `ON DELETE CASCADE`，删除用户即级联清理其全部数据。

---

## 同步文件格式（JSON）

```json
{
  "format": "vocab-tutor-sync",
  "version": 1,
  "exportedAt": "2026-10-13T17:00:00",
  "username": "alice",
  "settings": {
    "dailyWordCount": 20,
    "questionTypes": "EnToCn,CnToEn,SimilarMatch,Cloze",
    "theme": "light",
    "fontSize": 13,
    "audioEnabled": true,
    "showPhonetic": true,
    "autoAddWrong": true,
    "strictSpelling": true,
    "updatedAt": "2026-10-13T16:59:00"
  },
  "wrongEntries": [
    {
      "spelling": "abandon", "book": "CET4", "questionType": "EnToCn",
      "wrongCount": 2, "correctCount": 1,
      "lastWrongAt": "2026-10-12T10:00:00", "lastReviewAt": "",
      "nextReviewAt": "2026-10-14T10:00:00",
      "priority": "NORMAL", "status": "REVIEWING",
      "myAnswer": "B", "remark": "", "updatedAt": "2026-10-12T10:00:00"
    }
  ]
}
```

- 错题条目以 `(book, spelling)` 作为跨库稳定标识，导入时据此匹配本地单词。
- 冲突策略：`较新者优先(PreferNewer)` / `以本地为准(PreferLocal)` / `以导入为准(PreferRemote)` / `合并(Merge)`。
