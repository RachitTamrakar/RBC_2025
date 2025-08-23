# Remote Bluetooth Car (Arduino Uno) – HM-10 + Dabble Only

Project controls a differential drive car via an HM-10 BLE module using the Dabble app (GamePad module). HC-06 classic SPP support has been removed.

## Build / Upload

Single environment: `uno_hm10` (default).

```
pio run -t upload
```

## Wiring

Assuming L298N (similar for L293D):

| Function | Arduino Pin | L298N | Notes |
|----------|-------------|-------|-------|
| Left Fwd PWM | 5 | IN1 | PWM |
| Left Rev PWM | 6 | IN2 | PWM |
| Right Fwd PWM | 9 | IN3 | PWM |
| Right Rev PWM | 10 | IN4 | PWM |
| Enable A | 5V (or jumper ENA) | ENA | If board has jumpers you can leave ENA tied high |
| Enable B | 5V (or jumper ENB) | ENB |  |
| Motor Power | External (e.g. 7.4V LiPo) | 12V | Don't power motors from 5V reg |
| GND | GND | GND | Common ground between Arduino, driver, battery |

HM-10:
- VCC -> 5V (some boards require 3.3V; check yours)
- GND -> GND
- TXD -> D2 (SoftwareSerial used internally by Dabble) *Note: Dabble library abstracts this; default uses pins 2 (RX) & 3 (TX).*
- RXD -> D3 (with voltage divider if module not 5V tolerant on RX)

(If you need different pins, adjust Dabble configuration per its docs.)

## Usage
1. Install Dabble app.
2. Open GamePad module.
3. Power Arduino with HM-10 connected; module advertises.
4. In app connect to the HM-10.
5. Use directional pad / joystick; Start or Select to stop.

## Extending
- Add more commands (e.g., 'X' for spin) in `handleCommandChar`.
- Tune turning behavior in `turnLeft/turnRight`.
- Add battery monitoring via an analog pin.

## Safety
- Lift wheels off the ground for first tests.
- Ensure common ground across all components.
- Never power motors directly from Arduino 5V regulator.

## License
MIT
