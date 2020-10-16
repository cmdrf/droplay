# droplay
DRO music file command line player

## About

DRO files are created by DOSBox by recording all register writes to the emulated
SoundBlaster or AdLib sound cards. These files can later be played back by software
like this one.

droplay uses SDL for sound playback.

## Credits

Based on droplay.c from the Chocolate Doom source by Simon Howard

Contains the Nuked OPL3 emulator by Alexey Khokholov

## Build Instructions

If you have SDL installed in a standard location where CMake can find it, building
should work like this:
```
mkdir build
cd build
cmake ..
make
```

## Running

droplay takes a single argument for the file to play:
```
droplay <.dro file>
```

## Additional Information
- http://www.shikadi.net/moddingwiki/DRO_Format
