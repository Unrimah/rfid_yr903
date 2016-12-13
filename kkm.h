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

#define RCV_BUFFER_SIZE 1100
#define SND_BUFFER_SIZE 512
#define MAX_ARGS	40

//wait for responce from device in msec, 1 sec default
#define RESPONCE_PERIOD 1000

using namespace std;

int 		odd(int value);

int 		set_interface_attribs (int fd, int speed, int parity);

void 		set_blocking (int fd, int should_block);

int 		readport(int fd, char *buf, int len);

int 		perform_kkm(int fd, char *kkm, char **params, int param_count);

int 		perform_vkp80ii(int fd, const char *tosend, int length, char (*pbuf)[RCV_BUFFER_SIZE], int expected);

int 		perform_iskra(int fd, const char *tosend, int length, char (*pbuf)[RCV_BUFFER_SIZE], int expected);

unsigned int	utf8tocpp866(std::string* const value);

int		set_params(int count, char ***params, const char *param1, ...);

