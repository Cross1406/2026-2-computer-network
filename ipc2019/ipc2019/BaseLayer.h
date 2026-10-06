#pragma once
// BaseLayer.h: interface for the CBaseLayer class.
//
//////////////////////////////////////////////////////////////////////

#include "pch.h"
#include"stdafx.h"

#if !defined(AFX_BASELAYER_H__041C5A07_23A9_4CBC_970B_8743460A7DA9__INCLUDED_)
#define AFX_BASELAYER_H__041C5A07_23A9_4CBC_970B_8743460A7DA9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// 모든 프로토콜 계층이 공통으로 상속하는 추상 기반 클래스.
//
// 각 계층은 아래 방향 송신에는 mp_UnderLayer를 사용하고,
// 위 방향 수신에는 mp_aUpperLayer[]를 사용한다.
// 실제 연결은 CLayerManager가 프로그램 시작 시 설정한다.
class CBaseLayer
{
public:
	// LayerManager가 문자열 이름으로 계층을 찾을 때 사용한다.
	char* GetLayerName();

	CBaseLayer* GetUnderLayer();
	CBaseLayer* GetUpperLayer(int nindex);
	void			SetUnderUpperLayer(CBaseLayer* pUULayer = NULL);
	void			SetUpperUnderLayer(CBaseLayer* pUULayer = NULL);
	void			SetUnderLayer(CBaseLayer* pUnderLayer = NULL);
	void			SetUpperLayer(CBaseLayer* pUpperLayer = NULL);

	CBaseLayer(char* pName = NULL);
	virtual ~CBaseLayer();

	// param : unsigned char*	- the data of the upperlayer
	//         int				- the length of data
	// 상위 계층에서 내려온 데이터와 실제 길이를 하위 계층으로 보낸다.
	virtual	BOOL	Send(unsigned char*, int) { return FALSE; }
	// param : unsigned char*	- the data of the underlayer
	// 하위 계층에서 올라온 데이터를 검사/역캡슐화하여 상위 계층으로 전달한다.
	virtual	BOOL	Receive(unsigned char* ppayload) { return FALSE; }
	virtual BOOL Receive(unsigned char* data, int length) { return Receive(data); }
	virtual	BOOL	Receive() { return FALSE; }

protected:
	char* m_pLayerName;
	CBaseLayer* mp_UnderLayer;							// UnderLayer pointer
	CBaseLayer* mp_aUpperLayer[MAX_LAYER_NUMBER];		// UpperLayer pointer
	int				m_nUpperLayerCount;						// UpperLayer Count
};

#endif // !defined(AFX_BASELAYER_H__041C5A07_23A9_4CBC_970B_8743460A7DA9__INCLUDED_)
