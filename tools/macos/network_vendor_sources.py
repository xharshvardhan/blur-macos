#!/usr/bin/env python3
"""[Blur2PC] Derive native sources from Unix projects plus newer PC neutral implementations."""
import argparse,json,pathlib,xml.etree.ElementTree as ET
PROJECTS=['bdPlatform','bdCore','bdSocket','bdConnection','bdNet','bdLobby','bdPeer','bdClientServer','bdExample/bdExampleCommon','bdExample/bdExampleCommonLobby']
# This stale entry is also in the Windows project. No implementation/header or references
# exist in this source drop; UPnP itself remains included through its three actual units.
STALE={'bdNet/bdUPnP/bdUPnPListener.cpp'}
# [Blur2PC] Current project-listed callers depend on these original neutral
# class implementations, but all shipped project lists omit the key-function
# units. UBSan's valid vptr checks expose their absent RTTI at game link time.
# Keep this explicit dependency closure tied to its authoritative caller, rather
# than globbing unrelated vendor sources or fabricating type information.
IMPLEMENTATION_DEPENDENCIES={
 'bdLobby/bdTitleUtilities/bdTitleUtilities.cpp': (
  'bdLobby/bdTitleUtilities/bdOnlineUserInfo.cpp',),
 'bdLobby/bdHTTPWrapper/bdHTTPWrapper.cpp': (
  'bdLobby/bdContentStreaming/bdUploadInterceptor.cpp',
  'bdLobby/bdContentStreaming/bdDownloadInterceptor.cpp'),
}
def authorities(root):
 vendor=root/'code/vendor/demonware/2.0';result=[]
 for name in PROJECTS:
  directory=vendor/name;stem=directory.name
  result.append((directory/(stem+'-unix.vpj'),False))
  pc=directory/(stem+'-win32-vc8.vcproj')
  if not pc.is_file():pc=directory/(stem+'-win32.vcproj')
  if pc.is_file():result.append((pc,True))
 return result
def derive(root):
 vendor=root/'code/vendor/demonware/2.0';result=[];seen=set()
 for project,windows in authorities(root):
  for entry in ET.parse(project).findall('.//File' if windows else './/F'):
   name=entry.attrib.get('RelativePath' if windows else 'N','').replace('\\','/')
   if not name.endswith(('.cpp','.c')):continue
   # Unix project implementations own native OS services. PC projects contribute
   # newer platform-independent modules absent from the older Unix project list.
   if windows and '-win32.' in name:continue
   source=(project.parent/name).resolve();relative=source.relative_to(vendor).as_posix()
   if not source.is_file():
    if relative in STALE:continue
    raise RuntimeError(f'Missing authoritative vendor source: {source}')
   if relative not in seen:
    seen.add(relative);result.append(source.relative_to(root).as_posix())
 for caller,dependencies in IMPLEMENTATION_DEPENDENCIES.items():
  if caller not in seen:
   raise RuntimeError(f'Implementation dependency has no authoritative caller: {caller}')
  caller_text=(vendor/caller).read_text()
  caller_header=(vendor/caller).with_suffix('.h')
  if caller_header.is_file() and caller_header.relative_to(vendor).as_posix() in caller_text:
   caller_text+='\n'+caller_header.read_text()
  for relative in dependencies:
   source=vendor/relative;header=source.with_suffix('.h')
   if not source.is_file() or not header.is_file():
    raise RuntimeError(f'Missing original implementation/header: {relative}')
   if header.relative_to(vendor).as_posix() not in caller_text:
    raise RuntimeError(f'Caller no longer includes required class header: {relative}')
   if relative not in seen:
    seen.add(relative);result.append(source.relative_to(root).as_posix())
 return result
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--root',type=pathlib.Path,required=True);p.add_argument('--cmake',type=pathlib.Path);args=p.parse_args();sources=derive(args.root.resolve())
 if args.cmake:args.cmake.write_text('set(BLUR_MAC_DEMONWARE_SOURCES\n'+''.join('    "${BLUR_TOP}/'+s+'"\n' for s in sources)+')\nset(BLUR_MAC_DEMONWARE_PROJECTS\n'+''.join('    "'+str(p)+'"\n' for p,_ in authorities(args.root.resolve()))+')\n')
 else:print(json.dumps({'sources':sources,'count':len(sources)},indent=2))
