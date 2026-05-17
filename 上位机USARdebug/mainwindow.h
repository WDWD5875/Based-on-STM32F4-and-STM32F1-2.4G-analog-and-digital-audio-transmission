#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSerialPort;
class QSpinBox;
class QTextEdit;
class QTimer;
class WaveformWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void buildInterface();
    void refreshPorts();
    void openOrCloseSerial();
    void readSerialData();
    void processSerialData();
    void sendData();
    void openAudioFile();
    void startStream();
    void pauseStream();
    void stopStream();
    void updateSerialUi(bool opened);
    void appendReceivePreview(const QByteArray &data);
    void trimReceivePreview();
    void parseAckData(const QByteArray &data);
    void handleAck(quint16 status);
    void sendNextAudioBlock();
    void updateStreamUi();
    QString ackStatusText(quint16 status) const;
    QByteArray parseHexText(const QString &text, bool *ok) const;
    QString bytesToHexText(const QByteArray &data) const;

    enum class StreamState {
        Idle,
        WaitReady,
        Paused,
        Finished
    };

    static constexpr int AudioBlockSize = 450;
    static constexpr unsigned char AudioSilence = 0x80;
    static constexpr quint16 AckReady = 0x0001;
    static constexpr quint16 AckBufLow = 0x0002;
    static constexpr quint16 AckBufHigh = 0x0004;
    static constexpr quint16 AckBufEmpty = 0x0008;
    static constexpr quint16 AckBufOverflow = 0x0010;
    static constexpr quint16 AckSpiError = 0x0020;
    static constexpr quint16 AckRunning = 0x0040;

    Ui::MainWindow *ui;
#ifdef HAVE_QT_SERIALPORT
    QSerialPort *m_serial = nullptr;
#endif
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_openButton = nullptr;
    QTextEdit *m_receiveEdit = nullptr;
    QLineEdit *m_sendEdit = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_openAudioButton = nullptr;
    QPushButton *m_startStreamButton = nullptr;
    QPushButton *m_pauseStreamButton = nullptr;
    QPushButton *m_stopStreamButton = nullptr;
    QPushButton *m_clearReceiveButton = nullptr;
    QPushButton *m_printReceiveButton = nullptr;
    QCheckBox *m_hexReceiveCheck = nullptr;
    QCheckBox *m_limitReceiveCheck = nullptr;
    QCheckBox *m_hexSendCheck = nullptr;
    QCheckBox *m_timedSendCheck = nullptr;
    QSpinBox *m_receivePreviewBytesSpin = nullptr;
    QSpinBox *m_receivePreviewIntervalSpin = nullptr;
    QSpinBox *m_sendIntervalSpin = nullptr;
    QLabel *m_audioFileLabel = nullptr;
    QLabel *m_audioFormatLabel = nullptr;
    QLabel *m_streamStatusLabel = nullptr;
    QLabel *m_ackStatusLabel = nullptr;
    QLabel *m_progressLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    WaveformWidget *m_audioWaveform = nullptr;
    QTimer *m_sendTimer = nullptr;
    QTimer *m_rxProcessTimer = nullptr;
    QByteArray m_rxPendingBuffer;
    QByteArray m_pcmData;
    QByteArray m_ackBuffer;
    QString m_audioFilePath;
    StreamState m_streamState = StreamState::Idle;
    qint32 m_currentBaudRate = 0;
    quint64 m_rxBytes = 0;
    quint64 m_txBytes = 0;
    qsizetype m_audioOffset = 0;
    quint16 m_lastAckStatus = 0;
    qint64 m_lastReceivePreviewMs = 0;
    qint64 m_lastStatusUpdateMs = 0;
    qint64 m_lastRxRateMs = 0;
    quint64 m_lastRxRateBytes = 0;
    quint64 m_rxBytesPerSecond = 0;
};

#endif // MAINWINDOW_H
