"""Package an authenticated two-image release; never publish automatically."""
import argparse,struct,hashlib,json
from pathlib import Path
from cryptography.hazmat.primitives import serialization,hashes
from cryptography.hazmat.primitives.asymmetric import padding
p=argparse.ArgumentParser();p.add_argument('--release',type=int,required=True);p.add_argument('--wheel',type=Path,required=True);p.add_argument('--gateway',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument("--app-major",type=int,default=1)
a=p.parse_args();assert 0<a.release<2**32 and 0<a.app_major<2**32
root=Path(__file__).resolve().parents[2];key=serialization.load_pem_private_key((root/'.private/release-signing.pem').read_bytes(),None)
w=a.wheel.read_bytes();g=a.gateway.read_bytes()
assert len(w)<=0x600000 and len(g)<=0x1e0000 and w[0]==g[0]==0xe9
for data,name,chip in ((w,'wheel_demo',9),(g,'gateway_demo',0)):
 assert struct.unpack_from('<H',data,12)[0]==chip,'Wrong target chip'
 assert struct.unpack_from('<I',data,32)[0]==0xabcd5432,'Missing app descriptor'
 assert data[80:112].rstrip(b'\0').decode()==name,'Wrong application project'
 marker=('BMWDEMO-ID:'+('wheel_cvs8161' if chip==9 else 'lilygo_xy32_v1_1')+':p1:c1:r1:END').encode().ljust(64,b'\0')
 assert data.count(marker)==1,'Wrong immutable board/config/partition identity'
payload=struct.pack('<4sIII',b'BMW1',a.release,len(w),len(g))+hashlib.sha256(w).digest()+hashlib.sha256(g).digest()+struct.pack('<IIII',1,1,1,a.app_major)
assert len(payload)==96
signed=payload+key.sign(payload,padding.PKCS1v15(),hashes.SHA256())
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'manifest.bin').write_bytes(signed);(a.output/'wheel.bin').write_bytes(w);(a.output/'gateway.bin').write_bytes(g)
(a.output/'manifest.json').write_text(json.dumps({'release':a.release,'tag':f'system-r{a.release}','schema':1,'wheel_sha256':hashlib.sha256(w).hexdigest(),'gateway_sha256':hashlib.sha256(g).hexdigest(),'signature':'RSA3072 PKCS1v1.5 SHA256 over first96 manifest.bin bytes'},indent=2),encoding='utf-8')
print('Review and publish assets together to tag system-r'+str(a.release))
