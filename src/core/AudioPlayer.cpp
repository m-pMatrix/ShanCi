#include "core/AudioPlayer.h"
#include "core/Logger.h"
#include "core/Paths.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

#ifdef VOCAB_HAS_MULTIMEDIA
#include <QAudioOutput>
#include <QMediaPlayer>
#else
#include <qt_windows.h>
#include <mmsystem.h>
#endif

namespace vt {

#ifdef VOCAB_HAS_MULTIMEDIA
AudioPlayer::AudioPlayer(QObject *parent) : QObject(parent) {
    m_player = new QMediaPlayer(this);
    m_output = new QAudioOutput(this);
    m_player->setAudioOutput(m_output);
    m_output->setVolume(1.0);
}
AudioPlayer::~AudioPlayer() = default;

void AudioPlayer::playFile(const QString &filePath) {
    if (!m_enabled || !QFileInfo::exists(filePath)) return;
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->play();
}

void AudioPlayer::playUrl(const QString &url) {
    if (!m_enabled || url.isEmpty()) return;
    m_player->setSource(QUrl(url));
    m_player->play();
}

void AudioPlayer::stop() { if (m_player) m_player->stop(); }

#else
// ---- 回退实现：Windows MCI ----
AudioPlayer::AudioPlayer(QObject *parent) : QObject(parent) {}
AudioPlayer::~AudioPlayer() { stop(); }

void AudioPlayer::playFile(const QString &filePath) {
    if (!m_enabled || !QFileInfo::exists(filePath)) return;
    stop();
    const QString shortPath = QDir::toNativeSeparators(filePath);
    const QString openCmd = QStringLiteral("open \"%1\" type mpegvideo alias vocabvoice").arg(shortPath);
    if (mciSendStringW(reinterpret_cast<const wchar_t *>(openCmd.utf16()), nullptr, 0, nullptr) == 0) {
        m_mciOpen = true;
        const QString playCmd = QStringLiteral("play vocabvoice from 0");
        mciSendStringW(reinterpret_cast<const wchar_t *>(playCmd.utf16()), nullptr, 0, nullptr);
    }
}

void AudioPlayer::playUrl(const QString &) {
    // 无多媒体模块时不支持流式播放
}

void AudioPlayer::stop() {
    if (m_mciOpen) {
        const QString closeCmd = QStringLiteral("close vocabvoice");
        mciSendStringW(reinterpret_cast<const wchar_t *>(closeCmd.utf16()), nullptr, 0, nullptr);
        m_mciOpen = false;
    }
}
#endif

void AudioPlayer::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled) stop();
}

void AudioPlayer::playWord(const QString &spelling, bool uk) {
    if (!m_enabled || spelling.isEmpty()) return;
    const QString suffix = uk ? QStringLiteral("_uk.mp3") : QStringLiteral("_us.mp3");
    const QString local = Paths::audioDir() + QStringLiteral("/") + spelling.toLower() + suffix;
    if (QFileInfo::exists(local)) {
        playFile(local);
        return;
    }
    const QString type = uk ? QStringLiteral("1") : QStringLiteral("2");
    const QString url = QStringLiteral("https://dict.youdao.com/dictvoice?audio=%1&type=%2")
                            .arg(QString::fromUtf8(QUrl::toPercentEncoding(spelling)), type);
    playUrl(url);
}

} // namespace vt
