
#ifndef _OSREFTABLEEX_H_
#define _OSREFTABLEEX_H_
//��Ϊdarwin�����ñ�������ͬ��key���������ַ���hash�����ֲ���������һ�㡣��˿���ʹ��STL�е�map���д��档
//ע��������Ǻ������ϵģ�����Darwin�Դ���
#include<map>
#include<string>
using namespace std;

#include "OSCond.h"
#include "OSHeaders.h"
class OSRefTableEx
{
public:
	class OSRefEx
	{
	private:
		void *mp_Object;//���õĶ������������
		int m_Count;//��ǰ���ö��������ֻ��Ϊ0ʱ�������������
		OSCond  m_Cond;//to block threads waiting for this ref.
	public:
		OSRefEx() :mp_Object(NULL), m_Count(0) {}
		OSRefEx(void * pobject) :mp_Object(pobject), m_Count(0) {}
	public:
		void *GetObjectPtr() const { return mp_Object; }
		int GetRefNum() const { return m_Count; }
		OSCond *GetCondPtr() { return &m_Cond; }
		void AddRef(int num) { m_Count += num; }
	};
private:
	map<string, OSRefTableEx::OSRefEx*> m_Map;//��stringΪkey����OSRefExΪvalue
	OSMutex         m_Mutex;//�ṩ��map�Ļ������
public:
	OSRefEx *    Resolve(const string& keystring);//���ö���
	OS_Error     Release(const string& keystring);//�ͷ�����
	OS_Error     Register(const string& keystring, void* pobject);//���뵽map��
	OS_Error     UnRegister(const string& keystring);//��map���Ƴ�

	OS_Error TryUnRegister(const string& keystring);//�����Ƴ����������Ϊ0,���Ƴ�������true�����򷵻�false
public:
	int GetEleNumInMap() const { return m_Map.size(); }//���map�е�Ԫ�ظ���
	OSMutex *GetMutex() { return &m_Mutex; }//�������ṩ����ӿ�
	map<string, OSRefTableEx::OSRefEx*> *GetMap() { return &m_Map; }
};
typedef map<string, OSRefTableEx::OSRefEx*> OSHashMap;
typedef map<string, OSRefTableEx::OSRefEx*>::iterator OSRefIt;

class OSRefReleaserEx
{
public:

	OSRefReleaserEx(OSRefTableEx* inTable, const string& inKeyString) : fOSRefTable(inTable), fOSRefKey(inKeyString) {}
	~OSRefReleaserEx() { fOSRefTable->Release(fOSRefKey); }

	string GetRefKey() const { return fOSRefKey; }

private:

	OSRefTableEx*	fOSRefTable;
	string			fOSRefKey;
};

#endif _OSREFTABLEEX_H_