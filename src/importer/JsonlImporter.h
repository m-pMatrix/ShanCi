#pragma once
//
// JsonlImporter.h - 词表导入（有道 JSONL 格式）
//
#include "data/Repositories.h"
#include "domain/Entities.h"

#include <QString>
#include <functional>

namespace vt {

struct ImportStats {
    int inserted = 0;
    int updated = 0;
    int skipped = 0;
    int failed = 0;
    qint64 elapsedMs = 0;
    int total() const { return inserted + updated + skipped + failed; }
};

class JsonlImporter {
public:
    explicit JsonlImporter(WordRepository &repo) : m_repo(repo) {}

    // 解析单行 JSON 为 Word（不落库），便于测试
    static Word parseLine(const QByteArray &line, BookType book, bool *ok);

    // 从文件导入；book 为空时按行内 bookId 推断
    ImportStats importFile(const QString &path, bool replaceExisting = false,
                           const std::function<void(int, int)> &progress = {});

    // 导入目录下所有 *.jsonl（按文件名推断书别）
    ImportStats importDirectory(const QString &dir, bool replaceExisting = false);

    // 依据文件名/ bookId 推断书别
    static BookType inferBook(const QString &hint);

private:
    WordRepository &m_repo;
};

} // namespace vt
