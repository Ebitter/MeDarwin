######################################################################
# Automatically generated by qmake (3.0) ?? 9? 21 10:10:28 2018
######################################################################
BASE_PATH =$$PWD/../

QT -= core gui
TEMPLATE = app
TARGET = streamserver
INCLUDEPATH += .
INCLUDEPATH += \
         $$BASE_PATH/Include \
         $$BASE_PATH/EasyDarwin/Include \
         $$BASE_PATH/EasyDarwin/APIStubLib \
         $$BASE_PATH/EasyDarwin/APICommonCode \
         $$BASE_PATH/EasyDarwin/APIModules \
         $$BASE_PATH/EasyDarwin/APIModules/EasyHLSModule \
         $$BASE_PATH/EasyDarwin/APIModules/QTSSFileModule \
         $$BASE_PATH/EasyDarwin/APIModules/EasyCMSModule \
         $$BASE_PATH/EasyDarwin/APIModules/EasyRelayModule \
         $$BASE_PATH/EasyDarwin/APIModules/EasyRedisModule \
         $$BASE_PATH/EasyDarwin/APIModules/QTSSAdminModule \
         $$BASE_PATH/EasyDarwin/APIModules/QTSSAccessModule \
         $$BASE_PATH/EasyDarwin/APIModules/QTSSReflectorModule \
         $$BASE_PATH/EasyDarwin/APIModules/QTSSAccessLogModule \
         $$BASE_PATH/EasyDarwin/APIModules/QTSSFlowControlModule \
         $$BASE_PATH/EasyDarwin/APIModules/QTSSPOSIXFileSysModule \
         $$BASE_PATH/EasyDarwin/APIModules/QTSSMP3StreamingModule \
         $$BASE_PATH/EasyDarwin/APIStubLib \
         $$BASE_PATH/EasyDarwin/RTPMetaInfoLib \
         $$BASE_PATH/EasyDarwin/RTCPUtilitiesLib \
         $$BASE_PATH/EasyDarwin/QTFileLib \
         $$BASE_PATH/EasyDarwin/PrefsSourceLib \
         $$BASE_PATH/EasyDarwin/Server.tproj \
         $$BASE_PATH/EasyDarwin/RTSPClientLib \
         $$BASE_PATH/EasyRedisClient \
         $$BASE_PATH/CommonUtilitiesLib \
         $$BASE_PATH/HTTPUtilitiesLib \
         $$BASE_PATH/EasyProtocol/Include \
         $$BASE_PATH/EasyProtocol/jsoncpp/include \
         $$BASE_PATH/EasyProtocol

DESTDIR=./x64

DEFINES += __PTHREADS_MUTEXES__
DEFINES += __PTHREADS__
DEFINES += ASSERT
DEFINES += COMMON_UTILITIES_LIB
DEFINES += DSS_USE_API_CALLBACKS
DEFINES += _REENTRANT
DEFINES += __USE_POSIX
DEFINES += __linux__
unix:{
LIBS += -L$$PWD/../CommonUtilitiesLib/x64 -lCommonUtilitiesLib
LIBS += -L$$PWD/../EasyProtocol/EasyProtocol/x64/ -lEasyProtocol
LIBS += -L$$PWD/../EasyProtocol/jsoncpp/x64/ -ljsoncpp
LIBS += -L$$PWD/../EasyRedisClient/x64/ -leasyredisclient
LIBS += -L$$PWD/../EasyDarwin/x64/ -lLogModule
LIBS += -L$$PWD/./Lib/x64/ -leasyrtspclient
LIBS += -L$$PWD/./Lib/x64/ -leasyhls
LIBS += -L$$PWD/./Lib/x64/ -lEasyAACEncoder
LIBS += -L$$PWD/./Lib/x64/ -leasypusher
LIBS += -ldl
LIBS += -lcrypt


LIBS += -L$$PWD/../Lib/x64/ -lPocoJSON
LIBS += -L$$PWD/../Lib/x64/ -lPocoXML
LIBS += -L$$PWD/../Lib/x64/ -lPocoUtil
LIBS += -L$$PWD/../Lib/x64/ -lPocoFoundation
}

# Input
HEADERS += defaultPaths.h \
           APICommonCode/QTAccessFile.h \
           APICommonCode/QTSS3GPPModuleUtils.h \
           APICommonCode/QTSSMemoryDeleter.h \
           APICommonCode/QTSSModuleUtils.h \
           APICommonCode/QTSSRollingLog.h \
           APICommonCode/SDPSourceInfo.h \
           APICommonCode/SourceInfo.h \
           APIStubLib/QTSS.h \
           APIStubLib/QTSS_Private.h \
           APIStubLib/QTSSRTSPProtocol.h \
           Include/EasyAACEncoderAPI.h \
           Include/EasyHLSAPI.h \
           Include/EasyPusherAPI.h \
           Include/EasyRTSPClientAPI.h \
           Include/EasyTypes.h \
           Include/tinystr.h \
           Include/tinyxml.h \
           PrefsSourceLib/FilePrefsSource.h \
           PrefsSourceLib/NetInfoPrefsSource.h \
           PrefsSourceLib/nilib2.h \
           PrefsSourceLib/PrefsSource.h \
           PrefsSourceLib/XMLParser.h \
           PrefsSourceLib/XMLPrefsParser.h \
           QTFileLib/QTAtom.h \
           QTFileLib/QTAtom_dref.h \
           QTFileLib/QTAtom_elst.h \
           QTFileLib/QTAtom_hinf.h \
           QTFileLib/QTAtom_mdhd.h \
           QTFileLib/QTAtom_mvhd.h \
           QTFileLib/QTAtom_stco.h \
           QTFileLib/QTAtom_stsc.h \
           QTFileLib/QTAtom_stsd.h \
           QTFileLib/QTAtom_stss.h \
           QTFileLib/QTAtom_stsz.h \
           QTFileLib/QTAtom_stts.h \
           QTFileLib/QTAtom_tkhd.h \
           QTFileLib/QTAtom_tref.h \
           QTFileLib/QTFile.h \
           QTFileLib/QTFile_FileControlBlock.h \
           QTFileLib/QTHintTrack.h \
           QTFileLib/QTRTPFile.h \
           QTFileLib/QTTrack.h \
           RTCPUtilitiesLib/RTCPAckPacket.h \
           RTCPUtilitiesLib/RTCPAckPacketFmt.h \
           RTCPUtilitiesLib/RTCPAPPNADUPacket.h \
           RTCPUtilitiesLib/RTCPAPPPacket.h \
           RTCPUtilitiesLib/RTCPAPPQTSSPacket.h \
           RTCPUtilitiesLib/RTCPNADUPacketFmt.h \
           RTCPUtilitiesLib/RTCPPacket.h \
           RTCPUtilitiesLib/RTCPRRPacket.h \
           RTCPUtilitiesLib/RTCPSRPacket.h \
           RTPMetaInfoLib/RTPMetaInfoPacket.h \
           RTSPClientLib/ClientSession.h \
           RTSPClientLib/PlayerSimulator.h \
           RTSPClientLib/RTPPacket.h \
           RTSPClientLib/RTSPClient.h \
           RTSPClientLib/RTSPRelaySession.h \
           Server.tproj/GenerateXMLPrefs.h \
           Server.tproj/HTTPSession.h \
           Server.tproj/HTTPSessionInterface.h \
           Server.tproj/QTSSCallbacks.h \
           Server.tproj/QTSSDataConverter.h \
           Server.tproj/QTSSDictionary.h \
           Server.tproj/QTSSErrorLogModule.h \
           Server.tproj/QTSServer.h \
           Server.tproj/QTSServerInterface.h \
           Server.tproj/QTSServerPrefs.h \
           Server.tproj/QTSSExpirationDate.h \
           Server.tproj/QTSSFile.h \
           Server.tproj/QTSSMessages.h \
           Server.tproj/QTSSModule.h \
           Server.tproj/QTSSPrefs.h \
           Server.tproj/QTSSSocket.h \
           Server.tproj/QTSSStream.h \
           Server.tproj/QTSSUserProfile.h \
           Server.tproj/RTCPTask.h \
           Server.tproj/RTPBandwidthTracker.h \
           Server.tproj/RTPOverbufferWindow.h \
           Server.tproj/RTPPacketResender.h \
           Server.tproj/RTPSession.h \
           Server.tproj/RTPSession3GPP.h \
           Server.tproj/RTPSessionInterface.h \
           Server.tproj/RTPStream.h \
           Server.tproj/RTPStream3gpp.h \
           Server.tproj/RTSPProtocol.h \
           Server.tproj/RTSPRequest.h \
           Server.tproj/RTSPRequest3GPP.h \
           Server.tproj/RTSPRequestInterface.h \
           Server.tproj/RTSPRequestStream.h \
           Server.tproj/RTSPResponseStream.h \
           Server.tproj/RTSPSession.h \
           Server.tproj/RTSPSession3GPP.h \
           Server.tproj/RTSPSessionInterface.h \
           Server.tproj/RunServer.h \
           APIModules/EasyCMSModule/EasyCMSModule.h \
           APIModules/EasyCMSModule/EasyCMSSession.h \
           APIModules/EasyHLSModule/EasyHLSModule.h \
           APIModules/EasyHLSModule/EasyHLSSession.h \
           APIModules/EasyRedisModule/EasyRedisModule.h \
           APIModules/EasyRelayModule/EasyRelayModule.h \
           APIModules/EasyRelayModule/EasyRelaySession.h \
           APIModules/QTSSAccessLogModule/QTSSAccessLogModule.h \
           APIModules/QTSSAccessModule/AccessChecker.h \
           APIModules/QTSSAccessModule/QTSSAccessModule.h \
           APIModules/QTSSAdminModule/AdminElementNode.h \
           APIModules/QTSSAdminModule/AdminQuery.h \
           APIModules/QTSSAdminModule/frozen.h \
           APIModules/QTSSAdminModule/mongoose.h \
           APIModules/QTSSAdminModule/QTSSAdminModule.h \
           APIModules/QTSSDemoAuthorizationModule.bproj/QTSSDemoAuthorizationModule.h \
           APIModules/QTSSDemoRedirectModule.bproj/QTSSDemoRedirectModule.h \
           APIModules/QTSSDemoSMILModule.bproj/QTSSDemoSMILModule.h \
           APIModules/QTSSDSAuthModule/DSAccessChecker.h \
           APIModules/QTSSDSAuthModule/QTSSDSAuthModule.h \
           APIModules/QTSSFileModule/QTSSFileModule.h \
           APIModules/QTSSFilePrivsModule.bproj/QTSSFilePrivsModule.h \
           APIModules/QTSSFlowControlModule/QTSSFlowControlModule.h \
           APIModules/QTSSHomeDirectoryModule/DirectoryInfo.h \
           APIModules/QTSSHomeDirectoryModule/QTSSHomeDirectoryModule.h \
           APIModules/QTSSHttpFileModule/QTSSHttpFileModule.h \
           APIModules/QTSSMP3StreamingModule/QTSSMP3StreamingModule.h \
           APIModules/QTSSPOSIXFileSysModule/QTSSPosixFileSysModule.h \
           APIModules/QTSSProxyModule/QTSSProxyModule.h \
           APIModules/QTSSRawFileModule.bproj/QTSSRawFileModule.h \
           APIModules/QTSSReflectorModule/QTSSReflectorModule.h \
           APIModules/QTSSReflectorModule/QTSSRelayModule.h \
           APIModules/QTSSReflectorModule/QTSSSplitterModule.h \
           APIModules/QTSSReflectorModule/RCFSourceInfo.h \
           APIModules/QTSSReflectorModule/ReflectorOutput.h \
           APIModules/QTSSReflectorModule/ReflectorSession.h \
           APIModules/QTSSReflectorModule/ReflectorStream.h \
           APIModules/QTSSReflectorModule/RelayOutput.h \
           APIModules/QTSSReflectorModule/RelaySDPSourceInfo.h \
           APIModules/QTSSReflectorModule/RelaySession.h \
           APIModules/QTSSReflectorModule/RTPSessionOutput.h \
           APIModules/QTSSReflectorModule/RTSPSourceInfo.h \
           APIModules/QTSSReflectorModule/SequenceNumberMap.h \
           APIModules/QTSSRefMovieModule/QTSSRefMovieModule.h \
           APIModules/QTSSRTPFileModule/QTSSRTPFileModule.h \
           APIModules/QTSSRTPFileModule/RTPFileSession.h \
           APIModules/QTSSSpamDefenseModule.bproj/QTSSSpamDefenseModule.h \
           APIModules/QTSSWebDebugModule/QTSSWebDebugModule.h \
           APIModules/QTSSWebStatsModule/QTSSWebStatsModule.h \
           APIModules/QTSSDSAuthModule/DSWrappers/CDirService.h \
           APIModules/QTSSDSAuthModule/DSWrappers/DSBuffer.h \
           APIModules/QTSSDSAuthModule/DSWrappers/DSDataList.h \
           APIModules/QTSSDSAuthModule/DSWrappers/DSDataNode.h \
            ../Include/PlatformHeader.h
SOURCES += APICommonCode/QTAccessFile.cpp \
           APICommonCode/QTSS3GPPModuleUtils.cpp \
           APICommonCode/QTSSModuleUtils.cpp \
           APICommonCode/QTSSRollingLog.cpp \
           APICommonCode/SDPSourceInfo.cpp \
           APICommonCode/SourceInfo.cpp \
           APIStubLib/QTSS_Private.cpp \
           PrefsSourceLib/FilePrefsSource.cpp \
           #PrefsSourceLib/NetInfoPrefsSource.cpp \
          # PrefsSourceLib/nilib2.c \
           PrefsSourceLib/XMLParser.cpp \
           PrefsSourceLib/XMLPrefsParser.cpp \
           QTFileLib/QTAtom.cpp \
           QTFileLib/QTAtom_dref.cpp \
           QTFileLib/QTAtom_elst.cpp \
           QTFileLib/QTAtom_hinf.cpp \
           QTFileLib/QTAtom_mdhd.cpp \
           QTFileLib/QTAtom_mvhd.cpp \
           QTFileLib/QTAtom_stco.cpp \
           QTFileLib/QTAtom_stsc.cpp \
           QTFileLib/QTAtom_stsd.cpp \
           QTFileLib/QTAtom_stss.cpp \
           QTFileLib/QTAtom_stsz.cpp \
           QTFileLib/QTAtom_stts.cpp \
           QTFileLib/QTAtom_tkhd.cpp \
           QTFileLib/QTAtom_tref.cpp \
           QTFileLib/QTFile.cpp \
           QTFileLib/QTFile_FileControlBlock.cpp \
           QTFileLib/QTHintTrack.cpp \
           QTFileLib/QTRTPFile.cpp \
           QTFileLib/QTTrack.cpp \
           RTCPUtilitiesLib/RTCPAckPacket.cpp \
           RTCPUtilitiesLib/RTCPAPPNADUPacket.cpp \
           RTCPUtilitiesLib/RTCPAPPPacket.cpp \
           RTCPUtilitiesLib/RTCPAPPQTSSPacket.cpp \
           RTCPUtilitiesLib/RTCPPacket.cpp \
           RTCPUtilitiesLib/RTCPSRPacket.cpp \
           RTPMetaInfoLib/RTPMetaInfoPacket.cpp \
           #RTSPClientLib/ClientSession.cpp \
           RTSPClientLib/RTSPClient.cpp \
           Server.tproj/GenerateXMLPrefs.cpp \
           Server.tproj/HTTPSession.cpp \
           Server.tproj/HTTPSessionInterface.cpp \
           Server.tproj/main.cpp \
           Server.tproj/QTSSCallbacks.cpp \
           Server.tproj/QTSSDataConverter.cpp \
           Server.tproj/QTSSDictionary.cpp \
           Server.tproj/QTSSErrorLogModule.cpp \
           Server.tproj/QTSServer.cpp \
           Server.tproj/QTSServerInterface.cpp \
           Server.tproj/QTSServerPrefs.cpp \
           Server.tproj/QTSSExpirationDate.cpp \
           Server.tproj/QTSSFile.cpp \
           Server.tproj/QTSSMessages.cpp \
           Server.tproj/QTSSModule.cpp \
           Server.tproj/QTSSPrefs.cpp \
           Server.tproj/QTSSSocket.cpp \
           Server.tproj/QTSSUserProfile.cpp \
           Server.tproj/RTCPTask.cpp \
           Server.tproj/RTPBandwidthTracker.cpp \
           Server.tproj/RTPOverbufferWindow.cpp \
           Server.tproj/RTPPacketResender.cpp \
           Server.tproj/RTPSession.cpp \
           Server.tproj/RTPSession3GPP.cpp \
           Server.tproj/RTPSessionInterface.cpp \
           Server.tproj/RTPStream.cpp \
           Server.tproj/RTPStream3gpp.cpp \
           Server.tproj/RTSPProtocol.cpp \
           Server.tproj/RTSPRequest.cpp \
           Server.tproj/RTSPRequest3GPP.cpp \
           Server.tproj/RTSPRequestInterface.cpp \
           Server.tproj/RTSPRequestStream.cpp \
           Server.tproj/RTSPResponseStream.cpp \
           Server.tproj/RTSPSession.cpp \
           Server.tproj/RTSPSession3GPP.cpp \
           Server.tproj/RTSPSessionInterface.cpp \
           Server.tproj/RunServer.cpp \
           Server.tproj/CrashHelper.cpp \
           #Server.tproj/win32main.cpp \
           APIModules/EasyCMSModule/EasyCMSModule.cpp \
           APIModules/EasyCMSModule/EasyCMSSession.cpp \
           APIModules/EasyHLSModule/EasyHLSModule.cpp \
           APIModules/EasyHLSModule/EasyHLSSession.cpp \
           APIModules/EasyRedisModule/EasyRedisModule.cpp \
           APIModules/EasyRelayModule/EasyRelayModule.cpp \
           APIModules/EasyRelayModule/EasyRelaySession.cpp \
           #APIModules/OSMemory_Modules/OSMemory_Modules.cpp \
           APIModules/QTSSAccessLogModule/QTSSAccessLogModule.cpp \
           APIModules/QTSSAccessModule/AccessChecker.cpp \
           APIModules/QTSSAccessModule/QTSSAccessModule.cpp \
           APIModules/QTSSAdminModule/AdminElementNode.cpp \
           APIModules/QTSSAdminModule/AdminQuery.cpp \
           APIModules/QTSSAdminModule/frozen.c \
           APIModules/QTSSAdminModule/mongoose.c \
           APIModules/QTSSAdminModule/QTSSAdminModule.cpp \
           APIModules/QTSSDemoAuthorizationModule.bproj/QTSSDemoAuthorizationModule.cpp \
           APIModules/QTSSDemoRedirectModule.bproj/QTSSDemoRedirectModule.cpp \
           APIModules/QTSSDemoSMILModule.bproj/QTSSDemoSMILModule.cpp \
           #APIModules/QTSSDSAuthModule/DSAccessChecker.cpp \
           #APIModules/QTSSDSAuthModule/QTSSDSAuthModule.cpp \
           APIModules/QTSSFileModule/QTSSFileModule.cpp \
           APIModules/QTSSFilePrivsModule.bproj/QTSSFilePrivsModule.cpp \
           APIModules/QTSSFlowControlModule/QTSSFlowControlModule.cpp \
           APIModules/QTSSHomeDirectoryModule/DirectoryInfo.cpp \
           APIModules/QTSSHomeDirectoryModule/QTSSHomeDirectoryModule.cpp \
           #APIModules/QTSSHttpFileModule/QTSSHttpFileModule.cpp \
           APIModules/QTSSMP3StreamingModule/QTSSMP3StreamingModule.cpp \
           APIModules/QTSSPOSIXFileSysModule/QTSSPosixFileSysModule.cpp \
           #APIModules/QTSSProxyModule/QTSSProxyModule.cpp \
           APIModules/QTSSRawFileModule.bproj/QTSSRawFileModule.cpp \
           APIModules/QTSSReflectorModule/QTSSReflectorModule.cpp \
           APIModules/QTSSReflectorModule/QTSSRelayModule.cpp \
           #APIModules/QTSSReflectorModule/QTSSSplitterModule.cpp \
           APIModules/QTSSReflectorModule/RCFSourceInfo.cpp \
           APIModules/QTSSReflectorModule/ReflectorSession.cpp \
           APIModules/QTSSReflectorModule/ReflectorStream.cpp \
           APIModules/QTSSReflectorModule/RelayOutput.cpp \
           APIModules/QTSSReflectorModule/RelaySDPSourceInfo.cpp \
           APIModules/QTSSReflectorModule/RelaySession.cpp \
           APIModules/QTSSReflectorModule/RTPSessionOutput.cpp \
           APIModules/QTSSReflectorModule/RTSPSourceInfo.cpp \
           APIModules/QTSSReflectorModule/SequenceNumberMap.cpp \
           APIModules/QTSSRefMovieModule/QTSSRefMovieModule.cpp \
           #APIModules/QTSSRTPFileModule/QTSSRTPFileModule.cpp \
           #APIModules/QTSSRTPFileModule/RTPFileSession.cpp \
           APIModules/QTSSSpamDefenseModule.bproj/QTSSSpamDefenseModule.cpp \
           APIModules/QTSSWebDebugModule/QTSSWebDebugModule.cpp \
           APIModules/QTSSWebStatsModule/QTSSWebStatsModule.cpp \
           #APIModules/QTSSDSAuthModule/DSWrappers/CDirService.cpp \
           #APIModules/QTSSDSAuthModule/DSWrappers/DSBuffer.cpp
            ../HTTPUtilitiesLib/HTTPRequest.cpp \
    ../HTTPUtilitiesLib/HTTPRequestStream.cpp \
    ../HTTPUtilitiesLib/HTTPResponseStream.cpp \
    ../HTTPUtilitiesLib/HTTPProtocol.cpp
