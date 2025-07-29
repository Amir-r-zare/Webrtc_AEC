#ifndef AUDIOCONTROLLER_H
#define AUDIOCONTROLLER_H

#include <QObject>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QQueue>
#include "WebrtcAEC3.h"

class AudioController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStatusChanged)
    Q_PROPERTY(bool isServer READ isServer NOTIFY modeChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int serverPort READ serverPort WRITE setServerPort NOTIFY serverPortChanged)
    Q_PROPERTY(bool enableAEC READ enableAEC WRITE setEnableAEC NOTIFY enableAECChanged)


public:
    enum Mode {
        ServerMode,
        ClientMode
    };

    explicit AudioController(QObject *parent = nullptr);
    ~AudioController();

    // Properties
    bool isConnected() const { return isConnected_; }
    bool isServer() const { return mode_ == ServerMode; }
    QString statusMessage() const { return statusMessage_; }
    int serverPort() const { return serverPort_; }
    void setServerPort(int port);

    void setEnableAEC(bool value)
    {
        if (enableAEC_ != value) {
            enableAEC_ = value;
            processor_.enable_aec_ = value;
            emit enableAECChanged();
        }
    }

    bool enableAEC(){
        return enableAEC_;
    }

public slots:
    void startServer();
    void connectToServer(const QString &serverAddress);
    void disconnect();
    void setMode(int mode); // 0 = Server, 1 = Client

signals:
    void connectionStatusChanged();
    void modeChanged();
    void statusMessageChanged();
    void serverPortChanged();
    void enableAECChanged();

private slots:
    void onNewConnection();
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onBinaryMessageReceived(const QByteArray &message);
    void processAudio();

private:
    void initializeAudio();
    void cleanupAudio();
    void cleanupNetwork();
    void setStatusMessage(const QString &message);
    void sendAudioData(const QByteArray &data);

    // Audio components
    QAudioInput *audioInput_;
    QAudioOutput *audioOutput_;
    QIODevice *inputDevice_;
    QIODevice *outputDevice_;
    QTimer *audioTimer_;
    WebrtcAEC3 processor_;
    QQueue<std::vector<int16_t>> farBufferQueue_;
    const int farDelayFrames_ = 3; // 3*10ms = 30ms delay for echo

    // Network components
    QWebSocketServer *server_;
    QWebSocket *clientSocket_;
    QList<QWebSocket *> connectedClients_;

    // State
    Mode mode_;
    bool isConnected_;
    QString statusMessage_;
    int serverPort_;
    bool audioInitialized_;

    bool enableAEC_;
};

#endif // AUDIOCONTROLLER_H
