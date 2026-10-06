// LayerManager.cpp: implementation of the CLayerManager class.
//
//////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "stdafx.h"
#include "LayerManager.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLayerManager::CLayerManager()
	: m_nLayerCount(0),
	mp_sListHead(NULL),
	mp_sListTail(NULL),
	m_nTop(-1)
{

}

CLayerManager::~CLayerManager()
{

}

void CLayerManager::AddLayer(CBaseLayer* pLayer)
{
	mp_aLayers[m_nLayerCount++] = pLayer;
}

CBaseLayer* CLayerManager::GetLayer(int nindex)
{
	return mp_aLayers[nindex];
}

CBaseLayer* CLayerManager::GetLayer(char* pName)
{
	for (int i = 0; i < m_nLayerCount; i++)
	{
		if (!strcmp(pName, mp_aLayers[i]->GetLayerName()))
			return mp_aLayers[i];
	}

	return NULL;
}

// 예: "NI ( *Ethernet ( *ChatApp ( *ChatDlg ) *File ( *ChatDlg ) ) )"
// 위 문자열을 토큰으로 분해한 뒤 실제 계층 포인터를 연결한다.
void CLayerManager::ConnectLayers(char* pcList)
{
	MakeList(pcList);
	LinkLayer(mp_sListHead);
	int arr;
	arr = 3;
}

// 연결 문자열을 공백 단위 토큰 리스트로 변환한다.
// strtok_s가 입력 버퍼를 변경하므로 복사본을 만들어 처리한다.
void CLayerManager::MakeList(char* pcList)
{
	// strtok_s modifies its buffer, but pcList is a string literal.
	size_t nSize = strlen(pcList) + 1;
	char* pcCopy = new char[nSize];
	strcpy_s(pcCopy, nSize, pcList);

	char* pcNext = NULL;
	for (char* pcToken = strtok_s(pcCopy, " ", &pcNext);
		pcToken;
		pcToken = strtok_s(NULL, " ", &pcNext))
	{
		AddNode(AllocNode(pcToken));
	}

	delete[] pcCopy;
}

CLayerManager::PNODE CLayerManager::AllocNode(char* pcName)
{
	PNODE node = new NODE;
	ASSERT(node);

	strcpy_s(node->token, pcName);
	node->next = NULL;

	return node;
}

void CLayerManager::AddNode(PNODE pNode)
{
	if (!mp_sListHead)
	{
		mp_sListHead = mp_sListTail = pNode;
	}
	else
	{
		mp_sListTail->next = pNode;
		mp_sListTail = pNode;
	}
}


void CLayerManager::Push(CBaseLayer* pLayer)
{
	if (m_nTop >= MAX_LAYER_NUMBER)
	{
#ifdef _DEBUG
		TRACE("The Stack is full.. so cannot run the push operation.. \n");
#endif
		return;
	}

	mp_Stack[++m_nTop] = pLayer;
}

CBaseLayer* CLayerManager::Pop()
{
	if (m_nTop < 0)
	{
#ifdef _DEBUG
		TRACE("The Stack is empty.. so cannot run the pop operation.. \n");
#endif
		return NULL;
	}

	CBaseLayer* pLayer = mp_Stack[m_nTop];
	mp_Stack[m_nTop] = NULL;
	m_nTop--;

	return pLayer;
}

CBaseLayer* CLayerManager::Top()
{
	if (m_nTop < 0)
	{
#ifdef _DEBUG
		TRACE("The Stack is empty.. so cannot run the top operation.. \n");
#endif
		return NULL;
	}

	return mp_Stack[m_nTop];
}

// 토큰을 왼쪽부터 읽으며 스택을 사용해 분기 구조를 연결한다.
// Ethernet 위에 ChatApp과 File이 동시에 연결되는 이유가 이 분기 때문이다.
void CLayerManager::LinkLayer(PNODE pNode)
{
	CBaseLayer* pLayer = NULL;

	while (pNode)
	{
		if (!pLayer)
			pLayer = GetLayer(pNode->token);
		else
		{
			if (*pNode->token == '(')
				Push(pLayer);
			else if (*pNode->token == ')')
				Pop();
			else
			{
				char cMode = *pNode->token;
				char* pcName = pNode->token + 1;

				pLayer = GetLayer(pcName);

				switch (cMode)
				{
				case '*': Top()->SetUpperUnderLayer(pLayer); break;
				case '+': Top()->SetUpperLayer(pLayer); break;
				case '-': Top()->SetUnderLayer(pLayer); break;
				}
			}
		}

		pNode = pNode->next;
	}
}

void CLayerManager::DeAllocLayer()
{
	for (int i = 0; i < this->m_nLayerCount; i++)
		delete this->mp_aLayers[i];
}
