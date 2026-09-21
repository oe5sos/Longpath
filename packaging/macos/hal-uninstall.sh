#!/bin/bash
set -e
sudo rm -rf "/Library/Audio/Plug-Ins/HAL/LongpathVAX.driver"
rm -f /dev/shm/longpath-vax-* /dev/shm/longpath-vax-tx /dev/shm/nereussdr-vax-* /dev/shm/nereussdr-tx 2>/dev/null || true
sudo killall coreaudiod 2>/dev/null || true
echo "Longpath VAX HAL plugin uninstalled."
