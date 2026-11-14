#include "domain/Types.h"

namespace vt {

QString toKey(BookType v) {
    switch (v) {
    case BookType::CET4: return QStringLiteral("CET4");
    case BookType::CET6: return QStringLiteral("CET6");
    case BookType::Custom: return QStringLiteral("CUSTOM");
    }
    return QStringLiteral("CUSTOM");
}

QString toKey(QuestionType v) {
    switch (v) {
    case QuestionType::EnToCn: return QStringLiteral("EnToCn");
    case QuestionType::CnToEn: return QStringLiteral("CnToEn");
    case QuestionType::SimilarMatch: return QStringLiteral("SimilarMatch");
    case QuestionType::Cloze: return QStringLiteral("Cloze");
    }
    return QStringLiteral("EnToCn");
}

QString toKey(EntrySource v) {
    switch (v) {
    case EntrySource::WordBook: return QStringLiteral("WordBook");
    case EntrySource::WrongBook: return QStringLiteral("WrongBook");
    case EntrySource::Manual: return QStringLiteral("Manual");
    case EntrySource::New: return QStringLiteral("New");
    case EntrySource::Review: return QStringLiteral("Review");
    }
    return QStringLiteral("WordBook");
}

QString toKey(PaperMode v) {
    switch (v) {
    case PaperMode::Random: return QStringLiteral("Random");
    case PaperMode::WordBookOnly: return QStringLiteral("WordBookOnly");
    case PaperMode::WrongOnly: return QStringLiteral("WrongOnly");
    case PaperMode::Mixed: return QStringLiteral("Mixed");
    case PaperMode::Plan: return QStringLiteral("Plan");
    }
    return QStringLiteral("Mixed");
}

QString toKey(WrongStatus v) {
    switch (v) {
    case WrongStatus::New: return QStringLiteral("NEW");
    case WrongStatus::Reviewing: return QStringLiteral("REVIEWING");
    case WrongStatus::Mastered: return QStringLiteral("MASTERED");
    }
    return QStringLiteral("NEW");
}

QString toKey(WrongPriority v) {
    switch (v) {
    case WrongPriority::Low: return QStringLiteral("LOW");
    case WrongPriority::Normal: return QStringLiteral("NORMAL");
    case WrongPriority::High: return QStringLiteral("HIGH");
    }
    return QStringLiteral("NORMAL");
}

QString toKey(SyncDirection v) {
    return v == SyncDirection::Export ? QStringLiteral("EXPORT") : QStringLiteral("IMPORT");
}

QString toKey(SyncStatus v) {
    switch (v) {
    case SyncStatus::Success: return QStringLiteral("SUCCESS");
    case SyncStatus::Partial: return QStringLiteral("PARTIAL");
    case SyncStatus::Failed: return QStringLiteral("FAILED");
    }
    return QStringLiteral("SUCCESS");
}

BookType bookTypeFromKey(const QString &k, BookType def) {
    const QString s = k.trimmed().toUpper();
    if (s == QLatin1String("CET4")) return BookType::CET4;
    if (s == QLatin1String("CET6")) return BookType::CET6;
    if (s == QLatin1String("CUSTOM")) return BookType::Custom;
    return def;
}

QuestionType questionTypeFromKey(const QString &k, QuestionType def) {
    const QString s = k.trimmed();
    if (s.compare(QLatin1String("EnToCn"), Qt::CaseInsensitive) == 0) return QuestionType::EnToCn;
    if (s.compare(QLatin1String("CnToEn"), Qt::CaseInsensitive) == 0) return QuestionType::CnToEn;
    if (s.compare(QLatin1String("SimilarMatch"), Qt::CaseInsensitive) == 0) return QuestionType::SimilarMatch;
    if (s.compare(QLatin1String("Cloze"), Qt::CaseInsensitive) == 0) return QuestionType::Cloze;
    return def;
}

EntrySource entrySourceFromKey(const QString &k, EntrySource def) {
    const QString s = k.trimmed();
    if (s.compare(QLatin1String("WordBook"), Qt::CaseInsensitive) == 0) return EntrySource::WordBook;
    if (s.compare(QLatin1String("WrongBook"), Qt::CaseInsensitive) == 0) return EntrySource::WrongBook;
    if (s.compare(QLatin1String("Manual"), Qt::CaseInsensitive) == 0) return EntrySource::Manual;
    if (s.compare(QLatin1String("New"), Qt::CaseInsensitive) == 0) return EntrySource::New;
    if (s.compare(QLatin1String("Review"), Qt::CaseInsensitive) == 0) return EntrySource::Review;
    return def;
}

PaperMode paperModeFromKey(const QString &k, PaperMode def) {
    const QString s = k.trimmed();
    if (s.compare(QLatin1String("Random"), Qt::CaseInsensitive) == 0) return PaperMode::Random;
    if (s.compare(QLatin1String("WordBookOnly"), Qt::CaseInsensitive) == 0) return PaperMode::WordBookOnly;
    if (s.compare(QLatin1String("WrongOnly"), Qt::CaseInsensitive) == 0) return PaperMode::WrongOnly;
    if (s.compare(QLatin1String("Mixed"), Qt::CaseInsensitive) == 0) return PaperMode::Mixed;
    if (s.compare(QLatin1String("Plan"), Qt::CaseInsensitive) == 0) return PaperMode::Plan;
    return def;
}

WrongStatus wrongStatusFromKey(const QString &k, WrongStatus def) {
    const QString s = k.trimmed().toUpper();
    if (s == QLatin1String("NEW")) return WrongStatus::New;
    if (s == QLatin1String("REVIEWING")) return WrongStatus::Reviewing;
    if (s == QLatin1String("MASTERED")) return WrongStatus::Mastered;
    return def;
}

WrongPriority wrongPriorityFromKey(const QString &k, WrongPriority def) {
    const QString s = k.trimmed().toUpper();
    if (s == QLatin1String("LOW")) return WrongPriority::Low;
    if (s == QLatin1String("NORMAL")) return WrongPriority::Normal;
    if (s == QLatin1String("HIGH")) return WrongPriority::High;
    return def;
}

SyncDirection syncDirectionFromKey(const QString &k, SyncDirection def) {
    const QString s = k.trimmed().toUpper();
    if (s == QLatin1String("EXPORT")) return SyncDirection::Export;
    if (s == QLatin1String("IMPORT")) return SyncDirection::Import;
    return def;
}

SyncStatus syncStatusFromKey(const QString &k, SyncStatus def) {
    const QString s = k.trimmed().toUpper();
    if (s == QLatin1String("SUCCESS")) return SyncStatus::Success;
    if (s == QLatin1String("PARTIAL")) return SyncStatus::Partial;
    if (s == QLatin1String("FAILED")) return SyncStatus::Failed;
    return def;
}

QString displayName(QuestionType v) {
    switch (v) {
    case QuestionType::EnToCn: return QStringLiteral("英文选中文释义");
    case QuestionType::CnToEn: return QStringLiteral("中文释义拼写英文");
    case QuestionType::SimilarMatch: return QStringLiteral("相似词匹配词义");
    case QuestionType::Cloze: return QStringLiteral("选词填空");
    }
    return QString();
}

QString displayName(PaperMode v) {
    switch (v) {
    case PaperMode::Random: return QStringLiteral("随机抽取");
    case PaperMode::WordBookOnly: return QStringLiteral("单词本");
    case PaperMode::WrongOnly: return QStringLiteral("错题本");
    case PaperMode::Mixed: return QStringLiteral("混合");
    case PaperMode::Plan: return QStringLiteral("每日背诵计划");
    }
    return QString();
}

QString displayName(EntrySource v) {
    switch (v) {
    case EntrySource::WordBook: return QStringLiteral("单词本");
    case EntrySource::WrongBook: return QStringLiteral("错题本");
    case EntrySource::Manual: return QStringLiteral("手动");
    case EntrySource::New: return QStringLiteral("新词");
    case EntrySource::Review: return QStringLiteral("复习");
    }
    return QString();
}

QString displayName(WrongStatus v) {
    switch (v) {
    case WrongStatus::New: return QStringLiteral("新错题");
    case WrongStatus::Reviewing: return QStringLiteral("复习中");
    case WrongStatus::Mastered: return QStringLiteral("已掌握");
    }
    return QString();
}

QString displayName(SyncDirection v) {
    return v == SyncDirection::Export ? QStringLiteral("导出") : QStringLiteral("导入");
}

QString displayName(BookType v) {
    switch (v) {
    case BookType::CET4: return QStringLiteral("四级");
    case BookType::CET6: return QStringLiteral("六级");
    case BookType::Custom: return QStringLiteral("自定义");
    }
    return QString();
}

QStringList questionTypesToKeys(const QList<QuestionType> &types) {
    QStringList out;
    for (auto t : types) out << toKey(t);
    return out;
}

QList<QuestionType> questionTypesFromKeys(const QStringList &keys) {
    QList<QuestionType> out;
    for (const auto &k : keys) {
        const QString s = k.trimmed();
        if (s.isEmpty()) continue;
        out << questionTypeFromKey(s);
    }
    return out;
}

} // namespace vt
