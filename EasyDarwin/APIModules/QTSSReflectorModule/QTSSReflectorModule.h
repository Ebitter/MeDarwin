/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2008 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 */
/*
    File:       QTSSReflectorModule.h

    Contains:   QTSS API module 
                    
    
*/

#ifndef _QTSSREFLECTORMODULE_H_
#define _QTSSREFLECTORMODULE_H_

#include "QTSS.h"



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
//#include <conio.h>
//#include <WinSock2.h>

#pragma comment(lib,"ws2_32.lib")


#define  MAXDATASIZE 1500
#define  H264 96                   //��������

typedef struct
{
	unsigned char v;               //!< �汾��
	unsigned char p;			   //!< Padding bit, Padding MUST NOT be used
	unsigned char x;			   //!< Extension, MUST be zero
	unsigned char cc;       	   //!< CSRC count, normally 0 in the absence of RTP mixers 		
	unsigned char m;			   //!< Marker bit ������� m = 1 ,��Ƭ�������һ���� = 1����Ƭ�������� m = 0 ,��ϰ���׼ȷ 
	unsigned char pt;			   //!< 7 bits, �������� H264 96 
	unsigned int seq;			   //!< ����
	unsigned int timestamp;	       //!< timestamp, 27 MHz for H.264 �¼���
	unsigned int ssrc;			   //!< ���
	unsigned char *  payload;      //!< the payload including payload headers
	unsigned int paylen;		   //!< length of payload in bytes
} RTPpacket_t;

typedef struct
{
	/*  0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|V=2|P|X|  CC   |M|     PT      |       sequence number         |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                           timestamp                           |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|           synchronization source (SSRC) identifier            |
	+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	|            contributing source (CSRC) identifiers             |
	|                             ....                              |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	//intel ��cpu ��intelΪС���ֽ��򣨵Ͷ˴浽�׵�ַ�� ��������Ϊ����ֽ��򣨸߶˴浽�͵�ַ��
	/*intel ��cpu �� �߶�->csrc_len:4 -> extension:1-> padding:1 -> version:2 ->�Ͷ�
	���ڴ��д洢 ��
	��->4001���ڴ��ַ��version:2
	4002���ڴ��ַ��padding:1
	4003���ڴ��ַ��extension:1
	��->4004���ڴ��ַ��csrc_len:4

	���紫����� ��
	�߶�->version:2->padding:1->extension:1->csrc_len:4->�Ͷ�  (Ϊ��ȷ���ĵ�������ʽ)

	��������ڴ� ��
	��->4001���ڴ��ַ��version:2
	4002���ڴ��ַ��padding:1
	4003���ڴ��ַ��extension:1
	��->4004���ڴ��ַ��csrc_len:4
	�����ڴ���� ���߶�->csrc_len:4 -> extension:1-> padding:1 -> version:2 ->�Ͷ� ��
	����
	unsigned char csrc_len:4;        // expect 0
	unsigned char extension:1;       // expect 1
	unsigned char padding:1;         // expect 0
	unsigned char version:2;         // expect 2
	*/
	/* byte 0 */
	unsigned char csrc_len : 4;        /* expect 0 */
	unsigned char extension : 1;       /* expect 1, see RTP_OP below */
	unsigned char padding : 1;         /* expect 0 */
	unsigned char version : 2;         /* expect 2 */
	/* byte 1 */
	unsigned char payloadtype : 7;     /* RTP_PAYLOAD_RTSP */
	unsigned char marker : 1;          /* expect 1 */
	/* bytes 2,3 */
	unsigned int seq_no;
	/* bytes 4-7 */
	unsigned int timestamp;
	/* bytes 8-11 */
	unsigned int ssrc;              /* stream number is used here. */
} RTP_HEADER;


typedef struct
{
	unsigned char forbidden_bit;           //! Should always be FALSE
	unsigned char nal_reference_idc;       //! NALU_PRIORITY_xxxx
	unsigned char nal_unit_type;           //! NALU_TYPE_xxxx  
	unsigned int startcodeprefix_len;      //! Ԥ��
	unsigned int len;                      //! Ԥ��
	unsigned int max_size;                 //! Ԥ��
	unsigned char * buf;                   //! Ԥ��
	unsigned int lost_packets;             //! Ԥ��
} NALU_t;

/*
+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|F|NRI|  Type   |
+---------------+
*/
typedef struct
{
	//byte 0
	unsigned char TYPE : 5;
	unsigned char NRI : 2;
	unsigned char F : 1;
} NALU_HEADER; // 1 BYTE 

/*
+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|F|NRI|  Type   |
+---------------+
*/
typedef struct
{
	//byte 0
	unsigned char TYPE : 5;
	unsigned char NRI : 2;
	unsigned char F : 1;
} FU_INDICATOR; // 1 BYTE 

/*
+---------------+
|0|1|2|3|4|5|6|7|
+-+-+-+-+-+-+-+-+
|S|E|R|  Type   |
+---------------+
*/
typedef struct
{
	//byte 0
	unsigned char TYPE : 5;
	unsigned char R : 1;
	unsigned char E : 1;
	unsigned char S : 1;          //��Ƭ����һ���� S = 1 ,����= 0 ��    
} FU_HEADER;   // 1 BYTES 


int OpenBitstreamFile(char *fn);                           //�򿪱��ش洢�ļ�
void FreeNALU(NALU_t *n);                                   //�ͷ�nal ��Դ     
NALU_t *AllocNALU(int buffersize);                          //����nal ��Դ
int RtptoH264(char *bufIn, int len);         //���



extern "C"
{
    EXPORT QTSS_Error QTSSReflectorModule_Main(void* inPrivateArgs);
}



#endif //_QTSSREFLECTORMODULE_H_
