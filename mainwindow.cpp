#include "mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QObject *parent)
    : QObject(parent)
{
    // Audio format config
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);

    QAudioDeviceInfo inputInfo = QAudioDeviceInfo::defaultInputDevice();
    QAudioDeviceInfo outputInfo = QAudioDeviceInfo::defaultOutputDevice();

    if (!inputInfo.isFormatSupported(format)) {
        qWarning() << "Default format not supported for input. Using nearest.";
        format = inputInfo.nearestFormat(format);
    }

    if (!outputInfo.isFormatSupported(format)) {
        qWarning() << "Default format not supported for output. Using nearest.";
        format = outputInfo.nearestFormat(format);
    }

    audioInput_ = new QAudioInput(inputInfo, format, this);
    audioOutput_ = new QAudioOutput(outputInfo, format, this);

    inputDevice_ = audioInput_->start();
    outputDevice_ = audioOutput_->start();

    // WebRTC AEC3 init
    processor_.setConfig(WebrtcAEC3::SAMPLE_RATE, ConfigValue(48000));
    processor_.setConfig(WebrtcAEC3::ENABLE_AEC, ConfigValue(true));
    processor_.setConfig(WebrtcAEC3::AEC_LEVEL, ConfigValue(2)); // High suppression
    processor_.start();

    // Read audio and process every 10ms = 480 samples (mono)
    connect(&timer_, &QTimer::timeout, this, [=]() {
        const int frameSize = 480 * 2; // 480 samples * 2 bytes
        if (audioInput_->bytesReady() >= frameSize) {
            QByteArray rawData = inputDevice_->read(frameSize);

            // Convert to int16_t vector
            std::vector<int16_t> near(frameSize / 2);
            memcpy(near.data(), rawData.constData(), frameSize);

            // Dummy far vector (no far-end in this loopback test)
            std::vector<int16_t> far(near.size(), 0);

            std::vector<int16_t> out;
            try {
                processor_.process(near, far, out);
            } catch (std::exception &e) {
                qWarning() << "Processing failed:" << e.what();
                return;
            }

            // Write processed data to output
            QByteArray processed(reinterpret_cast<const char*>(out.data()), out.size() * sizeof(int16_t));
            outputDevice_->write(processed);
        }
    });

    timer_.start(10); // Every 10 ms
}

MainWindow::~MainWindow() {
    audioInput_->stop();
    audioOutput_->stop();
}
