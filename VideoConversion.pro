QT += core gui widgets
CONFIG += c++17
TARGET = VideoConversion
TEMPLATE = app
VERSION = 1.0.0
DEFINES += QT_DEPRECATED_WARNINGS
win32-g++:QMAKE_CXXFLAGS += -Wall -Wextra -Wpedantic
include(Application/Application.pri)
include(Control/Control.pri)
include(Interface/Interface.pri)
