# Post-flash probe plan (run on our own firmware, never on stock)

Do these on the bench with the bag out, one at a time. Nothing below is run on the stock firmware.

1. LED order: press **LED Test**. The ring shows red, green, blue, then white for 2 s each at 30 percent.
   If red and green are swapped, rebuild with `led_order: RGB`. If red and blue are swapped, use `BGR`.
2. Fans: set Control Mode to MANUAL, Inlet Fan Manual 33, then 66, then 100. Confirm which fan moves.
   If inlet and outlet are swapped in the chamber, swap the two numbers' roles in the YAML (GPIO3 and GPIO1).
3. Mister: set Control Mode MANUAL, turn Humidifier on for about 10 seconds with a hand under the outlet
   and confirm mist. Then lower Mist Level to 50 and watch the pulse pattern (5 s on, 5 s off).
4. GPIO0 and GPIO6: leave HIGH (stock does). Only if something fails to power up, toggle them from a
   dev build, one at a time, while watching the ring, fans and sensor.
5. SHT3x: Current Temperature and Current Humidity should match a reference hygrometer within a couple of percent.
