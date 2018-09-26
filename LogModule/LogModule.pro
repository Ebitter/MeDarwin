#-------------------------------------------------
#
# Project created by QtCreator 2016-06-21T15:26:24
#
#-------------------------------------------------


QT          -= core gui
TARGET      = LogModule
TEMPLATE    = lib
CONFIG    = staticlib
CONFIG 		+= plugin


BASE_PATH =$$PWD/../
DESTDIR=../EasyDarwin/x64

INCLUDEPATH += \
         $$BASE_PATH/3rdparty/poco1.9.0/include

LIBS+= -L$$DESTDIR
unix:{

LIBS += -L$$PWD/../Lib/x64/ -lPocoJSON
LIBS += -L$$PWD/../Lib/x64/ -lPocoXML
LIBS += -L$$PWD/../Lib/x64/ -lPocoUtil
LIBS += -L$$PWD/../Lib/x64/ -lPocoFoundation
}
QMAKE_CXXFLAGS += -std=c++11 -fpermissive -fPIC
message(LogModule-------$$LIBS)

SOURCES += Logger.cpp

HEADERS +=
