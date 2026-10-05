# Reflection

## What worked

The DHT22 and the LDR were already wired from earlier practicals, so those two
worked straight away and I could spend my time on the new parts. Putting the
BMP280 and the OLED on the same I2C bus worked once both were wired properly.
Seeing two devices reply on two different addresses over the same two wires
made I2C much clearer than reading about it did. Giving each sensor its own
millis() interval also worked well, and subscribing to assignment1/sensors/#
once was enough to check all four values were arriving.

## Challenges

Choosing the third sensor was the first problem. I had a PIR but it only gives
a digital output, so it could not meet the I2C requirement and I had to get a
BMP280 instead. The second problem was the OLED. The I2C scanner only found
the BMP280 at 0x76 and nothing at 0x3C. The third was the network: the ESP32
kept printing connection dots and never joined the hotspot. The fourth was
timing. My first version read all three sensors in one loop with a short delay,
which polled the DHT22 much faster than its two second minimum.

## How I resolved them

Running the I2C scanner before the main sketch was the most useful thing I did,
because it showed the problem was wiring and not code. I rewired the OLED and
scanned again until both addresses showed up. The network failed because the
hotspot was on 5 GHz, which the ESP32 cannot see, so I turned on Maximize
Compatibility to force 2.4 GHz. For the timing I gave each sensor its own
interval and its own last-run timestamp, so the DHT22 no longer holds up the
other two.