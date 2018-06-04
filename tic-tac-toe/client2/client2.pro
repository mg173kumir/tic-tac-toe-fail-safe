#-------------------------------------------------
#
# Project created by QtCreator 2017-03-02T17:53:31
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = client2
TEMPLATE = app

INCLUDEPATH += header

SOURCES += ./src/main.cpp\
        ./src/game.cpp

HEADERS  += ./header/game.h

FORMS    += game.ui
