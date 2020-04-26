# ttydump

Display bytes read from a specified serial port in various output formats.
Kind of like `hexdump`, but for your serial port.

## Usage

Argument | Option | Comment
--- | --- | ---
`-p` | Device path | **Required**, example: `/dev/cu.usbserial-DEADA55`
`-b <baud>` | Baud rate | *Optional*, default: `115200`
`-o <filename>` | Output filename | *Optional*, binary output file path, example: `~/path/to/file.out`
`-w <columns>` | Column width | *Optional*, `1-128`, default: `8 bytes`
`-s` | Single line output | *Optional*, default: `off`
`-c` | Color output | *Optional*, default: `on`
`-d` | Decimal output | *Optional*, default: `off`
`-z` | Zero prefix output | *Optional*, default: `off`
`-a` | ASCII output format | Output ASCII printable characters + `\x00` style escaped bytes for non-printables
`-m` | MIDI output format | Interpret and display received bytes as MIDI packets
`-h` | Show command help | Show this list without opening a connection

## Prerequisites

* Serial port or usb-serial adapter
* Unix/Linux-based operating system with `/dev/cu.*` or `/dev/tty.*`
* `gcc` & `libc` (or similar)
* Optional: Color-enabled terminal emulator

## Compiling

The program is entirely contained within a single source (`src/ttydump.c`), so compile it as you please:
```
$ gcc ./src/ttydump.c -o ./bin/ttydump
```

## Installing

Copy or symlink the executable to a location in your `$PATH`, for example:

```
$ cp ./bin/ttydump /usr/local/bin/
$ ln -s ./bin/ttydump /usr/local/bin/ttydump
```

## Examples

Default output format (hexadecimal, 8-byte column width, 115200 baud)
Only the device path argument `-p` is required:
```
$ ttydump -p /dev/ttyUSB0
```

Decimal, zero-prefixed, 16 columns, 9600 baud:
```
$ ttydump -p /dev/ttyUSB0 -b 9600 -w 16 -z
```

MIDI, default baud rate (115200), color-coded status bytes, decimal:
```
$ ttydump -p /dev/cu.usbmodem001 -mcd
```

Hexadecimal, 4 columns, default baud rate (115200), additionally write raw binary data to output file:
```
$ ttydump -p /dev/cu.usbserial-DEADA55 -o ~/test/file.out
```

## Notes

* I have not tested extensively on any platforms other than macOS 10.12, macOS 10.14, and Ubuntu 18.04. Nonetheless, no special or OS-specific functionality is used (to my knowledge, other than the required platform-specific baud rate defines), and there are no dependencies outside of the standard C library, so it should hopefully compile and run.

* If you find a problem on *one of the above platforms*, please report it.

* The program uses an advisory lock mechanism, `flock()`, on the opened serial device, but unless this is also implemented in other utilities you are using (for example, `screen`), multiple processes may be able to open the device simultaneously, which can cause strange behavior. This is not unique to this utility.

* On Linux (and possibly others), make sure your user is a member of the group that owns the serial tty device (this is the `dialout` group by default on Ubuntu for example). You can add yourself to it with the following command: `sudo gpasswd --add ${USER} dialout`

