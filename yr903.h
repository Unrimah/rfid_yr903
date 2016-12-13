/**
 * \file
 * \brief RFID controller YR903 command set header file
 * \author Vladimir Bogdanov
 * \version 1.0.0
 * \date November 2016
 */

#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

//#define YR903_USE_FILE

#define RCV_BUFFER_SIZE 256 ///< Data receiving buffer size
#define SND_BUFFER_SIZE 256 ///< Data sending buffer size

///Cycle waiting period for reading from device in usec, 1 msec default
#define READ_CYCLE_PERIOD 1000

///Wait for responce from device in READ_CYCLE_PERIODs, 1 sec default
#define RESPONCE_PERIOD 1000

/// Length of data packet carrying label data
#define YR903_LABEL_PKT_LEN 21

/// Serial port loopback parameters constant for DART-MX6
#define TIOCM_LOOP 0x8000

#define YR903_POWEROFF "0" ///< sets rfid powering off via sysfs gpio
#define YR903_POWERON  "1" ///< sets rfid powering on via sysfs gpio

/// sysfs gpios prefix
#define YR903_PATH_GPIO   "/sys/class/gpio/gpio"
/// sysfs gpio exporting node
#define YR903_PATH_EXPORT "/sys/class/gpio/export"
/// rfid/barcode scaner selection gpio: rfid = 0, barcode = 1.
#define YR903_PATH_SELECT "123"
/// rfid powering gpio
#define YR903_PATH_POWER  "124"

/**
 * Command list for yr903 according to specification
 */
enum yr903_cmd
{
        YR903_CMD_RECEIVE_ONLY                        = 0x00, ///< service value for continious receiving
        YR903_CMD_read_gpio_value                     = 0x60, ///< not tested
        YR903_CMD_write_gpio_value                    = 0x61, ///< not tested
        YR903_CMD_set_reader_identifier               = 0x67, ///< not tested
        YR903_CMD_get_reader_identifier               = 0x68, ///< not tested
        YR903_CMD_reset                               = 0x70, ///< tested OK
        YR903_CMD_set_uart_baudrate                   = 0x71, ///< not tested
        YR903_CMD_get_firmware_version                = 0x72, ///< tested OK
        YR903_CMD_set_reader_address                  = 0x73, ///< not tested
        YR903_CMD_set_work_antenna                    = 0x74, ///< tested OK
        YR903_CMD_set_output_power                    = 0x76, ///< not tested
        YR903_CMD_get_output_power                    = 0x77, ///< not tested
        YR903_CMD_set_frequency_region                = 0x78, ///< not tested
        YR903_CMD_get_frequency_region                = 0x79, ///< not tested
        YR903_CMD_set_beeper_mode                     = 0x7A, ///< tested OK
        YR903_CMD_inventory                           = 0x80, ///< not tested
        YR903_CMD_read                                = 0x81, ///< not tested
        YR903_CMD_write                               = 0x82, ///< not tested
        YR903_CMD_lock                                = 0x83, ///< not tested
        YR903_CMD_kill                                = 0x84, ///< not tested
        YR903_CMD_real_time_inventory                 = 0x89, ///< tested OK
        YR903_CMD_customized_session_target_inventory = 0x8B, ///< not tested
        YR903_CMD_get_inventory_buffer                = 0x90, ///< not tested
        YR903_CMD_get_and_reset_inventory_buffer      = 0x91, ///< not tested
        YR903_CMD_get_inventory_buffer_tag_count      = 0x92, ///< not tested
        YR903_CMD_reset_inventory_buffer              = 0x93  ///< not tested
};

/**
 * \brief Sets attributes to designated character device
 *
 * This function sets UART attributes to designated character device
 * and also controls incircuit loopback.
 *
 * \param[in] fd opened file descriptor, pointing to character device
 * \param[in] speed baudrate constant
 * \param[in] parity parity bit mode
 * \param[in] loopback architecture-specific constant to control incircuit loopback,
 * use TIOCM_LOOP value to set.
 * \retval 0 on success
 * \retval -1 on error
 */
int set_interface_attribs (int fd, int speed, int parity, int loopback);

//int readport (int fd, char *buf, int len);

int perform_yr903 (int fd, \
                        unsigned char address, \
                        yr903_cmd command, \
                        const unsigned char *data, \
                        unsigned int data_length, \
                        char (*pbuf)[RCV_BUFFER_SIZE]);
                        //, unsigned int expected);

int power_yr903 (const char* action);

//void set_blocking (int fd, int should_block);

//int 		perform_exec(int fd, char **params, int param_count);

//int 		odd(int value);

//unsigned int	utf8tocpp866 (std::string* const value);

//int		set_params(int count, char ***params, const char *param1, ...);
