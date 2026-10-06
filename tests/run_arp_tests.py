#!/usr/bin/env python3
"""Run portable protocol + actual Ethernet/ARP sources with minimal MFC test shim."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
compiler = os.environ.get('CXX', 'g++')
flags = ['-std=c++14', '-g', '-Wall', '-Wextra', '-Wno-unused-parameter',
         '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-pthread']
shim = r'''
#pragma once
#include <cstring>
#include <cstdint>
#include <mutex>
using BOOL = int;
constexpr BOOL TRUE = 1, FALSE = 0;
#define MAX_LAYER_NUMBER 255
#define ETHER_MAX_DATA_SIZE 1500
#define ETHER_HEADER_SIZE 14
#define UNREFERENCED_PARAMETER(p) (void)(p)
extern std::uint64_t testClock;
inline std::uint64_t GetTickCount64() { return testClock; }
class CCriticalSection { public: std::recursive_mutex mutex; };
class CSingleLock {
    std::unique_lock<std::recursive_mutex> lock;
public:
    CSingleLock(CCriticalSection* section, BOOL) : lock(section->mutex) {}
};
'''
for directory, layer_test in [('ipc2019/ipc2019','arp_layers_test.cpp'),
                              ('week06_arp/ARP','week06_layers_test.cpp')]:
    source = root / directory
    print(f'Testing {directory}', flush=True)
    with tempfile.TemporaryDirectory(prefix='arp-tests-') as tmp:
        work = Path(tmp)
        for name in ['BaseLayer.h','BaseLayer.cpp','EthernetLayer.h','EthernetLayer.cpp',
                     'ARPLayer.h','ARPLayer.cpp','ARPProtocol.h']:
            shutil.copy2(source/name, work/name)
        for name in ['pch.h','stdafx.h','afxmt.h','ipc2019.h','ARPApp.h']:
            (work/name).write_text(shim if name=='pch.h' else '#pragma once\n#include "pch.h"\n')
        protocol = work/'protocol'
        subprocess.run([compiler,*flags,'-I',str(work),str(root/'tests/arp_protocol_test.cpp'),'-o',str(protocol)],check=True)
        subprocess.run([str(protocol)],check=True)
        layers = work/'layers'
        subprocess.run([compiler,*flags,'-I',str(work),str(root/'tests'/layer_test),
                        *(str(work/name) for name in ['BaseLayer.cpp','EthernetLayer.cpp','ARPLayer.cpp']),
                        '-o',str(layers)],check=True)
        subprocess.run([str(layers)],check=True)
subprocess.run(['python3',str(root/'tests/check_week06_project.py')],check=True)
print('All ARP tests passed (ASan + UBSan). Windows MFC/Npcap live test is separate.')
