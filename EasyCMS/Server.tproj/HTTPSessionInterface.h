/*
	Copyleft (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.EasyDarwin.org
*/
/*
    File:       HTTPSessionInterface.h

    Contains:   Presents an API for session-wide resources for modules to use.
                Implements the CMS Session dictionary for QTSS API. 
*/

#ifndef __HTTPSESSIONINTERFACE_H__
#define __HTTPSESSIONINTERFACE_H__

#include "HTTPRequestStream.h"
#include "HTTPResponseStream.h"
#include "Task.h"
#include "QTSS.h"
#include "QTSSDictionary.h"
#include "atomic.h"
#include "base64.h"
#include <string>
#include <boost/thread/condition.hpp>

#include "OSRefTableEx.h"
#include "EasyProtocolDef.h"
#include "EasyProtocol.h"
#include <map>

using namespace EasyDarwin::Protocol;
using namespace std;

class HTTPSessionInterface : public QTSSDictionary, public Task
{
public:

    //Initialize must be called right off the bat to initialize dictionary resources
    static void     Initialize();
    
    HTTPSessionInterface();
    virtual ~HTTPSessionInterface();
    
    //Is this session alive? If this returns false, clean up and begone as
    //fast as possible
    Bool16 IsLiveSession()      { return fSocket.IsConnected() && fLiveSession; }
    
    // Allows clients to refresh the timeout
    void RefreshTimeout()       { fTimeoutTask.RefreshTimeout(); }

    // In order to facilitate sending out of band data on the RTSP connection,
    // other objects need to have direct pointer access to this object. But,
    // because this object is a task object it can go away at any time. If # of
    // object holders is > 0, the RTSPSession will NEVER go away. However,
    // the object managing the session should be aware that if IsLiveSession returns
    // false it may be wise to relinquish control of the session
    void IncrementObjectHolderCount() { (void)atomic_add(&fObjectHolders, 1); }
    void DecrementObjectHolderCount();
    
    //Two main things are persistent through the course of a session, not
    //associated with any one request. The RequestStream (which can be used for
    //getting data from the client), and the socket. OOps, and the ResponseStream
    HTTPRequestStream*  GetInputStream()    { return &fInputStream; }
    HTTPResponseStream* GetOutputStream()   { return &fOutputStream; }
    TCPSocket*          GetSocket()         { return &fSocket; }
    OSMutex*            GetSessionMutex()   { return &fSessionMutex; }
    
    UInt32              GetSessionIndex()      { return fSessionIndex; }
    
    // Request Body Length
    // This object can enforce a length of the request body to prevent callers
    // of Read() from overrunning the request body and going into the next request.
    // -1 is an unknown request body length. If the body length is unknown,
    // this object will do no length enforcement. 
    void                SetRequestBodyLength(SInt32 inLength)   { fRequestBodyLen = inLength; }
    SInt32              GetRemainingReqBodyLen()                { return fRequestBodyLen; }

	QTSS_Error UpdateDevSnap(const char* inSnapTime, const char* inSnapJpg) const;
	void UnRegDevSession() const;
    
    // QTSS STREAM FUNCTIONS
    
    // Allows non-buffered writes to the client. These will flow control.
    
    //THE FIRST ENTRY OF THE IOVEC MUST BE BLANK!!!
    virtual QTSS_Error WriteV(iovec* inVec, UInt32 inNumVectors, UInt32 inTotalLength, UInt32* outLenWritten);
    virtual QTSS_Error Write(void* inBuffer, UInt32 inLength, UInt32* outLenWritten, UInt32 inFlags);
    virtual QTSS_Error Read(void* ioBuffer, UInt32 inLength, UInt32* outLenRead);
    virtual QTSS_Error RequestEvent(QTSS_EventType inEventMask);

    enum
    {
        kMaxUserNameLen         = 32,
        kMaxUserPasswordLen     = 32
    };

	void PushNVRMessage(EasyNVRMessage &msg) { fNVRMessageQueue.push_back(msg); }

protected:
    enum
    {
        kFirstHTTPSessionID     = 1,    //UInt32
    };

    char                fSessionID[QTSS_MAX_SESSION_ID_LENGTH];

	//char				fSerial[EASY_MAX_SERIAL_LENGTH];
	//StrPtrLen			fDevSerialPtr;
	string				fDevSerial;
	boost::mutex		fNVROperatorMutex;
	boost::mutex		fStreamReqCountMutex;
	boost::condition_variable fCond;
	EasyNVRMessageQueue fNVRMessageQueue;
	//OSRef				fDevRef;

    TimeoutTask         fTimeoutTask;//allows the session to be timed out
    
    HTTPRequestStream   fInputStream;
    HTTPResponseStream  fOutputStream;
    
    OSMutex             fSessionMutex;

    //+rt  socket we get from "accept()"
    TCPSocket           fSocket;
    TCPSocket*          fOutputSocketP;
    TCPSocket*          fInputSocketP;  // <-- usually same as fSocketP, unless we're HTTP Proxying
    
    void        SnarfInputSocket( HTTPSessionInterface* fromRTSPSession );
    
    Easy_SessionType    fSessionType;	//��ͨsocket,NVR,���������������
	//UInt32				fTerminalType;	//�ն�����ARM��PC��Android��IOS

    Bool16              fLiveSession;
    unsigned int        fObjectHolders;

    UInt32              fSessionIndex;
    UInt32              fLocalAddr;
    UInt32              fRemoteAddr;
    SInt32              fRequestBodyLen;
    
    UInt16              fLocalPort;
    UInt16              fRemotePort;
    
	Bool16				fAuthenticated;

    static unsigned int		sSessionIndexCounter;

    // Dictionary support Param retrieval function
    static void*        SetupParams(QTSSDictionary* inSession, UInt32* outLen);
    
    static QTSSAttrInfoDict::AttrInfo   sAttributes[];
	
	//add,�Ϲ⣬start
	#define CliStartStreamTimeout	30000//�ͻ��˿�ʼ����ʱʱ�䣬��λΪms
	#define CliSNapShotTimeout		30000//�ͻ���ץ�ĳ�ʱʱ�䣬��λΪms  
	#define SessionIDTimeout		30000//sessionID��redis�ϵĴ��ʱ�䣬��λΪms

	strDevice fDevice;//add,�洢�豸��Ϣ������Session�������豸ʱ��Ч
	struct strInfo
	{
		int iResponse;//�豸�Ļ�Ӧ��
		UInt32 uTimeoutNum;//ѭ���ȴ�����
		bool bWaitingState;//�ȴ�״̬��־
		UInt32 uWaitingTime;//�ȴ�ʱ��>0ʱ��Ҫ�ȴ�


		UInt32 uCseq;//�ͻ��˵�CSeq
		string strDssIP;//�豸ʵ�������ĵ�ַ
		string strDssPort;
		string strReserve;//�豸ʵ������������
		string strProtocol;//�豸ʵ������Э��
		
		string strType;//ץ��ͼƬ����
		string strSnapShot;//�豸ץ��ͼƬ��base64����
	};
	struct strMessage
	{
		UInt32 uCseq;//�ͻ�����Ϣ��CSeq
		void *pObject;//Session����ָ��
		int iMsgType;//��Ϣ����
	};
	typedef map<UInt32,strMessage> MsgMap;
	char* fRequestBody;//�洢��������ݲ���

	OSMutex fMutexCSeq;//fCSeq�������ʵ�֣���Ϊ���ܶ���߳�ͬʱfCSeq++,��MsgMap��ͬʹ��һ��������
	UInt32 fCSeq;//��ǰSession��Է���������ʱ��fCSeqÿ�μ�1
	MsgMap fMsgMap;//�洢�ͻ��˷�������Ϣ
	strInfo fInfo;//�洢�豸��������Ϣ

public:
	strDevice *GetDeviceInfo(){return &fDevice;}
	UInt32 GetCSeq(){OSMutexLocker MutexLocker(&fMutexCSeq);return fCSeq++;}
	void  InsertToMsgMap(UInt32 uCSeq,strMessage& strMsg){OSMutexLocker MutexLocker(&fMutexCSeq);fMsgMap[uCSeq]=strMsg;}
	bool  FindInMsgMap(UInt32 uCSeq,strMessage& strMsg)
	{
		OSMutexLocker MutexLocker(&fMutexCSeq);
		MsgMap::iterator it_l=fMsgMap.find(uCSeq);
		if(it_l==fMsgMap.end())//û���ҵ�
			return false;
		else
		{
			strMsg=it_l->second;
			fMsgMap.erase(it_l);
			return true;
		}
	}
	void ReleaseMsgMap()//�ͷŶ��ڶ��������,�������һֱ���ڣ��޷��õ��ͷ�
	{
		OSMutexLocker MutexLocker(&fMutexCSeq);
		MsgMap::iterator it_l;
		for(it_l=fMsgMap.begin();it_l!=fMsgMap.end();it_l++)
		{
			//if(it_l->second.iMsgType==MSG_CLI_CMS_STREAM_START_REQ||it_l->second.iMsgType==MSG_CLI_CMS_START_RECORD_REQ||it_l->second.iMsgType==MSG_CLI_CMS_SNAP_SHOT_REQ)
			((HTTPSessionInterface*)(it_l->second.pObject))->DecrementObjectHolderCount();//��������
		}
	}
	//add,�Ϲ⣬end
};
#endif // __HTTPSESSIONINTERFACE_H__

