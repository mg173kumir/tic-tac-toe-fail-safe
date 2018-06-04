#-------------------------------------------------
#
# Project created by QtCreator 2017-03-03T18:24:32
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = server2
TEMPLATE = app

INCLUDEPATH += header

SOURCES += ./src/main.cpp\
        ./src/server.cpp \
    ./src/game_sess.cpp

HEADERS  += ./header/server.h \
    ./header/game_sess.h

FORMS    += server.ui
