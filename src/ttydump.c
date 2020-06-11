//	Displays bytes read from a specified serial port in various output formats
//	Optional color-coded output
//	Optional MIDI packet parsing
//	Optional ASCII character output
//	Optional single-line output
//	Optional raw binary output to file

#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/file.h>

//	Global constants
#define RX_BUFFER_SIZE 255
#define DEF_BAUD_RATE 115200
#define MIN_COLUMN_WIDTH 1
#define DEF_COLUMN_WIDTH 8
#define MAX_COLUMN_WIDTH 128
#define EXIT_UNLOCKED 1
#define EXIT_LOCKED 2
#define ESC_COLOR_GREEN "\033[32m"
#define ESC_COLOR_MAGENTA "\033[35m"
#define ESC_COLOR_YELLOW "\033[93m"
#define ESC_COLOR_MIDI_NOTE_ON "\033[32m"
#define ESC_COLOR_MIDI_NOTE_OFF "\033[35m"
#define ESC_COLOR_MIDI_CC "\033[36m"
#define ESC_COLOR_MIDI_PB "\033[93m"
#define ESC_COLOR_MIDI_AT "\033[94m"
#define ESC_COLOR_RESET "\033[0m"
#define ESC_CLEAR_OUTPUT "\e[1;1H\e[2J"
#define NANOSECONDS_PER_SECOND ((long)(1000000000l))

//	Subtract two timespec structures (sec, ms)
void timespec_sub(struct timespec *start, struct timespec *end,
	struct timespec *result) {
	if (end->tv_nsec > start->tv_nsec) {
		result->tv_nsec = end->tv_nsec - start->tv_nsec;
		result->tv_sec = end->tv_sec - start->tv_sec;
	} else if (end->tv_nsec < start->tv_nsec) {
		result->tv_nsec = NANOSECONDS_PER_SECOND - (start->tv_nsec - end->tv_nsec);
		result->tv_sec = end->tv_sec - start->tv_sec - 1;
	} else {
		result->tv_nsec = 0;
		result->tv_sec = end->tv_sec - start->tv_sec;
	}
}

//	Convert a struct timespec to a double in seconds
double timespec_dec(struct timespec *t) {
	return (double)(t->tv_sec + ((double)t->tv_nsec / (double)NANOSECONDS_PER_SECOND));
}

//	Convert baud rate to platform-defined constant
int convert_baud_rate(int rate) {
	switch (rate) {
		case 50:		return B50;
		case 75:		return B75;
		case 110:		return B110;
		case 134:		return B134;
		case 150:		return B150;
		case 200:		return B200;
		case 300:		return B300;
		case 600:		return B600;
		case 1200:		return B1200;
		case 1800:		return B1800;
		case 2400:		return B2400;
		case 4800:		return B4800;
		case 9600:		return B9600;
		case 19200:		return B19200;
		case 38400:		return B38400;
		case 57600:		return B57600;
		case 115200:	return B115200;
		case 230400:	return B230400;
		
#if (defined(__APPLE__) && !defined(_POSIX_C_SOURCE)) || defined(_DARWIN_C_SOURCE)
		case 7200:		return B7200;
		case 14400:		return B14400;
		case 28800:		return B28800;
		case 76800:		return B76800;
#endif  /* ((__APPLE__ && !_POSIX_C_SOURCE) || _DARWIN_C_SOURCE) */

#if (defined(__linux__) && defined(__ASM_GENERIC_POSIX_TYPES_H))
		case 460800:	return B460800;
		case 500000:	return B500000;
		case 576000:	return B576000;
		case 921600:	return B921600;
		case 1000000:	return B1000000;
		case 1152000:	return B1152000;
		case 1500000:	return B1500000;
		case 2000000:	return B2000000;
		case 2500000:	return B2500000;
		case 3000000:	return B3000000;
		case 3500000:	return B3500000;
		case 4000000:	return B4000000;
#endif	/* (__linux__) && (__ASM_GENERIC_POSIX_TYPES_H) */

		default:		return 0;
	}
}

//	Application context structure type
typedef struct {
	FILE *fd;
	int tty;
	struct timespec ts;
} app_context_t;

//	Command line options
typedef struct {
	uint8_t opt_p, opt_o, opt_w, opt_x, opt_c, opt_d, opt_z,
			opt_t, opt_n, opt_s, opt_a, opt_m, opt_h, opt_b;
	char *val_p, *val_o;
	uint8_t val_w;
	uint32_t val_b;
} cmd_options_t;

void print_usage(void) {
	printf(
		"Usage:\n"
		"-p  Device path            (required, example: /dev/cu.usbserial*)\n"
		"-b  Baud rate              (optional, default: %d)\n"
		"-o  Output filename        (optional, binary output file path)\n"
		"-w  Column width           (optional, %d-%d, default: %d bytes)\n"
		"-x  Single line output     (optional, default: off)\n"
		"-c  Color output           (optional, default: on)\n"
		"-d  Decimal output         (optional, default: off)\n"
		"-z  Zero prefix output     (optional, default: off)\n"
		"-t  Show timestamp         (optional, default: off)\n"
		"-n  Show time delta (ns)   (optional, default: off)\n"
		"-s  Show time delta (sec)  (optional, default: off)\n"
		"-a  ASCII output format\n"
		"-m  MIDI output format\n"
		"-h  Show command help\n",
		DEF_BAUD_RATE,
		MIN_COLUMN_WIDTH,
		MAX_COLUMN_WIDTH,
		DEF_COLUMN_WIDTH
	);
}

void print_options(cmd_options_t *opt) {
	//	Debug option parsing
	fprintf(stderr, "Options:\n"
		"-p: %d, %s\n"
		"-b: %d, %d\n"
		"-o: %d, %s\n"
		"-w: %d, %d\n"
		"-x: %d\n"
		"-c: %d\n"
		"-d: %d\n"
		"-z: %d\n"
		"-t: %d\n"
		"-n: %d\n"
		"-s: %d\n"
		"-a: %d\n"
		"-m: %d\n",
		opt->opt_p, (opt->opt_p) ? opt->val_p : "(null)",
		opt->opt_b, opt->val_b,
		opt->opt_o, (opt->opt_o) ? opt->val_o : "(null)",
		opt->opt_w, opt->val_w,
		opt->opt_x,
		opt->opt_c,
		opt->opt_d,
		opt->opt_z,
		opt->opt_t,
		opt->opt_n,
		opt->opt_s,
		opt->opt_a,
		opt->opt_m
	);
}

void print_timestamp(app_context_t *app, cmd_options_t *opt) {
	//	Read the current system time
	struct timespec ts, td;
	clock_gettime(CLOCK_REALTIME, &ts);
	//	Print the current timestamp
	if (opt->opt_t) {
		fprintf(stderr, "%ld: ", ts.tv_sec * NANOSECONDS_PER_SECOND + ts.tv_nsec);
	}
	//	Print the time in seconds difference since the last timestamp
	if (opt->opt_n || opt->opt_s) {
		if (app->ts.tv_sec == 0 && app->ts.tv_nsec == 0) {
			app->ts.tv_sec = ts.tv_sec;
			app->ts.tv_nsec = ts.tv_nsec;
		}
		timespec_sub(&app->ts, &ts, &td);
		if (opt->opt_n) {
			fprintf(stderr, "+%012ld: ", td.tv_sec * NANOSECONDS_PER_SECOND + td.tv_nsec);
		}
		if (opt->opt_s) {
			fprintf(stderr, "%.6f: ", timespec_dec(&td));
		}
	}
	//	Store the current timestamp back to the application context
	app->ts.tv_sec = ts.tv_sec;
	app->ts.tv_nsec = ts.tv_nsec;
}

void print_byte_midi(uint8_t *p, app_context_t *app, cmd_options_t *opt) {
	//	Determine if this is a status byte
	if (*p & 0x80) {
		//	If so, start a new line or clear output
		if (opt->opt_x) {
			fprintf(stderr, ESC_CLEAR_OUTPUT);
		} else {
			fprintf(stderr, "\n");
		}
		//	Print a timestamp and/or time difference if option is selected
		if (opt->opt_t || opt->opt_n || opt->opt_s) {
			print_timestamp(app, opt);
		}
	}
	
	//	Color MIDI status bytes if option is enabled
	if (opt->opt_c) {
		switch (*p & 0xF0) {
			//	Note on (green)
			case 0x90:
				fprintf(stderr, ESC_COLOR_MIDI_NOTE_ON);
				break;
			//	Note off (magenta)
			case 0x80:
				fprintf(stderr, ESC_COLOR_MIDI_NOTE_OFF);
				break;
			//	CC (cyan)
			case 0xb0:
				fprintf(stderr, ESC_COLOR_MIDI_CC);
				break;
			//	Aftertouch (blue)
			case 0xd0:
				fprintf(stderr, ESC_COLOR_MIDI_AT);
				break;
			//	Pitch bend (yellow)
			case 0xe0:
				fprintf(stderr, ESC_COLOR_MIDI_PB);
				break;
			//	Default (clear)
			default:
				fprintf(stderr, ESC_COLOR_RESET);
				break;
		}
	}
	
	//	Print byte to terminal based on format options
	if (opt->opt_z) {
		//	Right aligned, zero prefixed
		if (opt->opt_d) {
			//	Decimal, right aligned, zero prefixed
			fprintf(stderr, "%03d ", *p);
		} else {
			//	Hexadecimal, right aligned, zero prefixed
			fprintf(stderr, "%02x ", *p);
		}
	} else {
		//	Right aligned
		if (opt->opt_d) {
			//	Decimal, right aligned
			fprintf(stderr, "%3d ", *p);
		} else {
			//	Hexadecimal, right aligned
			fprintf(stderr, "%2x ", *p);
		}
	}
}

void print_byte_ascii(uint8_t *p, app_context_t *app, cmd_options_t *opt) {
	static uint8_t last_char = 0;
	static uint8_t byte_count = 0;
	
	//	Clear screen after newline if single line mode is enabled
	if (opt->opt_x && last_char == '\n') {
		fprintf(stderr, ESC_CLEAR_OUTPUT);
	}
	
	//	Print a timestamp and/or time difference if either option is enabled
	if ((opt->opt_t || opt->opt_n || opt->opt_s) && (last_char == '\n' || last_char == 0)) {
		print_timestamp(app, opt);
	}
	
	//	If printable, print character, otherwise print escaped
	if ((isprint(*p) || iscntrl(*p)) && *p != '\\') {
		//	If last character was non-printable, start a new line or clear the screen
		if ((!isprint(last_char) && !iscntrl(last_char)) || last_char == '\\') {
			if (opt->opt_x) {
				fprintf(stderr, ESC_CLEAR_OUTPUT);
			} else {
				fprintf(stderr, "\n");
			}
			//	Print a timestamp and/or time difference if option is selected
			if (opt->opt_t || opt->opt_n || opt->opt_s) {
				print_timestamp(app, opt);
			}
		}
		//	Print the printable character
		fprintf(stderr, "%c", *p);
		//	Reset non-printable byte count if we get a printable character
		byte_count = 0;
	} else {
		//	Print newline or clear screen at the start of a non-printable character sequence
		if (byte_count == 0) {
			if (opt->opt_x) {
				fprintf(stderr, ESC_CLEAR_OUTPUT);
			} else {
				fprintf(stderr, "\n");
			}
			//	Print a timestamp and/or time difference if option is selected
			if (opt->opt_t || opt->opt_n || opt->opt_s) {
				print_timestamp(app, opt);
			}
		}
		//	Print non-printable byte based on options
		if (opt->opt_c) {
			//	Color output
			if (opt->opt_d) {
				//	Color, decimal format
				fprintf(stderr,"%s\\%03d%s", ESC_COLOR_GREEN, *p, ESC_COLOR_RESET);
			} else {
				//	Color, hexadecimal format
				fprintf(stderr,"%s\\x%02x%s", ESC_COLOR_GREEN, *p, ESC_COLOR_RESET);
			}
		} else {
			//	Non-color output
			if (opt->opt_d) {
				//	Decimal format
				fprintf(stderr, "\\%03d", *p);
			} else {
				//	Hexadecimal format
				fprintf(stderr, "\\x%02x", *p);
			}
		}
		
		//	Increment byte count for non-printable characters
		byte_count++;
		if (byte_count >= opt->val_w) {
			byte_count = 0;
		}
	}
	
	//	Save character for comparison next function call
	last_char = *p;
}

void print_byte_raw(uint8_t *p, app_context_t *app, cmd_options_t *opt) {
	static uint8_t byte_count = 0;
	
	//	Determine if this is a new line
	if (byte_count == 0) {
		//	If so, start a new line or clear output
		if (opt->opt_x) {
			fprintf(stderr, ESC_CLEAR_OUTPUT);
		} else {
			fprintf(stderr, "\n");
		}
		//	Print a timestamp and/or time difference if option is selected
		if (opt->opt_t || opt->opt_n || opt->opt_s) {
			print_timestamp(app, opt);
		}
	}
	
	//	Print byte to terminal based on format options
	if (opt->opt_z) {
		//	Right aligned, zero prefixed
		if (opt->opt_d) {
			//	Decimal, right aligned, zero prefixed
			fprintf(stderr, "%03d ", *p);
		} else {
			//	Hexadecimal, right aligned, zero prefixed
			fprintf(stderr, "%02x ", *p);
		}
	} else {
		//	Right aligned
		if (opt->opt_d) {
			//	Decimal, right aligned
			fprintf(stderr, "%3d ", *p);
		} else {
			//	Hexadecimal, right aligned
			fprintf(stderr, "%2x ", *p);
		}
	}
	
	//	Increment byte count and reset if count = column width
	byte_count++;
	if (byte_count >= opt->val_w) {
		byte_count = 0;
	}
}

//	Configure options
int config_opt(int argc, char **argv, app_context_t *app, cmd_options_t *opt) {
	int i;
	
	if (argc < 2) {
		print_usage();
		return -1;
	}
	
	//	Initialize data structures
	memset((void*)app, 0, sizeof(app_context_t));
	memset((void*)opt, 0, sizeof(cmd_options_t));
	
	//	Parse command line options
	while ((i = getopt(argc, argv, "xcdztnsamhp:b:o:w:")) != -1) {
		switch (i) {
			case 'x':
				opt->opt_x = 1;
				break;
			case 'c':
				opt->opt_c = 1;
				break;
			case 'd':
				opt->opt_d = 1;
				break;
			case 'z':
				opt->opt_z = 1;
				break;
			case 't':
				opt->opt_t = 1;
				break;
			case 'n':
				opt->opt_n = 1;
				break;
			case 's':
				opt->opt_s = 1;
				break;
			case 'a':
				opt->opt_a = 1;
				break;
			case 'm':
				opt->opt_m = 1;
				break;
			case 'h':
				print_usage();
				return 0;
			case 'p':
				opt->opt_p = 1;
				opt->val_p = strdup(optarg);
				break;
			case 'b':
				opt->opt_b = 1;
				opt->val_b = (uint32_t) strtol(optarg, NULL, 10);
				break;
			case 'o':
				opt->opt_o = 1;
				opt->val_o = strdup(optarg);
				break;
			case 'w':
				opt->opt_w = 1;
				opt->val_w = (uint8_t) strtol(optarg, NULL, 10);
				break;
			case '?':
				switch (optopt) {
					case 'p':
					case 'b':
					case 'o':
					case 'w':
						fprintf(stderr, "%sError%s: Option '%c' requires a value\n",
							ESC_COLOR_MAGENTA,
							ESC_COLOR_RESET,
							optopt);
						break;
					default:
						fprintf(stderr, "%sError%s: Unknown option '%c'\n",
							ESC_COLOR_MAGENTA,
							ESC_COLOR_RESET,
							optopt);
						break;
				}
				return -1;
			default:
				fprintf(stderr, "Error parsing command line options\n");
				print_usage();
				return -1;
		}
	}
	
	//	Check for required options
	if (!opt->opt_p) {
		fprintf(stderr,
			"%sError%s: '-p' (device path) option required\n",
			ESC_COLOR_MAGENTA,
			ESC_COLOR_RESET
		);
		print_usage();
		return -1;
	}
	
	//	Check for option conflicts
	if (opt->opt_a && opt->opt_m) {
		fprintf(stderr,
			"%sError%s: '-a' (ASCII) and '-m' (MIDI) output formats are exclusive\n",
			ESC_COLOR_MAGENTA,
			ESC_COLOR_RESET
		);
		print_usage();
		return -1;
	}
	
	//	Check for superfluous options
	if (opt->opt_m && opt->opt_w) {
		fprintf(stderr,
			"%sWarning%s: '-w' (Column width) does not apply to '-m' (MIDI) output option\n",
			ESC_COLOR_YELLOW,
			ESC_COLOR_RESET
		);
	}
	if (!opt->opt_m && !opt->opt_a && opt->opt_c) {
		fprintf(stderr,
			"%sWarning%s: '-c' (Color output) requires '-m' (MIDI) or '-a' (ASCII) option\n",
			ESC_COLOR_YELLOW,
			ESC_COLOR_RESET
		);
	}
	if (opt->opt_z && opt->opt_a) {
		fprintf(stderr,
			"%sWarning%s: '-z' (Zero-prefix) does not apply to '-a' (ASCII) option\n",
			ESC_COLOR_YELLOW,
			ESC_COLOR_RESET
		);
	}
	
	//	Set default output format if invoked without any display options
	if (opt->opt_x | opt->opt_c | opt->opt_d | opt->opt_z | opt->opt_t |
		opt->opt_n | opt->opt_s | opt->opt_a | opt->opt_m == 0) {
		opt->opt_c = 1;
	}
	
	//	Validate baud rate if specified, otherwise set default
	if (opt->opt_b) {
		opt->val_b = convert_baud_rate(opt->val_b);
		if (!opt->val_b) {
			fprintf(stderr,
				"%sError%s: Unsupported baud rate '-b'\n",
				ESC_COLOR_MAGENTA,
				ESC_COLOR_RESET
			);
			print_usage();
			return -1;
		}
	} else {
		opt->val_b = convert_baud_rate(DEF_BAUD_RATE);
	}
	
	//	Set default column width for raw and ASCII output
	if (!opt->opt_m) {
		if (opt->opt_w) {
			if (opt->val_w < MIN_COLUMN_WIDTH || opt->val_w > MAX_COLUMN_WIDTH) {
				fprintf(stderr,
					"%sError%s: Invalid raw output width '-w', (%d-%d)\n",
					ESC_COLOR_MAGENTA,
					ESC_COLOR_RESET,
					MIN_COLUMN_WIDTH,
					MAX_COLUMN_WIDTH
				);
				print_usage();
				return -1;
			}
		} else {
			opt->val_w = DEF_COLUMN_WIDTH;
		}
	}
	
	return 0;
}

//	Configure tty
int config_tty(app_context_t *app, cmd_options_t *opt) {
	struct termios tty;
	int rc;
	
	//	Open device file descriptor
	fprintf(stderr, "Opening device %s...\n", opt->val_p);
	app->tty = open(opt->val_p, O_RDONLY | O_NOCTTY | O_SYNC);
	if (app->tty < 0) {
		fprintf(stderr, "%sError%s: Opening device %s (%d): %s\n",
			ESC_COLOR_MAGENTA,
			ESC_COLOR_RESET,
			opt->val_p, errno, strerror(errno));
		return EXIT_UNLOCKED;
	}
	fprintf(stderr, "Opened %s\n", opt->val_p);
	
	//	Apply exclusive, non-blocking advisory lock on tty once obtained
	rc = flock(app->tty, LOCK_EX | LOCK_NB);
	if (rc) {
		fprintf(stderr, "%sError%s: Couldn't obtain exclusive lock on '%s': %s\n",
			ESC_COLOR_MAGENTA,
			ESC_COLOR_RESET,
			opt->val_p, strerror(errno));
		return EXIT_UNLOCKED;
	}
	
	//	Check tty attributes
	rc = tcgetattr(app->tty, &tty);
	if (rc) {
		fprintf(stderr, "%s: Error: tcgetattr: %s\n", __func__, strerror(errno));
		return EXIT_LOCKED;
	}
	
	//	Initialize struct and set tty baud rate
	cfmakeraw(&tty);
	cfsetospeed(&tty, opt->val_b);
	cfsetispeed(&tty, opt->val_b);
	
	//	Configure tty
	tty.c_cflag |= (
		CREAD | CS8 | CLOCAL
	);
	
	//	Set to blocking single-character read()
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;
	
	//	Write configured tty attributes
	rc = tcsetattr(app->tty, TCSANOW, &tty);
	if (rc) {
		fprintf(stderr, "%s: Error: tcsetattr: %s\n", __func__, strerror(errno));
		return EXIT_LOCKED;
	}
	
	//	Flush tty
	tcflush(app->tty, TCIOFLUSH);
	return 0;
}

int main(int argc, char **argv) {
	int len, count, rc;
	uint8_t *p, buffer[RX_BUFFER_SIZE];
	app_context_t app;
	cmd_options_t opt;
	
	//	Allow SIGINT to interrupt main read() loop
	sigaction(SIGINT, NULL, 0);
	
	//	Configure options
	rc = config_opt(argc, argv, &app, &opt);
	if (rc) {
		return rc;
	}
	
	#ifdef DEBUG_PRINT_OPTIONS
	print_options(&opt);
	#endif
	
	//	Open output file if option is specified
	if (opt.opt_o && opt.val_o) {
		fprintf(stderr, "Opening output file %s...\n", opt.val_o);
		app.fd = fopen(opt.val_o, "wb");
		if (!app.fd) {
			fprintf(stderr, "%sError%s: Couldn't open output file '%s': %s\n",
				ESC_COLOR_MAGENTA,
				ESC_COLOR_RESET,
				opt.val_o, strerror(errno));
			return -1;
		}
		fprintf(stderr, "Opened %s\n", opt.val_o);
	}
	
	//	Configure tty attributes
	fflush(stderr);
	rc = config_tty(&app, &opt);
	if (rc) {
		switch (rc) {
			case EXIT_UNLOCKED:
				goto exit_unlocked;
			default:
				goto exit_locked;
		}
	}
	
	//	Read bytes from tty and write formatted output to stderr
	while (1) {
		len = read(app.tty, buffer, sizeof(buffer) - 1);
		if (len > 0) {
			count = 0;
			p = buffer;
			while (1) {
				
				//	Print in specified output format
				if (opt.opt_m) print_byte_midi(p, &app, &opt);
				else if (opt.opt_a) print_byte_ascii(p, &app, &opt);
				else print_byte_raw(p, &app, &opt);
				
				//	Optionally write binary data to output file
				if (opt.opt_o && app.fd) {
					fwrite((void*)p, sizeof(uint8_t), 1, app.fd);
				}
				
				//	Increment read pointer
				p++;
				
				//	Determine if there are more bytes to process
				if (++count == len) {
					break;
				}
			}
		} else if (len < 0 && errno == EINTR) {
		//	Exit on read() interrupt
			fprintf(stderr, "\n");
			goto exit_locked;
		} else if (len < 0) {
			fprintf(stderr, "Read error: %s\n", strerror(errno));
			return 0;
		} else {
			fprintf(stderr, "Read timeout\n");
			return 0;
		}
		fflush(stderr);
		fflush(app.fd);
	}
	
	//	Remove advisory lock on tty file descriptor
	exit_locked:
	rc = flock(app.tty, LOCK_UN);
	if (rc) {
		fprintf(stderr, "Couldn't unlock '%s': %s\n",
			opt.val_p, strerror(errno));
	}
	
	//	Close file descriptors
	exit_unlocked:
	close(app.tty);
	fclose(app.fd);
	
	//	Free implicitly allocated strings
	if (opt.val_p) {
		free(opt.val_p);
	}
	if (opt.val_o) {
		free(opt.val_o);
	}
	
	return 0;
}
