******************************************************************************
* Instructions                                                               *
******************************************************************************

Copy the extracted folder to any location and run ControllerModeMonitor.exe.
The application will not show any window and add an icon to the system
notification tray. Click the icon for the menu.

Select "Controllers > Add Device" to add a controller to enter controller mode
upon detection. Click a controller on the Controllers menu to stop monitoring
for that controller. 

Use the TV menu to select a Roku TV to turn on and select an HDMI input to 
display the computer output on that display. It is recommended to use Steam's
big picture mode settings to change the primary display to this device when
big picture mode is activated. For this functionality, on your Roku TV must
have the "Settings > System > Advanced system settings > Control by mobile
apps" feature must be set to "Enabled". Roku TVs are discovered using SSDP.
If any other application is also attempting to discover devices or otherwise
using port 1900, TV discovery may not function.

Use the Audio menu to change the audio output device when entering controller
mode. The current audio device is shown with "(Default)". Once controller mode
is  closed the audio device will be restored to whatever the output audio was
when controller mode was entered. If ControllerModeMonitor is closed (or the
system crashes) while in controller mode, the next time ControllerModeMonitor
is opened a notification will be displayed to restore the audio device as 
would have been done if controller mode had be closed normally.

******************************************************************************
* Licences                                                                   *
******************************************************************************

**************************************
* curl                               *
**************************************
COPYRIGHT AND PERMISSION NOTICE

Copyright (c) 1996 - 2025, Daniel Stenberg, daniel@haxx.se, and many
contributors, see the THANKS file.

All rights reserved.

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall
not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization of the copyright
holder.

**************************************
* pugixml                            *
**************************************

This software is based on pugixml library (http://pugixml.org). pugixml is
Copyright (C) 2006-2025 Arseny Kapoulkine.