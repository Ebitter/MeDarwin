/*
	Copyleft (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.EasyDarwin.org
*/

#include"OSRefTableEx.h"
#include <errno.h>

OSRefTableEx::OSRefEx * OSRefTableEx::Resolve(const string& keystring)//���ö��󣬴��ڷ���ָ�룬���򷵻�NULL
{
	OSMutexLocker locker(&m_Mutex);
	if (m_Map.find(keystring) == m_Map.end())//������
		return NULL;
	m_Map[keystring]->AddRef(1);
	return m_Map[keystring];
}

OS_Error OSRefTableEx::Release(const string& keystring)//�ͷ�����
{
	OSMutexLocker locker(&m_Mutex);
	if (m_Map.find(keystring) == m_Map.end())//������
		return EPERM;
	//make sure to wakeup anyone who may be waiting for this resource to be released
	m_Map[keystring]->AddRef(-1);//��������
	m_Map[keystring]->GetCondPtr()->Signal();
	return OS_NoErr;
}

OS_Error OSRefTableEx::Register(const string& keystring, void* pobject)//���뵽map��
{
	Assert(pobject != NULL);
	if (pobject == NULL)
		return EPERM;

	OSMutexLocker locker(&m_Mutex);//����
	if (m_Map.find(keystring) != m_Map.end())//�Ѿ����֣��ܾ��ظ���key
		return EPERM;
	OSRefTableEx::OSRefEx *RefTemp = new OSRefTableEx::OSRefEx(pobject);//����value���ڴ�new����UnRegister��delete
	m_Map[keystring] = RefTemp;//���뵽map��
	return OS_NoErr;
}

OS_Error OSRefTableEx::UnRegister(const string& keystring)//��map���Ƴ����������ü���Ϊ0�������Ƴ�
{
	OSMutexLocker locker(&m_Mutex);
	if (m_Map.find(keystring) == m_Map.end())//�����ڵ�ǰkey
		return EPERM;
	//make sure that no one else is using the object
	while (m_Map[keystring]->GetRefNum() > 0)
		m_Map[keystring]->GetCondPtr()->Wait(&m_Mutex);

	delete m_Map[keystring];//�ͷ�
	m_Map.erase(keystring);//�Ƴ�
	return OS_NoErr;
}

OS_Error OSRefTableEx::TryUnRegister(const string& keystring)
{
	OSMutexLocker locker(&m_Mutex);
	if (m_Map.find(keystring) == m_Map.end())//�����ڵ�ǰkey
		return EPERM;
	if (m_Map[keystring]->GetRefNum() > 0)
		return EPERM;
	// At this point, this is guarenteed not to block, because
	// we've already checked that the refCount is low.
	delete m_Map[keystring];//�ͷ�
	m_Map.erase(keystring);//�Ƴ�
	return OS_NoErr;
}