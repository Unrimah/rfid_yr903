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
#include <list>
#include <time.h>
#include <stdarg.h>

#include "kkm.h"

using namespace std;

int odd(int value)
{
	return (value & 1);
}

int set_params(int count, char ***params, const char *param1, ...)
{
	int i;
	if (count < 1) return 1;
	va_list vl;
	va_start(vl, param1);
	cout << "1" << endl;
	(*params)[0] = (char *) param1;
	cout << "1" << endl;
	for (i = 1; i < count; i++)
	{
	cout << "3, i=" << i << endl;
		(*params)[i] = (char *) va_arg(vl, const char*);
	cout << "4" << endl;
	}
	cout << "5" << endl;
	va_end(vl);
	return 0;
}
/*
The values for speed are B115200, B230400, B9600, B19200, B38400, B57600, B1200, B2400, B4800, etc. The values for parity are 0 (meaning no parity), PARENB|PARODD (enable parity and use odd), PARENB (enable parity and use even), PARENB|PARODD|CMSPAR (mark parity), and PARENB|CMSPAR (space parity).

"Blocking" sets whether a read() on the port waits for the specified number of characters to arrive. Setting no blocking means that a read() returns however many characters are available without waiting for more, up to the buffer limit.
*/
int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                printf ("error %d from tcgetattr", errno);
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 50;      //5;      // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
                printf("error %d from tcsetattr", errno);
                return -1;
        }
        return 0;
}

void set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                printf ("error %d from tggetattr", errno);
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 50;   // 5;         // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                printf ("error %d setting term attributes", errno);
}

int readport(int fd, char *buf, int len)
{
	int res = 0;
	int i = 0;

	while (((res = read (fd, buf, len)) < 0) && (errno == EAGAIN) && (i < RESPONCE_PERIOD))
	{
		i++;
		usleep( 1000 );
	}
	return res;
}

int perform_vkp80ii(int fd, const char *tosend, int length, char (*pbuf)[1100], int expected)
{
	char	get_buf [RCV_BUFFER_SIZE];
	int	i;
	int	res;
	int	received;

	memset( get_buf, 0, sizeof( get_buf));

	cout << "Sending " << length << " bytes:" << endl;
	for ( i = 0; i < length; i++ ) printf(" %2.2hhX", tosend[ i ]);

	res = write (fd, tosend, length); 
	cout << endl << res << " bytes sent." << endl;

	if ((expected == 0) || (pbuf == 0)) return 0;

	received = 0;
	while ((received < expected) && ((res = readport(fd, &get_buf[received], sizeof(get_buf))) >= 0))
	{
		received += res;
	}
	cout << "Resulted " << received << " of " << expected << " bytes: ";
	for ( i = 0; i < received; i++ ) 
	{
		printf(" %2.2hhX", get_buf[ i ]);
		(*pbuf)[i] = get_buf[i];
	}
	cout << endl;

	return received;
}

//calculate BCC for array[0..(count-1)]
//add BCC characters to array at [count + (0..3)] positions
int add_bcc(char *array, int count)
{
	int i;
	unsigned short bcc = 0;
	for (i = 0; i < count; i++)
	{
		bcc += (unsigned char) array[i];
	}
//	printf("BCC %2.2hhX", (bcc >> 8) & 0xFF);
//	printf(" %2.2hhX\n", bcc & 0xFF);
	if (0x39 < (array[count + 2] = ((bcc >> 12) & 0x0F) + 0x30))
		array[count + 2] += 7;
	if (0x39 < (array[count + 3] = ((bcc >> 8) & 0x0F) + 0x30))
		array[count + 3] += 7;
	if (0x39 < (array[count + 0] = ((bcc >> 4) & 0x0F) + 0x30))
		array[count + 0] += 7;
	if (0x39 < (array[count + 1] = (bcc & 0x0F) + 0x30))
		array[count + 1] += 7;
	return 0;
}

//type data block to console, stop at 0x1C symbol
int type_data_1C(char *data)
{
	int i=0;
	while (data[i] != 0x1C)
		printf("%c", data[i++]);
	return ++i;
}

int perform_iskra(int fd, list<string> data, char (*pbuf)[RCV_BUFFER_SIZE], int expected)
{
	char	get_buf[RCV_BUFFER_SIZE];
	char	tosend [SND_BUFFER_SIZE];
	int	i;
	int	res;
	int	received;
	int 	length;
	string	item;
	unsigned char	mark_byte = rand() % 0xE0 + 0x20;

	if (data.empty()) return 0;

	memset( get_buf, 0, sizeof( get_buf));

	tosend[0] = 0x02; //STX
	tosend[1] = 'A';
	tosend[2] = 'E';
	tosend[3] = 'R';
	tosend[4] = 'F';
	tosend[5] = (char) mark_byte;
	length = 6;
	while (!data.empty())
	{
		item = data.front();
		for (i = 0; i < (signed) item.length(); i++)
		{
			tosend[length++] = item[i];
		}
		tosend[length++] = 0x1C; // Separator
		data.pop_front();
	}
	tosend[length++] = 0x03; //ETX
	add_bcc(tosend, length);
	length += 4; // BCC length

	cout << "Sending " << length << " bytes:" << endl;
	for ( i = 0; i < length; i++ ) printf(" %2.2hhX", tosend[ i ]);
	res = write (fd, tosend, length); 
	cout << endl << res << " bytes sent." << endl;

	cout << "String view: " << endl;
	for ( i = 0; i < length; i++ ) 
	{
		printf(" %c", tosend[ i ]);
	}
	cout << endl;

	if ((expected == 0) || (pbuf == 0)) return 0;

	received = 0;
	while ((received < expected) && ((res = readport(fd, &get_buf[received], sizeof(get_buf))) >= 0))
	{
		received += res;
	}
	cout << "Resulted " << received << " of " << expected << " bytes: ";
	for ( i = 0; i < received; i++ ) 
	{
		printf(" %2.2hhX", get_buf[ i ]);
		(*pbuf)[i] = get_buf[i];
	}
	cout << endl;
	(*pbuf)[received] = 0;
	cout << "String view: " << endl;
	for ( i = 0; i < received; i++ ) 
	{
		printf(" %c", get_buf[ i ]);
	}
	cout << endl;

	if ((received >= 34) && (get_buf[0] == 0x02))
	{
		length = 2;
		i = 0;
		printf("\nMark byte:      0x%2.2hhX", get_buf[1]);
		printf("\nCommand:        "); length += type_data_1C(&get_buf[length]);
		printf("\nConst state:    "); length += type_data_1C(&get_buf[length]);
		printf("\nCurr state:     "); length += type_data_1C(&get_buf[length]);
		printf("\nCommand result: "); length += type_data_1C(&get_buf[length]);
		printf("\nPrinter state:  "); length += type_data_1C(&get_buf[length]);
		while (get_buf[length] != 0x03)
		{
			printf("\nData block [%i]: ", i); length += type_data_1C(&get_buf[length]);
			i++;
		}
	}
	else
		cout << "STX not received!" << endl;

	cout << endl << endl;
	return received;
}

int perform_kkm(int fd, char *kkm, char **params, int params_count)
{
	char	get_buf[RCV_BUFFER_SIZE];
	char	tosend [SND_BUFFER_SIZE];
	time_t rawtime;
	struct tm * timeinfo;
	list<string> iskra_data;
	int i;

	memset(get_buf, 0, sizeof(get_buf));
	memset(tosend, 0, sizeof(tosend));

	time (&rawtime);
	timeinfo = localtime (&rawtime);

	if (!strcmp(kkm, "iskra"))
	{
		if (!strcmp(params[0], "session_start") && (params_count == 1))
		{
			iskra_data.push_back("01");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "dayshift_start") && (params_count == 2))
		{
			string shift_info(params[1]);

			iskra_data.push_back("02");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			utf8tocpp866(&shift_info);
			iskra_data.push_back(shift_info.c_str());
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "transmit_resources") && (params_count == 1))
		{
			iskra_data.push_back("03");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			perform_iskra(fd, iskra_data, &get_buf, 59);
		}
		else if (!strcmp(params[0], "transmit_numbers") && (params_count == 1))
		{
			iskra_data.push_back("35");
			perform_iskra(fd, iskra_data, &get_buf, 49);

		}
		else if (!strcmp(params[0], "print_log") && (params_count == 1))
		{
			iskra_data.push_back("72");
			perform_iskra(fd, iskra_data, &get_buf, 34);

		}
		else if (!strcmp(params[0], "transmit_serial_no") && (params_count == 1))
		{
			iskra_data.push_back("96");
			perform_iskra(fd, iskra_data, &get_buf, 46);

		}
		else if (!strcmp(params[0], "transmit_info") && (params_count == 1))
		{
			iskra_data.push_back("97");
			perform_iskra(fd, iskra_data, &get_buf, 116);
		}
		else if (!strcmp(params[0], "sphere_change") && (params_count > 1))
		{
			iskra_data.push_back("48");
			sprintf(tosend, "%02d", atoi(params[1]));
			iskra_data.push_back(tosend);
			if (params_count > 2)
			{
				string operator_info(params[2]);
				utf8tocpp866(&operator_info);
				iskra_data.push_back(operator_info.c_str());				
			}
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "skl_get") && (params_count == 1))
		{
			iskra_data.push_back("49");
			perform_iskra(fd, iskra_data, &get_buf, 48);
		}
		else if (!strcmp(params[0], "skl_print") && (params_count <= 2))
		{
			iskra_data.push_back("84");
			if (params_count == 2)
			{
				sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[1]) & 0x0F), (atoi(params[1]) >> 4));
				iskra_data.push_back(tosend);
			}
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "skl_erase") && (params_count == 1))
		{
			iskra_data.push_back("85");
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "skl_dump") && (params_count == 2))
		{
			iskra_data.push_back("86");
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[1]) & 0x0F), (atoi(params[1]) >> 4));
			iskra_data.push_back(tosend);
			perform_iskra(fd, iskra_data, &get_buf, 1091);
		}
		else if (!strcmp(params[0], "report_documents_by_time") && (params_count >= 3))
		{
			iskra_data.push_back("7D");
			iskra_data.push_back(params[1]);
			iskra_data.push_back(params[2]);
			if (params_count > 3)
			{
				iskra_data.push_back(params[3]);
			}
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "report_documents_by_numbers") && (params_count >= 3))
		{
			iskra_data.push_back("7E");
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[1]) & 0x0F), (atoi(params[1]) >> 4));
			iskra_data.push_back(tosend);
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[2]) & 0x0F), (atoi(params[2]) >> 4));
			iskra_data.push_back(tosend);
			if (params_count > 3)
			{
				sprintf(tosend, "%2.2hhX", (atoi(params[3]) & 0x0F));
				iskra_data.push_back(tosend);
			}
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "report_document_by_number") && (params_count >= 2))
		{
			iskra_data.push_back("7F");
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[1]) & 0x0F), (atoi(params[1]) >> 4));
			iskra_data.push_back(tosend);
			if (params_count > 2)
			{
				sprintf(tosend, "%2.2hhX", (atoi(params[2]) & 0x0F));
				iskra_data.push_back(tosend);
			}
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "fiscalization") && (params_count == 7))
		{
			iskra_data.push_back("04");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			iskra_data.push_back(params[1]); // old password
			iskra_data.push_back(params[2]); // new password
			iskra_data.push_back(params[3]); // new reg number
			iskra_data.push_back(params[4]); // new INN
			sprintf(tosend, "%2.2hhX", (atoi(params[5]) & 0x0F));
			iskra_data.push_back(tosend); // new shpere of business
			sprintf(tosend, "%2.2hhX", (atoi(params[6]) & 0x0F));
			iskra_data.push_back(tosend); // enable storing rezult value in fiscal memory
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "short_fiscal_report_by_dates") && (params_count == 4))
		{
			iskra_data.push_back("05");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			iskra_data.push_back(params[1]); // access password
			iskra_data.push_back(params[2]); // start date
			iskra_data.push_back(params[3]); // end date
			perform_iskra(fd, iskra_data, &get_buf, 72);
		}
		else if (!strcmp(params[0], "short_fiscal_report_by_numbers") && (params_count == 4))
		{
			iskra_data.push_back("06");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			iskra_data.push_back(params[1]); // access password
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[2]) & 0x0F), (atoi(params[2]) >> 4));
			iskra_data.push_back(tosend); //start number
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[3]) & 0x0F), (atoi(params[3]) >> 4));
			iskra_data.push_back(tosend); //end number
			perform_iskra(fd, iskra_data, &get_buf, 72);
		}
		else if (!strcmp(params[0], "full_fiscal_report_by_dates") && (params_count == 4))
		{
			iskra_data.push_back("07");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			iskra_data.push_back(params[1]); // access password
			iskra_data.push_back(params[2]); // start date
			iskra_data.push_back(params[3]); // end date
			perform_iskra(fd, iskra_data, &get_buf, 72);
		}
		else if (!strcmp(params[0], "full_fiscal_report_by_numbers") && (params_count == 4))
		{
			iskra_data.push_back("08");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			iskra_data.push_back(params[1]); // access password
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[2]) & 0x0F), (atoi(params[2]) >> 4));
			iskra_data.push_back(tosend); //start number
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[3]) & 0x0F), (atoi(params[3]) >> 4));
			iskra_data.push_back(tosend); //end number
			perform_iskra(fd, iskra_data, &get_buf, 72);
		}
		else if (!strcmp(params[0], "eklz_activate") && (params_count == 1))
		{
			iskra_data.push_back("09");
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "eklz_close_archive") && (params_count == 1))
		{
			iskra_data.push_back("8D");
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "eklz_activation_result") && (params_count == 1))
		{
			iskra_data.push_back("8F");
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "short_report_closing_shifts_by_dates") && (params_count == 3))
		{
			iskra_data.push_back("88");
			iskra_data.push_back("05"); //short report
			iskra_data.push_back(params[1]); //start date
			iskra_data.push_back(params[2]); //end date
			iskra_data.push_back("00"); //? section number (reserved)
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "full_report_closing_shifts_by_dates") && (params_count == 3))
		{
			iskra_data.push_back("88");
			iskra_data.push_back("04"); //full report
			iskra_data.push_back(params[1]); //start date
			iskra_data.push_back(params[2]); //end date
			iskra_data.push_back("00"); //? section number (reserved)
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "short_report_closing_shifts_by_numbers") && (params_count == 3))
		{
			iskra_data.push_back("89");
			iskra_data.push_back("07"); //short report
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[1]) & 0x0F), (atoi(params[1]) >> 4));
			iskra_data.push_back(tosend); //start number
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[2]) & 0x0F), (atoi(params[2]) >> 4));
			iskra_data.push_back(tosend); //end number
			iskra_data.push_back("00"); //? section number (reserved)
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "full_report_closing_shifts_by_numbers") && (params_count == 3))
		{
			iskra_data.push_back("89");
			iskra_data.push_back("06"); //full report
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[1]) & 0x0F), (atoi(params[1]) >> 4));
			iskra_data.push_back(tosend); //start number
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[2]) & 0x0F), (atoi(params[2]) >> 4));
			iskra_data.push_back(tosend); //end number
			iskra_data.push_back("00"); //? section number (reserved)
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "control_sheet_by_shift_number") && (params_count == 2))
		{
			iskra_data.push_back("8C");
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[1]) & 0x0F), (atoi(params[1]) >> 4));
			iskra_data.push_back(tosend); //shift number
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "shift_results_by_shift_number") && (params_count == 2))
		{
			iskra_data.push_back("8E");
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[1]) & 0x0F), (atoi(params[1]) >> 4));
			iskra_data.push_back(tosend); //shift number
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "print_document_by_kpk") && (params_count == 2))
		{
			iskra_data.push_back("8A");
			sprintf(tosend, "%08d", atoi(params[1]));
			iskra_data.push_back(tosend); //kpk
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "get_last_kpk") && (params_count == 1))
		{
			iskra_data.push_back("3F");
			perform_iskra(fd, iskra_data, &get_buf, 43);
		}
		else if (!strcmp(params[0], "get_document_by_kpk") && (params_count == 2))
		{
			iskra_data.push_back("3E");
			sprintf(tosend, "%08d", atoi(params[1]));
			iskra_data.push_back(tosend); //kpk
			perform_iskra(fd, iskra_data, &get_buf, 1024);
		}
		else if (!strcmp(params[0], "eklz_dump") && (params_count == 2))
		{
			iskra_data.push_back("87");
			sprintf(tosend, "%2.2hhX%2.2hhX", (atoi(params[1]) & 0x0F), (atoi(params[1]) >> 4));
			iskra_data.push_back(tosend);
			perform_iskra(fd, iskra_data, &get_buf, 1091);
		}
		else if (!strcmp(params[0], "document_start") && (params_count == 5))
		{
			iskra_data.push_back("10");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			sprintf(tosend, "%02d", atoi(params[1]));
			iskra_data.push_back(tosend); //document type
			string printline(params[2]);
			utf8tocpp866(&printline);
			iskra_data.push_back(printline.c_str()); //operator's identifier
			sprintf(tosend, "%02X", atoi(params[3]));
			iskra_data.push_back(tosend); // copies number
			iskra_data.push_back(params[4]); //bill identifier
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "sale") && (params_count == 8))
		{
			string printline;
			iskra_data.push_back("11");
			printline = params[1];
			utf8tocpp866(&printline);
			iskra_data.push_back(printline.c_str()); //goods/service name
			iskra_data.push_back(params[2]); //goods/service article
			iskra_data.push_back(params[3]); //price
			iskra_data.push_back(params[4]); //quantity/weight
			printline = params[5];
			utf8tocpp866(&printline);
			iskra_data.push_back(printline.c_str()); //measure unit name (kg/pcs/etc)
			sprintf(tosend, "%02X", atoi(params[6]));
			iskra_data.push_back(tosend); // section index accoring to EKLZ
			printline = params[7];
			utf8tocpp866(&printline);
			iskra_data.push_back(printline.c_str()); //section name
			perform_iskra(fd, iskra_data, &get_buf, 64);
		}
		else if (!strcmp(params[0], "sale_to_section") && (params_count == 6))
		{
			iskra_data.push_back("18");
			sprintf(tosend, "%02X", atoi(params[1]));
			iskra_data.push_back(tosend); // section index accoring to EKLZ
			iskra_data.push_back("00"); //googs/service index (not used)
			iskra_data.push_back(params[2]); //price
			iskra_data.push_back(params[3]); //quantity/weight
			string printline(params[4]);
			utf8tocpp866(&printline);
			iskra_data.push_back(printline.c_str()); //measure unit name (kg/pcs/etc)
			iskra_data.push_back(params[5]); //goods/service article
			perform_iskra(fd, iskra_data, &get_buf, 64);
		}
		else if (!strcmp(params[0], "barcode128c") && (params_count == 2))
		{
			int barcode_len = strlen(params[1]);
			int step;
			int tmp;
			int index;

			iskra_data.push_back("1A"); // barcode cmd
			iskra_data.push_back("49"); // 49 - CODE128
			iskra_data.push_back("02"); // hri symbols under barcode
			iskra_data.push_back("00"); // hri font A
			iskra_data.push_back("A2"); // barcode height
			iskra_data.push_back("03"); // barcode width
			memset(tosend, 0, sizeof(tosend));
			tosend[0] = '7';
			tosend[1] = 'B'; //set code128c
			tosend[2] = '4';
			tosend[3] = '3'; //set code128c
			index = 4;
			if (odd(barcode_len))
			{
				tosend[index++] = '0';
				tosend[index] = (params[1][0] - 0x30) % 16 + 0x30;
				if (tosend[index] > 0x39) tosend[index]+= 7; // A - F
				index++;
			}
		
			for (step = odd(barcode_len); step < barcode_len; step+=2)
			{
				tmp = 10 * (params[1][step] - 0x30) + (params[1][step+1] - 0x30);
				tosend[index] = tmp / 16 + 0x30;
				if (tosend[index] > 0x39) tosend[index]+= 7; // A - F
				index++;
				tosend[index] = tmp % 16 + 0x30;
				if (tosend[index] > 0x39) tosend[index]+= 7; // A - F
				index++;
			}
			iskra_data.push_back(tosend); // barcode content
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "total") && (params_count == 1))
		{
			iskra_data.push_back("12");
			perform_iskra(fd, iskra_data, &get_buf, 49);
		}
		else if (!strcmp(params[0], "payment") && (params_count >= 3))
		{
			iskra_data.push_back("13");
			sprintf(tosend, "%02X", atoi(params[1]));
			iskra_data.push_back(tosend); // payment type
			iskra_data.push_back(params[2]); // payment amount
			if (params_count > 3)
				iskra_data.push_back(params[3]); // card name (VISA, MasterCard, etc)
			perform_iskra(fd, iskra_data, &get_buf, 64);
		}
		else if (!strcmp(params[0], "document_close") && (params_count == 1))
		{
			iskra_data.push_back("14");
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "discount") && (params_count == 4))
		{
			iskra_data.push_back("15");
			iskra_data.push_back("01"); //discount
			iskra_data.push_back(params[1]); // discount percents
			iskra_data.push_back(params[2]); // discount amount
			iskra_data.push_back(params[3]); // discount description
			perform_iskra(fd, iskra_data, &get_buf, 70);
		}
		else if (!strcmp(params[0], "markup") && (params_count == 4))
		{
			iskra_data.push_back("15");
			iskra_data.push_back("00"); //markup
			iskra_data.push_back(params[1]); // markup percents
			iskra_data.push_back(params[2]); // markup amount
			iskra_data.push_back(params[3]); // markup description
			perform_iskra(fd, iskra_data, &get_buf, 70);
		}
		else if (!strcmp(params[0], "subtotal") && (params_count == 1))
		{
			iskra_data.push_back("16");
			perform_iskra(fd, iskra_data, &get_buf, 49);
		}
		else if (!strcmp(params[0], "document_cancel") && (params_count == 5))
		{
			iskra_data.push_back("17");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "tax_rate") && (params_count == 2))
		{
			iskra_data.push_back("1B");
			sprintf(tosend, "%02X", atoi(params[1]));
			iskra_data.push_back(tosend); // tax rate index
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "comment") && (params_count == 2))
		{
			string printline(params[1]);
			utf8tocpp866(&printline);

			iskra_data.push_back("1C");
			iskra_data.push_back(printline.c_str()); // comment strings '|' separated
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "presenter_control") && (params_count == 4))
		{
			iskra_data.push_back("6F");
			sprintf(tosend, "%02X", atoi(params[1]));
			iskra_data.push_back(tosend); // 00 withdraw, 01 push
			sprintf(tosend, "%02X", atoi(params[2]));
			iskra_data.push_back(tosend); // 00 use presenter, 01 no presenter
			sprintf(tosend, "%02X", atoi(params[3]));
			iskra_data.push_back(tosend); // 00 set, 01 execute
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "switch_to_vkp80ii") && (params_count == 1))
		{
			iskra_data.push_back("70");
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "arbitrary_document_start") && (params_count == 1))
		{
			iskra_data.push_back("50");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "arbitrary_comment") && (params_count == 2))
		{
			string printline(params[1]);
			utf8tocpp866(&printline);

			iskra_data.push_back("51");
			iskra_data.push_back(printline.c_str()); // comment string
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "arbitrary_comments") && (params_count > 1))
		{
			iskra_data.push_back("56");
			for (i = 1; i < params_count; i++)
			{
				string printline(params[i]);
				utf8tocpp866(&printline);

				iskra_data.push_back(printline.c_str()); // comment string
			}
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "arbitrary_document_close") && (params_count == 1))
		{
			iskra_data.push_back("52");
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "x_report") && (params_count == 1))
		{
			iskra_data.push_back("30");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}
		else if (!strcmp(params[0], "z_report") && (params_count == 1))
		{
			iskra_data.push_back("31");
			strftime(tosend, 7, "%d%m%g", timeinfo); //current date
			iskra_data.push_back(tosend);
			strftime(tosend, 5, "%H%M", timeinfo); //current time
			iskra_data.push_back(tosend);
			perform_iskra(fd, iskra_data, &get_buf, 34);
		}

		else
			cout << "!!!1 Unexpected command: " << params[0] << endl;
	}
	else if (!strcmp(kkm, "vkp80ii"))
	{
		if (!strcmp(params[0], "print") && (params_count == 2))
		{
			string printline(params[1]);
			utf8tocpp866(&printline);
			perform_vkp80ii(fd, printline.c_str(), printline.length(), &get_buf, 0);
		}
		else if (!strcmp(params[0], "BS") && (params_count == 1))
		{
			perform_vkp80ii(fd, "\x08", 1, &get_buf, 0);
		}
		else if (!strcmp(params[0], "HT") && (params_count == 1))
		{
			perform_vkp80ii(fd, "\x09", 1, &get_buf, 0);
		}
		else if (!strcmp(params[0], "LF") && (params_count == 1))
		{
			perform_vkp80ii(fd, "\x0A", 1, &get_buf, 0);
		}
		else if (!strcmp(params[0], "FF") && (params_count == 1))
		{
			perform_vkp80ii(fd, "\x0C", 1, &get_buf, 0);
		}
		else if (!strcmp(params[0], "CR") && (params_count == 1))
		{
			perform_vkp80ii(fd, "\x0D", 1, &get_buf, 0);
		}
		else if (!strcmp(params[0], "transmit_status") && (params_count == 1))
		{
			perform_vkp80ii(fd, "\x10\x04\x14", 3, &get_buf, 6);
		}
		else if (!strcmp(params[0], "CAN") && (params_count == 1))
		{
			perform_vkp80ii(fd, "\x18", 1, &get_buf, 0);
		}
		else if (!strcmp(params[0], "print_mode_page") && (params_count == 1))
		{
			perform_vkp80ii(fd, "\x1B\x0C", 2, &get_buf, 0);
		}
		else if (!strcmp(params[0], "right_spacing_set") && (params_count == 2))
		{
			tosend[0] = 0x1B;
			tosend[1] = 0x20;
			tosend[2] = (char) atoi(params[1]);
			perform_vkp80ii(fd, tosend, 3, &get_buf, 0);
		}
		else if (!strcmp(params[0], "switch_to_iskra") && (params_count == 1))
		{
			tosend[0] = 0x1B;
			tosend[1] = 0x1B;
			perform_vkp80ii(fd, tosend, 2, &get_buf, 34);
		}
		else if (!strcmp(params[0], "print_mode_set") && (params_count == 2))
		{
			tosend[0] = 0x1B;
			tosend[1] = 0x21;
			tosend[2] = (char) atoi(params[1]);
			perform_vkp80ii(fd, tosend, 3, &get_buf, 0);
		}
		else if (!strcmp(params[0], "abs_position_set") && (params_count == 3))
		{
			tosend[0] = 0x1B;
			tosend[1] = 0x24;
			tosend[2] = (char) atoi(params[1]);
			tosend[3] = (char) atoi(params[2]);
			perform_vkp80ii(fd, tosend, 4, &get_buf, 0);
		}
		else if (!strcmp(params[0], "rel_v_position_set") && (params_count == 3))
		{
			tosend[0] = 0x1B;
			tosend[1] = 0x28;
			tosend[2] = 0x76;
			tosend[3] = (char) atoi(params[1]);
			tosend[4] = (char) atoi(params[2]);
			perform_vkp80ii(fd, tosend, 5, &get_buf, 0);
		}
		else if (!strcmp(params[0], "justification_set") && (params_count == 2))
		{
			tosend[0] = 0x1B;
			tosend[1] = 0x61;
			tosend[2] = (char) atoi(params[1]);
			perform_vkp80ii(fd, tosend, 3, &get_buf, 0);
		}
		else if (!strcmp(params[0], "transmit_paper_sensor") && (params_count == 1))
		{
			perform_vkp80ii(fd, "\x1B\x76", 2, &get_buf, 1);
		}
		else if (!strcmp(params[0], "negative_set") && (params_count == 2))
		{
			tosend[0] = 0x1D;
			tosend[1] = 0x42;
			tosend[2] = (char) atoi(params[1]);
			perform_vkp80ii(fd, tosend, 3, &get_buf, 0);
		}
		else if (!strcmp(params[0], "hri_position_set") && (params_count == 2))
		{
			tosend[0] = 0x1D;
			tosend[1] = 0x48;
			tosend[2] = (char) atoi(params[1]);
			perform_vkp80ii(fd, tosend, 3, &get_buf, 0);
		}
		else if (!strcmp(params[0], "barcode_128c") && (params_count == 2))
		{
			int barcode_len = strlen(params[1]);
			int step;

			tosend[0] = 0x1D;
			tosend[1] = 0x6B;
			tosend[2] = 73; // code128
			tosend[3] = (barcode_len + odd(barcode_len)) / 2 + 2;
			tosend[4] = '{';
			tosend[5] = 'C'; //set code128c
			perform_vkp80ii(fd, tosend, 6, &get_buf, 0);
			memset(tosend, 0, sizeof(tosend));
			for (step = 0; step < barcode_len; step++)
			{
				params[1][step] -= 0x30;
			}
			for (step = odd(barcode_len); step < barcode_len + odd(barcode_len); step++)
			{
				if (!odd(step))
				{
					tosend[step / 2] += 10 * params[1][step - odd(barcode_len)];
				}
				else
				{
					tosend[step / 2] += params[1][step - odd(barcode_len)];
				}
			}
			perform_vkp80ii(fd, tosend, (barcode_len + odd(barcode_len)) / 2, &get_buf, 0);
		}
		else if (!strcmp(params[0], "barcode_width_set") && (params_count == 2))
		{
			tosend[0] = 0x1D;
			tosend[1] = 0x77;
			tosend[2] = (char) atoi(params[1]);
			perform_vkp80ii(fd, tosend, 3, &get_buf, 0);
		}
		else if (!strcmp(params[0], "transmit_length") && (params_count == 1))
		{
			if (perform_vkp80ii(fd, "\x1D\xE3", 2, &get_buf, 20) > 0)
			{
				cout << "Answer: " << get_buf << endl;
			}
		}
		else
		{
			cout << "!!!2 Unexpected command: " << params[0] << endl;
		}
		cout << "Proceeding " << params[0] << " finished." << endl;
	}
	else
		cout << "!!!3 Unexpected type of kkm: " << kkm << endl;
	return 0;
}

unsigned int utf8tocpp866(std::string* const value)
{
	size_t iWnstr = 0;
	for (size_t iUTF8 = 0; iUTF8 < value->size(); iUTF8++, iWnstr++)
	{
		switch ((unsigned char) (*value)[iUTF8])
		{
		case 0xD0:

			if (++iUTF8 == value->size())
			{
				{
					cout << "utf8tocpp866: unexpected end of string " << value[iUTF8] << endl;
				}
				return 0;
			}

			switch ((unsigned char) (*value)[iUTF8])
			{
			case 0x90: //A
				(*value)[iWnstr] = 0x80;
				break;
			case 0x91: //Б
				(*value)[iWnstr] = 0x81;
				break;
			case 0x92: //В
				(*value)[iWnstr] = 0x82;
				break;
			case 0x93: //Г
				(*value)[iWnstr] = 0x83;
				break;
			case 0x94: //Д
				(*value)[iWnstr] = 0x84;
				break;
			case 0x95: //Е
				(*value)[iWnstr] = 0x85;
				break;
			case 0x81: //Ё
				(*value)[iWnstr] = 0xF0;
				break;
			case 0x96: //Ж
				(*value)[iWnstr] = 0x86;
				break;
			case 0x97: //З
				(*value)[iWnstr] = 0x87;
				break;
			case 0x98: //И
				(*value)[iWnstr] = 0x88;
				break;
			case 0x99: //Й
				(*value)[iWnstr] = 0x89;
				break;
			case 0x9A: //К
				(*value)[iWnstr] = 0x8A;
				break;
			case 0x9B: //Л
				(*value)[iWnstr] = 0x8B;
				break;
			case 0x9C: //М
				(*value)[iWnstr] = 0x8C;
				break;
			case 0x9D: //Н
				(*value)[iWnstr] = 0x8D;
				break;
			case 0x9E: //О
				(*value)[iWnstr] = 0x8E;
				break;
			case 0x9F: //П
				(*value)[iWnstr] = 0x8F;
				break;
			case 0xA0: //Р
				(*value)[iWnstr] = 0x90;
				break;
			case 0xA1: //С
				(*value)[iWnstr] = 0x91;
				break;
			case 0xA2: //Т
				(*value)[iWnstr] = 0x92;
				break;
			case 0xA3: //У
				(*value)[iWnstr] = 0x93;
				break;
			case 0xA4: //Ф
				(*value)[iWnstr] = 0x94;
				break;
			case 0xA5: //Х
				(*value)[iWnstr] = 0x95;
				break;
			case 0xA6: //Ц
				(*value)[iWnstr] = 0x96;
				break;
			case 0xA7: //Ч
				(*value)[iWnstr] = 0x97;
				break;
			case 0xA8: //Ш
				(*value)[iWnstr] = 0x98;
				break;
			case 0xA9: //Щ
				(*value)[iWnstr] = 0x99;
				break;
			case 0xAA: //Ъ
				(*value)[iWnstr] = 0x9A;
				break;
			case 0xAB: //Ы
				(*value)[iWnstr] = 0x9B;
				break;
			case 0xAC: //Ь
				(*value)[iWnstr] = 0x9C;
				break;
			case 0xAD: //Э
				(*value)[iWnstr] = 0x9D;
				break;
			case 0xAE: //Ю
				(*value)[iWnstr] = 0x9E;
				break;
			case 0xAF: //Я
				(*value)[iWnstr] = 0x9F;
				break;
			case 0xB0: //а
				(*value)[iWnstr] = 0xA0;
				break;
			case 0xB1: //б
				(*value)[iWnstr] = 0xA1;
				break;
			case 0xB2: //в
				(*value)[iWnstr] = 0xA2;
				break;
			case 0xB3: //г
				(*value)[iWnstr] = 0xA3;
				break;
			case 0xB4: //д
				(*value)[iWnstr] = 0xA4;
				break;
			case 0xB5: //е
				(*value)[iWnstr] = 0xA5;
				break;
			case 0xB6: //ж
				(*value)[iWnstr] = 0xA6;
				break;
			case 0xB7: //з
				(*value)[iWnstr] = 0xA7;
				break;
			case 0xB8: //и
				(*value)[iWnstr] = 0xA8;
				break;
			case 0xB9: //й
				(*value)[iWnstr] = 0xA9;
				break;
			case 0xBA: //к
				(*value)[iWnstr] = 0xAA;
				break;
			case 0xBB: //л
				(*value)[iWnstr] = 0xAB;
				break;
			case 0xBC: //м
				(*value)[iWnstr] = 0xAC;
				break;
			case 0xBD: //н
				(*value)[iWnstr] = 0xAD;
				break;
			case 0xBE: //о
				(*value)[iWnstr] = 0xAE;
				break;
			case 0xBF: //п
				(*value)[iWnstr] = 0xAF;
				break;
			default:
				{
					cout << "utf8tocpp866: wrong decoding D0 " << value[iUTF8] << endl;
				}
				break;
			}
			break;

		case 0xD1:

			if (++iUTF8 == value->size())
			{
				{
					cout << "utf8tocpp866: unexpected end of string " << value[iUTF8] << endl;
				}
				return 0;
			}

			switch ((unsigned char) (*value)[iUTF8])
			{
			case 0x80: //р
				(*value)[iWnstr] = 0xE0;
				break;
			case 0x81: //с
				(*value)[iWnstr] = 0xE1;
				break;
			case 0x82: //т
				(*value)[iWnstr] = 0xE2;
				break;
			case 0x83: //у
				(*value)[iWnstr] = 0xE3;
				break;
			case 0x84: //ф
				(*value)[iWnstr] = 0xE4;
				break;
			case 0x85: //х
				(*value)[iWnstr] = 0xE5;
				break;
			case 0x86: //ц
				(*value)[iWnstr] = 0xE6;
				break;
			case 0x87: //ч
				(*value)[iWnstr] = 0xE7;
				break;
			case 0x88: //ш
				(*value)[iWnstr] = 0xE8;
				break;
			case 0x89: //щ
				(*value)[iWnstr] = 0xE9;
				break;
			case 0x8A: //ъ
				(*value)[iWnstr] = 0xEA;
				break;
			case 0x8B: //ы
				(*value)[iWnstr] = 0xEB;
				break;
			case 0x8C: //ь
				(*value)[iWnstr] = 0xEC;
				break;
			case 0x8D: //э
				(*value)[iWnstr] = 0xED;
				break;
			case 0x8E: //ю
				(*value)[iWnstr] = 0xEE;
				break;
			case 0x8F: //я
				(*value)[iWnstr] = 0xEF;
				break;
			case 0x91: //ё
				(*value)[iWnstr] = 0xF1;
				break;
			default:
				{
					cout << "utf8tocpp866: wrong decoding D1 " << value[iUTF8] << endl;
				}
				break;
			}
			break;

		case 0xE2: //№

			if (++iUTF8 == value->size())
			{
				{
					cout << "utf8tocpp866: unexpected end of string " << value[iUTF8] << endl;
				}
				return 0;
			}

			switch ((unsigned char) (*value)[iUTF8])
			{
			case 0x84: //№

				if (++iUTF8 == value->size())
				{
					{
						cout << "utf8tocpp866: unexpected end of string " << value[iUTF8] << endl;
					}
					return 0;
				}

				switch ((unsigned char) (*value)[iUTF8])
				{
				case 0x96: //№
					(*value)[iWnstr] = 0xFC;
					break;
				default:
					{
						cout << "utf8tocpp866: wrong decoding E284 " << value[iUTF8] << endl;
					}
					break;
				}
				break;
			default:
				{
					cout << "utf8tocpp866: wrong decoding E2 " << value[iUTF8] << endl;
				}
				break;
			}
			break;

		default:
			(*value)[iWnstr] = (*value)[iUTF8];
			break;
		}
	}

	value->resize(iWnstr);

	return 0;
}

