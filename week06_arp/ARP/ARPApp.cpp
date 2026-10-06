#include "pch.h"
#include "ARPApp.h"
#include "ARPDialog.h"
CARPApp theApp;
BOOL CARPApp::InitInstance() {
    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&controls);
    CWinApp::InitInstance();
    AfxEnableControlContainer();
    SetRegistryKey(_T("CBNU Computer Network Week06ARP"));
    CARPDialog dialog;
    m_pMainWnd = &dialog;
    if (dialog.DoModal() == -1)
        AfxMessageBox(_T("ARP 대화상자를 만들지 못했습니다."));
    return FALSE;
}
