#!/bin/bash


DEV=HCSR_1      
trigger_pin=0
echo_pin=1
m=10        # Number of samples 
delta=100   # Sampling period

echo "Sending configuration parameters"

echo trigger_pin > "/sys/class/HCSR/$DEV/trigger"
echo -n "trigger pin: "
cat "/sys/class/HCSR/$DEV/trigger"

echo echo_pin > "/sys/class/HCSR/$DEV/echo"
echo -n "echo pin: "
cat "/sys/class/HCSR/$DEV/echo"

echo m > "/sys/class/HCSR/$DEV/number_samples"
echo -n "number of samples : "
cat "/sys/class/HCSR/$DEV/number_samples"

echo delta > "/sys/class/HCSR/$DEV/sampling_period"
echo -n "sampling period: "
cat "/sys/class/HCSR/$DEV/sampling_period"

echo "sending enable signal to start distance measurement"
echo 1 > "/sys/class/HCSR/$DEV/enable"
echo -n "enable: "
cat > "/sys/class/HCSR/$DEV/enable"

sleep 5

cat > "/sys/class/HCSR/$DEV/distance"
