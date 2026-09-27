"""Native tests of the production LED transport with a stateful fake RMT peripheral.
Hardware waveform/electrical integrity still needs bench observation.
"""
from pathlib import Path
import os, subprocess, unittest
ROOT=Path(__file__).resolve().parents[2]
class WheelLed(unittest.TestCase):
 def test_transport(self):
  src=ROOT/'firmware/wheel/components/wheel_io/wheel_led.c'
  includes=ROOT/'.test-build/led-includes'
  for name in ('esp_err.h','esp_attr.h','driver/gpio.h','driver/rmt_tx.h','wheel_board.h'):
   header=includes/name;header.parent.mkdir(parents=True,exist_ok=True);header.write_text('/* Test boundary declared by harness. */\n')
  subprocess.run([os.environ.get('HOST_CC', str(ROOT/'.test-build/host-clean/deps/ziglang/zig.exe')),
   'cc','-std=c11','-Wall','-Wextra','-Werror',str(ROOT/'tests/host/wheel_led_harness.c'),
   '-I'+str(includes),'-o',str(ROOT/'.test-build/wheel_led_test.exe')], check=True)
  subprocess.run([str(ROOT/'.test-build/wheel_led_test.exe')],check=True)
if __name__=='__main__':unittest.main()
