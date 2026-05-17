#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include <QByteArray>
#include <QWidget>

class WaveformWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget *parent = nullptr);

    void setPcmData(const QByteArray *pcmData);
    void setCursor(qsizetype cursor);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr int SampleRate = 44100;
    static constexpr int WindowSeconds = 3;
    static constexpr int WindowSamples = SampleRate * WindowSeconds;

    const QByteArray *m_pcmData = nullptr;
    qsizetype m_cursor = 0;
};

#endif // WAVEFORMWIDGET_H
