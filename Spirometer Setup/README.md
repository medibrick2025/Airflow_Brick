## 6.0 Assembly and Use Instructions

### 6.1 Spirometer

#### 6.1.1 Electrical Assembly

##### Soldering

###### SFM3300 Flow Sensor Connection (SFM headers)

The left-side header of the Spirometer MediBrick is dedicated to the SFM3300 airflow sensor interface.

Firmly insert wires or a pin header into the pads labeled **DATA**, **SCK**, **VDD**, **HEAT**, and **GND**.  
These pads are directly connected to the corresponding SFM3300 sensor pins.

Solder each connection securely, ensuring correct alignment and solid solder joints for reliable airflow measurement and heater control.

###### Microcontroller Interface (Right Side Header)

The right-side header is used to connect the Spirometer MediBrick to the Adafruit Feather ESP32-S3 microcontroller.

Insert wires or a header into the pads labeled **SDA**, **SCL**, **3V3**, **GND**, **EN**, **BAT**, **SEL**, **BTN**, and **GND**.  
Solder all required pins carefully, avoiding solder bridges between adjacent pads.

##### Wires

Attach color-coded wires to the I/O pads as follows:

- **Red**: Power (3V3)  
- **Black or Green**: Ground (GND)  
- **Blue or White**: I²C signals (SDA, SCL)  
- **Other colors**: Control signals (EN, BTN)

Wires may be inserted directly into the through-holes (perpendicular) or attached as short pigtail leads soldered onto the pads.

##### Suggested Connections to the Adafruit Feather ESP32-S3

The recommended connections between the Spirometer MediBrick and the ESP32-S3 are shown below:

| PCB | ESP |
|-----|-----|
| SDA | SDA |
| 3V3 | 3.3 |
| SCL | SCL |
| EN  | A5  |
| GND | GND |
| BAT | VBAT |
| SEL | GPIO 9 |
| BTN | Button |
| GND | Button |

<a href="./images/image01.jpeg" target="_blank">
  <img src="./images/image01.jpeg" alt="Spirometer setup photo 1" width="420">
</a>

<a href="./images/image02.jpeg" target="_blank">
  <img src="./images/image02.jpeg" alt="Spirometer setup photo 2" width="420">
</a>

<a href="./images/image03.jpeg" target="_blank">
  <img src="./images/image03.jpeg" alt="Spirometer setup photo 3" width="420">
</a>

<a href="./images/image04.jpeg" target="_blank">
  <img src="./images/image04.jpeg" alt="Spirometer setup photo 4" width="420">
</a>

<a href="./images/image05.jpeg" target="_blank">
  <img src="./images/image05.jpeg" alt="Spirometer setup photo 5" width="420">
</a>

##### Final Inspection

After assembly:

- Verify all solder joints are clean and fully wetted.
- Confirm correct wiring orientation for the SFM3300 sensor.
- Check continuity for power and ground connections using a multimeter.
- Ensure no short circuits exist between adjacent pins.
- Run an I²C check.
- Run the test code.

Once completed, the Spirometer MediBrick is ready for enclosure integration and functional testing with the ESP32-S3.

##User manual

Before powering on the device
The Spirometer MediBrick needs 3.7V from the battery to properly power the complete system. Make sure before the device is ran that the battery is correctly attached to its input on the microcontroller.  

## Battery Connection

<p align="center">
  <img src="./images/Battery_connection.jpeg" alt="Battery Connection" width="700">
</p>

Burning the code
To start collecting data from the Spirometer MediBrick, the most important step is to make sure that the code is properly uploaded to the system. This can be done by attaching the computer to the Spirometer device. Once connected, the code provided on the MediBrick GitHub page should be uploaded to Arduino IDE and uploaded onto the microcontroller. The display should show your inhale and exhale volumes around 0 if properly done.

Reading the Data
The display on the device shows the inhalation and exhalation volumes when in use and holds any readings if not. After use, the device will show the maximum achieved inhalation or exhalation volume.
Resetting Instructions
If an error occurs within the code itself, disconnect the spirometer sensor and re attach it. After the display powers back on, click the reset button found on the side of the device twice to fully reset the sensor. This will allow normal readings and measurements to be gathered

## Deep Sleep

<p align="center">
  <img src="./images/Deep_sleep.jpeg" alt="Deep Sleep" width="700">
</p>

