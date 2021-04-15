# ESP32-42e-Paper-Weather-Display-
A revised weather display based on a 7.5" version for the 4.2" display
ESP32-e-Paper-Weather-DisplayAn ESP32 and a 2.9" ePaper Display reads Weather Underground and displays the weather

OPEN WEATHER MAP VERSION

Go to Sketch > Include Library... > Manage Libraries.... Then, for each library, put its name into the text field to have its metadata pulled from the internet and displayed below. Select the latest version and install it. Make sure to come back to this dialog from time to time to keep each library up to date. Also make sure that you only have one version of each of the libraries installed.

'Mini Grafx' by Daniel Eichhorn 'ESP8266 WeatherStation' by Daniel Eichhorn (required for additional fonts) Download the software and install all 3 files in the same sketch folder.

ESP32_OWM_Current_Forecast_29_epaper_vX always choose the latest version.
ESP32_OWM_Current_Forecast_42_epaper_vX always choose the latest version.
ESP32_OWM_Current_Forecast_75_epaper_vX always choose the latest version.

owm_credentials2.h

ArialRounded.h

Obtain your OWM API key - it's free

Edit the owm_credentials2.h file in the IDE (TAB at top of IDE) and change your Language, Country, choose your units Metric or Imperial and be sure to find a valid weather station location on OpenWeatherMap, if your display has all blank values your location does not exist!.
OPEN WEATHER MAP VERSION - STREAMING JSON

The 'streaming' version requires *** Arduino JSON v6 or above *** it streams the Openweather and processes the data as received, rather than downloading all the data first, then decoding it. It uses a lot less memory this way. 

*** Use Arduino JSON v5.13.3 for the other versions.

Go to Sketch > Include Library... > Manage Libraries.... Then, for each library, put its name into the text field to have its metadata found and choose install.

Compile and upload the code - Enjoy!

Wiring Schematic for ALL Waveshare E-Paper Displays
![alt_text, width="300"](/Schematic.JPG)
