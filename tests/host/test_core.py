"""Behavior tests against the same portable C used by the firmware."""
import ctypes as C
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(os.environ.get("HOST_TEST_OUT",str(ROOT / ".test-build")))
OUT.mkdir(parents=True,exist_ok=True)
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
    def test_rpm_mask_contains_23_logical_pixels(self):
        self.assertEqual(lib.rpm_mask(800),0)
        self.assertEqual(lib.rpm_mask(2000),0)
        self.assertEqual(lib.rpm_mask(6500),0x7fffff)
        self.assertEqual(lib.rpm_mask(999999),0x7fffff)


class Assembly(C.Structure):
    _fields_=[("data",C.c_uint8*152),("size",C.c_size_t),("expected",C.c_size_t),("started",C.c_uint64)]
lib.recovery_fragment.argtypes=[C.POINTER(Assembly),C.c_void_p,C.c_size_t,C.c_uint64]
lib.recovery_fragment.restype=C.c_int
lib.manifest_layout_valid.argtypes=[C.c_void_p,C.c_size_t]
lib.manifest_layout_valid.restype=C.c_bool
class TransportTests(unittest.TestCase):
    def test_mtu23_complete_152_bytes(self):
        state=Assembly();data=bytes(range(152))
        for off in range(0,152,18):
            frame=bytes([off,152])+data[off:off+18]
            result=lib.recovery_fragment(C.byref(state),frame,len(frame),10+off)
            self.assertEqual(result,1 if off+18>=152 else 0)
        self.assertEqual(bytes(state.data),data)
    def test_fragment_overflow_and_timeout(self):
        state=Assembly();first=b'\x00\x18'+bytes(18)
        self.assertEqual(lib.recovery_fragment(C.byref(state),first,len(first),10),0)
        final=b'\x12\x18'+bytes(6)
        self.assertEqual(lib.recovery_fragment(C.byref(state),final,len(final),3011),-1)
        self.assertEqual(state.size,0)
        for frame in (b'',b'\x00\xffx',b'\x17\x18xx',bytes(21)):
            self.assertEqual(lib.recovery_fragment(C.byref(state),frame,len(frame),0),-1)
    def test_out_of_order_fragment_zeroizes(self):
        state=Assembly();first=b'\x00\x18'+bytes([42])*18
        lib.recovery_fragment(C.byref(state),first,len(first),1)
        bad=b'\x11\x18'+bytes(6)
        self.assertEqual(lib.recovery_fragment(C.byref(state),bad,len(bad),2),-1)
        self.assertEqual(bytes(state.data),bytes(152))
    def test_manifest_layout_rejects_targets_and_sizes(self):
        import struct
        data=bytearray(struct.pack('<4sIII',b'BMW1',2,1000,1000)+bytes([1])*64+struct.pack('<IIII',1,1,1,1)+bytes(384))
        self.assertTrue(lib.manifest_layout_valid(bytes(data),len(data)))
        for off,value in ((4,0),(8,0x600001),(12,0x1e0001),(80,2)):
            altered=data[:];struct.pack_into('<I',altered,off,value)
            self.assertFalse(lib.manifest_layout_valid(bytes(altered),len(altered)))
        self.assertFalse(lib.manifest_layout_valid(bytes(data),479))
        future=data[:];struct.pack_into("<I",future,92,2)
        self.assertTrue(lib.manifest_layout_valid(bytes(future),480))
        struct.pack_into("<I",future,88,2)
        self.assertFalse(lib.manifest_layout_valid(bytes(future),480))
    def test_prepare_size_and_embedded_nul(self):
        header=bytearray.fromhex('e99001040000000001000000020000000300000000000000')
        self.assertLess(lib.recovery_validate(bytes(header),24,True,True,True),0)
        header[3]=3;header[4]=11
        frame=bytes(header)+b'\x01\x08\x00password'
        self.assertLess(lib.recovery_validate(frame,len(frame),True,True,True),0)


class Journal(C.Structure):
    _fields_=[('schema',C.c_uint32),('phase',C.c_uint32),('release',C.c_uint32),('size',C.c_uint32),('tx',C.c_uint64),('hash',C.c_uint8*32),('address',C.c_uint32)]
class Session(C.Structure):
    _fields_=[('nonce',C.c_uint32),('request',C.c_uint32),('established',C.c_bool)]
for name,args,result in [
 ('update_prepare',[C.POINTER(Journal),C.c_uint32,C.c_uint64,C.c_void_p,C.c_uint32],C.c_int),
 ('update_reconcile_phase',[C.c_uint32,C.c_bool,C.c_bool],C.c_uint32),
 ('update_coordinator_action',[C.c_uint32,C.c_bool,C.c_bool],C.c_int),
 ('recovery_dispatch_accept',[C.POINTER(Session),C.c_void_p,C.c_size_t,C.c_uint32],C.c_int),
 ('recovery_auth_expired',[C.c_bool,C.c_bool,C.c_uint64,C.c_uint64],C.c_bool),
 ('recovery_health_ready',[C.c_uint64,C.c_uint64,C.c_uint32,C.c_uint64,C.c_uint32],C.c_bool),
 ('telemetry_is_fresh',[C.c_bool,C.c_bool,C.c_uint64,C.c_uint64],C.c_bool),
 ('candidate_matches',[C.c_uint32,C.c_void_p,C.c_uint32,C.c_void_p],C.c_bool),
 ('update_progress_percent',[C.c_uint32,C.c_uint32],C.c_uint),
 ('portal_request_accept',[C.c_uint32,C.c_uint32,C.c_bool],C.c_int)]:
    fn=getattr(lib,name);fn.argtypes=args;fn.restype=result
class RuntimePolicyTests(unittest.TestCase):
    def test_journal_reset_failure_retry_and_valid_gate(self):
        j=Journal(); digest=b'A'*32
        self.assertEqual(lib.update_prepare(C.byref(j),2,10,digest,1),0)
        self.assertEqual(j.phase,1)
        self.assertEqual(lib.update_prepare(C.byref(j),2,10,digest,1),0)
        j.phase=2
        self.assertEqual(lib.update_coordinator_action(j.phase,True,True),2)
        j.phase=lib.update_reconcile_phase(j.phase,False,False)
        self.assertEqual(j.phase,5)
        self.assertEqual(lib.update_coordinator_action(j.phase,True,False),0)
        self.assertNotEqual(lib.update_prepare(C.byref(j),2,10,digest,1),0)
        self.assertEqual(lib.update_prepare(C.byref(j),2,11,digest,1),0)
        j.phase=3
        self.assertEqual(lib.update_reconcile_phase(3,True,True),3)
        self.assertEqual(lib.update_reconcile_phase(3,False,False),5)
        j.phase=lib.update_reconcile_phase(3,False,True)
        self.assertEqual(j.phase,4)
        self.assertEqual(lib.update_prepare(C.byref(j),2,12,digest,2),0)
        self.assertEqual(lib.update_coordinator_action(j.phase,True,False),3)
        self.assertEqual(lib.update_coordinator_action(j.phase,False,False),0)
    def test_both_role_auth_deadlines(self):
        for role in ('wheel','gateway'):
            with self.subTest(role=role):
                self.assertFalse(lib.recovery_auth_expired(True,False,100,15099))
                self.assertTrue(lib.recovery_auth_expired(True,False,100,15100))
                self.assertFalse(lib.recovery_auth_expired(True,True,100,20000))
    def test_boot_requires_both_actual_runtime_heartbeats(self):
        self.assertTrue(lib.recovery_health_ready(4000,3950,30,3900,12))
        self.assertFalse(lib.recovery_health_ready(4000,100,1,3900,12))
        self.assertFalse(lib.recovery_health_ready(4000,3950,30,100,1))
        self.assertFalse(lib.recovery_health_ready(1000,950,30,900,12))
    def test_dispatch_sessions_replay_and_download_cancel(self):
        import struct
        g=Session()
        def send(session,request,op=1,nonce=101):
            f=struct.pack('<BBBBHHIIQ',0xe9,0x90,1,op,0,0,session,request,88)
            return lib.recovery_dispatch_accept(C.byref(g),f,len(f),nonce)
        self.assertNotEqual(send(101,1,6),0)
        self.assertEqual(send(101,2),0)
        self.assertNotEqual(send(202,3),0)
        self.assertNotEqual(send(101,2),0)
        self.assertEqual(send(101,4,6),0)
        self.assertNotEqual(send(101,3,6),0)
        self.assertEqual(send(202,1,nonce=202),0)
        self.assertNotEqual(send(101,5,6,nonce=202),0)
        self.assertEqual(send(202,2,6,nonce=202),0)
    def test_telemetry_age_disconnect_and_candidate_identity(self):
        self.assertTrue(lib.telemetry_is_fresh(True,True,1000,3500))
        for values in [(True,True,1000,3501),(False,True,1000,1001),(True,False,1000,1001),(True,True,0,1001)]:
            self.assertFalse(lib.telemetry_is_fresh(*values))
        self.assertTrue(lib.candidate_matches(3,b'A'*32,3,b'A'*32))
        self.assertFalse(lib.candidate_matches(3,b'A'*32,4,b'A'*32))
        self.assertFalse(lib.candidate_matches(3,b'A'*32,3,b'B'*32))
    def test_progress_and_portal_idempotence(self):
        self.assertEqual([lib.update_progress_percent(a,b) for a,b in [(0,0),(25,100),(100,100),(101,100)]],[0,25,100,100])
        self.assertEqual(lib.portal_request_accept(8,8,True),0)
        self.assertEqual(lib.portal_request_accept(8,8,False),-1)
        self.assertEqual(lib.portal_request_accept(8,7,True),-1)
        self.assertEqual(lib.portal_request_accept(8,9,False),1)
    def test_idle_fragment_expiry_zeroizes_credentials(self):
        lib.recovery_fragment_expire.argtypes=[C.POINTER(Assembly),C.c_uint64]
        lib.recovery_fragment_expire.restype=C.c_bool
        state=Assembly();state.size=8;state.expected=40;state.started=100;state.data[0]=99
        self.assertFalse(lib.recovery_fragment_expire(C.byref(state),3100))
        self.assertTrue(lib.recovery_fragment_expire(C.byref(state),3101))
        self.assertEqual(bytes(state),bytes(C.sizeof(state)))
    def test_sensor_configuration_write_faults_cannot_be_valid(self):
        Write=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.c_uint8)
        Read=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint8,C.POINTER(C.c_uint8))
        Delay=C.CFUNCTYPE(None,C.c_void_p,C.c_uint)
        lib.sensor_configure.argtypes=[C.c_void_p,Write,Read,Delay];lib.sensor_configure.restype=C.c_bool
        for failed in (None,(0x3d,0),(0x3d,12),(0x3b,0)):
            registers={0:0xa0,0x3d:0,0x3b:0}
            def write(ctx,reg,value):
                if (reg,value)==failed:return -1
                registers[reg]=value;return 0
            def read(ctx,reg,out):out[0]=registers[reg];return 0
            self.assertEqual(lib.sensor_configure(None,Write(write),Read(read),Delay(lambda c,n:None)),failed is None)
if __name__=="__main__":
    unittest.main()
