/**
 * \file
 * \brief RFID controller YR903 command set implementation file
 * \author Vladimir Bogdanov
 * \version 1.0.0
 * \date November 2016
 */

#include <iostream>
#include <fstream>
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
#include <sys/stat.h>

#include "yr903.h"
/*
int odd(int value)
{
	return (value & 1);
}
*/
int set_interface_attribs (int fd, int speed, int parity, int loopback)
{
	int value;
        struct termios tty;

        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                printf ("error %d from tcgetattr\n", errno);
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
                printf ("error %d from tcsetattr\n", errno);
                return -1;
        }

	ioctl(fd, TIOCMGET, &value);
	value |= loopback;
	ioctl(fd, TIOCMSET, &value);
	if (0 != loopback)
	{
		printf ("Internal loopback set!\n");
	}
        return 0;
}
/*
void set_blocking (int fd, int should_block)
{
        struct termios tty;
        memset (&tty, 0, sizeof tty);
        if (tcgetattr (fd, &tty) != 0)
        {
                printf ("error %d from tggetattr\n", errno);
                return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 50;   // 5;         // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
                printf ("error %d setting term attributes\n", errno);
}
*/

/**
 * \brief Read to buffer from designated character device
 *
 * This function reads data from UART to internal buffer and then copies it to
 * given buffer. TODO: fix this.
 * It tries to reads data until during waiting period (see READ_CYCLE_PERIOD,
 * RESPONCE_PERIOD), but not more than len parameter.
 *
 * \param[in] fd opened file descriptor, pointing to character device
 * \param[out] buf where to put all that was read
 * \param[in] len available buffer length
 * \return actual read length
 * \retval -1 on error, use errno to get error details
 */
int readport (int fd, char *buf, int len)
{
	int res = 0;
	int i = 0;

//	printf("Entered readport()\n");
	do
	{
		i++;
		usleep (READ_CYCLE_PERIOD);
		res = read (fd, buf, len);
	}
	while ((((res < 0) && (errno == EAGAIN)) || (res == 0)) \
		&& (i < RESPONCE_PERIOD));
//	printf("Leaving readport() with i = %d, res = %d, errno = %d.\n", i, res, errno);

	return res;
}

/**
 * \brief Calculates checksum for data packet according to specification
 *
 * This function is copied from manufacturer's datasheet. It calculates
 * proper checksum byte for data packet.
 *
 * \param[in] uBuff buffer with packet data preparing to transmission
 * \param[in] uBuffLen length if this buffer
 * \return value to be used as last byte of data packet
 */
unsigned char checksum (unsigned char *uBuff, unsigned char uBuffLen)
{
	unsigned char i;
	unsigned char uSum = 0;
	for (i = 0; i < uBuffLen; ++i)
	{
		uSum = uSum + uBuff[i];
	}
	uSum = (~uSum) + 1;
	return uSum;
}

/**
 * \brief Executes command for yr903 and receives answer into buffer
 *
 * This function executes designated command with parameters, then listens
 * to the answer packet, until it's length reaches value that is found
 * in its #2 byte (see "expected = get_buf[1];").
 * Special command value YR903_CMD_RECEIVE_ONLY is used to read "next"
 * incoming data packet.
 *
 * \param[in] fd file descriptor of communication device
 * \param[in] address reader's address
 * \param[in] command command to execute
 * \param[in] data buffer of data to send
 * \param[in] data_length length of buffer
 * \param[out] pbuf pointer to buffer of data to receive
 * \return count of returned bytes
 */
int perform_yr903 (int fd, \
			unsigned char address, \
			yr903_cmd command, \
			const unsigned char* data, \
			unsigned int data_length, \
			char (*pbuf)[RCV_BUFFER_SIZE])
			//, unsigned int expected)
{
	using namespace std;

	char          get_buf [RCV_BUFFER_SIZE];
	char          tosend [SND_BUFFER_SIZE];
	unsigned int  i = 0;
	int	      res;
	unsigned char length;
	unsigned int  received = 0;

	memset (get_buf, 0, sizeof (get_buf));

	if (command != YR903_CMD_RECEIVE_ONLY)
	{
		tosend[0] = 0xA0; // Set packet head byte due to protocol
		tosend[1] = 1 + 1 + data_length + 1; // Packet length = address +
					     //   + command + params +
					     //   + checksum bytes
		tosend[2] = address;
		tosend[3] = (unsigned char) command;
		length = 4; // intermediate package length
		if ( NULL != data )
		{
			for (i = 0; i < data_length; ++i)
			{
				tosend[length++] = data[i];
			}
		}
		tosend[length] = checksum ( (unsigned char*) tosend, length);
		++length;

		cout << "Sending " << +length << " bytes:" << endl;
		for (i = 0; i < length; ++i)
		{
			printf (" %2.2hhX", tosend[i]);
		}

		res = write (fd, tosend, length);
		cout << endl << res << " bytes sent." << endl;
	}
//	if ((expected == 0) || (pbuf == 0)) return 0;
	if (NULL == pbuf) return 0;

#ifndef YR903_USE_FILE
// Disable this block to make continious output files of command sequences
	unsigned int  expected = 2; // read packet head and length

	while ((received < expected) && \
	      ((res = readport (fd, &get_buf[received], sizeof (get_buf))) >= 0))
	{
//		cout << "received " << res << ", " << endl;
		received += res;
	}
//	cout << "received " << received << ", " << " expected " << expected << ", res " << res << endl;
	if (res < 0)
	{
		if (errno == EAGAIN)
		{ // Found nothing in this port during RESPONCE_PERIOD
			cout << "Nothing responded!" << endl;
			return res;
		}
		cout << "Returned " << res << " bytes, errno = " << errno << endl;
		return res;
	}

	expected += get_buf[1]; //read packet data if needed
	while ((received < expected) && \
	      ((res = readport (fd, &get_buf[received], sizeof (get_buf))) >= 0))
	{
//		cout << "received " << res << ", " << endl;
		received += res;
	}
	if (res < 0)
	{
		if (errno == EAGAIN)
		{ // Found nothing in this port during RESPONCE_PERIOD
			cout << "Nothing responded!" << endl;
			return res;
		}
		cout << "Returned " << res << " bytes, errno = " << errno << endl;
		return res;
	}

	cout << "Resulted " << received << " of " << expected << " bytes: ";
	for ( i = 0; i < received; i++ )
	{
		printf(" %2.2hhX", get_buf[ i ]);
		(*pbuf)[i] = get_buf[i];
	}
	cout << endl;
#endif //YR903_USE_FILE

	return received;
}

/**
 * \brief Turns on/off yr903 power pin
 *
 * This function searches for gpio subsystem, creates GPIOs defined in
 * YR903_PATH_POWER and YR903_PATH_SELECT if needed, tunes them to "out"
 * and then "select" GPIO turns to 0 (according to scheme) and "power" GPIO
 * turns to value given as parameter
 *
 * \param[in] action defined values YR903_POWERON or YR903_POWEROFF
 * \return 0 on success
 */
int power_yr903 (const char* action)
{
	using namespace std;

	struct stat info;
	ofstream f_export;
	ofstream f_gpio_select_value;
	ofstream f_gpio_select_direction;
	ofstream f_gpio_power_value;
	ofstream f_gpio_power_direction;
//	string   tmp_path;

//	tmp_path = YR903_PATH_GPIO YR903_PATH_SELECT;
//	if ( stat ( tmp_path.c_str(), &info ) != 0 )
	if ( stat ( YR903_PATH_GPIO YR903_PATH_SELECT, &info ) != 0 )
	{ // selection gpio not exists
		if ( access ( YR903_PATH_EXPORT, F_OK ) == -1)
		{ // gpio exporting path NOT exists
			std::cout << "No gpio subsystem found!" << std::endl;
			return -1;
		}
		// gpio exporting path exists
		f_export.open ( YR903_PATH_EXPORT, ios::out );
		if (f_export.good())
		{
			f_export << YR903_PATH_SELECT;
			f_export.close();
		}
		else
		{ // gpio exporting path could NOT be opened
			std::cout << "Gpio exporting not accessible!" << std::endl;
			return -1;
		}
		usleep(200000); // sleep for 200 msec
	}

	f_gpio_select_direction.open ( YR903_PATH_GPIO YR903_PATH_SELECT "/direction", ios::out );
	if ( f_gpio_select_direction.good() )
	{
		f_gpio_select_direction << "out";
		f_gpio_select_direction.close();
	}
	else
	{ //
		std::cout << "Gpio exporting failed! [1]" << std::endl;
		return -1;
	}

	f_gpio_select_value.open ( YR903_PATH_GPIO YR903_PATH_SELECT "/value", ios::out );
	if ( f_gpio_select_value.good() )
	{
		f_gpio_select_value << "0";
		f_gpio_select_value.close();
	}
	else
	{ //
		std::cout << "Gpio exporting failed! [2]" << std::endl;
		return -1;
	}

	if ( stat ( YR903_PATH_GPIO YR903_PATH_POWER, &info ) != 0 )
	{ // selection gpio not exists
		if ( access ( YR903_PATH_EXPORT, F_OK ) == -1)
		{ // gpio exporting path NOT exists
			std::cout << "No gpio subsystem found!" << std::endl;
			return -1;
		}
		// gpio exporting path exists
		f_export.open ( YR903_PATH_EXPORT, ios::out );
		if (f_export.good())
		{
			f_export << YR903_PATH_POWER;
			f_export.close();
		}
		else
		{ // gpio exporting path could NOT be opened
			std::cout << "Gpio exporting not accessible!" << std::endl;
			return -1;
		}
		usleep(200000); // sleep for 200 msec
	}

	f_gpio_power_direction.open ( YR903_PATH_GPIO YR903_PATH_POWER "/direction", ios::out );
	if ( f_gpio_power_direction.good() )
	{
		f_gpio_power_direction << "out";
		f_gpio_power_direction.close();
	}
	else
	{ //
		std::cout << "Gpio exporting failed! [3]" << std::endl;
		return -1;
	}

	f_gpio_power_value.open ( YR903_PATH_GPIO YR903_PATH_POWER "/value", ios::out );
	if ( f_gpio_power_value.good() )
	{
		f_gpio_power_value << action;
		f_gpio_power_value.close();
		usleep(200000); // sleep for 200 msec
	}
	else
	{ //
		std::cout << "Gpio exporting failed! [4]" << std::endl;
		return -1;
	}

	return 0;
}
/*
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
*/
