"""Release envelope authenticity and target checks, using actual built images."""
import os
import hashlib,struct,subprocess,tempfile,unittest,sys
from pathlib import Path
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import padding,rsa
from cryptography.exceptions import InvalidSignature
ROOT=Path(__file__).resolve().parents[2]
class ReleaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory=tempfile.TemporaryDirectory()
        cls.output=Path(cls.directory.name)
        subprocess.run([sys.executable,str(ROOT/"tools/release/package.py"),"--release","2","--wheel",str(ROOT/"firmware/wheel/build/wheel_demo.bin"),"--gateway",str(ROOT/"firmware/gateway/build/gateway_demo.bin"),"--output",str(cls.output)],check=True)
        cls.data=(cls.output/"manifest.bin").read_bytes()
        cls.public=serialization.load_pem_private_key((ROOT/".private/release-signing.pem").read_bytes(),None).public_key()
    @classmethod
    def tearDownClass(cls):cls.directory.cleanup()
    def test_signature_and_actual_hashes(self):
        self.assertEqual(len(self.data),480)
        self.public.verify(self.data[96:],self.data[:96],padding.PKCS1v15(),hashes.SHA256())
        for name,offset in (("wheel",16),("gateway",48)):
            self.assertEqual(hashlib.sha256((self.output/(name+".bin")).read_bytes()).digest(),self.data[offset:offset+32])
    def test_future_app_major_signature_and_layout(self):
        import ctypes as C
        payload=bytearray(self.data[:96]);struct.pack_into('<I',payload,92,2)
        key=serialization.load_pem_private_key((ROOT/'.private/release-signing.pem').read_bytes(),None)
        signature=key.sign(payload,padding.PKCS1v15(),hashes.SHA256())
        self.public.verify(signature,payload,padding.PKCS1v15(),hashes.SHA256())
        lib=C.CDLL(str(Path(os.environ.get('HOST_TEST_OUT',str(ROOT/'.test-build')))/'wheel_core.dll'));lib.manifest_layout_valid.argtypes=[C.c_void_p,C.c_size_t];lib.manifest_layout_valid.restype=C.c_bool
        self.assertTrue(lib.manifest_layout_valid(bytes(payload)+signature,480))
        struct.pack_into('<I',payload,88,2)
        self.assertFalse(lib.manifest_layout_valid(bytes(payload)+signature,480))
    def test_manifest_tamper(self):
        changed=bytearray(self.data[:96]);changed[4]^=1
        with self.assertRaises(InvalidSignature):self.public.verify(self.data[96:],changed,padding.PKCS1v15(),hashes.SHA256())
    def test_wrong_key(self):
        wrong=rsa.generate_private_key(public_exponent=65537,key_size=3072).public_key()
        with self.assertRaises(InvalidSignature):wrong.verify(self.data[96:],self.data[:96],padding.PKCS1v15(),hashes.SHA256())
    def test_binary_tamper(self):
        data=bytearray((self.output/"wheel.bin").read_bytes());data[-1]^=1
        self.assertNotEqual(hashlib.sha256(data).digest(),self.data[16:48])
    def test_same_project_wrong_board_rejected(self):
        image=bytearray((self.output/'wheel.bin').read_bytes())
        marker=b'BMWDEMO-ID:wheel_cvs8161:p1:c1:r1:END'
        offset=image.index(marker);image[offset+11]^=1
        wrong=self.output/'wrong-board.bin';wrong.write_bytes(image)
        result=subprocess.run([sys.executable,str(ROOT/'tools/release/package.py'),'--release','3','--wheel',str(wrong),'--gateway',str(self.output/'gateway.bin'),'--output',str(self.output/'bad-board')],capture_output=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn(b'immutable board',result.stderr)
    def test_wrong_target_rejected(self):
        result=subprocess.run([sys.executable,str(ROOT/"tools/release/package.py"),"--release","3","--wheel",str(ROOT/"firmware/gateway/build/gateway_demo.bin"),"--gateway",str(ROOT/"firmware/wheel/build/wheel_demo.bin"),"--output",str(self.output/"bad")],capture_output=True)
        self.assertNotEqual(result.returncode,0)
if __name__=="__main__":unittest.main()
