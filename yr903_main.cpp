/**
 * \file
 * \brief RFID controller YR903 console utility
 * \author Vladimir Bogdanov
 * \version 1.0.0
 * \date November 2016
 * This is console utility for testing and demonstration purposes on working
 * with YR903 RFID controller and yr903.\* set of source code.
 *
 * Usage examples:
 * \$ yr903 /dev/ttyUSB0 setup 115200
 * \$ yr903 /dev/ttyUSB0 exec reset
 * \$ yr903 /dev/ttyUSB0 exec set_beeper_mode 2
 */

#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#include "yr903.h"

#define YR903_MAX_ARGS	10  ///< Max arguments allowed

int perform_exec(int fd, char **params, int params_count)
{
	using namespace std;

	char           get_buf[RCV_BUFFER_SIZE];
	unsigned char  par_buf [SND_BUFFER_SIZE];
	unsigned char  address;
	enum yr903_cmd command;

	memset(get_buf, 0, sizeof(get_buf));
	memset(par_buf, 0, sizeof(par_buf));

	if (!strcmp(params[0], "reset") && (params_count == 2))
	{
		command = YR903_CMD_reset;
		address = (char) atoi(params[1]); // address
		perform_yr903(fd, address, command, NULL, 0, &get_buf);//, 6);
	}
	else if (!strcmp(params[0], "get_firmware_version") && (params_count == 2))
	{
		command = YR903_CMD_get_firmware_version;
		address = (char) atoi(params[1]); // address
		perform_yr903(fd, address, command, NULL, 0, &get_buf);//, 7);
	}
	else if (!strcmp(params[0], "set_work_antenna") && (params_count == 3))
	{
		command = YR903_CMD_set_work_antenna;
		address = (char) atoi(params[1]); // address
		par_buf[0] = (char) atoi(params[2]); // working antenna
		perform_yr903(fd, address, command, par_buf, 1, &get_buf);//, 6);
	}
	else if (!strcmp(params[0], "set_beeper_mode") && (params_count == 3))
	{
		command = YR903_CMD_set_beeper_mode;
		address = (char) atoi(params[1]); // address
		par_buf[0] = (char) atoi(params[2]); // beeper mode
		perform_yr903(fd, address, command, par_buf, 1, &get_buf);//, 6);
	}
	else if (!strcmp(params[0], "real_time_inventory") && (params_count == 3))
	{
		command = YR903_CMD_real_time_inventory;
		address = (char) atoi(params[1]); // address
		par_buf[0] = (char) atoi(params[2]); // channel
		// Read all packets with labels (if any) and the service one.
		if (YR903_LABEL_PKT_LEN == \
			perform_yr903(fd, address, command, \
					par_buf, 1, &get_buf)) //, 6);
		{
			do
			{
				//Work with label data here
			}
			while (YR903_LABEL_PKT_LEN == \
				perform_yr903(fd, address, YR903_CMD_RECEIVE_ONLY, \
						NULL, 0, &get_buf));//, 6);
		}
	}
	else
	{
		cout << "Unexpected command or wrong arguments format: " \
			<< params[0] << endl;
	}
	cout << "Proceeding " << params[0] << " finished." << endl;

	return 0;
}

int main (int argc, char **argv)
{
	using namespace std;

	int	fd;
	string	devicename;
	char	**params = new (char*[YR903_MAX_ARGS]);
	int 	loopback = 0; // no incircuit loopback

	srand (time (NULL));

	if ((argc > 3) && (argc < YR903_MAX_ARGS + 3))
	{
		fd = open (argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (fd < 0)
		{
			cout << "Failed to open device: " << argv[1] << endl;
			perror (argv[1]);
			return 1;
		}

#ifdef YR903_USE_FILE
// Enable this block to make continious output files of command sequences
		if (lseek(fd, 0, SEEK_END) == -1)
		{
			cout << "Failed to lseek: returned -1" << endl;
			perror (argv[1]);
			return 1;
		}
#endif //YR903_USE_FILE

		if (!strcmp (argv[2], "setup"))
		{
			cout << "Proceeding " << argv[2] << ": ";
			if (!strcmp (argv[3], "1200") ||
				!strcmp (argv[3], "2400") ||
				!strcmp (argv[3], "4800") ||
				!strcmp (argv[3], "9600") ||
				!strcmp (argv[3], "19200") ||
				!strcmp (argv[3], "38400") ||
				!strcmp (argv[3], "57600") ||
				!strcmp (argv[3], "115200"))
			{
				if ((argc > 4) && (!strcmp (argv[4], "loopback")))
				{
					loopback = TIOCM_LOOP; // set incurcuit loopback
				}
				switch (atoi (argv[3]))
				{
				case 1200:
					set_interface_attribs (fd, B1200, 0, loopback);
					cout << "1200 baud." << endl;
					break;
				case 2400:
					set_interface_attribs (fd, B2400, 0, loopback);
					cout << "2400 baud." << endl;
					break;
				case 4800:
					set_interface_attribs (fd, B4800, 0, loopback);
					cout << "4800 baud." << endl;
					break;
				case 9600:
					set_interface_attribs (fd, B9600, 0, loopback);
					cout << "9600 baud." << endl;
					break;
				case 19200:
					set_interface_attribs (fd, B19200, 0, loopback);
					cout << "19200 baud." << endl;
					break;
				case 38400:
					set_interface_attribs (fd, B38400, 0, loopback);
					cout << "38400 baud." << endl;
					break;
				case 57600:
					set_interface_attribs (fd, B57600, 0, loopback);
					cout << "57600 baud." << endl;
					break;
				case 115200:
					set_interface_attribs (fd, B115200, 0, loopback);
					cout << "115200 baud." << endl;
					break;
				}
			}
			else
			{
				cout << "Wrong setup speed." << endl;
				cout << "Use 1200 2400 4800 9600 19200 38400 57600 115200 values." << endl;
			}
		}
		else if (!strcmp (argv[2], "exec"))
		{
			params =  &argv[3];
			cout << "Proceeding " << argv[2] << ": " << argv[3] << endl;
			perform_exec (fd, params, argc - 3);
		}
		else if (!strcmp (argv[2], "power"))
		{
			if (!strcmp (argv[3], "on"))
			{
				power_yr903 (YR903_POWERON);
			} else if (!strcmp (argv[3], "off"))
			{
				power_yr903 (YR903_POWEROFF);
			} else
			{
				cout << "Wrong power parameter." << endl;
				cout << "Use \"on\" or \"off\" values." << endl;
			}
		}
		else
		{
			cout << "To see usage start \"yr903\" without parameters " << endl;
		}

		close (fd);
	}
	else
	{
		cout << "Usage:" << endl <<
			"yr903 [serial_device] [action] [parameters...]" << endl << endl <<
			"yr903 /dev/ttyUSB0 setup 115200" << endl <<
			"yr903 /dev/ttyUSB0 setup 115200 loopback" << endl <<
			"yr903 /dev/ttyUSB0 power on" << endl <<
			"yr903 /dev/ttyUSB0 power off" << endl <<
			"yr903 /dev/ttyUSB0 exec reset 01" << endl <<
		        "yr903 /dev/ttyUSB0 exec set_beeper_mode 01 00" << endl <<
			"yr903 /dev/ttyUSB0 exec get_firmware_version 01" << endl <<
			"yr903 /dev/ttyUSB0 exec set_work_antenna 01 00" << endl <<
			"yr903 /dev/ttyUSB0 exec real_time_inventory 01 01" << endl;
	}

	return 0;
}
