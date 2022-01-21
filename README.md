# SOS.IO

This repository contains the original [SOS Plugin v1.6.0-beta6](https://gitlab.com/bakkesplugins/sos/sos-plugin) with extended functionality such as connection saving and prompt-based setup, along with Socket.IO implementation. An updater plugin is also contained in this repository. 

## Building

To build this project, you will need the Visual Studio 2022 Build Tools. There is a nodejs project in this directory that can automate the installation / build steps. To install the build tools, execute the following command: 
```
yarn install-tools
```
Note: this may take a while depending on your internet connection.

To build both Updater and SOSIO, execute the following command:
```
yarn build
```

You can view the built DLL's under `x64/Release`.