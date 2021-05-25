osk-sdl(1) "osk-sdl"

# NAME

OSK-SDL

# SYNOPSIS

osk-sdl -d DISK -n NAME [OPTION]

# DESCRIPTION

OSK-SDL is a lightweight on screen keyboard used to unlock encrypted root partitions on mobile devices.

# OPTIONS

## Mandatory

*-t, --testmode*
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

*-o <path>*
	Locaton of the osk-sdl config override file. Does not cause a failure if it couldn't be loaded.

*-v, --verbose*
	Enable verbose output

*-k, --keyscript*
	Enable keyscript mode. Password will be output to STDOUT for use with another program such as cryptsetup-initramfs.

*-G, --no-gles*
	Do not use OpenGL ES driver unless it is the default driver. This is the default behavior when using SW rendering.

*-s, --software-rendering*
	Force software rendering, instead of hardware accelerated graphics.

# EXAMPLES

*Decrypt /dev/sda1 to name "root"*
	osk-sdl -d /dev/sda1 -n root -c /etc/osk.conf

# SEE ALSO
	*osk.conf*(5)

# AUTHORS

*Undef* <debian@undef.tools>
