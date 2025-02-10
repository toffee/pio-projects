[Documentation](https://nodemcu.readthedocs.io/en/latest/getting-started/)

I used:
 * cloudbuilder - the firmwaer are on bin directory on this repository
 * flash using NodeMCU PyFlasher - download the exe and run
 * upload the code using NodeMCU-Tool (Node.js) - install first node.js (including tools to build modules) and the install/run nodemcu-tool
 
The application:
 * init.lua - the bootstrap (taken from https://nodemcu.readthedocs.io/en/latest/upload/#initlua)
 * credentialslua - contains wifi credentials + mqttconfig
 * application.lua - every second read the GPIO pin and if the value is changed publisg to mqtt