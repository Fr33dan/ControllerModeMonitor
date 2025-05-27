# ControllerModeMonitor

Controller Mode monitor is designed to create console-like functionality from a Windows PC.

When a wireless controller is turned on/connected, CMM activates Steam controller UI mode (aka Big Picture mode) and if desired, instructs a TV to switch to the computer (mimicking CEC functionality) and changes the audio device. When the controller is disconnected/turned the reverse is done.

If your TV is not your normal primary display, it is recommended to use Steam settings to adjust the primary display when controller UI mode is activated.

Most graphics drivers do not support CEC funcionality, to achieve the same effect network based APIs are used. TV functionality only works with Roku TVs at this time, but is designed so that other TV types may be added in the future. To find TVs on the network SSDP broadcasting is used. You will need to enable [ECP control](https://developer.roku.com/docs/developer-program/dev-tools/external-control-api.md) by going to **Settings > System > Advanced system settings > Control by mobile apps** and setting to "enabled"

Originally written as a PowerShell script which was bulky, I wanted more functionality so I used it as an excuse to (re)learn C++ to create a lightweight implementation.