#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "waveformwidget.h"

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextOption>
#include <QTimer>
#include <QVBoxLayout>

#ifdef HAVE_QT_SERIALPORT
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
#ifdef HAVE_QT_SERIALPORT
    , m_serial(new QSerialPort(this))
#endif
    , m_sendTimer(new QTimer(this))
    , m_rxProcessTimer(new QTimer(this))
{
    ui->setupUi(this);
    buildInterface();
    refreshPorts();

#ifdef HAVE_QT_SERIALPORT
    connect(m_serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);
#endif
    connect(m_sendTimer, &QTimer::timeout, this, &MainWindow::sendData);
    connect(m_rxProcessTimer, &QTimer::timeout, this, &MainWindow::processSerialData);
    m_rxProcessTimer->setInterval(10);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::buildInterface()
{
    setWindowTitle(tr("USARTdebug - STM32 串口调试"));
    resize(760, 620);

    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);

    auto *serialGroup = new QGroupBox(tr("串口设置"), central);
    auto *serialLayout = new QGridLayout(serialGroup);
    m_portCombo = new QComboBox(serialGroup);
    m_baudCombo = new QComboBox(serialGroup);
    m_baudCombo->setEditable(true);
    m_baudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "230400",
                           "460800", "921600", "1000000", "2000000", "4000000", "8000000"});
    m_baudCombo->setCurrentText("921600");

    m_dataBitsCombo = new QComboBox(serialGroup);
    m_dataBitsCombo->addItems({"8", "7", "6", "5"});
    m_parityCombo = new QComboBox(serialGroup);
    m_parityCombo->addItems({tr("无校验"), tr("偶校验"), tr("奇校验")});
    m_stopBitsCombo = new QComboBox(serialGroup);
    m_stopBitsCombo->addItems({"1", "1.5", "2"});

    m_refreshButton = new QPushButton(tr("刷新"), serialGroup);
    m_openButton = new QPushButton(tr("打开串口"), serialGroup);

    serialLayout->addWidget(new QLabel(tr("串口")), 0, 0);
    serialLayout->addWidget(m_portCombo, 0, 1);
    serialLayout->addWidget(m_refreshButton, 0, 2);
    serialLayout->addWidget(new QLabel(tr("波特率")), 1, 0);
    serialLayout->addWidget(m_baudCombo, 1, 1, 1, 2);
    serialLayout->addWidget(new QLabel(tr("数据位")), 2, 0);
    serialLayout->addWidget(m_dataBitsCombo, 2, 1, 1, 2);
    serialLayout->addWidget(new QLabel(tr("校验位")), 3, 0);
    serialLayout->addWidget(m_parityCombo, 3, 1, 1, 2);
    serialLayout->addWidget(new QLabel(tr("停止位")), 4, 0);
    serialLayout->addWidget(m_stopBitsCombo, 4, 1, 1, 2);
    serialLayout->addWidget(m_openButton, 5, 0, 1, 3);

    auto *receiveGroup = new QGroupBox(tr("接收"), central);
    auto *receiveLayout = new QVBoxLayout(receiveGroup);
    m_receiveEdit = new QTextEdit(receiveGroup);
    m_receiveEdit->setReadOnly(true);
    m_receiveEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_receiveEdit->setWordWrapMode(QTextOption::WrapAnywhere);
    m_receiveEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_receiveEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    auto *receiveLimitLayout = new QHBoxLayout;
    m_limitReceiveCheck = new QCheckBox(tr("限制显示"), receiveGroup);
    m_limitReceiveCheck->setChecked(true);
    m_receivePreviewBytesSpin = new QSpinBox(receiveGroup);
    m_receivePreviewBytesSpin->setRange(1, 4096);
    m_receivePreviewBytesSpin->setValue(256);
    m_receivePreviewBytesSpin->setSuffix(tr(" byte"));
    m_receivePreviewIntervalSpin = new QSpinBox(receiveGroup);
    m_receivePreviewIntervalSpin->setRange(10, 10000);
    m_receivePreviewIntervalSpin->setValue(500);
    m_receivePreviewIntervalSpin->setSuffix(tr(" ms"));
    receiveLimitLayout->addWidget(m_limitReceiveCheck);
    receiveLimitLayout->addWidget(new QLabel(tr("单次")));
    receiveLimitLayout->addWidget(m_receivePreviewBytesSpin);
    receiveLimitLayout->addWidget(new QLabel(tr("间隔")));
    receiveLimitLayout->addWidget(m_receivePreviewIntervalSpin);
    receiveLimitLayout->addStretch();

    auto *receiveToolsLayout = new QHBoxLayout;
    m_hexReceiveCheck = new QCheckBox(tr("HEX显示"), receiveGroup);
    m_printReceiveButton = new QPushButton(tr("关闭打印"), receiveGroup);
    m_printReceiveButton->setCheckable(true);
    m_printReceiveButton->setChecked(true);
    m_clearReceiveButton = new QPushButton(tr("清空接收"), receiveGroup);
    receiveToolsLayout->addWidget(m_hexReceiveCheck);
    receiveToolsLayout->addStretch();
    receiveToolsLayout->addWidget(m_printReceiveButton);
    receiveToolsLayout->addWidget(m_clearReceiveButton);

    receiveLayout->addWidget(m_receiveEdit);
    receiveLayout->addLayout(receiveLimitLayout);
    receiveLayout->addLayout(receiveToolsLayout);

    auto *audioGroup = new QGroupBox(tr("音频流发送"), central);
    auto *audioLayout = new QGridLayout(audioGroup);
    m_openAudioButton = new QPushButton(tr("打开音频"), audioGroup);
    m_startStreamButton = new QPushButton(tr("开始发送"), audioGroup);
    m_pauseStreamButton = new QPushButton(tr("暂停"), audioGroup);
    m_stopStreamButton = new QPushButton(tr("停止"), audioGroup);
    m_audioFileLabel = new QLabel(tr("未选择文件"), audioGroup);
    m_audioFormatLabel = new QLabel(tr("目标格式: 44100 Hz / mono / unsigned 8-bit PCM"), audioGroup);
    m_streamStatusLabel = new QLabel(tr("状态: 空闲"), audioGroup);
    m_ackStatusLabel = new QLabel(tr("ACK: --"), audioGroup);
    m_progressLabel = new QLabel(tr("进度: 0 / 0 B"), audioGroup);
    m_audioFileLabel->setMinimumWidth(0);
    m_audioFileLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_audioFileLabel->setWordWrap(true);
    audioLayout->addWidget(m_openAudioButton, 0, 0);
    audioLayout->addWidget(m_audioFileLabel, 0, 1, 1, 3);
    audioLayout->addWidget(m_audioFormatLabel, 1, 0, 1, 4);
    audioLayout->addWidget(m_startStreamButton, 2, 0);
    audioLayout->addWidget(m_pauseStreamButton, 2, 1);
    audioLayout->addWidget(m_stopStreamButton, 2, 2);
    audioLayout->addWidget(m_streamStatusLabel, 3, 0, 1, 2);
    audioLayout->addWidget(m_ackStatusLabel, 3, 2, 1, 2);
    audioLayout->addWidget(m_progressLabel, 4, 0, 1, 4);

    auto *waveformGroup = new QGroupBox(tr("Audio PCM Scope"), central);
    auto *waveformLayout = new QVBoxLayout(waveformGroup);
    m_audioWaveform = new WaveformWidget(waveformGroup);
    m_audioWaveform->setPcmData(&m_pcmData);
    waveformLayout->addWidget(m_audioWaveform);

    auto *sendGroup = new QGroupBox(tr("发送"), central);
    auto *sendLayout = new QGridLayout(sendGroup);
    m_sendEdit = new QLineEdit(sendGroup);
    m_sendEdit->setPlaceholderText(tr("文本发送，或勾选HEX后输入: 01 03 00 00"));
    m_sendButton = new QPushButton(tr("发送"), sendGroup);
    m_hexSendCheck = new QCheckBox(tr("HEX发送"), sendGroup);
    m_timedSendCheck = new QCheckBox(tr("定时发送"), sendGroup);
    m_sendIntervalSpin = new QSpinBox(sendGroup);
    m_sendIntervalSpin->setRange(10, 60000);
    m_sendIntervalSpin->setValue(1000);
    m_sendIntervalSpin->setSuffix(tr(" ms"));
    sendLayout->addWidget(m_sendEdit, 0, 0, 1, 3);
    sendLayout->addWidget(m_sendButton, 0, 3);
    sendLayout->addWidget(m_hexSendCheck, 1, 0);
    sendLayout->addWidget(m_timedSendCheck, 1, 1);
    sendLayout->addWidget(m_sendIntervalSpin, 1, 2, 1, 2);

    m_statusLabel = new QLabel(tr("未打开 | RX 0 B | TX 0 B"), central);
    m_statusLabel->setMinimumWidth(0);
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_statusLabel->setWordWrap(true);

    mainLayout->addWidget(serialGroup);
    mainLayout->addWidget(receiveGroup, 1);
    mainLayout->addWidget(audioGroup);
    mainLayout->addWidget(waveformGroup);
    mainLayout->addWidget(sendGroup);
    mainLayout->addWidget(m_statusLabel);
    setCentralWidget(central);

    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::openOrCloseSerial);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::sendData);
    connect(m_sendEdit, &QLineEdit::returnPressed, this, &MainWindow::sendData);
    connect(m_openAudioButton, &QPushButton::clicked, this, &MainWindow::openAudioFile);
    connect(m_startStreamButton, &QPushButton::clicked, this, &MainWindow::startStream);
    connect(m_pauseStreamButton, &QPushButton::clicked, this, &MainWindow::pauseStream);
    connect(m_stopStreamButton, &QPushButton::clicked, this, &MainWindow::stopStream);
    connect(m_clearReceiveButton, &QPushButton::clicked, this, [this]() {
        m_lastReceivePreviewMs = 0;
        m_receiveEdit->clear();
    });
    connect(m_printReceiveButton, &QPushButton::toggled, this, [this](bool checked) {
        m_printReceiveButton->setText(checked ? tr("关闭打印") : tr("开启打印"));
    });
    connect(m_timedSendCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            m_sendTimer->start(m_sendIntervalSpin->value());
        } else {
            m_sendTimer->stop();
        }
    });
    connect(m_sendIntervalSpin, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_sendTimer->isActive()) {
            m_sendTimer->start(value);
        }
    });
    updateStreamUi();
}

void MainWindow::refreshPorts()
{
    const QString currentPort = m_portCombo->currentData().toString();
    m_portCombo->clear();

#ifdef HAVE_QT_SERIALPORT
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        const QString text = info.description().isEmpty()
                                 ? info.portName()
                                 : QString("%1 - %2").arg(info.portName(), info.description());
        m_portCombo->addItem(text, info.portName());
    }
#else
    m_portCombo->addItem(tr("未安装 Qt SerialPort 组件"), {});
#endif

    const int index = m_portCombo->findData(currentPort);
    if (index >= 0) {
        m_portCombo->setCurrentIndex(index);
    }
}

void MainWindow::openOrCloseSerial()
{
#ifndef HAVE_QT_SERIALPORT
    QMessageBox::warning(this, tr("缺少组件"), tr("当前 Qt Kit 没有安装 Qt SerialPort，安装后重新配置工程即可启用串口。"));
    return;
#else
    if (m_serial->isOpen()) {
        m_sendTimer->stop();
        m_timedSendCheck->setChecked(false);
        stopStream();
        m_serial->close();
        m_rxProcessTimer->stop();
        updateSerialUi(false);
        return;
    }

    if (m_portCombo->currentData().toString().isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("没有可用串口，请检查连接后刷新。"));
        return;
    }

    bool baudOk = false;
    const qint32 baudRate = m_baudCombo->currentText().toInt(&baudOk);
    if (!baudOk || baudRate <= 0) {
        QMessageBox::warning(this, tr("提示"), tr("波特率无效。"));
        return;
    }

    m_serial->setPortName(m_portCombo->currentData().toString());
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(static_cast<QSerialPort::DataBits>(m_dataBitsCombo->currentText().toInt()));
    m_serial->setParity(m_parityCombo->currentIndex() == 1 ? QSerialPort::EvenParity
                                                           : m_parityCombo->currentIndex() == 2 ? QSerialPort::OddParity
                                                                                                : QSerialPort::NoParity);
    m_serial->setStopBits(m_stopBitsCombo->currentIndex() == 1 ? QSerialPort::OneAndHalfStop
                                                               : m_stopBitsCombo->currentIndex() == 2 ? QSerialPort::TwoStop
                                                                                                      : QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, tr("打开失败"), m_serial->errorString());
        return;
    }

    m_currentBaudRate = baudRate;
    m_lastRxRateMs = 0;
    m_lastRxRateBytes = m_rxBytes;
    m_rxBytesPerSecond = 0;
    m_lastReceivePreviewMs = 0;
    m_ackBuffer.clear();
    m_rxPendingBuffer.clear();
    m_rxProcessTimer->start();
    updateSerialUi(true);
    updateStreamUi();
#endif
}

void MainWindow::readSerialData()
{
#ifndef HAVE_QT_SERIALPORT
    return;
#else
    const QByteArray data = m_serial->readAll();
    if (data.isEmpty()) {
        return;
    }

    m_rxBytes += static_cast<quint64>(data.size());
    parseAckData(data);

    const bool printEnabled = m_printReceiveButton->isChecked();
    const bool streamNeedsAck = m_streamState == StreamState::WaitReady || m_streamState == StreamState::Paused;
    if (!printEnabled && !streamNeedsAck) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastStatusUpdateMs >= 200) {
            m_lastStatusUpdateMs = now;
            updateSerialUi(true);
        }
        return;
    }

    m_rxPendingBuffer.append(data);
    constexpr qsizetype maxPendingBytes = 65536;
    if (m_rxPendingBuffer.size() > maxPendingBytes) {
        m_rxPendingBuffer = m_rxPendingBuffer.right(maxPendingBytes);
    }
#endif
}

void MainWindow::processSerialData()
{
    if (m_rxPendingBuffer.isEmpty()) {
#ifdef HAVE_QT_SERIALPORT
        if (m_serial->isOpen()) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - m_lastStatusUpdateMs >= 200) {
                m_lastStatusUpdateMs = now;
                updateSerialUi(true);
            }
        }
#endif
        return;
    }

    const QByteArray data = m_rxPendingBuffer;
    m_rxPendingBuffer.clear();
    if (m_printReceiveButton->isChecked()) {
        appendReceivePreview(data);
    }
#ifdef HAVE_QT_SERIALPORT
    updateSerialUi(m_serial->isOpen());
#else
    updateSerialUi(false);
#endif
}

void MainWindow::sendData()
{
#ifndef HAVE_QT_SERIALPORT
    return;
#else
    if (!m_serial->isOpen()) {
        return;
    }

    QByteArray data;
    if (m_hexSendCheck->isChecked()) {
        bool ok = false;
        data = parseHexText(m_sendEdit->text(), &ok);
        if (!ok) {
            QMessageBox::warning(this, tr("HEX格式错误"), tr("请使用类似 01 03 00 00 的格式。"));
            return;
        }
    } else {
        data = m_sendEdit->text().toUtf8();
        data.append('\n');
    }

    const qint64 written = m_serial->write(data);
    if (written > 0) {
        m_txBytes += static_cast<quint64>(written);
    }
    updateSerialUi(true);
#endif
}

void MainWindow::openAudioFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择音频文件"),
        QString(),
        tr("Audio Files (*.mp3 *.wav *.flac *.aac *.m4a *.ogg);;All Files (*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        QMessageBox::critical(this, tr("转换失败"), tr("无法创建临时目录。"));
        return;
    }

    const QString rawPath = tempDir.filePath("audio_u8_44100_mono.raw");
    static QString ffmpegProgram = "ffmpeg";
    QProcess ffmpeg;
    const QStringList arguments = {"-y", "-i", filePath, "-ac", "1", "-ar", "44100", "-f", "u8", rawPath};
    ffmpeg.setProgram(ffmpegProgram);
    ffmpeg.setArguments(arguments);
    ffmpeg.start();
    if (!ffmpeg.waitForStarted(3000)) {
        const QString selectedFfmpeg = QFileDialog::getOpenFileName(
            this,
            tr("选择 ffmpeg.exe"),
            QString(),
            tr("ffmpeg.exe (ffmpeg.exe);;Executable (*.exe);;All Files (*)"));
        if (selectedFfmpeg.isEmpty()) {
            QMessageBox::critical(this, tr("转换失败"), tr("无法启动 ffmpeg，请确认 ffmpeg 已加入 PATH，或手动选择 ffmpeg.exe。"));
            return;
        }

        ffmpegProgram = selectedFfmpeg;
        ffmpeg.setProgram(ffmpegProgram);
        ffmpeg.setArguments(arguments);
        ffmpeg.start();
        if (!ffmpeg.waitForStarted(3000)) {
            QMessageBox::critical(this, tr("转换失败"), tr("仍然无法启动所选 ffmpeg.exe。"));
            return;
        }
    }
    if (!ffmpeg.waitForFinished(-1) || ffmpeg.exitStatus() != QProcess::NormalExit || ffmpeg.exitCode() != 0) {
        const QString errorText = QString::fromLocal8Bit(ffmpeg.readAllStandardError());
        QMessageBox::critical(this, tr("转换失败"), errorText.isEmpty() ? tr("ffmpeg 转换失败。") : errorText);
        return;
    }

    QFile rawFile(rawPath);
    if (!rawFile.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("读取失败"), tr("无法读取转换后的 PCM 文件。"));
        return;
    }

    m_pcmData = rawFile.readAll();
    m_audioFilePath = filePath;
    m_audioOffset = 0;
    m_streamState = StreamState::Idle;
    m_audioFileLabel->setText(QFileInfo(filePath).fileName());
    updateStreamUi();
}

void MainWindow::startStream()
{
#ifndef HAVE_QT_SERIALPORT
    return;
#else
    if (!m_serial->isOpen()) {
        QMessageBox::warning(this, tr("提示"), tr("请先打开串口。"));
        return;
    }
    if (m_pcmData.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("请先打开音频文件。"));
        return;
    }

    if (m_streamState == StreamState::Finished || m_audioOffset >= m_pcmData.size()) {
        m_audioOffset = 0;
    }

    const bool hadReadyToken = (m_lastAckStatus & AckReady) != 0;
    m_ackBuffer.clear();
    m_rxPendingBuffer.clear();
    m_serial->clear(QSerialPort::Input);
    m_lastAckStatus = 0;

    m_streamState = StreamState::WaitReady;
    updateStreamUi();
    if (hadReadyToken) {
        sendNextAudioBlock();
    }
#endif
}

void MainWindow::pauseStream()
{
    if (m_streamState == StreamState::WaitReady) {
        m_streamState = StreamState::Paused;
    } else if (m_streamState == StreamState::Paused) {
        m_streamState = StreamState::WaitReady;
        if ((m_lastAckStatus & AckReady) != 0) {
            sendNextAudioBlock();
        }
    }
    updateStreamUi();
}

void MainWindow::stopStream()
{
    m_streamState = StreamState::Idle;
    m_audioOffset = 0;
    updateStreamUi();
}

void MainWindow::updateSerialUi(bool opened)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastRxRateMs == 0) {
        m_lastRxRateMs = now;
        m_lastRxRateBytes = m_rxBytes;
        m_rxBytesPerSecond = 0;
    } else if (now - m_lastRxRateMs >= 1000) {
        const qint64 elapsedMs = now - m_lastRxRateMs;
        const quint64 deltaBytes = m_rxBytes - m_lastRxRateBytes;
        m_rxBytesPerSecond = static_cast<quint64>((deltaBytes * 1000ULL) / static_cast<quint64>(elapsedMs));
        m_lastRxRateMs = now;
        m_lastRxRateBytes = m_rxBytes;
    }

    m_openButton->setText(opened ? tr("关闭串口") : tr("打开串口"));
    m_portCombo->setEnabled(!opened);
    m_baudCombo->setEnabled(!opened);
    m_dataBitsCombo->setEnabled(!opened);
    m_parityCombo->setEnabled(!opened);
    m_stopBitsCombo->setEnabled(!opened);
    m_refreshButton->setEnabled(!opened);
#ifdef HAVE_QT_SERIALPORT
    m_statusLabel->setText(QString("%1 | RX %2 B | RX速率 %3 B/s | TX %4 B")
                               .arg(opened ? tr("已打开 %1 @ %2").arg(m_serial->portName()).arg(m_currentBaudRate)
                                           : tr("未打开"))
                               .arg(m_rxBytes)
                               .arg(m_rxBytesPerSecond)
                               .arg(m_txBytes));
#else
    m_statusLabel->setText(tr("未安装 Qt SerialPort"));
#endif
}

void MainWindow::parseAckData(const QByteArray &data)
{
    m_ackBuffer.append(data);
    constexpr qsizetype maxAckBufferBytes = 1024;
    if (m_ackBuffer.size() > maxAckBufferBytes) {
        m_ackBuffer = m_ackBuffer.right(maxAckBufferBytes);
    }

    while (m_ackBuffer.size() >= 4) {
        int headerIndex = -1;
        for (int i = 0; i <= m_ackBuffer.size() - 2; ++i) {
            if (static_cast<unsigned char>(m_ackBuffer.at(i)) == 0xAC
                && static_cast<unsigned char>(m_ackBuffer.at(i + 1)) == 0xCA) {
                headerIndex = i;
                break;
            }
        }

        if (headerIndex < 0) {
            m_ackBuffer = (static_cast<unsigned char>(m_ackBuffer.back()) == 0xAC)
                              ? QByteArray(1, m_ackBuffer.back())
                              : QByteArray();
            return;
        }

        if (headerIndex > 0) {
            m_ackBuffer.remove(0, headerIndex);
        }
        if (m_ackBuffer.size() < 4) {
            return;
        }

        const quint16 status = static_cast<unsigned char>(m_ackBuffer.at(2))
                               | (static_cast<quint16>(static_cast<unsigned char>(m_ackBuffer.at(3))) << 8);
        m_ackBuffer.remove(0, 4);
        handleAck(status);
    }
}

void MainWindow::handleAck(quint16 status)
{
    m_lastAckStatus = status;
    updateStreamUi();

    if (m_streamState == StreamState::WaitReady && (status & AckReady) != 0) {
        sendNextAudioBlock();
    }
}

void MainWindow::sendNextAudioBlock()
{
#ifndef HAVE_QT_SERIALPORT
    return;
#else
    if (!m_serial->isOpen() || m_streamState != StreamState::WaitReady || m_pcmData.isEmpty()) {
        return;
    }

    QByteArray block(AudioBlockSize, static_cast<char>(AudioSilence));
    const qsizetype remaining = m_pcmData.size() - m_audioOffset;
    const qsizetype copySize = qMin<qsizetype>(AudioBlockSize, qMax<qsizetype>(0, remaining));
    if (copySize > 0) {
        memcpy(block.data(), m_pcmData.constData() + m_audioOffset, static_cast<size_t>(copySize));
    }

    const qint64 written = m_serial->write(block);
    if (written != block.size()) {
        QMessageBox::warning(this, tr("发送异常"), tr("音频块写入串口不完整。"));
        m_streamState = StreamState::Paused;
        updateStreamUi();
        return;
    }

    m_txBytes += static_cast<quint64>(written);
    m_audioOffset += copySize;
    if (copySize < AudioBlockSize || m_audioOffset >= m_pcmData.size()) {
        m_streamState = StreamState::Finished;
    }

    updateSerialUi(true);
    updateStreamUi();
#endif
}

void MainWindow::updateStreamUi()
{
    QString stateText;
    switch (m_streamState) {
    case StreamState::Idle:
        stateText = tr("空闲");
        break;
    case StreamState::WaitReady:
        stateText = tr("等待 READY");
        break;
    case StreamState::Paused:
        stateText = tr("暂停");
        break;
    case StreamState::Finished:
        stateText = tr("完成");
        break;
    }

    m_streamStatusLabel->setText(tr("状态: %1").arg(stateText));
    m_ackStatusLabel->setText(tr("ACK: %1").arg(ackStatusText(m_lastAckStatus)));
    m_progressLabel->setText(tr("进度: %1 / %2 B").arg(m_audioOffset).arg(m_pcmData.size()));
    m_pauseStreamButton->setText(m_streamState == StreamState::Paused ? tr("继续") : tr("暂停"));
    m_startStreamButton->setEnabled(!m_pcmData.isEmpty());
    m_pauseStreamButton->setEnabled(m_streamState == StreamState::WaitReady || m_streamState == StreamState::Paused);
    m_stopStreamButton->setEnabled(m_streamState != StreamState::Idle);

    if (m_audioWaveform != nullptr) {
        const qsizetype previewCursor = (m_audioOffset == 0 && !m_pcmData.isEmpty())
                                            ? qMin<qsizetype>(m_pcmData.size(), 44100 * 3)
                                            : m_audioOffset;
        m_audioWaveform->setCursor(previewCursor);
    }
}

QString MainWindow::ackStatusText(quint16 status) const
{
    if (status == 0) {
        return "--";
    }

    QStringList flags;
    if ((status & AckReady) != 0) {
        flags << "READY";
    }
    if ((status & AckBufLow) != 0) {
        flags << "BUF_LOW";
    }
    if ((status & AckBufHigh) != 0) {
        flags << "BUF_HIGH";
    }
    if ((status & AckBufEmpty) != 0) {
        flags << "BUF_EMPTY";
    }
    if ((status & AckBufOverflow) != 0) {
        flags << "BUF_OVERFLOW";
    }
    if ((status & AckSpiError) != 0) {
        flags << "SPI_ERROR";
    }
    if ((status & AckRunning) != 0) {
        flags << "RUNNING";
    }
    const quint16 known = AckReady | AckBufLow | AckBufHigh | AckBufEmpty
                          | AckBufOverflow | AckSpiError | AckRunning;
    if ((status & ~known) != 0) {
        flags << QString("0x%1").arg(status & ~known, 4, 16, QLatin1Char('0')).toUpper();
    }
    return flags.isEmpty() ? QString("0x%1").arg(status, 4, 16, QLatin1Char('0')).toUpper()
                           : flags.join(" | ");
}

void MainWindow::appendReceivePreview(const QByteArray &data)
{
    if (!m_printReceiveButton->isChecked()) {
        return;
    }

    QByteArray previewData = data;
    if (m_limitReceiveCheck->isChecked()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const int intervalMs = m_receivePreviewIntervalSpin->value();
        if (m_lastReceivePreviewMs != 0 && now - m_lastReceivePreviewMs < intervalMs) {
            return;
        }

        m_lastReceivePreviewMs = now;
        previewData = data.left(m_receivePreviewBytesSpin->value());
    }

    m_receiveEdit->moveCursor(QTextCursor::End);
    if (m_hexReceiveCheck->isChecked()) {
        m_receiveEdit->insertPlainText(bytesToHexText(previewData) + " ");
    } else {
        m_receiveEdit->insertPlainText(QString::fromUtf8(previewData));
    }
    m_receiveEdit->moveCursor(QTextCursor::End);
    trimReceivePreview();
}

void MainWindow::trimReceivePreview()
{
    constexpr int maxChars = 200000;
    QTextDocument *doc = m_receiveEdit->document();
    const int extraChars = doc->characterCount() - maxChars;
    if (extraChars <= 0) {
        return;
    }

    QTextCursor cursor(doc);
    cursor.setPosition(0);
    cursor.setPosition(extraChars, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    m_receiveEdit->moveCursor(QTextCursor::End);
}

QByteArray MainWindow::parseHexText(const QString &text, bool *ok) const
{
    QString compact = text;
    compact.remove(' ');
    compact.remove('\t');
    compact.remove('\r');
    compact.remove('\n');

    if (compact.size() % 2 != 0) {
        *ok = false;
        return {};
    }

    const QByteArray bytes = QByteArray::fromHex(compact.toLatin1());
    *ok = bytes.size() * 2 == compact.size();
    return bytes;
}

QString MainWindow::bytesToHexText(const QByteArray &data) const
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}
