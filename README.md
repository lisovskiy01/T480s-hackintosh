# T480s Hackintoshing guide
To make it easy, here's published complete set of necessary files and links to hackintosh your T480s. This is my own config, boots at my machine and works for me. It should boot at any T480s (maybe T480 too), but _I give no guarantee_. You can change and add any kexts and patches yourself, commits are always welcomed!
## What doesn't work
* **Wi-Fi & Bluetooth** - Intel card is not supported, use a dongle or get a compatible card (such as DW1560 BCM94352Z)
* **IR Cameras** - maybe __howdy for Linux__ can be ported, didn't try
* **Fingerprint Sensor** - disabled
* **F5-F12** - patch needed
* **TrackPoint** - can be patched with SSDT
* Power Management is weak, often running hot, sometimes with fan

## Guide
### Step 1: Prepare files and bootable USB
###### Download the installer
Use a MacBook or a macOS VM (find a guide online) to download and install dosdude1's [macOS Mojave Patcher Tool for Unsupported Macs](http://dosdude1.com/mojave/). Then, in the top bar, open **Tools>Download macOS Mojave...**. Place the installer in the /Applications folder. [Download macOS Mojave](https://raw.githubusercontent.com/lisovskiy01/T480s-hackintosh/master/src/donwloadMacOS.png)

###### Copy the installer
Use an 8-16GB USB stick. Using macOS's Disk Utility, format it as an 'MacOS Extended Journaled' named "Install macOS [version]".
Open Terminal. Run `sudo /Applications/Install\ macOS\ Mojave.app/Contents/Resources/createinstallmedia --volume /Volumes/Install\ macOS\ Mojave`

###### Install Clover
Download [Clover installer](https://sourceforge.net/projects/cloverefiboot/). Install it **ON THE USB DRIVE**, choosing these options:
[Clover installer scrnsht](https://raw.githubusercontent.com/lisovskiy01/T480s-hackintosh/master/src/CloverIntaller.png)
After that, copy files from CLOVER directory in this repository to the `/Volumes/EFI/EFI/CLOVER/`, replacing all existent files. Don't unmount the EFI partition yet

###### Generate config.plist
Run the `gen.sh` script (or `gen.bat` for Windows) **on the host T480s machine** from tools/ directory in this repository. Move the generated file to /Volumes/EFI/EFI/CLOVER, replacing the existent one. You can unmount the USB Drive now.

### Step 2: Set BIOS settings
Reboot your machine and access the BIOS settings. Restore default settings and change these:

| Main Menu | Sub 1 | Sub 2 | Sub 3 |
| --------- | ----- | ----- | ----- |
| Config | >> Security | >> Security Chip | Security Chip [DISABLED] |
|   |   | >> Fingerprint | Predesktop Authentication [DISABLED] |
|   |   | >> Virtualization | Intel Virtualization Technology [DISABLED] |
|   |   |   | Intel VT-d Feature [DISABLED] |
|   |   | >> I/O Port Access | Wireless WAN [DISABLED] |
|   |   |   | Memory Card Slot [DISABLED] |
|   |   |   | Fingerprint Reader [DISABLED] |
|   |   | >> Secure Boot Configuration | Secure Boot [DISABLED] |
|   |   | >> Intel SGX | Intel SGX Control [DISABLED] |
|   | >> Startup | UEFI/Legacy Boot [UEFI Only] |   |
|   |   | CSM Support [Yes] |   |

### Step 3: Install
Just proceed with the usual install.

### Step 4: Post-install
After booting and configuration, install Clover on the main hard drive selecting the same options, but do not reboot yet.
Run `diskutil list` and find the EFI partition of your hard drive and USB drive, mount them both, and copy **config.plist, kexts/Others/, ACPI/, drivers64UEFI and drivers/** from the USB Drive to EFI partition on the main hard drive. Reboot.

## Note
This config is not the best. Further ACPI patching and kexts are needed to be done, but this is the config done.

## References
* [tylernguyen's guide for X1C6 hackintosh](https://github.com/tylernguyen/x1c6-hackintosh)
* [kk1987's guide for T480s hackintosh](https://github.com/kk1987/T480s-hackintosh)
* [RehabMan's guide for SSDT\DSDT patching](https://www.tonymacx86.com/threads/guide-patching-laptop-dsdt-ssdts.152573/)


## P.S
I am not a hackintoshing pro, so this guide just explains how I did it. It's probably very wrong. If you know better, your commit is welcomed.
