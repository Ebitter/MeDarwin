#-------------------------------------------------
#
# Project created by QtCreator 2016-06-21T15:26:24
#
#-------------------------------------------------


QT          -= core gui
TARGET      = LogModule
TEMPLATE    = lib
CONFIG 		+= plugin


BASE_PATH =$$PWD/../


win32 {

DESTDIR=../EasyDarwin/WinNTSupport/Debug
INCLUDEPATH += \
         $$BASE_PATH/3rdparty/poco1.7.8/include
#    QMAKE_CFLAGS_DEBUG += -MTd
#    QMAKE_CXXFLAGS_DEBUG += -MTd


    LIBS += -L$$PWD/../EasyDarwin/Lib/ -lPocoJSONd
    LIBS += -L$$PWD/../EasyDarwin/Lib/ -lPocoXMLd
    LIBS += -L$$PWD/../EasyDarwin/Lib/ -lPocoUtild
    LIBS += -L$$PWD/../EasyDarwin/Lib/ -lPocoFoundationd
}
unix {
    CONFIG    = staticlib
    DESTDIR=../EasyDarwin/x64
    INCLUDEPATH += \
             $$BASE_PATH/3rdparty/poco1.9.0/include
}

unix:{
    LIBS+= -L$$DESTDIR
    LIBS += -L$$PWD/../Lib/x64/ -lPocoJSON
    LIBS += -L$$PWD/../Lib/x64/ -lPocoXML
    LIBS += -L$$PWD/../Lib/x64/ -lPocoUtil
    LIBS += -L$$PWD/../Lib/x64/ -lPocoFoundation
}
QMAKE_CXXFLAGS += -std=c++11 -fpermissive -fPIC
message(LogModule-------$$LIBS)

SOURCES += Logger.cpp

HEADERS +=
