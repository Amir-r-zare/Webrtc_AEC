QT += core multimedia
QT -= gui
CONFIG += console c++11
CONFIG -= app_bundle

TEMPLATE = app
TARGET = VoiceLoopback

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

DEFINES += WEBRTC_POSIX
# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        *.cpp

HEADERS += \
        *.h

LIBS += $$PWD/libwebrtc_aec.a
LIBS += $$PWD/libgflags_nothreads.a
LIBS += $$PWD/libgflags.a



# Compiler flags
QMAKE_CXXFLAGS += -Wall -Wextra
QMAKE_CXXFLAGS_RELEASE += -O2
QMAKE_CXXFLAGS_DEBUG += -g -O0

# Define for WebRTC compatibility
DEFINES += WEBRTC_POSIX WEBRTC_LINUX


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
