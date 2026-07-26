#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Color codes for pretty output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Target Steam directory
TARGET_DIR="/run/media/system/NVME_GAME_1/SteamLibrary/steamapps/common/RealRTCW"

# 1. Parse arguments
BUILD_STEAM=true
CLEAN_BUILD=false
RUN_AFTER_BUILD=false
RUN_TESTS=false
RUN_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--nosteam)
            BUILD_STEAM=false
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -r|--run)
            RUN_AFTER_BUILD=true
            shift
            # Collect all remaining arguments to pass to the game
            while [[ $# -gt 0 ]]; do
                RUN_ARGS+=("$1")
                shift
            done
            ;;
        -h|--help)
            echo "Usage: $0 [options] [--game-args...]"
            echo ""
            echo "Options:"
            echo "  -n, --nosteam   Build standalone executable without Steam integration."
            echo "  -c, --clean     Perform a clean build by running 'make clean' first."
            echo "  -t, --test      Build and run the pure algorithmic unit test suite."
            echo "  -r, --run       Launch RealRTCW via the launcher script after a successful build."
            echo "                  All subsequent arguments are forwarded directly to the launcher."
            echo "  -h, --help      Display this help menu."
            echo ""
            echo "Examples:"
            echo "  $0 --nosteam"
            echo "  $0 -c"
            echo "  $0 -t"
            echo "  $0 -r +set sv_cheats 1"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use -h or --help for usage information."
            exit 1
            ;;
    esac
done

echo -e "${BLUE}=== RealRTCW Build Script ===${NC}"

# Verify target directory is mounted/accessible before starting
if [ ! -d "$TARGET_DIR" ]; then
    echo -e "${RED}Error: Target directory does not exist:${NC}"
    echo -e "  ${YELLOW}$TARGET_DIR${NC}"
    echo "Please make sure your NVMe drive is mounted."
    exit 1
fi

# 2. Detect container environment vs host
INSIDE_CONTAINER=false
if [ -f /run/.containerenv ] || [ -f /run/.toolboxenv ] || [ "${container}" = "podman" ] || [ "${container}" = "docker" ]; then
    INSIDE_CONTAINER=true
fi

# Determine how to run commands (directly or via distrobox)
USE_DISTROBOX=false
DISTROBOX_NAME=""
if [ "$INSIDE_CONTAINER" = false ]; then
    if command -v distrobox &> /dev/null; then
        # Check if Bazzite-dev-nvidia or Bazzite-dev-env container exists
        if distrobox list 2>/dev/null | grep -q "Bazzite-dev-env"; then
            USE_DISTROBOX=true
            DISTROBOX_NAME="Bazzite-dev-env"
        elif distrobox list 2>/dev/null | grep -q "Bazzite-dev-nvidia"; then
            USE_DISTROBOX=true
            DISTROBOX_NAME="Bazzite-dev-nvidia"
        fi
    fi
fi

# Helper function to run commands in the correct context
run_build_cmd() {
    if [ "$USE_DISTROBOX" = true ]; then
        distrobox enter "$DISTROBOX_NAME" -- "$@"
    else
        "$@"
    fi
}

if [ "$USE_DISTROBOX" = true ]; then
    echo -e "${BLUE}Running build commands inside '$DISTROBOX_NAME' distrobox container...${NC}"
else
    if [ "$INSIDE_CONTAINER" = true ]; then
        echo -e "${BLUE}Running build commands directly inside container...${NC}"
    else
        echo -e "${YELLOW}Warning: Compatible distrobox container not found. Compiling on host...${NC}"
    fi
fi

if [ "$BUILD_STEAM" = true ]; then
    STEAM_VAL=1
    BUILD_FLAVOR="steam"
else
    STEAM_VAL=0
    BUILD_FLAVOR="nosteam"
fi
BUILD_DIR_NAME="build/release-linux-x86_64-${BUILD_FLAVOR}"

if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Clean build requested. Cleaning build artifacts...${NC}"
    run_build_cmd make STEAM="${STEAM_VAL}" clean
fi

# Determine parallel build job factor
if [ -n "$JOBS" ]; then
    NUM_JOBS="$JOBS"
elif command -v nproc &>/dev/null; then
    NUM_JOBS=$(nproc)
elif command -v sysctl &>/dev/null; then
    NUM_JOBS=$(sysctl -n hw.ncpu)
else
    NUM_JOBS=2
fi

# Start build timer
START_TIME=$(date +%s)

# 3. Build the project
echo -e "${GREEN}Compiling RealRTCW (STEAM=${STEAM_VAL}) with ${NUM_JOBS} parallel jobs...${NC}"
if ! run_build_cmd make -j"${NUM_JOBS}" STEAM="${STEAM_VAL}"; then
    echo -e "${RED}Build failed! Try a clean build if you encountered configuration issues:${NC}"
    echo -e "${YELLOW}  ./compile.sh --clean${NC}"
    exit 1
fi

if [ "$RUN_TESTS" = true ]; then
    echo -e "${BLUE}Compiling and running Algorithmic Unit Tests...${NC}"
    if ! run_build_cmd make STEAM="${STEAM_VAL}" "${BUILD_DIR_NAME}/test_runner"; then
        echo -e "${RED}Failed to compile test_runner!${NC}"
        exit 1
    fi
    if ! run_build_cmd ./"${BUILD_DIR_NAME}/test_runner"; then
        echo -e "${RED}Unit tests failed!${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}Deploying compiled binaries to target directory...${NC}"
if ! run_build_cmd make STEAM="${STEAM_VAL}" COPYDIR="$TARGET_DIR" copyfiles; then
    echo -e "${RED}Deployment failed!${NC}"
    exit 1
fi

# Package and deploy UI changes to z_zz_sofia.pk3
if [ -d "$SCRIPT_DIR/ui" ]; then
    echo -e "${GREEN}Packaging UI menus into z_zz_sofia.pk3...${NC}"
    if command -v zip &> /dev/null; then
        # Create or update z_zz_sofia.pk3 in the target Steam main folder
        mkdir -p "$TARGET_DIR/main"
        (cd "$SCRIPT_DIR" && zip -r -0 -q "$TARGET_DIR/main/z_zz_sofia.pk3" ui)
    else
        echo -e "${YELLOW}Warning: 'zip' utility not found. Could not package UI into z_zz_sofia.pk3.${NC}"
    fi
fi


# Also double check and copy launcher helper script if desired
LAUNCHER_SRC="/home/sofia/Desktop/RealRTCW-native-launcher.sh"
LAUNCHER_DEST="$TARGET_DIR/RealRTCW-native-launcher.sh"
if [ -f "$LAUNCHER_SRC" ]; then
    echo -e "${GREEN}Copying launcher helper script to Steam directory...${NC}"
    cp -v "$LAUNCHER_SRC" "$LAUNCHER_DEST"
    chmod +x "$LAUNCHER_DEST"
fi

# 3.5. Build and deploy Steamshim launcher natively (if STEAM build)
if [ "$BUILD_STEAM" = true ]; then
    echo -e "${BLUE}=== Compiling and Deploying Native Steam Launcher ===${NC}"
    echo -e "${GREEN}Detecting Steam library and Steamworks SDK headers...${NC}"

    # Locate the Steam client's 64-bit steam_api library.
    STEAM_LIB_PATH=""
    for path in \
      "$HOME/.steam/steam/ubuntu12_64" \
      "$HOME/.local/share/Steam/ubuntu12_64" \
      "$HOME/.local/share/Steam/steamrt64" \
      "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam/ubuntu12_64"
    do
      if [ -f "$path/libsteam_api.so" ]; then
        STEAM_LIB_PATH="$path"
        break
      fi
    done

    if [ -z "$STEAM_LIB_PATH" ]; then
        echo -e "${RED}Error: libsteam_api.so not found on the system. Cannot build Steamshim launcher.${NC}"
        exit 1
    fi
    echo -e "Found libsteam_api.so at: ${YELLOW}$STEAM_LIB_PATH${NC}"

    # Locate Steamworks SDK headers
    STEAMWORKS_HEADERS=""
    for path in \
      "/run/media/system/NVME_GAME_1/GitHub/proton-ge-custom/lsteamclient/steamworks_sdk_164" \
      "/run/media/system/NVME_GAME_1/GitHub/proton-ge-custom/lsteamclient/steamworks_sdk_161" \
      "/run/media/system/NVME_GAME_1/GitHub/proton-ge-custom/lsteamclient/steamworks_sdk_157" \
      "/run/media/system/NVME_GAME_1/GitHub/proton-ge-custom/lsteamclient/steamworks_sdk_153a"
    do
      if [ -f "$path/steam_api.h" ]; then
        STEAMWORKS_HEADERS="$path"
        break
      fi
    done

    if [ -z "$STEAMWORKS_HEADERS" ]; then
      # Try to find it dynamically in proton-ge-custom directory
      PROTON_GE_DIR="/run/media/system/NVME_GAME_1/GitHub/proton-ge-custom"
      if [ -d "$PROTON_GE_DIR" ]; then
        STEAMWORKS_HEADERS=$(find "$PROTON_GE_DIR" -name "steam_api.h" -path "*/steamworks_sdk_*" 2>/dev/null | head -n 1 | xargs dirname 2>/dev/null)
      fi
    fi

    if [ -z "$STEAMWORKS_HEADERS" ] || [ ! -d "$STEAMWORKS_HEADERS" ]; then
        echo -e "${RED}Error: Steamworks SDK headers not found. Cannot build Steamshim launcher.${NC}"
        exit 1
    fi
    echo -e "Found Steamworks SDK headers at: ${YELLOW}$STEAMWORKS_HEADERS${NC}"

    # Set up symlink to the headers
    echo "Creating Steamworks SDK symlink..."
    ln -sfn "$STEAMWORKS_HEADERS" "$SCRIPT_DIR/code/steamshim/launcher/steam"

    # Compile Steamshim launcher
    echo -e "${GREEN}Compiling Steamshim launcher natively...${NC}"
    mkdir -p "$SCRIPT_DIR/${BUILD_DIR_NAME}"
    if ! run_build_cmd g++ -o "$SCRIPT_DIR/${BUILD_DIR_NAME}/steamshim" \
        -Wall -O2 -DRELEASE=1 \
        "$SCRIPT_DIR/code/steamshim/launcher/steamshim_parent.cpp" \
        -I "$SCRIPT_DIR/code/steamshim/launcher" \
        -Wl,-rpath,'$ORIGIN' \
        "$STEAM_LIB_PATH/libsteam_api.so"; then
        echo -e "${RED}Failed to compile Steamshim launcher!${NC}"
        exit 1
    fi

    echo -e "${GREEN}Deploying Steamshim launcher to Steam directory...${NC}"
    cp -f "$SCRIPT_DIR/${BUILD_DIR_NAME}/steamshim" "$TARGET_DIR/steamshim"

    # Backup and replace Windows launcher.x64.exe with the native Linux ELF launcher
    if [ -f "$TARGET_DIR/launcher.x64.exe" ] && [ ! -f "$TARGET_DIR/launcher.x64.exe.bak" ]; then
        echo "Backing up original Windows launcher..."
        mv "$TARGET_DIR/launcher.x64.exe" "$TARGET_DIR/launcher.x64.exe.bak"
    fi
    cp -f "$SCRIPT_DIR/${BUILD_DIR_NAME}/steamshim" "$TARGET_DIR/launcher.x64.exe"

    # Symlink libsteam_api.so to the target directory
    if [ ! -f "$TARGET_DIR/libsteam_api.so" ]; then
        echo "Symlinking libsteam_api.so..."
        ln -sf "$STEAM_LIB_PATH/libsteam_api.so" "$TARGET_DIR/libsteam_api.so"
    fi

    # Ensure steam_appid.txt exists and contains the correct AppID
    echo "1379630" > "$TARGET_DIR/steam_appid.txt"
fi


END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

echo -e "${GREEN}Build and deployment completed in ${ELAPSED}s!${NC}"
echo -e "${BLUE}Executables deployed to:${NC}"
echo -e "  ${YELLOW}$TARGET_DIR/${NC}"

# 4. Optional Auto-Run
if [ "$RUN_AFTER_BUILD" = true ]; then
    if [ "$BUILD_STEAM" = true ] && [ -f "$LAUNCHER_DEST" ]; then
        echo -e "${GREEN}Launching RealRTCW (Steam mode) with arguments: ${RUN_ARGS[*]}...${NC}"
        cd "$TARGET_DIR"
        ./RealRTCW-native-launcher.sh "${RUN_ARGS[@]}"
    else
        echo -e "${GREEN}Launching RealRTCW (Standalone mode) with arguments: ${RUN_ARGS[*]}...${NC}"
        cd "$TARGET_DIR"
        ./RealRTCW.x86_64 "${RUN_ARGS[@]}"
    fi
fi
