#pragma once
//
// WordMapper.h - 单词行 <-> 实体的映射（内部复用）
//
#include "data/SqliteDb.h"
#include "domain/Entities.h"

#include <QString>

namespace vt {
namespace mapper {

// 单词表的列顺序（供 JOIN 查询复用），别名统一为 w
QString wordColumns(const QString &alias = QStringLiteral("w"));

// 从结果行读取 Word，base 为 wordColumns 起始列索引
Word wordFromStmt(const Stmt &st, int base);

// 序列化/反序列化（JSON 文本）
QString phrasesToJson(const QList<Phrase> &list);
QList<Phrase> phrasesFromJson(const QString &json);
QString sentencesToJson(const QList<Sentence> &list);
QList<Sentence> sentencesFromJson(const QString &json);
QString synonymsToJson(const QList<Synonym> &list);
QList<Synonym> synonymsFromJson(const QString &json);
QString relWordsToJson(const QList<RelWord> &list);
QList<RelWord> relWordsFromJson(const QString &json);
QString examsToJson(const QList<ExamItem> &list);
QList<ExamItem> examsFromJson(const QString &json);

} // namespace mapper
} // namespace vt
