
# Compile

Install needed libraries:

    sudo apt update
    sudo apt install libglfw3-dev
    sudo apt install libglew-dev
    

Build:

macOS (default)

    sh
    cmake -B build
    cmake --build build

Linux (explicit)

    sh
    cmake -B build -DPAWN_LINUX=ON
    cmake --build build

# Run

In the build directory run
    
    ./Pawn

**Note:** F toggles full screen
