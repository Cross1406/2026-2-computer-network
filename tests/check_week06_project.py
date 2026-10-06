"""Check a standalone weekly VS project: local sources, UI bindings and identity."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
root = Path(__file__).resolve().parents[1]
source = root/'week06_arp/ARP'
ns = {'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
project = ET.parse(source/'Week06ARP.vcxproj')
ET.parse(source/'Week06ARP.vcxproj.filters')
for node in project.getroot().iter():
    if node.tag.rsplit('}',1)[-1] in {'ClInclude','ClCompile','ResourceCompile','Image'} and 'Include' in node.attrib:
        assert (source/node.attrib['Include'].replace('\\','/')).is_file(), node.attrib['Include']
old = ET.parse(root/'ipc2019/ipc2019/ipc2019.vcxproj')
assert project.find('.//m:ProjectGuid',ns).text != old.find('.//m:ProjectGuid',ns).text
solution = (root/'week06_arp/Week06ARP.sln').read_text(encoding='utf-8-sig')
assert 'ARP\\Week06ARP.vcxproj' in solution
rc = (source/'ARP.rc').read_text()
ui = (source/'ARPDialog.cpp').read_text()
ids = (source/'resource.h').read_text()
for name in set(re.findall(r'IDC_\w+',ui)):
    assert name in rc and re.search(r'#define\s+'+name+r'\s+\d+',ids), name
for file in list(source.glob('*.h'))+list(source.glob('*.cpp')):
    content = file.read_text(encoding='utf-8-sig')
    if file.suffix == '.cpp':
        includes = re.findall(r'#include\s+"([^"]+)"',content)
        assert includes and includes[0] == 'pch.h', f'{file.name}: MSVC PCH must be the first include'
    for header in re.findall(r'#include\s+"([^"]+)"',content):
        assert header=='afxdialogex.h' or (source/header).is_file(), (file.name,header)
    assert 'ChatAppLayer' not in content and 'FileLayer' not in content, file
assert 'ether proto 0x0806' in (source/'NILayer.cpp').read_text()
assert 'ether proto 0x2080' not in (source/'NILayer.cpp').read_text()
print('Week06 standalone solution, sources, resources and ARP-only dependencies passed')
