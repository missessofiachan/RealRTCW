#!/usr/bin/env bash

# Exit immediately if a command fails
set -e

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Color codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== RealRTCW Unit Test Launcher ===${NC}"

# Delegate directly to compile.sh --test
exec ./compile.sh --test "$@"
