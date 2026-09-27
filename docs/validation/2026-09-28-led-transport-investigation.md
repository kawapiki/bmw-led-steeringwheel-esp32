# Intermittent LED flash investigation

## Evidence and confidence

The owner reported occasional flashes/data corruption. No logic-analyzer capture or before/after physical observation has been obtained by this worker, so the physical root cause remains unconfirmed.

At baseline `944cfc2`, chain 0 used RMT DMA with 96 symbols and chain 1 used non-DMA RMT with 96 symbols. Both stream a 576-bit LED payload plus reset through 48-symbol halves. At the existing 1.2 us bit period, this requires roughly 57.6 us refill service intervals during each frame. Channels were constructed from app_main; the radio workload also runs on core 0. ESP-IDF documents delayed refill interrupts as a cause of incorrect LED transmission. This is a concrete software susceptibility, consistent with the reported symptom, not proof of its physical cause. The high-RPM 4 Hz shift-light blink above 6500 RPM is intentional and must be distinguished from corruption.

ESP32-S3 has only one DMA-capable RMT TX channel. Enabling DMA on both original channel objects would fail allocation. No concurrent writes to the old LED buffers were found: the effects task owns them and refresh waits synchronously.

Sources inspected: pinned ESP-IDF v6.1 `esp_driver_rmt/src/rmt_tx.c`, `rmt_encoder_simple.c`, `esp_driver_gpio/src/gpio.c`; pinned led_strip encoder/device sources. See also [Espressif RMT FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/peripherals/rmt.html).

## Implemented mitigation

- One DMA TX channel serves GPIO3 and GPIO4 sequentially, matching the existing sequential frame schedule; the motor's separate RMT channel is unchanged.
- A 640-symbol DMA buffer contains all 576 data symbols, one 280 us reset symbol, and the driver's EOF before transmission starts. IDF's initial encoding pass fills the full buffer, and completion breaks the DMA descriptor ring. There is no mid-frame refill deadline.
- A simple encoder emits GRB/MSB-first symbols directly into that buffer. Existing 10 MHz WS2812 timing is preserved. The encoder's minimum chunk is 24 symbols, keeping its overflow buffer at 96 bytes; no additional 2308-byte pre-encoded frame is retained.
- GPIO is switched only while the channel is disabled. The disconnected GPIO is driven low with a pulldown during the brief transition. In IDF6.1, `gpio_set_direction(..., GPIO_MODE_OUTPUT)` calls `gpio_output_enable`, which resets the peripheral matrix route to plain GPIO before enabling output; merely setting its output-enable bit would not suffice.
- Each transfer waits at most 20 ms before attempting cancellation. All enabled paths disable the channel before buffer reuse. Disable failure latches the transport off and prevents further pixel mutation/transmission. Errors are logged at exponentially spaced counts.
- OTA IO-quiescence is acknowledged only after both quiet-pattern transfers succeed. LED pixel 0 remains button illumination, indices 1–23 remain RPM. Existing maintenance patterns and brightness are preserved.
- Recovery protocol v1, application interfaces, GPIO definitions, task/core placement and global sdkconfig are unchanged. Component CMake now directly requires esp_driver_rmt instead of led_strip.

## Verification actually performed

`python tests/host/test_wheel_led.py`: PASS. The native C harness compiles the production LED backend and checks full-frame DMA capacity, literal GRB/bit timing fixtures, first/last pixel placement, two independent chains, chunked encoder output, matrix detachment/low idle, input bounds, and injected GPIO-switch/enable/transmit/wait/disable failures. A failed stop is checked to prevent subsequent pixel mutation. These are host boundary tests, not hardware waveform measurements. `git diff --check`: PASS.

The first test run failed because the complete-frame backend did not yet exist. It did not reproduce an electrical flash. No firmware build, COM access, flash, reset, CAN action, commit or push was performed by this worker.

## Resource implications and pending integration

DMA symbol storage is 2560 bytes; pixel storage is 144 bytes; encoder overflow is 96 bytes, plus driver/encoder bookkeeping. Compared with the old 384-byte DMA buffer and two strip objects, expect approximately 2 KiB additional memory, not a measured heap delta. The old non-DMA LED channel is released. UI timing and peak HTTPS/OTA heap must be measured by the integrator because existing TLS headroom is tight.

Before declaring the reported problem fixed: build the wheel with IDF6.1, run both-chain/button/static/walking patterns while exercising BLE, Wi-Fi and HTTPS, observe at least several minutes, and test successful OTA quiescence. Record whether unexpected flashes persist, on which chain, and whether they coincide with haptic pulses. A scope/logic analyzer is needed if software mitigation does not remove the symptom; electrical supply or signal integrity is not excluded. Verify that only the selected physical chain changes during GPIO switching and check boot/startup illumination and haptics. The parent owns all device actions and commits.
