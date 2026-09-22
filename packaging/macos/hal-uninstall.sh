#!/bin/bash
set -e
sudo rm -rf "/Library/Audio/Plug-Ins/HAL/LongpathVAX.driver"
sudo rm -rf "/Library/Audio/Plug-Ins/HAL/NereusSDRVAX.driver"   # der Name vor 2026-09-17
rm -f /dev/shm/longpath-vax-* /dev/shm/nereussdr-vax-* /dev/shm/nereussdr-tx 2>/dev/null || true
sudo killall coreaudiod 2>/dev/null || true
echo "Longpath VAX HAL plugin uninstalled."
