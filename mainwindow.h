#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QObject>
#include <QAudioInput>
#include <QAudioOutput>
#include <QIODevice>
#include <QTimer>
#include "WebrtcAEC3.h"

class MainWindow : public QObject {
    Q_OBJECT

public:
    explicit MainWindow(QObject *parent = nullptr);
    ~MainWindow();

private:
    QAudioInput *audioInput_;
    QAudioOutput *audioOutput_;
    QIODevice *inputDevice_;
    QIODevice *outputDevice_;
    QTimer timer_;
    QByteArray inputBuffer_;

    WebrtcAEC3 processor_;
};

#endif // MAINWINDOW_H
