#pragma once
#include "resource.h"
class CARPApp : public CWinApp {
public:
    BOOL InitInstance() override;
};
extern CARPApp theApp;
