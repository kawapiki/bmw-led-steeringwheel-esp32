"""Round-trip the actual LVGL C QR encoder at the wheel's 164px layout.
Requires built .test-build/qr-codegen.dll, numpy, Pillow and zxing-cpp==2.3.0.
These are host image tests, not a phone/panel scan qualification.
"""
import ctypes as C
from pathlib import Path
import unittest
import numpy as np
import zxingcpp

ROOT=Path(__file__).resolve().parents[2]
lib=C.CDLL(str(ROOT/'.test-build/qr-codegen.dll'))
U8=C.c_uint8
lib.qrcodegen_getMinFitVersion.argtypes=[C.c_int,C.c_size_t]
lib.qrcodegen_encodeBinary.argtypes=[C.POINTER(U8),C.c_size_t,C.POINTER(U8),C.c_int,C.c_int,C.c_int,C.c_int,C.c_bool]
lib.qrcodegen_encodeBinary.restype=C.c_bool
lib.qrcodegen_getSize.argtypes=[C.POINTER(U8)]
lib.qrcodegen_getModule.argtypes=[C.POINTER(U8),C.c_int,C.c_int]
lib.qrcodegen_getModule.restype=C.c_bool

class WifiQR(unittest.TestCase):
 def test_generated_passwords_roundtrip_at_display_size(self):
  for password in ['abcdefghjkmnpqrs','2345678923456789','zzzzzzzzzzzzzzzz','ab23cd45ef67gh89']:
   with self.subTest(password_case=password[:2]):
    payload=f'WIFI:T:WPA;S:BMW-Wheel;P:{password};;'.encode()
    minimum=lib.qrcodegen_getMinFitVersion(1,len(payload))
    # Same version-selection and integer scale as LVGL9.4 quiet-zone mode.
    version=min(range(minimum,minimum+3),key=lambda v:148%(17+4*(v+1)))
    scale=148//(17+4*(version+1))
    data=(U8*4096)();qr=(U8*4096)()
    C.memmove(data,payload,len(payload))
    self.assertTrue(lib.qrcodegen_encodeBinary(data,len(payload),qr,1,version,version,-1,True))
    side=lib.qrcodegen_getSize(qr)
    margin=8+(148-side*scale)//2
    self.assertGreaterEqual(margin,4*scale)
    pixels=np.full((164,164),255,dtype=np.uint8)
    for y in range(side):
     for x in range(side):
      if lib.qrcodegen_getModule(qr,x,y):
       pixels[margin+y*scale:margin+(y+1)*scale,margin+x*scale:margin+(x+1)*scale]=0
    result=zxingcpp.read_barcode(pixels)
    self.assertIsNotNone(result)
    self.assertEqual(result.text,payload.decode())

if __name__=='__main__': unittest.main()
