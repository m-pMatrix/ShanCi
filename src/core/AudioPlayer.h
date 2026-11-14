#pragma once
//
// AudioPlayer.h - 音频播放（第七层：辅助功能）
//   优先使用 QtMultimedia；未安装该模块时回退到 Windows MCI。
//
#include <QObject>
#include <QString>

#ifdef VOCAB_HAS_MULTIMEDIA
QT_BEGIN_NAMESPACE
class QMediaPlayer;
class QAudioOutput;
QT_END_NAMESPACE
#endif

namespace vt {

class AudioPlayer : public QObject {
    Q_OBJECT
public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void playFile(const QString &filePath);
    void playUrl(const QString &url);
    void playWord(const QString &spelling, bool uk = true);
    void stop();

private:
    bool m_enabled = true;
#ifdef VOCAB_HAS_MULTIMEDIA
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_output = nullptr;
#else
    bool m_mciOpen = false;
#endif
};

} // namespace vt
