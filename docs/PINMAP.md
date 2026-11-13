# Lykyn pin map (ESP32-C3)

Recovered by static analysis of the stock 0.17.0 app image (Arduino-ESP32 3.3.7, IDF 5.5.2).
Method: disassembled the RISC-V app image, resolved call targets, and tracked constant arguments
into `pinMode`, `digitalWrite`, `analogWrite`, `TwoWire::begin`, the FastLED RMT controller
and the Adafruit_NeoPixel constructor. No device was written to.

| GPIO | Role | Evidence | Confidence |
| --- | --- | --- | --- |
| 4 | I2C SDA (SHT3x at 0x44) | `Wire.begin(4, 5, 0)` called before `SHT3x.begin(Wire, 0x44)` | High |
| 5 | I2C SCL | same call | High |
| 3 | Inlet fan, PWM | `analogWrite(3, map(level, 0, 3, 0, 255))`, `pinMode(3, OUTPUT)` | High |
| 1 | Outlet fan, PWM | `analogWrite(1, map(level, 0, 3, 0, 255))`, `pinMode(1, OUTPUT)` | High |
| 10 | Ultrasonic mister, plain on/off | only reached by `digitalWrite(10, state)`. No `analogWrite`, LEDC or tone call ever takes pin 10 | High |
| 7 | Grow light: 16 pixel WS2812B ring, data | FastLED RMT controller built with pin 7, T1/T2/T3 = 250/625/375 ns, then `addLeds(ctrl, leds, 16, 0)` | High (pin, count, type). Colour order not determined |
| 2 | Status LED: 1 pixel NeoPixel, RGB order, 800 kHz | `Adafruit_NeoPixel(1, 2, NEO_RGB)` then `begin()`, `setPixelColor(0, ...)` | High |
| 9 | Reset button, input pull-up | `pinMode(9, INPUT_PULLUP)` plus interrupt; held 3 s = factory reset | High |
| 0 | Output, driven HIGH at boot, never changed | `pinMode(0, OUTPUT)`, `digitalWrite(0, 1)` once in setup | Behaviour High, purpose unknown |
| 6 | Output, driven HIGH at boot, never changed | `pinMode(6, OUTPUT)`, `digitalWrite(6, 1)` once in setup | Behaviour High, purpose unknown |
| 18, 19 | USB D-, D+ (native USB-Serial/JTAG) | stock touches them only in the USB init path | High |

## Fans
Level 0..3 maps to 8-bit duty 0, 85, 170, 255 on a default LEDC (1 kHz) channel. We use 1 kHz and a
33 percent floor so a fan never stalls at low duty.

## Mister
The microcontroller only switches it on or off. Mist intensity is therefore set by the driver board
(or fixed). The stock firmware cycles it (5 s on in range, 10 s on below minimum, 2 minute hard stop,
off at 92 percent RH). This firmware matches that and adds a Mist Level knob that pulses the line in a
10 second window.

## Grow light
Not "just USB power". The board has a data line (GPIO7) into a 16 pixel WS2812B ring. Stock behaviour:
mode MANUAL, colour hex string (saved value `#FFFFFF`), brightness 0 to 100 percent mapped to FastLED
global brightness 0 to 255 (saved value 50 gives 127). It also has smart schedule mode, an EMOTIONAL mode
(five mood colours) and about 30 animations (AURORA, BREATH, RGB_WAVE, CONFETTI, SUNSETFADE and so on).
WS2812B has no white die. "White" is all three channels on together.

## Still unknown
1. RGB order of the 16 pixel ring (GRB is the WS2812B default). Probe: LED Test button.
2. What GPIO0 and GPIO6 switch. We drive them HIGH at boot like stock.
3. Whether the mister driver reacts to pulsed drive the way a continuous drive behaves. Stock pulses it in
   5 to 10 second blocks, so 10 second windows are in the same range.
