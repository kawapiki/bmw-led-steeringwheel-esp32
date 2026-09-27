"""Create private development identity and signing key. Never runs a hardware command."""
from pathlib import Path
import secrets
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization
root=Path(__file__).resolve().parents[1]
private=root/'.private';private.mkdir(exist_ok=True)
keyfile=private/'release-signing.pem'
if not keyfile.exists():
 key=rsa.generate_private_key(public_exponent=65537,key_size=3072)
 keyfile.write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
key=serialization.load_pem_private_key(keyfile.read_bytes(),password=None)
pub=key.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.PKCS1)
(private/'release_public.h').write_text('#pragma once\nstatic const unsigned char release_public_key[] = {'+','.join(map(str,pub))+'};\n',encoding='utf-8')
pair=private/'pair_config.h'
if not pair.exists():pair.write_text('#pragma once\n#define PAIR_PASSKEY '+str(secrets.randbelow(900000)+100000)+'u\n',encoding='utf-8')
passkey=int(pair.read_text(encoding='utf-8').split('PAIR_PASSKEY ')[1].split('u')[0])
(private/'initial-nvs.csv').write_text('key,type,encoding,value\npairing,namespace,,\npasskey,data,u32,'+str(passkey)+'\n',encoding='utf-8')
print('Private development signing key and paired identity ready. Keep .private backed up securely.')
