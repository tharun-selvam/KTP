#!/bin/bash
# launch.sh - Open six Terminal windows arranged in a 2x3 grid and run user1, user2, ..., user6

# Change directory to the location of your executables.
cd ~/Documents/Acads/Networks/A4

# For a screen of 1440x900, we define:
# - Row height: 450
# - Column width: 480
# Coordinates are given as {left, top, right, bottom}
# Window positions:
#   Window 1 (user1): top-left: {0, 0, 480, 450}
#   Window 2 (user2): top-middle: {480, 0, 960, 450}
#   Window 3 (user3): top-right: {960, 0, 1440, 450}
#   Window 4 (user4): bottom-left: {0, 450, 480, 900}
#   Window 5 (user5): bottom-middle: {480, 450, 960, 900}
#   Window 6 (user6): bottom-right: {960, 450, 1440, 900}

osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user1"' \
          -e 'tell application "Terminal" to set bounds of front window to {0, 0, 480, 450}'

osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user2"' \
          -e 'tell application "Terminal" to set bounds of front window to {480, 0, 960, 450}'

osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user3"' \
          -e 'tell application "Terminal" to set bounds of front window to {960, 0, 1440, 450}'

osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user4"' \
          -e 'tell application "Terminal" to set bounds of front window to {0, 450, 480, 900}'

osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user5"' \
          -e 'tell application "Terminal" to set bounds of front window to {480, 450, 960, 900}'

osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user6"' \
          -e 'tell application "Terminal" to set bounds of front window to {960, 450, 1440, 900}'
