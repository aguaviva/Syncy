# Syncy

Android native app that uploads the pictures you take to your server via rsync&ssh

Warning, this is highly experimental

# Features
- It monitors the Camera folder, when a new file is created waits for 4 seconds, the rsyncs to your server
- Runs in the background


# Quick Start
- Generate a private key
  1. run `./dropbearkey -t rsa -f dropbear_rsa_host_key`
  2. copy it to your phones Document folder
- Set your server 
  - in your phone's Documents folder create a `server.txt` file with your server: `username@myserver.org:/tmp`  

# Compile and upload
  1. connect your cellphone, make sure adb works
  2. run `make run push`
  3. Give permissions manually

# Things I need help with
- Making the keyboard work with ImGui
  - Once this works you'll be able to configure things using the UI, until then the UI is just readonly.

# Notes
- Runs on Linux, it was actually developed and debugged on linux and then was ported to Android with minima effort.