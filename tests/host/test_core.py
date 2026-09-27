"""Behavior tests against the same portable C used by the firmware."""
import ctypes as C
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / ".test-build"
OUT.mkdir(exist_ok=True)
compiler = os.environ.get("HOST_CC", str(OUT/"zig/ziglang/zig.exe"))
subprocess.run([compiler, "cc", "-shared", "-O1", "-DCORE_HOST_TEST",
    "-I"+str(ROOT/"components/wheel_core/include"),
    str(ROOT/"components/wheel_core/wheel_core.c"), "-o", str(OUT/"wheel_core.dll")], check=True)
lib = C.CDLL(str(OUT/"wheel_core.dll"))
lib.recovery_validate.argtypes = [C.c_void_p, C.c_size_t, C.c_bool, C.c_bool, C.c_bool]
lib.recovery_validate.restype = C.c_int
class UpdateGate(C.Structure):
    _fields_ = [(x,C.c_bool) for x in ("standalone","online","valid","authenticated","fresh")] + [
        ("confirmed_release",C.c_uint32),("requested_release",C.c_uint32),
        ("confirmed_transaction",C.c_uint64),("requested_transaction",C.c_uint64),
        ("confirmed_digest",C.c_uint8*32),("requested_digest",C.c_uint8*32)]
lib.update_wheel_allowed.argtypes = [C.POINTER(UpdateGate)]
lib.update_wheel_allowed.restype = C.c_bool

def valid_gate():
    g=UpdateGate(False,True,True,True,True,2,2,42,42)
    for i in range(32):
        g.confirmed_digest[i]=g.requested_digest[i]=0xab
    return g

lib.rpm_mask.argtypes = [C.c_uint32]
lib.rpm_mask.restype = C.c_uint32

class RecoveryTests(unittest.TestCase):
    def test_v1_golden_hello(self):
        # magic/version/opcode, length/flags, session=1, request=2, transaction=3
        frame=bytes.fromhex("e9 90 01 01 00 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00 00 00 00 00")
        self.assertEqual(lib.recovery_validate(frame,len(frame),True,True,True),0)
    def test_credentials_require_both_encryption_and_bond(self):
        frame=bytes.fromhex("e9 90 01 03 0b 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00 00 00 00 00")+b"\x01\x08Apassword"
        self.assertEqual(lib.recovery_validate(frame,len(frame),True,True,True),0)
        self.assertEqual(lib.recovery_validate(frame,len(frame),False,True,True),-3)
        self.assertEqual(lib.recovery_validate(frame,len(frame),True,False,True),-3)
    def test_truncated_and_unknown_version(self):
        frame=bytearray.fromhex("e9 90 01 01 00 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00 00 00 00 00")
        self.assertEqual(lib.recovery_validate(bytes(frame),23,True,True,True),-1)
        frame[2]=2
        self.assertEqual(lib.recovery_validate(bytes(frame),24,True,True,True),-2)
    def test_wrong_lengths_and_flags(self):
        frame=bytearray.fromhex("e9 90 01 01 00 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00 00 00 00 00")
        frame[4]=1
        self.assertEqual(lib.recovery_validate(bytes(frame),24,True,True,True),-1)
        frame[4]=0; frame[6]=1
        self.assertEqual(lib.recovery_validate(bytes(frame),24,True,True,True),-2)
    def test_short_password_rejected(self):
        frame=bytes.fromhex("e9 90 01 03 04 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00 00 00 00 00")+b"\x01\x01Ax"
        self.assertEqual(lib.recovery_validate(frame,len(frame),True,True,True),-4)
    def test_credentials_reject_untrusted_bonded_peer(self):
        frame=bytes.fromhex("e9 90 01 03 0b 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00 00 00 00 00")+b"\x01\x08Apassword"
        self.assertEqual(lib.recovery_validate(frame,len(frame),True,True,False),-3)
    def test_null_and_oversized_frame(self):
        self.assertEqual(lib.recovery_validate(None,24,True,True,True),-1)
        self.assertEqual(lib.recovery_validate(bytes(153),153,True,True,True),-1)
    def test_zero_target_release_rejected(self):
        g=valid_gate();g.requested_release=0;g.standalone=True
        self.assertFalse(lib.update_wheel_allowed(C.byref(g)))
    def test_nonprintable_passphrase_rejected(self):
        frame=bytes.fromhex("e9 90 01 03 0b 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00 00 00 00 00")+b"\x01\x08Apas\x00word"
        self.assertEqual(lib.recovery_validate(frame,len(frame),True,True,True),-4)
    def test_gateway_first_and_matching_release(self):
        g=valid_gate()
        self.assertTrue(lib.update_wheel_allowed(C.byref(g)))
        for field,value in (("online",False),("valid",False),("confirmed_release",1)):
            g=valid_gate();setattr(g,field,value)
            self.assertFalse(lib.update_wheel_allowed(C.byref(g)),field)
        g=valid_gate();g.standalone=True;g.online=False
        self.assertTrue(lib.update_wheel_allowed(C.byref(g)))
    def test_gateway_confirmation_is_transaction_bound_and_fresh(self):
        for field,value in (("authenticated",False),("fresh",False),
                            ("confirmed_transaction",41),("requested_transaction",0)):
            g=valid_gate();setattr(g,field,value)
            self.assertFalse(lib.update_wheel_allowed(C.byref(g)),field)
    def test_gateway_digest_must_match(self):
        g=valid_gate();g.confirmed_digest[0]=0
        self.assertFalse(lib.update_wheel_allowed(C.byref(g)))
        g=valid_gate()
        for i in range(32):g.confirmed_digest[i]=g.requested_digest[i]=0
        self.assertFalse(lib.update_wheel_allowed(C.byref(g)))
    def test_rpm_never_uses_button_pixel(self):
        self.assertEqual(lib.rpm_mask(800),0)
        self.assertEqual(lib.rpm_mask(2000),0)
        self.assertEqual(lib.rpm_mask(6500),0x7fffff)
        self.assertEqual(lib.rpm_mask(999999),0x7fffff)

if __name__=="__main__":
    unittest.main()
