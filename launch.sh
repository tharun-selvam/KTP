#!/bin/bash
# launch.sh - Open six Terminal windows and run user1, user2, ..., user6

# Change directory to the location of your executables.
cd ~/Documents/Acads/Networks/A4

osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user1"'
osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user2"'
osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user3"'
osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user4"'
osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user5"'
osascript -e 'tell application "Terminal" to do script "cd \"'"$(pwd)"'\"; ./user6"'
