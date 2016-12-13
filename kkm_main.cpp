/*
 * Это консольная программа для работы с ККМ ПРИМ-21К на основе термопринтера Custom VKP-80-II
 * первый аргумент - устройство последовательного порта,
 * затем "setup" для настройки порта, либо "iskra" или "vkp80ii" для передачи команд
 * последующие - команды и их аргументы.
 * Примеры:
 * kkm /dev/ttyUSB0 setup 9600
 * kkm /dev/ttyS0 iskra barcode_128c 18446744073709551615
 * kkm /dev/ttyS0 vkp80ii transmit_status
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

#include "kkm.h"

using namespace std;

int main(int argc, char **argv)
{
	int	fd;
	string	devicename;
	char	**params = new(char*[MAX_ARGS]);

	srand (time(NULL));
	
	if (argc > 3)
	{
		fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (fd < 0)
		{
			cout << "Failed to open device: " << argv[1] << endl;
			perror(argv[1]);
			return 1;
		}
/*// Enable this block to make continious files of command sequences
		if (lseek(fd, 0, SEEK_END) == -1)
		{
			cout << "Failed to lseek: returned -1" << endl;
			perror(argv[1]);
			return 1;
		}
*/		

		if (!strcmp(argv[2], "setup"))
		{
			cout << "Proceeding " << argv[2] << ": ";			
			if (!strcmp(argv[3], "1200") ||
				!strcmp(argv[3], "2400") ||
				!strcmp(argv[3], "4800") ||
				!strcmp(argv[3], "9600") ||
				!strcmp(argv[3], "19200") ||
				!strcmp(argv[3], "38400") ||
				!strcmp(argv[3], "57600") ||
				!strcmp(argv[3], "115200"))
			{
				switch (atoi(argv[3]))
				{
				case 1200:
					set_interface_attribs (fd, B1200, 0);
					cout << "1200 baud." << endl;
					break;
				case 2400:
					set_interface_attribs (fd, B2400, 0);
					cout << "2400 baud." << endl;
					break;
				case 4800:
					set_interface_attribs (fd, B4800, 0);
					cout << "4800 baud." << endl;
					break;
				case 9600:
					set_interface_attribs (fd, B9600, 0);
					cout << "9600 baud." << endl;
					break;
				case 19200:
					set_interface_attribs (fd, B19200, 0);
					cout << "19200 baud." << endl;
					break;
				case 38400:
					set_interface_attribs (fd, B38400, 0);
					cout << "38400 baud." << endl;
					break;
				case 57600:
					set_interface_attribs (fd, B57600, 0);
					cout << "57600 baud." << endl;
					break;
				case 115200:
					set_interface_attribs (fd, B115200, 0);
					cout << "115200 baud." << endl;
					break;
				}
			}
			else
			{
				cout << "wrong speed." << endl;
				cout << "Use 1200 2400 4800 9600 19200 38400 57600 115200 values." << endl;
			}
		}
		else if (!strcmp(argv[2], "iskra") || !strcmp(argv[2], "vkp80ii"))
		{
			params =  &argv[3];
			cout << "Proceeding " << argv[2] << ": " << argv[3] << endl;
//			set_params(1, &params, "transmit_info");
//			set_params(2, &params, "skl_dump", "2");
//			perform_kkm(fd, argv[2], params, 2);
			perform_kkm(fd, argv[2], params, argc-3);
		}
		else
		{
			cout << "To see usage start \"kkm\" without parameters " << endl;
		}

		close( fd );		
	}
	else
	{
		cout << "Usage:" << endl <<
			"kkm [serial_device] [command] [parameters...]" << endl << endl <<
			"kkm /dev/ttyUSB0 setup 9600" << endl <<
			"kkm /dev/ttyS0 iskra barcode_128c 18446744073709551615" << endl <<
			"kkm /dev/ttyS0 vkp80ii transmit_status" << endl;
	}

	return 0;
}
