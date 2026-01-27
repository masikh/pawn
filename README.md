
# Compile

Install needed libraries:

    sudo apt update
    sudo apt install libglfw3-dev
    sudo apt install libglew-dev
    sudo apt install zlib1g-dev
    sudo apt install libglm-dev

Build:

macOS (default)

    sh
    cmake -B build
    cmake --build build

Linux (explicit)

    sh
    mkdir build
    cmake -B build -DPAWN_LINUX=ON
    cmake --build build

# Run

macOS

    copy build/Pawn.app to /Applications
    open /Applications/Pawn.app

Linux

run Pawn in the build directory

    ./build/Pawn
