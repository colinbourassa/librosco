// librosco - a communications library for the Rover MEMS
//
// setup.c: This file contains routines that perform the
//          setup/initialization of the library and the
//          serial port.

#if defined(WIN32) && defined(linux)
#error "Only one of 'WIN32' or 'linux' may be defined."
#endif

#include <unistd.h>
#include <fcntl.h>

#if defined(WIN32)
  #include <windows.h>
#else
  #include <string.h>
  #include <termios.h>
  #include <arpa/inet.h>
#endif

#include "rosco.h"
#include "rosco_internal.h"
#include "rosco_version.h"

/**
 * Sets initial values in the state-info struct.
 * Note that this routine does not actually open the serial port or attempt
 * to connect to the ECU; that requires mems_connect().
 * @param info 
 */
void mems_init(mems_info *info)
{
#if defined(WIN32)
    info->sd = INVALID_HANDLE_VALUE;
    info->mutex = CreateMutex(NULL, TRUE, NULL);
#else
    info->sd = 0;
    pthread_mutex_init(&info->mutex, NULL);
#endif
}

/**
 * Disconnects (if necessary) and closes the mutex handle.
 * @param info State information for the current connection.
 */
void mems_cleanup(mems_info *info)
{
#if defined(WIN32)
    if (mems_is_connected(info))
    {
        CloseHandle(info->sd);
        info->sd = INVALID_HANDLE_VALUE;
    }
    CloseHandle(info->mutex);
#else
    if (mems_is_connected(info))
    {
        close(info->sd);
        info->sd = 0;
    }
    pthread_mutex_destroy(&info->mutex);
#endif
}

/**
 * Returns version information for this build of the library.
 * @return Version of this build of librosco
 */
librosco_version mems_get_lib_version()
{
    librosco_version ver;

    ver.major = LIBROSCO_VER_MAJOR;
    ver.minor = LIBROSCO_VER_MINOR;
    ver.patch = LIBROSCO_VER_PATCH;

    return ver;
}

/**
 * Closes the serial device.
 * @param info State information for the current connection.
 */
void mems_disconnect(mems_info *info)
{
#if defined(WIN32)
    if (WaitForSingleObject(info->mutex, INFINITE) == WAIT_OBJECT_0)
    {
        if (mems_is_connected(info))
        {
            CloseHandle(info->sd);
            info->sd = INVALID_HANDLE_VALUE;
        }

        ReleaseMutex(info->mutex);
    }
#else
    pthread_mutex_lock(&info->mutex);

    if (mems_is_connected(info))
    {
        close(info->sd);
        info->sd = 0;
    }

    pthread_mutex_unlock(&info->mutex);
#endif
}

/**
 * Opens the serial port (or returns with success if it is already open.)
 * @param info State information for the current connection.
 * @param devPath Full path to the serial device (e.g. "/dev/ttyUSB0" or "COM2")
 * @return True if the serial device was successfully opened and its
 *   baud rate was set; false otherwise.
 */
bool mems_connect(mems_info *info, const char *devPath)
{
    bool result = false;

#if defined(WIN32)
    if (WaitForSingleObject(info->mutex, INFINITE) == WAIT_OBJECT_0)
    {
        result = mems_is_connected(info) || mems_openserial(info, devPath);
        ReleaseMutex(info->mutex);
    }
#else // Linux/Unix
    pthread_mutex_lock(&info->mutex);
    result = mems_is_connected(info) || mems_openserial(info, devPath);
    pthread_mutex_unlock(&info->mutex);
#endif

    return result;
}

/**
 * Opens the serial device for the USB<->TTL/serial converter and sets the
 * parameters for the link to match those on the MEMS ECU.
 * Uses OS-specific calls to do this in the correct manner.
 * Note for FreeBSD users: Do not use the ttyX devices, as they block
 * on the open() call while waiting for a carrier detect line, which
 * will never be asserted. Instead, use the equivalent cuaX device.
 * Example: /dev/cuaU0 (instead of /dev/ttyU0)
 * @return True if the open/setup was successful, false otherwise
 */
bool mems_openserial(mems_info *info, const char *devPath)
{
    bool retVal = false;

#if defined(linux) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)

    struct termios newtio;
    bool success = true;

    info->sd = open(devPath, O_RDWR | O_NOCTTY);

    if (info->sd > 0)
    {
        if (tcgetattr(info->sd, &newtio) != 0)
        {
            success = false;
        }

        if (success)
        {
            // set up the serial port:
            // * enable the receiver, set 8-bit fields, set local mode, disable hardware flow control, disable two stop bits
            // * set non-canonical mode, disable echos, disable signals
            // * disable all special handling of CR or LF, disable software flow control
            // * disable all output post-processing
            newtio.c_cflag &= ((CREAD | CS8 | CLOCAL) & ~(CRTSCTS & CSTOPB));
            newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
            newtio.c_iflag &= ~(INLCR | ICRNL | IGNCR | IXON | IXOFF | IXANY);
            newtio.c_oflag &= ~OPOST;

#if defined(linux) || defined(__APPLE__)
            // when waiting for responses, wait until we haven't received any
            // characters for a period of time before returning with failure
            newtio.c_cc[VTIME] = 1;
            newtio.c_cc[VMIN] = 0;
#else // BSD and other UNIXes
            // This is set higher than the 0.1 seconds used by the Linux/OSX
            // code, as values much lower than this cause the first echoed
            // byte to be missed when running under BSD.
            newtio.c_cc[VTIME] = 5;
            newtio.c_cc[VMIN] = 0;
#endif
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);

            // flush the serial buffers and set the new parameters
            if ((tcflush(info->sd, TCIFLUSH) != 0) ||
                (tcsetattr(info->sd, TCSANOW, &newtio) != 0))
            {
                close(info->sd);
                success = false;
            }
        }

        retVal = success;

        // close the device if it couldn't be configured
        if (retVal == false)
        {
            close(info->sd);
        }
    }

#elif defined(WIN32)

    DCB dcb;
    COMMTIMEOUTS commTimeouts;

    // open and get a handle to the serial device
    info->sd = CreateFile(devPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    // verify that the serial device was opened
    if (info->sd != INVALID_HANDLE_VALUE)
    {
        if (GetCommState(info->sd, &dcb) == TRUE)
        {
            // set the serial port parameters
            dcb.BaudRate = 9600;
            dcb.fParity = FALSE;
            dcb.fOutxCtsFlow = FALSE;
            dcb.fOutxDsrFlow = FALSE;
            dcb.fDtrControl = FALSE;
            dcb.fRtsControl = FALSE;
            dcb.ByteSize = 8;
            dcb.Parity = 0;
            dcb.StopBits = ONESTOPBIT;

            if ((SetCommState(info->sd, &dcb) == TRUE) &&
                (GetCommTimeouts(info->sd, &commTimeouts) == TRUE))
            {
                // modify the COM port parameters to wait 100 ms before timing out
                commTimeouts.ReadIntervalTimeout = 100;
                commTimeouts.ReadTotalTimeoutMultiplier = 0;
                commTimeouts.ReadTotalTimeoutConstant = 100;

                if (SetCommTimeouts(info->sd, &commTimeouts) == TRUE)
                {
                    retVal = true;
                }
            }
        }

        // the serial device was opened, but couldn't be configured properly;
        // close it before returning with failure
        if (!retVal)
        {
            CloseHandle(info->sd);
        }
    }

#endif

    return retVal;
}

/**
 * Checks the file descriptor for the serial device to determine if it has
 * already been opened.
 * @return True if the serial device is open; false otherwise.
 */
bool mems_is_connected(mems_info* info)
{
#if defined(WIN32)
    return (info->sd != INVALID_HANDLE_VALUE);
#else
    return (info->sd > 0);
#endif
}

