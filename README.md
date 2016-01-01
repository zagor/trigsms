# trigsms

Arduino sketch for a battery-powered device that sends an SMS when a switch is triggered.

It uses an Arduino Pro Mini 5V and a SIM900 GSM module.

When removing the onboard arduino voltage regulator and instead using an external LDO at 4.096 volts, this sketch draws 806 µA and thus can live for a long time on relatively modest battery capacity. However, since the SIM900 module radio draws a lot of power when active you can't go with too small a battery.
