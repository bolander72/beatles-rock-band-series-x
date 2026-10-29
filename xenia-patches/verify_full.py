"""Verify downloaded full-build identity, block hashes, x64 DLL exports and release imports."""
import argparse,base64,hashlib,json,struct,zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

def pe_metadata(data):
 u16=lambda n:struct.unpack_from('<H',data,n)[0]
 u32=lambda n:struct.unpack_from('<I',data,n)[0]
 assert data[:2]==b'MZ';h=u32(0x3c);assert data[h:h+4]==b'PE\0\0'
 assert u16(h+4)==0x8664,'Expected AMD64 native code'
 op=h+24;assert u16(op)==0x20b
 sections=h+24+u16(h+20)
 def offset(rva):
  for i in range(u16(h+6)):
   p=sections+i*40;va=u32(p+12);size=u32(p+16)
   if va<=rva<va+size:return u32(p+20)+rva-va
  raise ValueError('RVA outside file data')
 def string(rva):
  p=offset(rva);return data[p:data.index(b'\0',p)].decode('ascii')
 imports=[];symbols={};rva=u32(op+120)
 if rva:
  p=offset(rva)
  while any(data[p:p+20]):
   module=string(u32(p+12));imports.append(module);symbols[module]=[]
   thunk=offset(u32(p) or u32(p+16))
   while True:
    value=struct.unpack_from('<Q',data,thunk)[0];thunk+=8
    if not value:break
    symbols[module].append('#'+str(value&65535) if value&(1<<63) else string(value+2))
   p+=20
 exports={};rva=u32(op+112)
 if rva:
  p=offset(rva);base=u32(p+16);names=offset(u32(p+32));ords=offset(u32(p+36))
  for i in range(u32(p+24)):exports[string(u32(names+4*i))]=base+u16(ords+2*i)
 return {'machine':'AMD64','imports':imports,'import_symbols':symbols,'exports':exports}

def verify_runtime(images,runtime):
 with zipfile.ZipFile(runtime) as z:
  available={Path(name).name.lower():name for name in z.namelist() if name.lower().endswith('.dll')}
  checked=0
  for image in images:
   for module,names in image['import_symbols'].items():
    if module.lower() not in available:continue
    exports=pe_metadata(z.read(available[module.lower()]))['exports']
    missing=[name for name in names if name not in exports and not name.startswith('#')]
    assert not missing,(module,missing)
    checked+=len(names)
 return {'package':runtime.name,'verified_imported_functions':checked}

def verify(package,runtime=None,version='0.6.0.6',base_package=None):
 folder=package.parent
 entries=[line.split(maxsplit=1) for line in (folder/'SHA256SUMS.txt').read_text(encoding='utf-8-sig').splitlines() if line.strip()]
 expected=next(digest.lower() for digest,name in entries if name.strip().lstrip('*')==package.name)
 actual=hashlib.sha256(package.read_bytes()).hexdigest();assert actual==expected,'Package checksum mismatch'
 with zipfile.ZipFile(package) as z:
  assert z.testzip() is None,'ZIP CRC mismatch'
  ns={'a':'http://schemas.microsoft.com/appx/manifest/foundation/windows10'}
  manifest=ET.fromstring(z.read('AppxManifest.xml'));identity=manifest.find('a:Identity',ns).attrib
  assert identity['Name']=='XeniaRiffmasterLab' and identity['Publisher']=='CN=XeniaRiffmasterLab'
  assert identity['Version']==version and identity['ProcessorArchitecture']=='x64'
  signature=z.read('AppxSignature.p7x');assert signature[:4]==b'PKCX' and len(signature)>1000
  bm=ET.fromstring(z.read('AppxBlockMap.xml'));checked=0
  for entry in bm:
   name=entry.attrib['Name'].replace('\\','/');data=z.read(name);assert len(data)==int(entry.attrib['Size'])
   blocks=[b for b in entry if b.tag=='{http://schemas.microsoft.com/appx/2010/blockmap}Block']
   assert len(blocks)==(len(data)+65535)//65536 or (not data and len(blocks)==1),name
   for whole in entry.findall('{http://schemas.microsoft.com/appx/2021/blockmap}FileHash'):
    assert hashlib.sha256(data).digest()==base64.b64decode(whole.attrib['Hash']),name
   for i,block in enumerate(blocks):
    digest=hashlib.sha256(data[i*65536:(i+1)*65536]).digest()
    assert digest==base64.b64decode(block.attrib['Hash']),(name,i)
    checked+=1
  dll=z.read('riffinput.dll');exe=z.read('xeniacanary.exe')
  dm=pe_metadata(dll);em=pe_metadata(exe)
  for name in ['XInputGetState','XInputGetCapabilities','XInputSetState','XInputGetKeystroke','XInputEnable','XInputGetStateEx']:assert name in dm['exports']
  assert dm['exports']['XInputGetStateEx']==100
  for marker in [b'layout guitar=%d native=',b'state buttons=%04X whammy=%d tilt=%d']:assert marker in dll
  assert 'riffinput.dll'.encode('utf-16le') in exe
  assert not any(n.lower()=='gameinput.dll' for n in z.namelist()),'Use the Xbox OS GameInput DLL'
  for meta in [dm,em]:
   assert not any(n.lower() in ['msvcp140d.dll','vcruntime140d.dll','ucrtbased.dll','msvcp140d_app.dll','vcruntime140d_app.dll'] for n in meta['imports']),'Debug CRT dependency in release package'
  dependencies=[dict(e.attrib) for e in manifest.findall('a:Dependencies/a:PackageDependency',ns)]
  unchanged=None
  if base_package:
   excluded={'AppxManifest.xml','riffinput.dll','AppxBlockMap.xml','AppxSignature.p7x','[Content_Types].xml','AppxMetadata/CodeIntegrity.cat'}
   with zipfile.ZipFile(base_package) as previous:
    old_names={n for n in previous.namelist() if not n.endswith('/') and n not in excluded}
    new_names={n for n in z.namelist() if not n.endswith('/') and n not in excluded}
    assert old_names==new_names,'Unexpected payload added or removed'
    for name in old_names:assert previous.read(name)==z.read(name),('Unexpected payload change',name)
    unchanged={'base':base_package.name,'identical_payload_files':len(old_names),'emulator_sha256':hashlib.sha256(exe).hexdigest()}
   assert b'settings tilt_gain_percent=' in dll,'Missing configurable tilt adapter'
 runtime_result=verify_runtime([dm,em],runtime) if runtime else None
 return {'unchanged_payload':unchanged,'runtime_check':runtime_result,'package':package.name,'sha256':actual,'identity':identity,'verified_uncompressed_blocks':checked,'signature_container':'PKCX present; signed by the Windows build (device trust remains a deployment check)','adapter':dm,'emulator_release_imports':em['imports'],'dependencies':dependencies,'console_deployed':False,'song_restart_verified':False,'physical_input_timing_verified':False}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('package',type=Path);p.add_argument('--output',type=Path);p.add_argument('--runtime',type=Path);p.add_argument('--version',default='0.6.0.6');p.add_argument('--base',type=Path);a=p.parse_args();result=verify(a.package,a.runtime,a.version,a.base);s=json.dumps(result,indent=2)+'\n'
 if a.output:
  a.output.write_text(s)
  print(json.dumps({k:result[k] for k in ['package','sha256','identity','verified_uncompressed_blocks','runtime_check','console_deployed']},indent=2))
 else:print(s)
