/*
	Copyright (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.easydarwin.org
*/
#ifndef _EASY_MEDIA_SOURCE_H_
#define _EASY_MEDIA_SOURCE_H_

#include "Task.h"
#include "hi_type.h"
#include "hi_net_dev_sdk.h"
#include "hi_net_dev_errors.h"
#include "QTSS.h"
#include "EasyPusherAPI.h"
#include "TimeoutTask.h"

#define EASY_SNAP_BUFFER_SIZE 1024*1024

class EasyCameraSource : public Task
{
public:
	EasyCameraSource();
	virtual ~EasyCameraSource();

	static void Initialize(QTSS_ModulePrefsObject modulePrefs);

	QTSS_Error StartStreaming(Easy_StartStream_Params* params);
	QTSS_Error StopStreaming(Easy_StopStream_Params* params);
	QTSS_Error GetCameraState(Easy_CameraState_Params* params);
	QTSS_Error GetCameraSnap(Easy_CameraSnap_Params* params);

	QTSS_Error PushFrame(unsigned char* frame, int len);

	bool GetForceIFrameFlag() { return m_bForceIFrame; }
	void SetForceIFrameFlag(bool flag) { m_bForceIFrame = flag; }

	bool GetCameraLoginFlag() { return fCameraLogin; }
	void SetCameraLoginFlag(bool flag) { fCameraLogin = flag; }

	OSMutex* GetMutex() { return &fMutex; }

private:
	void saveStartStreamParams(Easy_StartStream_Params * inParams);

	SInt64 Run();

	void stopGettingFrames();
	void doStopGettingFrames();

	bool cameraLogin();

	bool getSnapData(unsigned char* pBuf, UInt32 uBufLen, int* uSnapLen);

	QTSS_Error netDevStartStream();
	void netDevStopStream();

private:
	//������������
	HI_U32 m_u32Handle;

	bool fCameraLogin;
	bool m_bStreamFlag;
	bool m_bForceIFrame;

	TimeoutTask *fTimeoutTask;

	Easy_Pusher_Handle fPusherHandle;

	OSMutex fMutex;
	OSMutex fStreamingMutex;

	typedef struct StartStreamInfo_Tag
	{
		char serial[32];
		char ip[32];
		char protocol[16];
		char channel[8];
		char streamId[64];
		UInt16 port;
	} StartStreamInfo;

	StartStreamInfo fStartStreamInfo;

	unsigned char* fCameraSnapPtr;

};

#endif //_EASY_MEDIA_SOURCE_H_
