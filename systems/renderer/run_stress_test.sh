#!/bin/bash

echo "=================================================================================="
echo "                    RENDERER STRESS TEST LAUNCHER                                "
echo "=================================================================================="
echo ""
echo "Preparing environment for accurate measurements..."
echo ""

# Check if we can set CPU governor (requires sudo)
if command -v cpupower &> /dev/null; then
    echo "Setting CPU governor to performance mode (may require sudo)..."
    sudo cpupower frequency-set -g performance 2>/dev/null || {
        echo "WARNING: Could not set CPU governor to performance mode"
        echo "         For best results, run: sudo cpupower frequency-set -g performance"
    }
fi

# Try to disable compositor if using X11
if [ "$XDG_SESSION_TYPE" = "x11" ]; then
    echo "Detected X11 session"
    
    # Check for common compositors
    if pgrep -x "compton" > /dev/null; then
        echo "Pausing Compton compositor..."
        killall -STOP compton 2>/dev/null
        trap "killall -CONT compton 2>/dev/null" EXIT
    elif pgrep -x "picom" > /dev/null; then
        echo "Pausing Picom compositor..."
        killall -STOP picom 2>/dev/null
        trap "killall -CONT picom 2>/dev/null" EXIT
    fi
fi

# Set process priority
echo "Setting high process priority..."
nice -n -10 ./renderer_stress_test 2>/dev/null || ./renderer_stress_test

echo ""
echo "Stress test completed."

# Reset CPU governor if we changed it
if command -v cpupower &> /dev/null; then
    echo "Resetting CPU governor..."
    sudo cpupower frequency-set -g ondemand 2>/dev/null || \
    sudo cpupower frequency-set -g powersave 2>/dev/null
fi