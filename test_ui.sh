#!/bin/bash
# Test script for UI - starts CC in background, then launches UI

echo "=== Space Skirmish UI Test ==="
echo "Starting command_center in background..."

# Start command_center in background
./command_center &
CC_PID=$!

# Wait for CC to initialize
sleep 1

echo "Starting UI (no run-dir needed - connects via IPC)..."
echo "UI shows 4 windows: [MAP] [UST] [UCM] [STD]"
echo "Press 'q' in UI to quit"
echo ""

# Start UI (will take over terminal with ncurses)
./ui

# When UI exits, stop CC gracefully
echo ""
echo "UI closed. Stopping command_center..."
kill -INT $CC_PID 2>/dev/null
wait $CC_PID 2>/dev/null

echo "Test complete."
