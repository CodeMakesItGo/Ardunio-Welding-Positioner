# Arduino-Welding-Positioner
Arduino-controlled rotary weld positioner


## Original source

https://github.com/CodeMakesItGo/Ardunio-Welding-Positioner

> This project uses the Arduino Uno.
>
> Haslip Cycle Works asked if I could make them a controller for their rotary weld positioner to be used with a tig welder. I was excited to help! However, we came across some EMF interference because of the tig welder high frequency start.
>
> Parts:
>
> * Arduino Uno
> * LCD Keypad Shield
> * CNC Single Axis TB6600 Stepper Motor Driver
> * 4th Axis Hollow Shaft Rotary Table Router
>
> Watch the build video here:
> https://www.youtube.com/watch?v=bhvVHksbcRk


## Modifications

Parts:

* Arduino Pro Mini (pin- and software-compatible with the Uno-based source)
* 1602A LCD Keypad Shield (No keypad; I2C backpack; same screen geometry)
* Home-made keypad with conductive rubber keys from an old printer
* CNC Single Axis TB6600 Stepper Motor Driver
* 42HD4027-O1 NEMA sepper driving a Al tube in 60mm-bore bearings with a
  3d-printed belt drive (design to be posted separately)

The main modifications are:

* The I2C-based display driver The gold+rubber keypad contacts do not bounce.
* They present the opposite problem of slow transitions, so the debouncing
* logic was replaced with a window filter to suppress transients.
