/* File: rdt_error.h
 * Description: File to define the different classes of errors that occur in a
 *              project. Source code that includes this header can use the line
 *              "ERROR(ERR_TYPE, exit);" to print the error message and
 *              optionally exit the program.
 */

#include <iostream>

static const std::string PROJECT_NAME = "libRDT";

#define ERR(f)															\
	f(ERR_OPEN,          1, "Failed to open the terminal file")			\
	f(ERR_MALLOC,        2, "Failed to allocate needed memory")			\
	f(ERR_SOCKET,        3, "Failed to open a socket")					\
	f(ERR_BIND,          4, "Error on binding")							\
	f(ERR_ACCEPT,        5, "Error on accepting")						\
	f(ERR_LISTEN,        6, "Error on listen")							\
	f(ERR_SOCKOPT,       7, "Error setting socket option")				\
	f(ERR_SELECT,        8, "Error on select")							\
	f(ERR_RECV,          9, "Error on recvfrom")						\
	f(ERR_CLOSE,        10, "Error on close")							\
	f(ERR_HOST,         11, "Failed to get the host name")				\
	f(ERR_CONNECT,      12, "Error on connecting to the host")			\
	f(ERR_SEND,         13, "Error on sendto")

#define _ERR_NAME(err, val, str) err,
enum ERR{ ERR(_ERR_NAME) };

#define _ERR_VAL(err, val, str) val,
static const int ERR_VAL[] = { ERR(_ERR_VAL) };

#define _ERR_STR(err, val, str) str,
static const char *ERR_STR[] = { ERR(_ERR_STR) };

#define ERROR(err, shouldExit) \
	std::cerr << PROJECT_NAME << " Error: " << ERR_STR[err] << "\n"; if(shouldExit){std::cerr << "Exiting now...\n"; exit(ERR_VAL[err]);} else{}
