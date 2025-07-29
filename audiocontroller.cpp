#include "audiocontroller.h"
#include <QDebug>
#include <QHostAddress>

AudioController::AudioController(QObject *parent)
    : QObject(parent)
    , audioInput_(nullptr)
    , audioOutput_(nullptr)
    , inputDevice_(nullptr)
    , outputDevice_(nullptr)
    , audioTimer_(new QTimer(this))
    , server_(nullptr)
    , clientSocket_(nullptr)
    , mode_(ServerMode)
    , isConnected_(false)
    , serverPort_(8080)
    , audioInitialized_(false)
{
    setStatusMessage("Ready");

    // Initialize WebRTC processor
    processor_.setConfig(WebrtcAEC3::SAMPLE_RATE, ConfigValue(48000));
    processor_.setConfig(WebrtcAEC3::ENABLE_AEC, ConfigValue(true));
    processor_.setConfig(WebrtcAEC3::AEC_LEVEL, ConfigValue(2));
    processor_.setConfig(WebrtcAEC3::ENABLE_AGC, ConfigValue(true));
    processor_.setConfig(WebrtcAEC3::SYSTEM_DELAY_MS, ConfigValue(8));
    processor_.setConfig(WebrtcAEC3::ENABLE_HP_FILTER, ConfigValue(true));
    processor_.setConfig(WebrtcAEC3::AEC_DELAY_AGNOSTIC, ConfigValue(false));
    processor_.setConfig(WebrtcAEC3::AEC_EXTENDED_FILTER, ConfigValue(false));
    processor_.setConfig(WebrtcAEC3::ENABLE_VOICE_DETECTION, ConfigValue(false));
    processor_.setConfig(WebrtcAEC3::NOISE_SUPPRESSION_LEVEL, ConfigValue(1));
    processor_.setConfig(WebrtcAEC3::ENABLE_TRANSIENT_SUPPRESSION, ConfigValue(false));

    connect(audioTimer_, &QTimer::timeout, this, &AudioController::processAudio);
}

AudioController::~AudioController() {
    cleanupAudio();
    cleanupNetwork();
}

void AudioController::setServerPort(int port) {
    if (serverPort_ != port) {
        serverPort_ = port;
        emit serverPortChanged();
    }
}

void AudioController::setMode(int mode) {
    if (mode_ != static_cast<Mode>(mode)) {
        mode_ = static_cast<Mode>(mode);
        emit modeChanged();

        // Cleanup existing connections when switching modes
        if (isConnected_) {
            disconnect();
        }
    }
}

void AudioController::startServer() {
    if (mode_ != ServerMode) {
        setStatusMessage("Error: Not in server mode");
        return;
    }

    cleanupNetwork();

    server_ = new QWebSocketServer(QStringLiteral("AudioServer"),
                                   QWebSocketServer::NonSecureMode, this);

    if (server_->listen(QHostAddress::Any, serverPort_)) {
        connect(server_, &QWebSocketServer::newConnection,
                this, &AudioController::onNewConnection);

        setStatusMessage(QString("Server listening on port %1").arg(serverPort_));
        qDebug() << "Server started on port" << serverPort_;
    } else {
        setStatusMessage("Failed to start server");
        qDebug() << "Server failed to start:" << server_->errorString();
        delete server_;
        server_ = nullptr;
    }
}

void AudioController::connectToServer(const QString &serverAddress) {
    if (mode_ != ClientMode) {
        setStatusMessage("Error: Not in client mode");
        return;
    }

    cleanupNetwork();

    clientSocket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(clientSocket_, &QWebSocket::connected,
            this, &AudioController::onWebSocketConnected);
    connect(clientSocket_, &QWebSocket::disconnected,
            this, &AudioController::onWebSocketDisconnected);
    connect(clientSocket_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &AudioController::onWebSocketError);
    connect(clientSocket_, &QWebSocket::binaryMessageReceived,
            this, &AudioController::onBinaryMessageReceived);

    QString url = QString("ws://%1:%2").arg(serverAddress).arg(serverPort_);
    setStatusMessage("Connecting...");
    clientSocket_->open(QUrl(url));
}

void AudioController::disconnect() {
    cleanupAudio();
    cleanupNetwork();

    isConnected_ = false;
    emit connectionStatusChanged();
    setStatusMessage("Disconnected");
}

//void AudioController::onNewConnection() {
//    QWebSocket *socket = server_->nextPendingConnection();

//    connect(socket, &QWebSocket::disconnected, this, [this, socket]() {
//        connectedClients_.removeAll(socket);
//        socket->deleteLater();

//        if (connectedClients_.isEmpty()) {
//            cleanupAudio();
//            isConnected_ = false;
//            emit connectionStatusChanged();
//            setStatusMessage(QString("Server listening on port %1").arg(serverPort_));
//        }
//    });

//    connect(socket, &QWebSocket::binaryMessageReceived,
//            this, &AudioController::onBinaryMessageReceived);

//    connectedClients_.append(socket);

//    if (!isConnected_) {
//        initializeAudio();
//        isConnected_ = true;
//        emit connectionStatusChanged();
//        setStatusMessage("Client connected");
//    }

//    qDebug() << "Client connected from" << socket->peerAddress().toString();
//}

void AudioController::onNewConnection() {
    QWebSocket *socket = server_->nextPendingConnection();

    if (!connectedClients_.isEmpty()) {
        // Already connected: reject new connection
        qDebug() << "Rejected extra client from" << socket->peerAddress().toString();
        socket->close();
        socket->deleteLater();
        return;
    }

    connect(socket, &QWebSocket::disconnected, this, [this, socket]() {
        connectedClients_.removeAll(socket);
        socket->deleteLater();

        if (connectedClients_.isEmpty()) {
            cleanupAudio();
            isConnected_ = false;
            emit connectionStatusChanged();
            setStatusMessage(QString("Server listening on port %1").arg(serverPort_));
        }
    });

    connect(socket, &QWebSocket::binaryMessageReceived,
            this, &AudioController::onBinaryMessageReceived);

    connectedClients_.append(socket);

    if (!isConnected_) {
        initializeAudio();
        isConnected_ = true;
        emit connectionStatusChanged();
        setStatusMessage("Client connected");
    }

    qDebug() << "Client connected from" << socket->peerAddress().toString();
}




void AudioController::onWebSocketConnected() {
    initializeAudio();
    isConnected_ = true;
    emit connectionStatusChanged();
    setStatusMessage("Connected to server");
    qDebug() << "Connected to server";
}

void AudioController::onWebSocketDisconnected() {
    cleanupAudio();
    isConnected_ = false;
    emit connectionStatusChanged();
    setStatusMessage("Disconnected from server");
    qDebug() << "Disconnected from server";
}

void AudioController::onWebSocketError(QAbstractSocket::SocketError error) {
    setStatusMessage("Connection error: " + clientSocket_->errorString());
    qDebug() << "WebSocket error:" << error << clientSocket_->errorString();
}

void AudioController::onBinaryMessageReceived(const QByteArray &message) {
    if (!audioInitialized_ || !outputDevice_) {
        return;
    }

    // Received audio data from remote peer - play it as "far" audio
    if (message.size() == 480 * 2) { // 10ms mono PCM
        // Convert to int16_t vector for far buffer
        std::vector<int16_t> farAudio(480);
        memcpy(farAudio.data(), message.constData(), message.size());

        // Add to far buffer queue for echo cancellation
        farBufferQueue_.enqueue(farAudio);
        if (farBufferQueue_.size() > farDelayFrames_) {
            farBufferQueue_.dequeue();
        }

        // Play the received audio
        outputDevice_->write(message);
    }
}

void AudioController::initializeAudio() {
    if (audioInitialized_) {
        return;
    }

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

    // Start WebRTC processor
    try {
        processor_.start();
        audioTimer_->start(10); // Every 10ms
        audioInitialized_ = true;
        qDebug() << "Audio initialized successfully";
    } catch (const std::exception &e) {
        qWarning() << "Failed to start audio processor:" << e.what();
        cleanupAudio();
    }
}

void AudioController::cleanupAudio() {
    if (!audioInitialized_) {
        return;
    }

    audioTimer_->stop();

    if (audioInput_) {
        audioInput_->stop();
        audioInput_->deleteLater();
        audioInput_ = nullptr;
    }

    if (audioOutput_) {
        audioOutput_->stop();
        audioOutput_->deleteLater();
        audioOutput_ = nullptr;
    }

    inputDevice_ = nullptr;
    outputDevice_ = nullptr;
    farBufferQueue_.clear();
    audioInitialized_ = false;

    qDebug() << "Audio cleaned up";
}

void AudioController::cleanupNetwork() {
    if (server_) {
        server_->close();
        for (QWebSocket *client : connectedClients_) {
            client->close();
        }
        connectedClients_.clear();
        server_->deleteLater();
        server_ = nullptr;
    }

    if (clientSocket_) {
        clientSocket_->close();
        clientSocket_->deleteLater();
        clientSocket_ = nullptr;
    }
}

void AudioController::setStatusMessage(const QString &message) {
    if (statusMessage_ != message) {
        statusMessage_ = message;
        emit statusMessageChanged();
    }
}

void AudioController::processAudio() {
    if (!audioInitialized_ || !inputDevice_ || !isConnected_) {
        return;
    }

    const int frameSize = 480 * 2; // 10ms mono PCM
    if (audioInput_->bytesReady() >= frameSize) {
        QByteArray rawData = inputDevice_->read(frameSize);

        // Convert to int16_t vector
        std::vector<int16_t> near(frameSize / 2);
        memcpy(near.data(), rawData.constData(), frameSize);

        // Get far buffer for echo cancellation
        std::vector<int16_t> far;
        if (farBufferQueue_.size() >= farDelayFrames_) {
            far = farBufferQueue_.dequeue();
        } else {
            far.resize(near.size(), 0); // fallback to zero
        }

        std::vector<int16_t> out;
        try {
            processor_.process(near, far, out);
        } catch (const std::exception &e) {
            qWarning() << "Processing failed:" << e.what();
            return;
        }

        // Send processed audio to remote peer
        QByteArray processedData(reinterpret_cast<const char*>(out.data()),
                               out.size() * sizeof(int16_t));
        sendAudioData(processedData);
    }
}

void AudioController::sendAudioData(const QByteArray &data) {
    if (mode_ == ServerMode) {
        // Send to all connected clients
        for (QWebSocket *client : connectedClients_) {
            if (client->state() == QAbstractSocket::ConnectedState) {
                client->sendBinaryMessage(data);
            }
        }
    } else if (mode_ == ClientMode && clientSocket_) {
        // Send to server
        if (clientSocket_->state() == QAbstractSocket::ConnectedState) {
            clientSocket_->sendBinaryMessage(data);
        }
    }
}
