/*
	Copyleft (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.EasyDarwin.org
*/
/*
	File:       HTTPSession.cpp
	Contains:   EasyCMS HTTPSession
*/

#include "HTTPSession.h"
#include "QTSServerInterface.h"
#include "OSMemory.h"
#include "EasyUtil.h"

#include "OSArrayObjectDeleter.h"
#include <boost/algorithm/string.hpp>
#include "QueryParamList.h"

#if __FreeBSD__ || __hpux__	
#include <unistd.h>
#endif

#include <errno.h>

#if __solaris__ || __linux__ || __sgi__	|| __hpux__
#include <crypt.h>
#endif

using namespace std;

static const int sWaitDeviceRspTimeout = 10;
static const int sHeaderSize = 2048;
static const int sIPSize = 20;
static const int sPortSize = 6;

#define	_WIDTHBYTES(c)		((c+31)/32*4)	// c = width * bpp
#define	SNAP_CAPTURE_TIME	30
#define SNAP_IMAGE_WIDTH	320
#define	SNAP_IMAGE_HEIGHT	180
#define	SNAP_SIZE			SNAP_IMAGE_WIDTH * SNAP_IMAGE_HEIGHT * 3 + 58

HTTPSession::HTTPSession()
	: HTTPSessionInterface(),
	fRequest(NULL),
	fReadMutex(),
	fCurrentModule(0),
	fState(kReadingFirstRequest)
{
	this->SetTaskName("HTTPSession");

	//All EasyCameraSession/EasyNVRSession/EasyHTTPSession
	QTSServerInterface::GetServer()->AlterCurrentHTTPSessionCount(1);

	fModuleState.curModule = NULL;
	fModuleState.curTask = this;
	fModuleState.curRole = 0;
	fModuleState.globalLockRequested = false;

	memset(&decodeParam, 0x00, sizeof(DECODE_PARAM_T));

	decodeParam.gopTally = SNAP_CAPTURE_TIME;

	decodeParam.imageData = new char[SNAP_SIZE];
	memset(decodeParam.imageData, 0, SNAP_SIZE);

	qtss_printf("Create HTTPSession:%s\n", fSessionID);
}

HTTPSession::~HTTPSession()
{
	if (decodeParam.imageData)
	{
		delete[]decodeParam.imageData;
		decodeParam.imageData = NULL;
	}

	fLiveSession = false;
	this->CleanupRequest();

	QTSServerInterface::GetServer()->AlterCurrentHTTPSessionCount(-1);

	qtss_printf("Release HTTPSession:%s\n", fSessionID);

	if (fRequestBody)
	{
		delete[]fRequestBody;
		fRequestBody = NULL;
	}
}

SInt64 HTTPSession::Run()
{
	EventFlags events = this->GetEvents();
	QTSS_Error err = QTSS_NoErr;

	OSThreadDataSetter theSetter(&fModuleState, NULL);

	if (events & Task::kKillEvent)
		fLiveSession = false;

	if (events & Task::kTimeoutEvent)
	{
		char msgStr[512];
		qtss_snprintf(msgStr, sizeof(msgStr), "Timeout HTTPSession��Device_serial[%s]\n", fDevice.serial_.c_str());
		QTSServerInterface::LogError(qtssMessageVerbosity, msgStr);
		fLiveSession = false;
	}

	while (this->IsLiveSession())
	{
		switch (fState)
		{
		case kReadingFirstRequest:
			{
				if ((err = fInputStream.ReadRequest()) == QTSS_NoErr)
				{
					fInputSocketP->RequestEvent(EV_RE);
					return 0;
				}

				if ((err != QTSS_RequestArrived) && (err != E2BIG))
				{
					// Any other error implies that the client has gone away. At this point,
					// we can't have 2 sockets, so we don't need to do the "half closed" check
					// we do below
					Assert(err > 0);
					Assert(!this->IsLiveSession());
					break;
				}

				if ((err == QTSS_RequestArrived) || (err == E2BIG))
					fState = kHaveCompleteMessage;
			}
			continue;

		case kReadingRequest:
			{
				OSMutexLocker readMutexLocker(&fReadMutex);

				if ((err = fInputStream.ReadRequest()) == QTSS_NoErr)
				{
					fInputSocketP->RequestEvent(EV_RE);
					return 0;
				}

				if ((err != QTSS_RequestArrived) && (err != E2BIG) && (err != QTSS_BadArgument))
				{
					//Any other error implies that the input connection has gone away.
					// We should only kill the whole session if we aren't doing HTTP.
					// (If we are doing HTTP, the POST connection can go away)
					Assert(err > 0);
					if (fOutputSocketP->IsConnected())
					{
						// If we've gotten here, this must be an HTTP session with
						// a dead input connection. If that's the case, we should
						// clean up immediately so as to not have an open socket
						// needlessly lingering around, taking up space.
						Assert(fOutputSocketP != fInputSocketP);
						Assert(!fInputSocketP->IsConnected());
						fInputSocketP->Cleanup();
						return 0;
					}
					else
					{
						Assert(!this->IsLiveSession());
						break;
					}
				}
				fState = kHaveCompleteMessage;
			}
		case kHaveCompleteMessage:
			{
				Assert(fInputStream.GetRequestBuffer());

				Assert(fRequest == NULL);
				fRequest = NEW HTTPRequest(&QTSServerInterface::GetServerHeader(), fInputStream.GetRequestBuffer());

				fReadMutex.Lock();
				fSessionMutex.Lock();

				fOutputStream.ResetBytesWritten();

				if ((err == E2BIG) || (err == QTSS_BadArgument))
				{
					ExecNetMsgErrorReqHandler(httpBadRequest);
					fState = kSendingResponse;
					break;
				}

				Assert(err == QTSS_RequestArrived);
				fState = kFilteringRequest;
			}

		case kFilteringRequest:
			{
				fTimeoutTask.RefreshTimeout();

				QTSS_Error theErr = SetupRequest();

				if (theErr == QTSS_WouldBlock)
				{
					this->ForceSameThread();
					fInputSocketP->RequestEvent(EV_RE);
					// We are holding mutexes, so we need to force
					// the same thread to be used for next Run()
					return 0;
				}

				if (theErr != QTSS_NoErr)
				{
					ExecNetMsgErrorReqHandler(httpBadRequest);
				}

				if (fOutputStream.GetBytesWritten() > 0)
				{
					fState = kSendingResponse;
					break;
				}

				fState = kPreprocessingRequest;
				break;
			}

		case kPreprocessingRequest:
			{
				ProcessRequest();

				if (fOutputStream.GetBytesWritten() > 0)
				{
					delete[] fRequestBody;
					fRequestBody = NULL;
					fState = kSendingResponse;
					break;
				}

				if (fInfo.uWaitingTime > 0)
				{
					this->ForceSameThread();
					// We are holding mutexes, so we need to force
					// the same thread to be used for next Run()
					UInt32 iTemp = fInfo.uWaitingTime;
					fInfo.uWaitingTime = 0;
					return iTemp;
				}

				delete[] fRequestBody;
				fRequestBody = NULL;
				fState = kCleaningUp;
				break;
			}

		case kProcessingRequest:
			{
				if (fOutputStream.GetBytesWritten() == 0)
				{
					ExecNetMsgErrorReqHandler(httpInternalServerError);
					fState = kSendingResponse;
					break;
				}

				fState = kSendingResponse;
			}
		case kSendingResponse:
			{
				Assert(fRequest != NULL);

				err = fOutputStream.Flush();

				if (err == EAGAIN)
				{
					// If we get this error, we are currently flow-controlled and should
					// wait for the socket to become writeable again
					fSocket.RequestEvent(EV_WR);
					this->ForceSameThread();
					// We are holding mutexes, so we need to force
					// the same thread to be used for next Run()
					return 0;
				}
				else if (err != QTSS_NoErr)
				{
					// Any other error means that the client has disconnected, right?
					Assert(!this->IsLiveSession());
					break;
				}

				fState = kCleaningUp;
			}

		case kCleaningUp:
			{
				// Cleaning up consists of making sure we've read all the incoming Request Body
				// data off of the socket
				if (this->GetRemainingReqBodyLen() > 0)
				{
					err = this->DumpRequestData();

					if (err == EAGAIN)
					{
						fInputSocketP->RequestEvent(EV_RE);
						this->ForceSameThread();    // We are holding mutexes, so we need to force
						// the same thread to be used for next Run()
						return 0;
					}
				}

				this->CleanupRequest();
				fState = kReadingRequest;
			}
		}
	}

	this->CleanupRequest();

	if (fObjectHolders == 0)
		return -1;

	return 0;
}

QTSS_Error HTTPSession::SendHTTPPacket(StrPtrLen* contentXML, Bool16 connectionClose, Bool16 decrement)
{
	//OSMutexLocker mutexLock(&fReadMutex);//Prevent data chaos 

	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(httpOK))
	{
		if (contentXML->Len)
			httpAck.AppendContentLengthHeader(contentXML->Len);

		if (connectionClose)
			httpAck.AppendConnectionCloseHeader();

		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream *pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);
		if (contentXML->Len > 0)
			pOutputStream->Put(contentXML->Ptr, contentXML->Len);

		if (pOutputStream->GetBytesWritten() != 0)
		{
			QTSS_Error theErr = pOutputStream->Flush();

			if (theErr == EAGAIN)
			{
				fSocket.RequestEvent(EV_WR);
				return QTSS_NoErr;
			}
		}
	}

	if (fObjectHolders && decrement)
		DecrementObjectHolderCount();

	if (connectionClose)
		this->Signal(Task::kKillEvent);

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::SetupRequest()
{
	QTSS_Error theErr = fRequest->Parse();
	if (theErr != QTSS_NoErr)
		return QTSS_BadArgument;

	if (fRequest->GetRequestPath() != NULL)
	{
		string sRequest(fRequest->GetRequestPath());

		if (!sRequest.empty())
		{
			boost::to_lower(sRequest);

			vector<string> path;
			if (boost::ends_with(sRequest, "/"))
			{
				boost::erase_tail(sRequest, 1);
			}
			boost::split(path, sRequest, boost::is_any_of("/"), boost::token_compress_on);
			if (path.size() == 2)
			{
				if (path[0] == "api" && path[1] == "getdevicelist")
				{
					return ExecNetMsgCSGetDeviceListReqRESTful(fRequest->GetQueryString());
				}
				if (path[0] == "api" && path[1] == "getdeviceinfo")
				{
					return ExecNetMsgCSGetCameraListReqRESTful(fRequest->GetQueryString());
				}
				if (path[0] == "api" && path[1] == "getdevicestream")
				{
					return ExecNetMsgCSGetStreamReqRESTful(fRequest->GetQueryString());
				}
			}

			EasyMsgExceptionACK rsp;
			string msg = rsp.GetMsg();

			HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

			if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
			{
				if (!msg.empty())
					httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

				httpAck.AppendConnectionCloseHeader();

				//Push HTTP Header to OutputBuffer
				char respHeader[sHeaderSize] = { 0 };
				StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
				strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

				HTTPResponseStream* pOutputStream = GetOutputStream();
				pOutputStream->Put(respHeader);

				//Push HTTP Content to OutputBuffer
				if (!msg.empty())
					pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
			}

			return QTSS_NoErr;
		}
	}

	//READ json Content

	//1��get json content length
	StrPtrLen* lengthPtr = fRequest->GetHeaderValue(httpContentLengthHeader);

	StringParser theContentLenParser(lengthPtr);
	theContentLenParser.ConsumeWhitespace();
	UInt32 content_length = theContentLenParser.ConsumeInteger(NULL);

	qtss_printf("HTTPSession read content-length:%d \n", content_length);

	if (content_length <= 0)
	{
		return QTSS_BadArgument;
	}

	// Check for the existence of 2 attributes in the request: a pointer to our buffer for
	// the request body, and the current offset in that buffer. If these attributes exist,
	// then we've already been here for this request. If they don't exist, add them.
	UInt32 theBufferOffset = 0;
	char* theRequestBody = NULL;
	UInt32 theLen = sizeof(theRequestBody);
	theErr = QTSS_GetValue(this, EasyHTTPSesContentBody, 0, &theRequestBody, &theLen);

	if (theErr != QTSS_NoErr)
	{
		// First time we've been here for this request. Create a buffer for the content body and
		// shove it in the request.
		theRequestBody = NEW char[content_length + 1];
		memset(theRequestBody, 0, content_length + 1);
		theLen = sizeof(theRequestBody);
		theErr = QTSS_SetValue(this, EasyHTTPSesContentBody, 0, &theRequestBody, theLen);// SetValue creates an internal copy.
		Assert(theErr == QTSS_NoErr);

		// Also store the offset in the buffer
		theLen = sizeof(theBufferOffset);
		theErr = QTSS_SetValue(this, EasyHTTPSesContentBodyOffset, 0, &theBufferOffset, theLen);
		Assert(theErr == QTSS_NoErr);
	}

	theLen = sizeof(theBufferOffset);
	QTSS_GetValue(this, EasyHTTPSesContentBodyOffset, 0, &theBufferOffset, &theLen);

	// We have our buffer and offset. Read the data.
	//theErr = QTSS_Read(this, theRequestBody + theBufferOffset, content_length - theBufferOffset, &theLen);
	theErr = fInputStream.Read(theRequestBody + theBufferOffset, content_length - theBufferOffset, &theLen);
	Assert(theErr != QTSS_BadArgument);

	if ((theErr != QTSS_NoErr) && (theErr != EAGAIN))
	{
		OSCharArrayDeleter charArrayPathDeleter(theRequestBody);

		// NEED TO RETURN HTTP ERROR RESPONSE
		return QTSS_RequestFailed;
	}
	/*
	if (theErr == QTSS_RequestFailed)
	{
		OSCharArrayDeleter charArrayPathDeleter(theRequestBody);

		// NEED TO RETURN HTTP ERROR RESPONSE
		return QTSS_RequestFailed;
	}
	*/
	qtss_printf("HTTPSession read content-length:%d (%d/%d) \n", theLen, theBufferOffset + theLen, content_length);
	if ((theErr == QTSS_WouldBlock) || (theLen < (content_length - theBufferOffset)))
	{
		//
		// Update our offset in the buffer
		theBufferOffset += theLen;
		(void)QTSS_SetValue(this, EasyHTTPSesContentBodyOffset, 0, &theBufferOffset, sizeof(theBufferOffset));
		// The entire content body hasn't arrived yet. Request a read event and wait for it.

		Assert(theErr == QTSS_NoErr);
		return QTSS_WouldBlock;
	}

	// get complete HTTPHeader+JSONContent
	fRequestBody = theRequestBody;
	Assert(theErr == QTSS_NoErr);

	////TODO:://
	//if (theBufferOffset < sHeaderSize)
	//	qtss_printf("Recv message: %s\n", fRequestBody);

	UInt32 offset = 0;
	(void)QTSS_SetValue(this, EasyHTTPSesContentBodyOffset, 0, &offset, sizeof(offset));
	char* content = NULL;
	(void)QTSS_SetValue(this, EasyHTTPSesContentBody, 0, &content, 0);

	return theErr;
}

void HTTPSession::CleanupRequest()
{
	if (fRequest != NULL)
	{
		// NULL out any references to the current request
		delete fRequest;
		fRequest = NULL;
	}

	fSessionMutex.Unlock();
	fReadMutex.Unlock();

	// Clear out our last value for request body length before moving onto the next request
	this->SetRequestBodyLength(-1);
}

Bool16 HTTPSession::OverMaxConnections(UInt32 buffer)
{
	QTSServerInterface* theServer = QTSServerInterface::GetServer();
	SInt32 maxConns = theServer->GetPrefs()->GetMaxConnections();
	Bool16 overLimit = false;

	if (maxConns > -1) // limit connections
	{
		UInt32 maxConnections = static_cast<UInt32>(maxConns) + buffer;
		if (theServer->GetNumServiceSessions() > maxConnections)
		{
			overLimit = true;
		}
	}
	return overLimit;
}

QTSS_Error HTTPSession::DumpRequestData()
{
	char theDumpBuffer[EASY_REQUEST_BUFFER_SIZE_LEN];

	QTSS_Error theErr = QTSS_NoErr;
	while (theErr == QTSS_NoErr)
		theErr = this->Read(theDumpBuffer, EASY_REQUEST_BUFFER_SIZE_LEN, NULL);

	return theErr;
}

QTSS_Error HTTPSession::ExecNetMsgDSPostSnapReq(const char* json)
{
	if (!fAuthenticated) return httpUnAuthorized;

	EasyMsgDSPostSnapREQ parse(json);

	string image = parse.GetBodyValue(EASY_TAG_IMAGE);
	string channel = parse.GetBodyValue(EASY_TAG_CHANNEL);
	string device_serial = parse.GetBodyValue(EASY_TAG_SERIAL);
	string strType = parse.GetBodyValue(EASY_TAG_TYPE);
	string strTime = parse.GetBodyValue(EASY_TAG_TIME);
	string reserve = parse.GetBodyValue(EASY_TAG_RESERVE);

	if (channel.empty())
		channel = "0";

	if (strTime.empty())
	{
		strTime = EasyUtil::NowTime(EASY_TIME_FORMAT_YYYYMMDDHHMMSSEx);
	}
	else//Time Filter 2015-07-20 12:55:30->20150720125530
	{
		EasyUtil::DelChar(strTime, '-');
		EasyUtil::DelChar(strTime, ':');
		EasyUtil::DelChar(strTime, ' ');
	}

	if ((image.size() <= 0) || (device_serial.size() <= 0) || (strType.size() <= 0) || (strTime.size() <= 0))
		return QTSS_BadArgument;

	image = EasyUtil::Base64Decode(image.data(), image.size());

	char jpgDir[512] = { 0 };
	qtss_sprintf(jpgDir, "%s%s", QTSServerInterface::GetServer()->GetPrefs()->GetSnapLocalPath(), device_serial.c_str());
	OS::RecursiveMakeDir(jpgDir);

	char jpgPath[512] = { 0 };

	//local path
	qtss_sprintf(jpgPath, "%s/%s_%s_%s.%s", jpgDir, device_serial.c_str(), channel.c_str(), strTime.c_str(), strType.c_str());

	FILE* fSnap = ::fopen(jpgPath, "wb");
	if (fSnap == NULL)
	{
		//DWORD e=GetLastError();
		return QTSS_NoErr;
	}

	if (fSessionType == EasyCameraSession)
	{
		fwrite(image.data(), 1, image.size(), fSnap);
	}
	else if (fSessionType == EasyNVRSession)
	{
		char decQueryString[EASY_MAX_URL_LENGTH] = { 0 };
		EasyUtil::Urldecode((unsigned char*)reserve.c_str(), reinterpret_cast<unsigned char*>(decQueryString));

		QueryParamList parList(decQueryString);
		int width = EasyUtil::String2Int(parList.DoFindCGIValueForParam("width"));
		int height = EasyUtil::String2Int(parList.DoFindCGIValueForParam("height"));
		int codec = EasyUtil::String2Int(parList.DoFindCGIValueForParam("codec"));

		rawData2Image((char*)image.data(), image.size(), codec, width, height);
		fwrite(decodeParam.imageData, 1, decodeParam.imageSize, fSnap);
	}	
	
	::fclose(fSnap);

	//web path
	char snapURL[512] = { 0 };
	qtss_sprintf(snapURL, "%s%s/%s_%s_%s.%s", QTSServerInterface::GetServer()->GetPrefs()->GetSnapWebPath(), device_serial.c_str(), device_serial.c_str(), channel.c_str(), strTime.c_str(), strType.c_str());
	fDevice.HoldSnapPath(snapURL, channel);

	EasyProtocolACK rsp(MSG_SD_POST_SNAP_ACK);
	EasyJsonValue header, body;

	header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
	header[EASY_TAG_CSEQ] = parse.GetHeaderValue(EASY_TAG_CSEQ);
	header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SUCCESS_OK;
	header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SUCCESS_OK);

	body[EASY_TAG_SERIAL] = device_serial;
	body[EASY_TAG_CHANNEL] = channel;

	rsp.SetHead(header);
	rsp.SetBody(body);
	string msg = rsp.GetMsg();

	//HTTP Header
	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
	{
		if (!msg.empty())
			httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream* pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);

		if (!msg.empty())
			pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
	}

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgErrorReqHandler(HTTPStatusCode errCode)
{
	//HTTP Header
	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(errCode))
	{
		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream* pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);
	}

	this->fLiveSession = false;

	return QTSS_NoErr;
}

/*
	1.��ȡTerminalType��AppType,�����߼���֤���������򷵻�400 httpBadRequest;
	2.��֤Serial��Token����Ȩ����֤���������򷵻�401 httpUnAuthorized;
	3.��ȡName��Tag��Ϣ���б��ر������д��Redis;
	4.�����APPTypeΪEasyNVR,��ȡChannelsͨ����Ϣ���ر������д��Redis
*/
QTSS_Error HTTPSession::ExecNetMsgDSRegisterReq(const char* json)
{
	QTSS_Error theErr = QTSS_NoErr;
	EasyMsgDSRegisterREQ regREQ(json);

	//update info each time
	if (!fDevice.GetDevInfo(json))
	{
		return  QTSS_BadArgument;
	}

	while (!fAuthenticated)
	{
		//1.��ȡTerminalType��AppType,�����߼���֤���������򷵻�400 httpBadRequest;
		int appType = regREQ.GetAppType();
		//int terminalType = regREQ.GetTerminalType();
		switch (appType)
		{
		case EASY_APP_TYPE_CAMERA:
			{
				fSessionType = EasyCameraSession;
				//fTerminalType = terminalType;
				break;
			}
		case EASY_APP_TYPE_NVR:
			{
				fSessionType = EasyNVRSession;
				//fTerminalType = terminalType;
				break;
			}
		default:
			{
				break;
			}
		}

		if (fSessionType >= EasyHTTPSession)
		{
			//�豸ע��Ȳ���EasyCamera��Ҳ����EasyNVR�����ش���
			theErr = QTSS_BadArgument;
			break;
		}

		//2.��֤Serial��Token����Ȩ����֤���������򷵻�401 httpUnAuthorized;
		string serial = regREQ.GetBodyValue(EASY_TAG_SERIAL);
		string token = regREQ.GetBodyValue(EASY_TAG_TOKEN);

		if (serial.empty())
		{
			theErr = QTSS_AttrDoesntExist;
			break;
		}

		//��֤Serial��Token�Ƿ�Ϸ�
		/*if (false)
		{
			theErr = QTSS_NotPreemptiveSafe;
			break;
		}*/

		OSRefTableEx* DeviceMap = QTSServerInterface::GetServer()->GetDeviceSessionMap();
		OS_Error regErr = DeviceMap->Register(fDevice.serial_, this);
		if (regErr == OS_NoErr)
		{
			//��redis�������豸
			char msgStr[512];
			qtss_snprintf(msgStr, sizeof(msgStr), "Device register��Device_serial[%s]\n", fDevice.serial_.c_str());
			QTSServerInterface::LogError(qtssMessageVerbosity, msgStr);

			QTSS_RoleParams theParams;
			theParams.StreamNameParams.inStreamName = const_cast<char *>(fDevice.serial_.c_str());
			UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRedisAddDevNameRole);
			for (UInt32 currentModule = 0; currentModule < numModules; currentModule++)
			{
				QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRedisAddDevNameRole, currentModule);
				(void)theModule->CallDispatch(Easy_RedisAddDevName_Role, &theParams);
			}
			fAuthenticated = true;
		}
		else
		{
			//�豸��ͻ��ʱ��ǰһ���豸������,��Ϊ�ϵ硢��������������ǲ���Ͽ��ģ�����豸���硢����ͨ˳֮��ͻ������ͻ��
			//һ�����ӵĳ�ʱʱ90�룬Ҫ�ȵ�90��֮���豸��������ע�����ߡ�
			OSRefTableEx::OSRefEx* theDevRef = DeviceMap->Resolve(fDevice.serial_);////////////////////////////////++
			if (theDevRef != NULL)//�ҵ�ָ���豸
			{
				OSRefReleaserEx releaser(DeviceMap, fDevice.serial_);
				HTTPSession* pDevSession = static_cast<HTTPSession *>(theDevRef->GetObjectPtr());//��õ�ǰ�豸�Ự
				pDevSession->Signal(Task::kKillEvent);//��ֹ�豸����
				//QTSServerInterface::GetServer()->GetDeviceSessionMap()->Release(fDevice.serial_);////////////////////////////////--
			}
			//��һ����Ȼ�������߳�ͻ����Ϊ��Ȼ���豸������Task::kKillEvent��Ϣ�����豸���ܲ���������ֹ�������Ҫѭ���ȴ��Ƿ��Ѿ���ֹ��
			theErr = QTSS_AttrNameExists;;
		}
		break;
	}

	if (theErr != QTSS_NoErr)	return theErr;

	//�ߵ���˵�����豸�ɹ�ע���������
	EasyProtocol req(json);
	EasyProtocolACK rsp(MSG_SD_REGISTER_ACK);
	EasyJsonValue header, body;
	header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
	header[EASY_TAG_CSEQ] = req.GetHeaderValue(EASY_TAG_CSEQ);
	header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SUCCESS_OK;
	header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SUCCESS_OK);

	body[EASY_TAG_SERIAL] = fDevice.serial_;
	body[EASY_TAG_SESSION_ID] = fSessionID;

	rsp.SetHead(header);
	rsp.SetBody(body);
	string msg = rsp.GetMsg();

	//������Ӧ����(HTTP Header)
	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
	{
		if (!msg.empty())
			httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream* pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);

		//����Ӧ������ӵ�HTTP�����������
		if (!msg.empty())
			pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
	}

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgCSFreeStreamReq(const char* json)//�ͻ��˵�ֱֹͣ������
{
	//�㷨����������ָ���豸�����豸���ڣ������豸����ֹͣ������
	/*//��ʱע�͵���ʵ��������Ҫ��֤��
	if(!fAuthenticated)//û�н�����֤����
	return httpUnAuthorized;
	*/

	EasyProtocol req(json);
	//��serial/channel�н�����serial��channel
	string strStreamName = req.GetBodyValue(EASY_TAG_SERIAL);//������
	if (strStreamName.size() <= 0)
		return QTSS_BadArgument;

	int iPos = strStreamName.find('/');
	if (iPos == string::npos)
		return QTSS_BadArgument;

	string strDeviceSerial = strStreamName.substr(0, iPos);
	string strChannel = strStreamName.substr(iPos + 1, strStreamName.size() - iPos - 1);

	//string strDeviceSerial	=	req.GetBodyValue(EASY_TAG_SERIAL);//�豸���к�
	//string strChannel	=	req.GetBodyValue(EASY_TAG_CHANNEL);//����ͷ���к�
	string strReserve = req.GetBodyValue(EASY_TAG_RESERVE);//StreamID
	string strProtocol = req.GetBodyValue(EASY_TAG_PROTOCOL);//Protocol

	//Ϊ��ѡ�������Ĭ��ֵ
	if (strChannel.empty())
		strChannel = "0";
	if (strReserve.empty())
		strReserve = "1";

	if ((strDeviceSerial.size() <= 0) || (strProtocol.size() <= 0))//�����ж�
		return QTSS_BadArgument;

	OSRefTableEx* DeviceMap = QTSServerInterface::GetServer()->GetDeviceSessionMap();
	OSRefTableEx::OSRefEx* theDevRef = DeviceMap->Resolve(strDeviceSerial);////////////////////////////////++
	if (theDevRef == NULL)//�Ҳ���ָ���豸
		return EASY_ERROR_DEVICE_NOT_FOUND;

	OSRefReleaserEx releaser(DeviceMap, strDeviceSerial);
	//�ߵ���˵������ָ���豸������豸����ֹͣ��������
	HTTPSession* pDevSession = static_cast<HTTPSession *>(theDevRef->GetObjectPtr());//��õ�ǰ�豸�ػ�

	EasyProtocolACK reqreq(MSG_SD_STREAM_STOP_REQ);
	EasyJsonValue headerheader, bodybody;

	char chTemp[16] = { 0 };
	UInt32 uDevCseq = pDevSession->GetCSeq();
	sprintf(chTemp, "%d", uDevCseq);
	headerheader[EASY_TAG_CSEQ] = string(chTemp);//ע������ط�����ֱ�ӽ�UINT32->int,��Ϊ���������ʧ��
	headerheader[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;

	bodybody[EASY_TAG_SERIAL] = strDeviceSerial;
	bodybody[EASY_TAG_CHANNEL] = strChannel;
	bodybody[EASY_TAG_RESERVE] = strReserve;
	bodybody[EASY_TAG_PROTOCOL] = strProtocol;

	reqreq.SetHead(headerheader);
	reqreq.SetBody(bodybody);

	string buffer = reqreq.GetMsg();

	Easy_SendMsg(pDevSession, const_cast<char*>(buffer.c_str()), buffer.size(), false, false);
	//DeviceMap->Release(strDeviceSerial);//////////////////////////////////////////////////////////--

	//ֱ�ӶԿͻ��ˣ�EasyDarWin)������ȷ��Ӧ
	EasyProtocolACK rsp(MSG_SC_FREE_STREAM_ACK);
	EasyJsonValue header, body;
	header[EASY_TAG_CSEQ] = req.GetHeaderValue(EASY_TAG_CSEQ);
	header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
	header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SUCCESS_OK;
	header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SUCCESS_OK);

	body[EASY_TAG_SERIAL] = strDeviceSerial;
	body[EASY_TAG_CHANNEL] = strChannel;
	body[EASY_TAG_RESERVE] = strReserve;
	body[EASY_TAG_PROTOCOL] = strProtocol;

	rsp.SetHead(header);
	rsp.SetBody(body);
	string msg = rsp.GetMsg();

	//������Ӧ����(HTTP Header)
	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
	{
		if (!msg.empty())
			httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream* pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);

		//����Ӧ������ӵ�HTTP�����������
		if (!msg.empty())
			pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
	}

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgDSStreamStopAck(const char* json)//�豸��ֹͣ������Ӧ
{
	if (!fAuthenticated)//û�н�����֤����
		return httpUnAuthorized;

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgCSGetStreamReqRESTful(const char* queryString)//�ŵ�ProcessRequest���ڵ�״̬ȥ����������ѭ������
{
	/*//��ʱע�͵���ʵ��������Ҫ��֤��
	if(!fAuthenticated)//û�н�����֤����
	return httpUnAuthorized;
	*/

	if (queryString == NULL)
	{
		return QTSS_BadArgument;
	}

	char decQueryString[EASY_MAX_URL_LENGTH] = { 0 };
	EasyUtil::Urldecode((unsigned char*)queryString, reinterpret_cast<unsigned char*>(decQueryString));

	QueryParamList parList(decQueryString);
	const char* chSerial = parList.DoFindCGIValueForParam(EASY_TAG_L_DEVICE);//��ȡ�豸���к�
	const char* chChannel = parList.DoFindCGIValueForParam(EASY_TAG_L_CHANNEL);//��ȡͨ��
	const char* chProtocol = parList.DoFindCGIValueForParam(EASY_TAG_L_PROTOCOL);//��ȡͨ��
	const char* chReserve = parList.DoFindCGIValueForParam(EASY_TAG_L_RESERVE);//��ȡͨ��

	//Ϊ��ѡ�������Ĭ��ֵ
	if (chChannel == NULL)
		chChannel = "0";
	if (chReserve == NULL)
		chReserve = "1";

	if (chSerial == NULL || chProtocol == NULL)
		return QTSS_BadArgument;

	EasyProtocolACK req(MSG_CS_GET_STREAM_REQ);//��restful�ӿںϳ�json��ʽ����
	EasyJsonValue header, body;

	char chTemp[16] = { 0 };//����ͻ��˲��ṩCSeq,��ô����ÿ�θ�������һ��Ψһ��CSeq
	UInt32 uCseq = GetCSeq();
	sprintf(chTemp, "%d", uCseq);

	header[EASY_TAG_CSEQ] = chTemp;
	header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
	body[EASY_TAG_SERIAL] = chSerial;
	body[EASY_TAG_CHANNEL] = chChannel;
	body[EASY_TAG_PROTOCOL] = chProtocol;
	body[EASY_TAG_RESERVE] = chReserve;

	req.SetHead(header);
	req.SetBody(body);

	string buffer = req.GetMsg();
	fRequestBody = new char[buffer.size() + 1];
	strcpy(fRequestBody, buffer.c_str());

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgCSGetStreamReq(const char* json)//�ͻ��˿�ʼ������
{
	/*//��ʱע�͵���ʵ��������Ҫ��֤��
	if(!fAuthenticated)//û�н�����֤����
	return httpUnAuthorized;
	*/

	EasyProtocol req(json);

	string strDeviceSerial = req.GetBodyValue(EASY_TAG_SERIAL);//�豸���к�
	string strChannel = req.GetBodyValue(EASY_TAG_CHANNEL);//����ͷ���к�
	string strProtocol = req.GetBodyValue(EASY_TAG_PROTOCOL);//Э��
	string strReserve = req.GetBodyValue(EASY_TAG_RESERVE);//������

	//��ѡ�������û�У�����Ĭ��ֵ���
	if (strChannel.empty())//��ʾΪEasyCamera�豸
		strChannel = "0";
	if (strReserve.empty())//û����������ʱĬ��Ϊ����
		strReserve = "1";

	if ((strDeviceSerial.size() <= 0) || (strProtocol.size() <= 0))//�����ж�
		return QTSS_BadArgument;

	string strCSeq = req.GetHeaderValue(EASY_TAG_CSEQ);
	UInt32 uCSeq = atoi(strCSeq.c_str());
	string strURL;//ֱ����ַ

	if (!fInfo.bWaitingState)//��һ�δ�������
	{
		OSRefTableEx* DeviceMap = QTSServerInterface::GetServer()->GetDeviceSessionMap();
		OSRefTableEx::OSRefEx* theDevRef = DeviceMap->Resolve(strDeviceSerial);////////////////////////////////++
		if (theDevRef == NULL)//�Ҳ���ָ���豸
			return EASY_ERROR_DEVICE_NOT_FOUND;

		OSRefReleaserEx releaser(DeviceMap, strDeviceSerial);
		//�ߵ���˵������ָ���豸
		HTTPSession* pDevSession = static_cast<HTTPSession *>(theDevRef->GetObjectPtr());//��õ�ǰ�豸�ػ�

		string strDssIP, strDssPort;
		char chDssIP[sIPSize] = { 0 };
		char chDssPort[sPortSize] = { 0 };

		QTSS_RoleParams theParams;
		theParams.GetAssociatedDarwinParams.inSerial = const_cast<char*>(strDeviceSerial.c_str());
		theParams.GetAssociatedDarwinParams.inChannel = const_cast<char*>(strChannel.c_str());
		theParams.GetAssociatedDarwinParams.outDssIP = chDssIP;
		theParams.GetAssociatedDarwinParams.outDssPort = chDssPort;

		UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRedisGetEasyDarwinRole);
		for (UInt32 currentModule = 0; currentModule < numModules; ++currentModule)
		{
			QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRedisGetEasyDarwinRole, currentModule);
			(void)theModule->CallDispatch(Easy_RedisGetEasyDarwin_Role, &theParams);
		}
		if (chDssIP[0] != 0)//�Ƿ���ڹ�����EasyDarWinת��������test,Ӧ����Redis�ϵ����ݣ���Ϊ�����ǲ��ɿ��ģ���EasyDarWin�ϵ������ǿɿ���
		{
			strDssIP = chDssIP;
			strDssPort = chDssPort;
			//�ϳ�ֱ����RTSP��ַ�������п��ܸ�����������Э�鲻ͬ�����ɲ�ͬ��ֱ����ַ����RTMP��HLS��
			string strSessionID;
			char chSessionID[128] = { 0 };

			QTSS_RoleParams theParamsGetStream;
			theParamsGetStream.GenStreamIDParams.outStreanID = chSessionID;
			theParamsGetStream.GenStreamIDParams.inTimeoutMil = SessionIDTimeout;

			numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRedisGenStreamIDRole);
			for (UInt32 currentModule = 0; currentModule < numModules; ++currentModule)
			{
				QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRedisGenStreamIDRole, currentModule);
				(void)theModule->CallDispatch(Easy_RedisGenStreamID_Role, &theParamsGetStream);
			}

			if (chSessionID[0] == 0)//sessionID��redis�ϵĴ洢ʧ��
			{
				//DeviceMap->Release(strDeviceSerial);/////////////////////////////////////////////--
				return EASY_ERROR_SERVER_INTERNAL_ERROR;
			}
			strSessionID = chSessionID;
			strURL = string("rtsp://").append(strDssIP).append(":").append(strDssPort).append("/")
				.append(strDeviceSerial).append("/")
				.append(strChannel).append(".sdp")
				.append("?token=").append(strSessionID);

			//�����Ѿ��ò����豸�ػ��ˣ��ͷ�����
			//DeviceMap->Release(strDeviceSerial);/////////////////////////////////////////////--
		}
		else
		{//�����ڹ�����EasyDarWin

			/*char ch_dss_ip[sIPSize] = { 0 };
			char ch_dss_port[sPortSize] = { 0 };*/
			QTSS_RoleParams theParamsRedis;
			theParamsRedis.GetBestDarwinParams.outDssIP = chDssIP;
			theParamsRedis.GetBestDarwinParams.outDssPort = chDssPort;

			numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRedisGetBestEasyDarwinRole);
			for (UInt32 currentModule = 0; currentModule < numModules; currentModule++)
			{
				QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRedisGetBestEasyDarwinRole, currentModule);
				(void)theModule->CallDispatch(Easy_RedisGetBestEasyDarwin_Role, &theParamsRedis);
			}

			if (chDssIP[0] == 0)//������DarWin
			{
				//DeviceMap->Release(strDeviceSerial);/////////////////////////////////////////////--
				return EASY_ERROR_SERVICE_NOT_FOUND;
			}
			//��ָ���豸���Ϳ�ʼ������

			strDssIP = chDssIP;
			strDssPort = chDssPort;
			EasyProtocolACK reqreq(MSG_SD_PUSH_STREAM_REQ);
			EasyJsonValue headerheader, bodybody;

			char chTemp[16] = { 0 };
			UInt32 uDevCseq = pDevSession->GetCSeq();
			sprintf(chTemp, "%d", uDevCseq);
			headerheader[EASY_TAG_CSEQ] = string(chTemp);//ע������ط�����ֱ�ӽ�UINT32->int,��Ϊ���������ʧ��
			headerheader[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;

			string strSessionID;
			char chSessionID[128] = { 0 };

			//QTSS_RoleParams theParams;
			theParams.GenStreamIDParams.outStreanID = chSessionID;
			theParams.GenStreamIDParams.inTimeoutMil = SessionIDTimeout;

			numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRedisGenStreamIDRole);
			for (UInt32 currentModule = 0; currentModule < numModules; currentModule++)
			{
				QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRedisGenStreamIDRole, currentModule);
				(void)theModule->CallDispatch(Easy_RedisGenStreamID_Role, &theParams);
			}
			if (chSessionID[0] == 0)//sessionID��redis�ϵĴ洢ʧ��
			{
				DeviceMap->Release(strDeviceSerial);/////////////////////////////////////////////--
				return EASY_ERROR_SERVER_INTERNAL_ERROR;
			}

			strSessionID = chSessionID;
			bodybody[EASY_TAG_STREAM_ID] = strSessionID;
			bodybody[EASY_TAG_SERVER_IP] = strDssIP;
			bodybody[EASY_TAG_SERVER_PORT] = strDssPort;
			bodybody[EASY_TAG_SERIAL] = strDeviceSerial;
			bodybody[EASY_TAG_CHANNEL] = strChannel;
			bodybody[EASY_TAG_PROTOCOL] = strProtocol;
			bodybody[EASY_TAG_RESERVE] = strReserve;

			reqreq.SetHead(headerheader);
			reqreq.SetBody(bodybody);

			string buffer = reqreq.GetMsg();
			//
			strMessage msgTemp;

			msgTemp.iMsgType = MSG_CS_GET_STREAM_REQ;//��ǰ�������Ϣ
			msgTemp.pObject = this;//��ǰ����ָ��
			msgTemp.uCseq = uCSeq;//��ǰ�����cseq

			pDevSession->InsertToMsgMap(uDevCseq, msgTemp);//���뵽Map�еȴ��ͻ��˵Ļ�Ӧ
			IncrementObjectHolderCount();
			Easy_SendMsg(pDevSession, const_cast<char*>(buffer.c_str()), buffer.size(), false, false);
			//DeviceMap->Release(strDeviceSerial);//////////////////////////////////////////////////////////--

			fInfo.bWaitingState = true;//�ȴ��豸��Ӧ
			fInfo.iResponse = 0;//��ʾ�豸��û�л�Ӧ
			fInfo.uTimeoutNum = 0;//��ʼ���㳬ʱ
			fInfo.uWaitingTime = 100;//��100msΪ����ѭ���ȴ���������ռ��CPU

			return QTSS_NoErr;
		}
	}
	else//�ȴ��豸��Ӧ 
	{
		if (fInfo.iResponse == 0)//�豸��û�л�Ӧ
		{
			fInfo.uTimeoutNum++;
			if (fInfo.uTimeoutNum > CliStartStreamTimeout / 100)//��ʱ��
			{
				fInfo.bWaitingState = false;//�ָ�״̬
				return httpRequestTimeout;
			}
			else//û�г�ʱ�������ȴ�
			{
				fInfo.uWaitingTime = 100;//��100msΪ����ѭ���ȴ���������ռ��CPU
				return QTSS_NoErr;
			}
		}
		else if (fInfo.uCseq != uCSeq)//�����������Ҫ�ģ������ǵ�һ������ʱ��ʱ���ڶ�������ʱ�����˵�һ���Ļ�Ӧ����ʱ����Ӧ�ü����ȴ��ڶ����Ļ�Ӧֱ����ʱ
		{
			fInfo.iResponse = 0;//�����ȴ�����һ�����ܺ���һ���߳�ͬʱ��ֵ������Ҳ���ܽ����ֻ����Ӱ�첻��
			fInfo.uTimeoutNum++;
			fInfo.uWaitingTime = 100;//��100msΪ����ѭ���ȴ���������ռ��CPU
			return QTSS_NoErr;
		}
		else if (fInfo.iResponse == EASY_ERROR_SUCCESS_OK)//��ȷ��Ӧ
		{
			fInfo.bWaitingState = false;//�ָ�״̬
			strReserve = fInfo.strReserve;//ʹ���豸�������ͺ�����Э��
			strProtocol = fInfo.strProtocol;
			//�ϳ�ֱ����ַ

			string strSessionID;
			char chSessionID[128] = { 0 };

			QTSS_RoleParams theParams;
			theParams.GenStreamIDParams.outStreanID = chSessionID;
			theParams.GenStreamIDParams.inTimeoutMil = SessionIDTimeout;

			UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRedisGenStreamIDRole);
			for (UInt32 currentModule = 0; currentModule < numModules; currentModule++)
			{
				QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRedisGenStreamIDRole, currentModule);
				(void)theModule->CallDispatch(Easy_RedisGenStreamID_Role, &theParams);
			}
			if (chSessionID[0] == 0)//sessionID��redis�ϵĴ洢ʧ��
			{
				return EASY_ERROR_SERVER_INTERNAL_ERROR;
			}
			strSessionID = chSessionID;
			strURL = string("rtsp://")
				.append(fInfo.strDssIP).append(":").append(fInfo.strDssPort).append("/")
				.append(strDeviceSerial).append("/")
				.append(strChannel).append(".sdp")
				.append("?token=").append(strSessionID);
		}
		else//�豸�����Ӧ
		{
			fInfo.bWaitingState = false;//�ָ�״̬
			return fInfo.iResponse;
		}
	}

	//�ߵ���˵���Կͻ��˵���ȷ��Ӧ,��Ϊ�����Ӧֱ�ӷ��ء�
	EasyProtocolACK rsp(MSG_SC_GET_STREAM_ACK);
	EasyJsonValue header, body;
	body[EASY_TAG_URL] = strURL;
	body[EASY_TAG_SERIAL] = strDeviceSerial;
	body[EASY_TAG_CHANNEL] = strChannel;
	body[EASY_TAG_PROTOCOL] = strProtocol;//�����ǰ�Ѿ��������򷵻�����ģ����򷵻�ʵ����������
	body[EASY_TAG_RESERVE] = strReserve;//�����ǰ�Ѿ��������򷵻�����ģ����򷵻�ʵ����������

	header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
	header[EASY_TAG_CSEQ] = strCSeq;
	header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SUCCESS_OK;
	header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SUCCESS_OK);

	rsp.SetHead(header);
	rsp.SetBody(body);
	string msg = rsp.GetMsg();

	//������Ӧ����(HTTP Header)
	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
	{
		if (!msg.empty())
			httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream* pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);

		//����Ӧ������ӵ�HTTP�����������
		if (!msg.empty())
			pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
	}

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgDSPushStreamAck(const char* json)//�豸�Ŀ�ʼ����Ӧ
{
	if (!fAuthenticated)//û�н�����֤����
		return httpUnAuthorized;

	//�����豸��������Ӧ�ǲ���Ҫ�ڽ��л�Ӧ�ģ�ֱ�ӽ����ҵ���Ӧ�Ŀͻ���Session����ֵ����	
	EasyProtocol req(json);

	string strDeviceSerial = req.GetBodyValue(EASY_TAG_SERIAL);//�豸���к�
	string strChannel = req.GetBodyValue(EASY_TAG_CHANNEL);//����ͷ���к�
	//string strProtocol		=	req.GetBodyValue("Protocol");//Э��,�ն˽�֧��RTSP����
	string strReserve = req.GetBodyValue(EASY_TAG_RESERVE);//������
	string strDssIP = req.GetBodyValue(EASY_TAG_SERVER_IP);//�豸ʵ��������ַ
	string strDssPort = req.GetBodyValue(EASY_TAG_SERVER_PORT);//�Ͷ˿�

	string strCSeq = req.GetHeaderValue(EASY_TAG_CSEQ);//����ǹؼ���
	string strStateCode = req.GetHeaderValue(EASY_TAG_ERROR_NUM);//״̬��

	if (strChannel.empty())
		strChannel = "0";
	if (strReserve.empty())
		strReserve = "1";

	UInt32 uCSeq = atoi(strCSeq.c_str());
	int iStateCode = atoi(strStateCode.c_str());

	strMessage strTempMsg;
	if (!FindInMsgMap(uCSeq, strTempMsg))
	{//�찡����Ȼ�Ҳ�����һ�����豸���͵�CSeq�������յĲ�һ��
		return QTSS_BadArgument;
	}
	else
	{
		HTTPSession* pCliSession = static_cast<HTTPSession *>(strTempMsg.pObject);//�������ָ������Ч�ģ���Ϊ֮ǰ���Ǹ������˻�����
		if (strTempMsg.iMsgType == MSG_CS_GET_STREAM_REQ)//�ͻ��˵Ŀ�ʼ������
		{
			if (iStateCode == EASY_ERROR_SUCCESS_OK)//ֻ����ȷ��Ӧ�Ž���һЩ��Ϣ�ı���
			{
				pCliSession->fInfo.strDssIP = strDssIP;
				pCliSession->fInfo.strDssPort = strDssPort;
				pCliSession->fInfo.strReserve = strReserve;
				//pCliSession->fInfo.strProtocol=strProtocol;
			}
			pCliSession->fInfo.uCseq = strTempMsg.uCseq;
			pCliSession->fInfo.iResponse = iStateCode;//���֮��ʼ�����ͻ��˶���
			pCliSession->DecrementObjectHolderCount();//���ڿ��Է��ĵİ�Ϣ��
		}
		else
		{
			return QTSS_BadArgument;
		}
	}
	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgCSGetDeviceListReqRESTful(const char* queryString)//�ͻ��˻���豸�б�
{
	/*
	if(!fAuthenticated)//û�н�����֤����
	return httpUnAuthorized;
	*/

	char decQueryString[EASY_MAX_URL_LENGTH] = { 0 };
	if (queryString != NULL)
	{
		EasyUtil::Urldecode((unsigned char*)queryString, reinterpret_cast<unsigned char*>(decQueryString));
	}
	QueryParamList parList(decQueryString);
	const char* chAppType = parList.DoFindCGIValueForParam(EASY_TAG_APP_TYPE);//APPType
	const char* chTerminalType = parList.DoFindCGIValueForParam(EASY_TAG_TERMINAL_TYPE);//TerminalType

	EasyProtocolACK rsp(MSG_SC_DEVICE_LIST_ACK);
	EasyJsonValue header, body;

	header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
	header[EASY_TAG_CSEQ] = 1;
	header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SUCCESS_OK;
	header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SUCCESS_OK);


	OSMutex* mutexMap = QTSServerInterface::GetServer()->GetDeviceSessionMap()->GetMutex();
	OSHashMap* deviceMap = QTSServerInterface::GetServer()->GetDeviceSessionMap()->GetMap();
	OSRefIt itRef;
	Json::Value* proot = rsp.GetRoot();

	{
		OSMutexLocker lock(mutexMap);
		int iDevNum = 0;

		for (itRef = deviceMap->begin(); itRef != deviceMap->end(); ++itRef)
		{
			strDevice* deviceInfo = static_cast<HTTPSession*>(itRef->second->GetObjectPtr())->GetDeviceInfo();
			if (chAppType != NULL)// AppType fileter
			{
				if (EasyProtocol::GetAppTypeString(deviceInfo->eAppType) != string(chAppType))
					continue;
			}
			if (chTerminalType != NULL)// TerminateType fileter
			{
				if (EasyProtocol::GetTerminalTypeString(deviceInfo->eDeviceType) != string(chTerminalType))
					continue;
			}

			iDevNum++;

			Json::Value value;
			value[EASY_TAG_SERIAL] = deviceInfo->serial_;//����ط������˱���,deviceMap�������ݣ�����deviceInfo�������ݶ��ǿ�
			value[EASY_TAG_NAME] = deviceInfo->name_;
			value[EASY_TAG_TAG] = deviceInfo->tag_;
			value[EASY_TAG_APP_TYPE] = EasyProtocol::GetAppTypeString(deviceInfo->eAppType);
			value[EASY_TAG_TERMINAL_TYPE] = EasyProtocol::GetTerminalTypeString(deviceInfo->eDeviceType);
			//����豸��EasyCamera,�򷵻��豸������Ϣ
			if (deviceInfo->eAppType == EASY_APP_TYPE_CAMERA)
			{
				value[EASY_TAG_SNAP_URL] = deviceInfo->snapJpgPath_;
			}
			(*proot)[EASY_TAG_ROOT][EASY_TAG_BODY][EASY_TAG_DEVICES].append(value);
		}
		body[EASY_TAG_DEVICE_COUNT] = iDevNum;
	}

	rsp.SetHead(header);
	rsp.SetBody(body);

	string msg = rsp.GetMsg();

	//������Ӧ����(HTTPͷ)
	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
	{
		if (!msg.empty())
			httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

		//��Ӧ��ɺ�Ͽ�����
		httpAck.AppendConnectionCloseHeader();

		//Push MSG to OutputBuffer
		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream* pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);
		if (!msg.empty())
			pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
	}

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgCSDeviceListReq(const char* json)//�ͻ��˻���豸�б�
{
	/*
	if(!fAuthenticated)//û�н�����֤����
	return httpUnAuthorized;
	*/
	EasyProtocol req(json);

	EasyProtocolACK rsp(MSG_SC_DEVICE_LIST_ACK);
	EasyJsonValue header, body;

	header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
	header[EASY_TAG_CSEQ] = req.GetHeaderValue(EASY_TAG_CSEQ);
	header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SUCCESS_OK;
	header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SUCCESS_OK);

	OSRefTableEx* DeviceMap = QTSServerInterface::GetServer()->GetDeviceSessionMap();
	OSMutex* mutexMap = DeviceMap->GetMutex();
	OSHashMap* deviceMap = DeviceMap->GetMap();
	OSRefIt itRef;
	Json::Value* proot = rsp.GetRoot();

	{
		OSMutexLocker lock(mutexMap);
		body[EASY_TAG_DEVICE_COUNT] = DeviceMap->GetEleNumInMap();
		for (itRef = deviceMap->begin(); itRef != deviceMap->end(); ++itRef)
		{
			Json::Value value;
			strDevice* deviceInfo = static_cast<HTTPSession*>(itRef->second->GetObjectPtr())->GetDeviceInfo();
			value[EASY_TAG_SERIAL] = deviceInfo->serial_;
			value[EASY_TAG_NAME] = deviceInfo->name_;
			value[EASY_TAG_TAG] = deviceInfo->tag_;
			value[EASY_TAG_APP_TYPE] = EasyProtocol::GetAppTypeString(deviceInfo->eAppType);
			value[EASY_TAG_TERMINAL_TYPE] = EasyProtocol::GetTerminalTypeString(deviceInfo->eDeviceType);
			//����豸��EasyCamera,�򷵻��豸������Ϣ
			if (deviceInfo->eAppType == EASY_APP_TYPE_CAMERA)
			{
				value[EASY_TAG_SNAP_URL] = deviceInfo->snapJpgPath_;
			}
			(*proot)[EASY_TAG_ROOT][EASY_TAG_BODY][EASY_TAG_DEVICES].append(value);
		}
	}

	rsp.SetHead(header);
	rsp.SetBody(body);
	string msg = rsp.GetMsg();

	//������Ӧ����(HTTPͷ)
	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
	{
		if (!msg.empty())
			httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

		//Push MSG to OutputBuffer
		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream* pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);
		if (!msg.empty())
			pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
	}

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgCSGetCameraListReqRESTful(const char* queryString)
{
	/*
		if(!fAuthenticated)//û�н�����֤����
		return httpUnAuthorized;
	*/

	if (queryString == NULL)
	{
		return QTSS_BadArgument;
	}

	char decQueryString[EASY_MAX_URL_LENGTH] = { 0 };
	EasyUtil::Urldecode((unsigned char*)queryString, reinterpret_cast<unsigned char*>(decQueryString));

	QueryParamList parList(decQueryString);
	const char* device_serial = parList.DoFindCGIValueForParam(EASY_TAG_L_DEVICE);//��ȡ�豸���к�

	if (device_serial == NULL)
		return QTSS_BadArgument;

	EasyProtocolACK rsp(MSG_SC_DEVICE_INFO_ACK);
	EasyJsonValue header, body;

	header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
	header[EASY_TAG_CSEQ] = 1;

	body[EASY_TAG_SERIAL] = device_serial;

	OSRefTableEx* DeviceMap = QTSServerInterface::GetServer()->GetDeviceSessionMap();
	OSRefTableEx::OSRefEx* theDevRef = DeviceMap->Resolve(device_serial);////////////////////////////////++
	if (theDevRef == NULL)//������ָ���豸
	{
		header[EASY_TAG_ERROR_NUM] = EASY_ERROR_DEVICE_NOT_FOUND;
		header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_DEVICE_NOT_FOUND);
	}
	else//����ָ���豸�����ȡ����豸������ͷ��Ϣ
	{
		OSRefReleaserEx releaser(DeviceMap, device_serial);

		header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SUCCESS_OK;
		header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SUCCESS_OK);

		Json::Value* proot = rsp.GetRoot();
		strDevice* deviceInfo = static_cast<HTTPSession*>(theDevRef->GetObjectPtr())->GetDeviceInfo();
		if (deviceInfo->eAppType == EASY_APP_TYPE_CAMERA)
		{
			body[EASY_TAG_SNAP_URL] = deviceInfo->snapJpgPath_;
		}
		else
		{
			EasyDevicesIterator itCam;
			body[EASY_TAG_CHANNEL_COUNT] = static_cast<HTTPSession*>(theDevRef->GetObjectPtr())->GetDeviceInfo()->channelCount_;
			for (itCam = deviceInfo->channels_.begin(); itCam != deviceInfo->channels_.end(); ++itCam)
			{
				Json::Value value;
				value[EASY_TAG_CHANNEL] = itCam->second.channel_;
				value[EASY_TAG_NAME] = itCam->second.name_;
				value[EASY_TAG_STATUS] = itCam->second.status_;
				value[EASY_TAG_SNAP_URL] = itCam->second.snapJpgPath_;
				(*proot)[EASY_TAG_ROOT][EASY_TAG_BODY][EASY_TAG_CHANNELS].append(value);
			}
		}
		//DeviceMap->Release(device_serial);////////////////////////////////--
	}
	rsp.SetHead(header);
	rsp.SetBody(body);
	string msg = rsp.GetMsg();

	//������Ӧ����(HTTPͷ)
	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
	{
		if (!msg.empty())
			httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

		//��Ӧ��ɺ�Ͽ�����
		httpAck.AppendConnectionCloseHeader();

		//Push MSG to OutputBuffer
		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream* pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);
		if (!msg.empty())
			pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
	}

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ExecNetMsgCSCameraListReq(const char* json)
{
	/*
	if(!fAuthenticated)//û�н�����֤����
	return httpUnAuthorized;
	*/

	EasyProtocol      req(json);
	string strDeviceSerial = req.GetBodyValue(EASY_TAG_SERIAL);

	if (strDeviceSerial.size() <= 0)
		return QTSS_BadArgument;

	EasyProtocolACK rsp(MSG_SC_DEVICE_INFO_ACK);
	EasyJsonValue header, body;

	header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
	header[EASY_TAG_CSEQ] = req.GetHeaderValue(EASY_TAG_CSEQ);
	header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SUCCESS_OK;
	header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SUCCESS_OK);
	body[EASY_TAG_SERIAL] = strDeviceSerial;

	OSRefTableEx* DeviceMap = QTSServerInterface::GetServer()->GetDeviceSessionMap();
	OSRefTableEx::OSRefEx* theDevRef = DeviceMap->Resolve(strDeviceSerial);////////////////////////////////++
	if (theDevRef == NULL)//������ָ���豸
	{
		return EASY_ERROR_DEVICE_NOT_FOUND;//�������������Ĵ���
	}
	else//����ָ���豸�����ȡ����豸������ͷ��Ϣ
	{
		OSRefReleaserEx releaser(DeviceMap, strDeviceSerial);

		Json::Value* proot = rsp.GetRoot();
		strDevice* deviceInfo = static_cast<HTTPSession*>(theDevRef->GetObjectPtr())->GetDeviceInfo();
		if (deviceInfo->eAppType == EASY_APP_TYPE_CAMERA)
		{
			body[EASY_TAG_SNAP_URL] = deviceInfo->snapJpgPath_;
		}
		else
		{
			EasyDevices *camerasInfo = &(deviceInfo->channels_);
			EasyDevicesIterator itCam;

			body[EASY_TAG_CHANNEL_COUNT] = static_cast<HTTPSession*>(theDevRef->GetObjectPtr())->GetDeviceInfo()->channelCount_;
			for (itCam = camerasInfo->begin(); itCam != camerasInfo->end(); ++itCam)
			{
				Json::Value value;
				value[EASY_TAG_CHANNEL] = itCam->second.channel_;
				value[EASY_TAG_NAME] = itCam->second.name_;
				value[EASY_TAG_STATUS] = itCam->second.status_;
				body[EASY_TAG_SNAP_URL] = itCam->second.snapJpgPath_;
				(*proot)[EASY_TAG_ROOT][EASY_TAG_BODY][EASY_TAG_CHANNELS].append(value);
			}
		}
		//DeviceMap->Release(strDeviceSerial);////////////////////////////////--
	}
	rsp.SetHead(header);
	rsp.SetBody(body);
	string msg = rsp.GetMsg();

	//������Ӧ����(HTTPͷ)
	HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

	if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
	{
		if (!msg.empty())
			httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

		//��Ӧ��ɺ�Ͽ�����
		httpAck.AppendConnectionCloseHeader();

		//Push MSG to OutputBuffer
		char respHeader[sHeaderSize] = { 0 };
		StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
		strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

		HTTPResponseStream *pOutputStream = GetOutputStream();
		pOutputStream->Put(respHeader);
		if (!msg.empty())
			pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
	}

	return QTSS_NoErr;
}

QTSS_Error HTTPSession::ProcessRequest()//��������
{
	//OSCharArrayDeleter charArrayPathDeleter(theRequestBody);//��Ҫ����ɾ������Ϊ����ִ�ж�Σ�����������Ĵ�����Ϻ��ٽ���ɾ��

	if (fRequestBody == NULL)//��ʾû����ȷ�Ľ�������SetUpRequest����û�н��������ݲ���
		return QTSS_NoErr;

	//��Ϣ����
	QTSS_Error theErr;

	EasyProtocol protocol(fRequestBody);
	int nNetMsg = protocol.GetMessageType(), nRspMsg = MSG_SC_EXCEPTION;

	switch (nNetMsg)
	{
	case MSG_DS_REGISTER_REQ://�����豸������Ϣ,�豸���Ͱ���NVR������ͷ����������
		theErr = ExecNetMsgDSRegisterReq(fRequestBody);
		nRspMsg = MSG_SD_REGISTER_ACK;
		break;
	case MSG_CS_GET_STREAM_REQ://�ͻ��˵Ŀ�ʼ������
		theErr = ExecNetMsgCSGetStreamReq(fRequestBody);
		nRspMsg = MSG_SC_GET_STREAM_ACK;
		break;
	case MSG_DS_PUSH_STREAM_ACK://�豸�Ŀ�ʼ����Ӧ
		theErr = ExecNetMsgDSPushStreamAck(fRequestBody);
		nRspMsg = MSG_DS_PUSH_STREAM_ACK;//ע�⣬����ʵ�����ǲ�Ӧ���ٻ�Ӧ��
		break;
	case MSG_CS_FREE_STREAM_REQ://�ͻ��˵�ֱֹͣ������
		theErr = ExecNetMsgCSFreeStreamReq(fRequestBody);
		nRspMsg = MSG_SC_FREE_STREAM_ACK;
		break;
	case MSG_DS_STREAM_STOP_ACK://�豸��EasyCMS��ֹͣ������Ӧ
		theErr = ExecNetMsgDSStreamStopAck(fRequestBody);
		nRspMsg = MSG_DS_STREAM_STOP_ACK;//ע�⣬����ʵ�����ǲ���Ҫ�ڽ��л�Ӧ��
		break;
	case MSG_CS_DEVICE_LIST_REQ://�豸�б�����
		theErr = ExecNetMsgCSDeviceListReq(fRequestBody);//add
		nRspMsg = MSG_SC_DEVICE_LIST_ACK;
		break;
	case MSG_CS_DEVICE_INFO_REQ://����ͷ�б�����,�豸�ľ�����Ϣ
		theErr = ExecNetMsgCSCameraListReq(fRequestBody);//add
		nRspMsg = MSG_SC_DEVICE_INFO_ACK;
		break;
	case MSG_DS_POST_SNAP_REQ://�豸�����ϴ�
		theErr = ExecNetMsgDSPostSnapReq(fRequestBody);
		nRspMsg = MSG_SD_POST_SNAP_ACK;
		break;
	default:
		theErr = ExecNetMsgErrorReqHandler(httpNotImplemented);
		break;
	}

	//��������������Զ�������һ��Ҫ����QTSS_NoErr
	if (theErr != QTSS_NoErr)//��������ȷ��Ӧ���ǵȴ����ض���QTSS_NoErr�����ִ��󣬶Դ������ͳһ��Ӧ
	{
		EasyProtocol req(fRequestBody);
		EasyProtocolACK rsp(nRspMsg);
		EasyJsonValue header;
		header[EASY_TAG_VERSION] = EASY_PROTOCOL_VERSION;
		header[EASY_TAG_CSEQ] = req.GetHeaderValue(EASY_TAG_CSEQ);

		switch (theErr)
		{
		case QTSS_BadArgument:
			header[EASY_TAG_ERROR_NUM] = EASY_ERROR_CLIENT_BAD_REQUEST;
			header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_CLIENT_BAD_REQUEST);
			break;
		case httpUnAuthorized:
			header[EASY_TAG_ERROR_NUM] = EASY_ERROR_CLIENT_UNAUTHORIZED;
			header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_CLIENT_UNAUTHORIZED);
			break;
		case QTSS_AttrNameExists:
			header[EASY_TAG_ERROR_NUM] = EASY_ERROR_CONFLICT;
			header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_CONFLICT);
			break;
		case EASY_ERROR_DEVICE_NOT_FOUND:
			header[EASY_TAG_ERROR_NUM] = EASY_ERROR_DEVICE_NOT_FOUND;
			header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_DEVICE_NOT_FOUND);
			break;
		case EASY_ERROR_SERVICE_NOT_FOUND:
			header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SERVICE_NOT_FOUND;
			header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SERVICE_NOT_FOUND);
			break;
		case httpRequestTimeout:
			header[EASY_TAG_ERROR_NUM] = EASY_ERROR_REQUEST_TIMEOUT;
			header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_REQUEST_TIMEOUT);
			break;
		case EASY_ERROR_SERVER_INTERNAL_ERROR:
			header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SERVER_INTERNAL_ERROR;
			header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SERVER_INTERNAL_ERROR);
			break;
		case EASY_ERROR_SERVER_NOT_IMPLEMENTED:
			header[EASY_TAG_ERROR_NUM] = EASY_ERROR_SERVER_NOT_IMPLEMENTED;
			header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_SERVER_NOT_IMPLEMENTED);
			break;
		default:
			header[EASY_TAG_ERROR_NUM] = EASY_ERROR_CLIENT_BAD_REQUEST;
			header[EASY_TAG_ERROR_STRING] = EasyProtocol::GetErrorString(EASY_ERROR_CLIENT_BAD_REQUEST);
			break;
		}

		rsp.SetHead(header);
		string msg = rsp.GetMsg();
		//������Ӧ����(HTTP Header)
		HTTPRequest httpAck(&QTSServerInterface::GetServerHeader(), httpResponseType);

		if (httpAck.CreateResponseHeader(!msg.empty() ? httpOK : httpNotImplemented))
		{
			if (!msg.empty())
				httpAck.AppendContentLengthHeader(static_cast<UInt32>(msg.length()));

			char respHeader[sHeaderSize] = { 0 };
			StrPtrLen* ackPtr = httpAck.GetCompleteHTTPHeader();
			strncpy(respHeader, ackPtr->Ptr, ackPtr->Len);

			HTTPResponseStream *pOutputStream = GetOutputStream();
			pOutputStream->Put(respHeader);

			//����Ӧ������ӵ�HTTP�����������
			if (!msg.empty())
				pOutputStream->Put(const_cast<char*>(msg.data()), msg.length());
		}
	}
	return theErr;
}

int	HTTPSession::yuv2BMPImage(unsigned int width, unsigned int height, char* yuvpbuf, unsigned int* rgbsize, unsigned char* rgbdata)
{
	int nBpp = 24;
	int dwW, dwH, dwWB;

	dwW = width;
	dwH = height;
	dwWB = _WIDTHBYTES(dwW * nBpp);

	// SaveFile to BMP
	BITMAPFILEHEADER bfh = { 0, };
	bfh.bfType = 0x4D42;
	bfh.bfSize = 0;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	if (nBpp == 16)
	{
		bfh.bfOffBits += sizeof(RGBQUAD) * 3;
	}
	else
	{
		bfh.bfOffBits += sizeof(RGBQUAD) * 1;
	}

	DWORD dwWriteLength = sizeof(BITMAPFILEHEADER);
	int rgbOffset = 0;
	memcpy(rgbdata + rgbOffset, (PVOID)&bfh, dwWriteLength);
	rgbOffset += dwWriteLength;

	BITMAPINFOHEADER	bih = { 0, };
	bih.biSize = sizeof(BITMAPINFOHEADER);
	bih.biWidth = dwW;
	bih.biHeight = -(INT)dwH;
	bih.biPlanes = 1;
	bih.biBitCount = nBpp;
	bih.biCompression = (nBpp == 16) ? BI_BITFIELDS : BI_RGB;
	bih.biSizeImage = dwWB * height;
	bih.biXPelsPerMeter = 0;
	bih.biYPelsPerMeter = 0;
	bih.biClrUsed = 0;
	bih.biClrImportant = 0;

	dwWriteLength = sizeof(BITMAPINFOHEADER);

	memcpy(rgbdata + rgbOffset, (PVOID)&bih, dwWriteLength);
	rgbOffset += dwWriteLength;

	if (nBpp == 24)
	{
		DWORD rgbQuad = 0;
		dwWriteLength = sizeof(rgbQuad);
		memcpy(rgbdata + rgbOffset, (PVOID)&rgbQuad, dwWriteLength);
		rgbOffset += dwWriteLength;
	}

	dwWriteLength = dwWB * height;
	memcpy(rgbdata + rgbOffset, (PVOID)yuvpbuf, dwWriteLength);
	rgbOffset += dwWriteLength;

	if (NULL != rgbsize)	*rgbsize = rgbOffset;

	return 0;
}

QTSS_Error HTTPSession::rawData2Image(char* rawBuf, int bufSize, int codec, int width, int height)
{
	QTSS_Error	theErr = QTSS_NoErr;
#ifndef __linux__

	if (NULL != decodeParam.ffdHandle)
	{
		FFD_Deinit(&decodeParam.ffdHandle);
		decodeParam.ffdHandle = NULL;
	}

	if (NULL == decodeParam.ffdHandle)
	{
		decodeParam.codec = codec;
		decodeParam.width = width;
		decodeParam.height = height;

		FFD_Init(&decodeParam.ffdHandle);
		FFD_SetVideoDecoderParam(decodeParam.ffdHandle, width, height, codec, 3);//AV_PIX_FMT_YUVJ444P,  ///< planar YUV 4:4:4, 24bpp, full scale (JPEG), deprecated in favor of PIX_FMT_YUV444P and setting color_range
	}

	int yuvdata_size = width * height * 3;
	char* yuvdata = new char[yuvdata_size + 1];
	memset(yuvdata, 0x00, yuvdata_size + 1);

	int snapHeight = height >= SNAP_IMAGE_HEIGHT ? SNAP_IMAGE_HEIGHT : height;
	int snapWidth = height >= SNAP_IMAGE_HEIGHT ? SNAP_IMAGE_WIDTH : height * 16 / 9;

	if (FFD_DecodeVideo3(decodeParam.ffdHandle, rawBuf, bufSize, yuvdata, snapWidth, snapHeight) != 0)
	{
		theErr = QTSS_RequestFailed;
	}
	else
	{
		memset(decodeParam.imageData, 0, SNAP_SIZE);
		yuv2BMPImage(snapWidth, snapHeight, (char*)yuvdata, &decodeParam.imageSize, (unsigned char*)decodeParam.imageData);
	}

	if (NULL != yuvdata)
	{
		delete[] yuvdata;
		yuvdata = NULL;
	}

#endif
	return theErr;
}