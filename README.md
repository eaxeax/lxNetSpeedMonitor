# lxNetSpeedMonitor
Show network speed by interface in lxpanel. Inspired by [NetSpeedMonitor](https://netspeedmonitor.net)

# building and installing
Install/update build dependencies.  
Arch:  
`sudo pacman -Sy cmake base-devel`  

Debian/Ubuntu and its derivatives:  
`sudo apt install cmake base-devel`  

If you are a cool kid not using the above distros, you probably know how to install development packages for your distro!

Build with CMake  
`cmake -S . -B build`  
`cmake --build build`

Install as root  
`sudo cmake --install build`

# usage
- Right click on panel then Panel Settings
- Navigate to Panel Applets then click Add
- Double click NetSpeedMonitor

# configuration
- Right click the applet in the panel then settings
- Type in the interface you want to track usage



