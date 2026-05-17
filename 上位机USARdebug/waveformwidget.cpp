#include "waveformwidget.h"

#include <QPainter>
#include <QSizePolicy>
#include <algorithm>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void WaveformWidget::setPcmData(const QByteArray *pcmData)
{
    m_pcmData = pcmData;
    update();
}

void WaveformWidget::setCursor(qsizetype cursor)
{
    m_cursor = std::max<qsizetype>(0, cursor);
    update();
}

void WaveformWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect plot = rect().adjusted(10, 10, -10, -18);
    painter.fillRect(rect(), QColor(15, 22, 30));
    if (plot.width() <= 2 || plot.height() <= 2) {
        return;
    }

    painter.setPen(QPen(QColor(48, 61, 74), 1));
    for (int i = 0; i <= 6; ++i) {
        const int x = plot.left() + i * plot.width() / 6;
        painter.drawLine(x, plot.top(), x, plot.bottom());
    }
    for (int i = 0; i <= 4; ++i) {
        const int y = plot.top() + i * plot.height() / 4;
        painter.drawLine(plot.left(), y, plot.right(), y);
    }

    const int centerY = plot.top() + (255 - 128) * plot.height() / 255;
    painter.setPen(QPen(QColor(91, 105, 119), 1, Qt::DashLine));
    painter.drawLine(plot.left(), centerY, plot.right(), centerY);

    painter.setPen(QColor(158, 171, 184));
    painter.drawText(plot.left(), height() - 4, QStringLiteral("3 s / 44100 Hz / unsigned 8-bit PCM / 0..255"));

    if (m_pcmData == nullptr || m_pcmData->isEmpty()) {
        painter.setPen(QColor(115, 127, 139));
        painter.drawText(plot, Qt::AlignCenter, QStringLiteral("No PCM"));
        return;
    }

    const qsizetype total = m_pcmData->size();
    const qsizetype end = std::clamp(m_cursor, qsizetype(0), total);
    const qsizetype start = std::max<qsizetype>(0, end - WindowSamples);
    const qsizetype count = end - start;
    if (count <= 0) {
        return;
    }

    painter.setClipRect(plot);
    painter.setPen(QPen(QColor(33, 190, 255), 1));

    const int columns = std::max(1, plot.width());
    for (int x = 0; x < columns; ++x) {
        const qsizetype s0 = start + count * x / columns;
        const qsizetype s1 = start + count * (x + 1) / columns;
        const qsizetype sampleEnd = std::max<qsizetype>(s0 + 1, s1);

        int minValue = 255;
        int maxValue = 0;
        for (qsizetype i = s0; i < sampleEnd && i < total; ++i) {
            const int value = static_cast<unsigned char>(m_pcmData->at(i));
            minValue = std::min(minValue, value);
            maxValue = std::max(maxValue, value);
        }

        const int yMax = plot.top() + (255 - maxValue) * plot.height() / 255;
        const int yMin = plot.top() + (255 - minValue) * plot.height() / 255;
        painter.drawLine(plot.left() + x, yMax, plot.left() + x, std::max(yMin, yMax + 1));
    }
}
