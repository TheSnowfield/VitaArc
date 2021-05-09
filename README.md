# VitaArc

Run Arcaea on your HENkaku PSVita.  
This is a cross-system ABI bridge to simulate the android environment of Arcaea in the SCE system.

`Arcaea` is a mobile game who developed by Lowiro.  
This project is not affiliated with Lowiro or Arcaea.

## Hint
This project now **under development**.  
You may not get the game running correctly.

## Get started
### Install
 - Download the `VitaArc.vpk` from the release page.  
 (random laughing) ~~cause now there's no release exists. XD~~
 - Connect your PSVita to PC via VitaShell and copy `VitaArc.vpk` into it.
 - Install VitaArc.

### Make directory tree
 - Download the game from the official website.
 - Extract the apk (~~ToS Warning~~) to any where on your PC.
 - Make a directory named `vitaarc` under `ux0:` root partition on your PSVita.
 - Copy `lib/arbeabi-v7a` to `ux0:vitaarc/library/`.
 - Copy `assets` to `ux0:vitaarc/`.
 - Make a directory named `persistent` under `ux0:vitaarc/`.
 
If you follows the tutorial correctly, The directory tree should be like
 - assets
   + app-data
   + audio
   + char
   + Default
   + Fonts
   + img
   + ...
 - library
   + armeabi-v7a
     - libcocos2dcpp.so
     - libcrashlytics.so
     - libcrashlytics-common.so
     - libcrashlytics-handler.so
     - libcrashlytics-trampoline.so
     - ...
 - persistent
### Play & PM
 - Click VitaArc bubble.
 - Wait for the game load.
 - Try your best to `PM` all of the songs.
