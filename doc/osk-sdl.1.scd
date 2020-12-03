osk-sdl(1) "osk-sdl @@VERSION@@"

# NAME

OSK-SDL

# SYNOPSIS

osk-sdl -d DISK -n NAME [OPTION]

# DESCRIPTION

OSK-SDL is a lightweight on screen keyboard used to unlock encrypted root partitions on mobile devices.

# OPTIONS

## Mandatory

*-t*
	Run in test mode, do not attempt to initialise the whole screen.

*-d <path>*
	Decrypt this disk. This argument is mandatory unless \-t is used.

*-n <name>*
	Name of the decrypted disk.


## Optional

*-h*
	Show help options

*-c <path>*
	Locaton of the osk-sdl config file. Defaults to "/etc/osk.conf".

*-v*
	Enable verbose output

*-k*
	Enable keyscript mode. Password will be output to STDOUT for use with another program such as cryptsetup-initramfs.

# EXAMPLES

*Decrypt /dev/sda1 to name "root"*
	osk-sdl -d /dev/sda1 -n root -c /etc/osk.conf

# AUTHORS

*Undef* <debian@undef.tools>