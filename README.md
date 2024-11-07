# Syncy

Android native app that uploads the pictures you take to your server via rsync&ssh

Warning, this is highly experimental

# Features
- It monitors the Camera folder, when a new file is created waits for 4 seconds, the rsyncs to your server
- Runs in the background

# Quick Start
- Generate a private key
  1. run `./dropbearkey -t rsa -f dropbear_rsa_host_key`
  2. put the dropbear_rsa_host_key file in the `lib` folder
- Set your server 
  - in Syncy.cpp search and replace `username@myserver.org:/tmp`  (super lame I know)
- Compile and upload
  1. connect your cellphone, make sure adb works
  2. run `make run push`

# Things I need help with
- Making the keyboard work with ImGui
  - Once this works you'll be able to configure things using the UI, until then the UI is just readonly.