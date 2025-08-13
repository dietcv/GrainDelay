# GrainDelay

A real-time granular delay effect with subsample-accurate grain triggering and single-sample feedback. 
Each grain can be pitch-shifted and overlapped to create complex textures ranging from subtle echoes to dense granular clouds. 
The plugin uses a sub-sample accurate event system for precise grain timing, eliminating aliasing for high trigger rates and supports up to 32 active grains with smart voice allocation.
The voice allocation system distributes each grain across the 32 available channels and checks which channel is currently free, dropping grains only when all channels are busy. 
This ensures that no grains are scheduled on a channel which is currently active.

### Requirements

- CMake >= 3.10
- SuperCollider source code

### Building

Clone the project:

    git clone https://github.com/dietcv/graindelay
    cd graindelay
    mkdir build
    cd build

Then, use CMake to configure and build it:

    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
    cmake --build . --config Release --target install

You may want to manually specify the install location in the first step to point it at your
SuperCollider extensions directory: add the option `-DCMAKE_INSTALL_PREFIX=/path/to/extensions`.

It's expected that the SuperCollider repo is cloned at `../supercollider` relative to this repo. If
it's not: add the option `-DSC_PATH=/path/to/sc/source`.

### Developing

Use the command in `regenerate` to update CMakeLists.txt when you add or remove files from the
project. You don't need to run it if you only change the contents of existing files. You may need to
edit the command if you add, remove, or rename plugins, to match the new plugin paths. Run the
script with `--help` to see all available options.
