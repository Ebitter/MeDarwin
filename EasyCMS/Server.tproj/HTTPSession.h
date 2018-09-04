/*
	Copyleft (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.EasyDarwin.org
*/
/*!
  \file    HTTPSession.h
  \author  Babosa@EasyDarwin.org
  \date    2014-12-03
  \version 1.0
  \mainpage ʹ������

  ���������Ҫ����\n
  Select -> ServiceSession -> DispatchMsgCenter -> ServiceSession -> Cleanup\n\n

  Copyright (c) 2014 EasyDarwin.org ��Ȩ����\n

  \defgroup ����Ԫ�����¼���������
*/

#ifndef __HTTP_SESSION_H__
#define __HTTP_SESSION_H__

#include "HTTPSessionInterface.h"
#include "HTTPRequest.h"
#include "TimeoutTask.h"
#include "QTSSModule.h"
#include "FFDecoderAPI.h"

using namespace std;

typedef struct __DECODE_PARAM_T
{
	int				codec;
	int				width;
	int				height;
	int				gopTally;
	char*			imageData;
	unsigned int	imageSize;

	FFD_HANDLE		ffdHandle;
} DECODE_PARAM_T;

class HTTPSession : public HTTPSessionInterface
{
public:
	HTTPSession();
	virtual ~HTTPSession();

	QTSS_Error SendHTTPPacket(StrPtrLen* contentXML, Bool16 connectionClose, Bool16 decrement);

	char* GetDeviceSnap() { return fDeviceSnap; };
	char* GetDeviceSerial() { return (char*)fDevSerial.c_str(); };

	void SetStreamPushInfo(EasyJsonValue &info) { fStreamPushInfo = info; }
	EasyJsonValue &GetStreamPushInfo() { return fStreamPushInfo; }

private:
	SInt64 Run();

	// Does request prep & request cleanup, respectively
	QTSS_Error SetupRequest();
	void CleanupRequest();

	QTSS_Error ProcessRequest();//�������󣬵����ŵ�һ��״̬��ȥ�������������ظ�ִ��
	QTSS_Error ExecNetMsgErrorReqHandler(HTTPStatusCode errCode);//��ϢĬ�ϴ�����
	QTSS_Error ExecNetMsgDSRegisterReq(const char* json);//�豸ע������
	QTSS_Error ExecNetMsgCSGetStreamReq(const char* json);//�ͻ�����������
	QTSS_Error ExecNetMsgDSPushStreamAck(const char* json);//�豸�Ŀ�ʼ����Ӧ
	QTSS_Error ExecNetMsgCSFreeStreamReq(const char *json);//�ͻ��˵�ֱֹͣ������
	QTSS_Error ExecNetMsgDSStreamStopAck(const char* json);//�豸��ֹͣ������Ӧ
	QTSS_Error ExecNetMsgDSPostSnapReq(const char* json);//�豸�Ŀ��ո�������

	QTSS_Error ExecNetMsgCSDeviceListReq(const char* json);//�ͻ��˻���豸�б�json�ӿ�
	QTSS_Error ExecNetMsgCSCameraListReq(const char* json);//�ͻ��˻������ͷ�б�json�ӿ�,�����豸����ΪNVRʱ��Ч

	QTSS_Error ExecNetMsgCSGetStreamReqRESTful(const char* queryString);//�ͻ�����������Restful�ӿ�		
	QTSS_Error ExecNetMsgCSGetDeviceListReqRESTful(const char* queryString);//�ͻ��˻���豸�б�,restful�ӿ�
	QTSS_Error ExecNetMsgCSGetCameraListReqRESTful(const char* queryString);//�ͻ��˻������ͷ�б�restful�ӿڣ������豸����ΪNVRʱ��Ч

	QTSS_Error DumpRequestData();//���������

	// test current connections handled by this object against server pref connection limit
	static Bool16 OverMaxConnections(UInt32 buffer);

	QTSS_Error rawData2Image(char* rawBuf, int bufSize, int codec, int width, int height);
	int	yuv2BMPImage(unsigned int width, unsigned int height, char* yuvpbuf, unsigned int* rgbsize, unsigned char* rgbdata);

	HTTPRequest*        fRequest;
	OSMutex             fReadMutex;

	enum
	{
		kReadingRequest = 0,	//��ȡ����
		kFilteringRequest = 1,	//���˱���
		kPreprocessingRequest = 2,	//Ԥ������
		kProcessingRequest = 3,	//������
		kSendingResponse = 4,	//������Ӧ����
		kCleaningUp = 5,	//��ձ��δ���ı�������

		kReadingFirstRequest = 6,	//��һ�ζ�ȡSession���ģ���Ҫ������SessionЭ�����֣�HTTP/TCP/RTSP�ȵȣ�
		kHaveCompleteMessage = 7		// ��ȡ�������ı���
	};

	UInt32 fCurrentModule;
	UInt32 fState;

	QTSS_RoleParams     fRoleParams;//module param blocks for roles.
	QTSS_ModuleState    fModuleState;

	char* fDeviceSnap;
	EasyJsonValue fStreamPushInfo;

	// Channel Snap
	DECODE_PARAM_T		decodeParam;
};
#endif // __HTTP_SESSION_H__

