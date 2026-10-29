"""Compile/link compiler repro variants on Windows. Never execute the binaries."""
import argparse,hashlib,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path(__file__).parent);a=p.parse_args()
root=a.root.resolve();results=root/'results';results.mkdir(exist_ok=True)
variants=[('base-release-ltcg','base',['/O2','/Ob3','/GL'],'/LTCG'),
          ('base-release-native','base',['/O2','/Ob3','/GL-'],'/LTCG:OFF'),
          ('base-conservative','base',['/Od','/Ob0','/GL-'],'/LTCG:OFF'),
          ('split-await-release','split-await',['/O2','/Ob3','/GL'],'/LTCG'),
          ('split-await-native','split-await',['/O2','/Ob3','/GL-'],'/LTCG:OFF')]
report=[]
for name,folder,flags,link in variants:
 out=root/'objects'/name;out.mkdir(parents=True,exist_ok=True)
 source=root/folder/'probe.cc'
 command=['cl','/nologo','/std:c++20','/EHsc','/MD','/Gw','/Zi','/bigobj','/permissive-','/DUNICODE','/D_UNICODE','/D_CRT_SECURE_NO_WARNINGS',*flags,'/I'+str(source.parent),str(source),'/Fo:'+str(out/'probe.obj'),'/Fe:'+str(out/'probe.exe'),'/Fd:'+str(out/'compile.pdb'),'windowsapp.lib','runtimeobject.lib','/link','/DEBUG',link]
 start=time.monotonic();status=None;timed_out=False
 with (results/(name+'.log')).open('w') as log:
  try: status=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=180).returncode
  except subprocess.TimeoutExpired: timed_out=True
 record={'variant':name,'compile_and_link_only':True,'executed':False,'exit_code':status,'exit_hex':None if status is None else hex(status&0xffffffff),'timed_out':timed_out,'seconds':round(time.monotonic()-start,2),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'command':command}
 report.append(record);print(json.dumps({k:v for k,v in record.items() if k!='command'}),flush=True)
 (results/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('All compiler cases completed; no generated executable was run.')
