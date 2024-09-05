#include <csse2310a4.h>
#include "common.h"
#include <sys/types.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024

/* Information of a single client - Contains all necessary variables including
 * image manipulation arguments, port number and in/out files
 */
typedef struct {
    char* portno;
    char* convert;
    char* degrees;
    char* width;
    char* height;
    char* direction;
    char* inFile;
    char* outFile;
} ClientInfo;

/* Binary Image data - Contains the binary image data and it's size / length in
 * bytes
 */
typedef struct {
    unsigned char* imageData;
    size_t count;
} ImageFile;

// Client Program Values
typedef enum {
    MAX_CMD_LEN = 9,
    MIN_CMD_LEN = 2,
    VALID_RESPONSE = 200
} ClientValues;

// Program/Client exit codes
typedef enum {
    USAGE_ERROR = 5,
    READ_ERROR = 16,
    WRITE_ERROR = 10,
    PORT_ERROR = 1,
    NO_DATA_ERROR = 6,
    CANNOT_WRITE_ERROR = 15,
    INVALID_RESPONSE_ERROR = 12,
    NETWORK_CLOSED_ERROR = 2
} ExitStatus;

// Command line option arguments
const char* const scaleArg = "--scale";
const char* const flipArg = "--flip";
const char* const rotateArg = "--rotate";
const char* const inArg = "--in";
const char* const outArg = "--out";

// Error messages
const char* const usageError
        = "Usage: uqimageclient portno [--scale width height | --flip direction"
          " | --rotate degrees] [--in infilename] [--out outputfilename]\n";
const char* const readError
        = "uqimageclient: unable to open file \"%s\" for reading\n";
const char* const writeError
        = "uqimageclient: unable to open file \"%s\" for writing\n";
const char* const portError
        = "uqimageclient: unable to establish connection to port \"%s\"\n";
const char* const noDataError = "uqimageclient: no data in input image\n";
const char* const cannotWriteError = "uqimageclient: unable to write output\n";
const char* const connectionError
        = "uqimageclient: server connection terminated\n";

/* usage_error()
 *
 * This function prints the usageError message and exits with a status
 * of enum ERROR_USAGE.
 *
 * Errors: When this function is called the program will always exits
 *     with status USAGE_ERROR.
 */
void usage_error(void)
{
    fprintf(stderr, usageError);
    fflush(stderr);
    exit(USAGE_ERROR);
}

/* network_closed_error()
 *
 * This function prints the connectionError message to stderr and exits with a
 * status of enum NETWORK_CLOSED_ERROR.
 *
 * Errors: When this function is called the program will always exit with
 *     status NETWORK_CLOSED_ERROR.
 */
void network_closed_error(void)
{
    fprintf(stderr, connectionError);
    fflush(stderr);
    exit(NETWORK_CLOSED_ERROR);
}

/* store_arg()
 *
 * This function firstly checks if the given type is either one of rotate, flip
 * or scale. Then checks that the 'arg' and 'arg2' are a valid argument for that
 * type. If both requirements are fulfilled then the function will store arg
 * and arg2 (if required) within the correct ClientInfo struct variable
 * depending on the type.
 *
 * arg: The first argument value.
 * arg2: The second argument value (Can be NULL for rotate and flip).
 * info: A pointer to an instance of the ClientInfo struct.
 *
 * Errors: If the type variable is not one of rotate, flip or scale then the
 *     program calls usage_error() and exits with status USAGE_ERROR.
 */
void store_arg(char* type, char* arg, char* arg2, ClientInfo* info)
{
    // Check rotate argument
    if (!strcmp(type, "rotate")) {
        if (check_rotate_arg(arg)) {
            info->degrees = arg;
        } else {
            usage_error();
        }
        // Check flip argument
    } else if (!strcmp(type, "flip")) {
        if (check_flip_arg(arg)) {
            info->direction = arg;
        } else {
            usage_error();
        }
        // Check scale argument
    } else if (!strcmp(type, "scale")) {
        if (check_scale_arg(arg, arg2)) {
            info->width = arg;
            info->height = arg2;
        } else {
            usage_error();
        }
    }
}

/* process_command_line()
 *
 * This function checks the following:
 * 1. Command line argc is larger than 1 (Port number must be specified)
 * 2. The command line specifiers are one of --scale, --flip, --rotate, --in
 *    and --out exactly.
 * 3. If the specifier is --scale than it is followed by two arguments
 * 4. If the specifier is --flip than it is followed by an argument
 * 5. If the specifier is --rotate than it is followed by an argument
 * 6. No specifiers can be listed more than once and that only one of the the
 *    three specifiers (--scale, --flip, --rotate) are present.
 *
 * argc: Number of command line arguments
 * argv: Command line arguments
 *
 * Returns: A 'filled' out instance of the ClientInfo struct.
 * Errors: If the command line given doesn't fit any of the list of checks then
 *     the function calls usage_error() and exits with status USAGE_ERROR.
 */
ClientInfo process_command_line(int argc, char** argv)
{
    ClientInfo info = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    if (argc < MIN_CMD_LEN || argc > MAX_CMD_LEN || is_empty(argv[1])) {
        usage_error();
    } else {
        info.portno = argv[1];
    }

    // Loop to check each argv argument
    for (int i = 2; i < argc; i++) {
        // Check that arguments following specifiers exist
        if ((i + 1 >= argc)
                || (!strcmp(scaleArg, argv[i]) && (i + 2 >= argc))) {
            usage_error();
        }

        // Check command line argument specifier and their arguments and
        // store the argument values within the correct variables
        if (!info.convert && !strcmp(rotateArg, argv[i])) { // --Rotate
            info.convert = "rotate";
            store_arg(info.convert, argv[i + 1], NULL, &info);
        } else if (!info.convert && !strcmp(flipArg, argv[i])) { // --Flip
            info.convert = "flip";
            store_arg(info.convert, argv[i + 1], NULL, &info);
        } else if (!info.convert && !strcmp(scaleArg, argv[i])) { // --Scale
            info.convert = "scale";
            store_arg(info.convert, argv[i + 1], argv[i + 2], &info);
            i++;
        } else if (!info.inFile && !strcmp(inArg, argv[i])
                && !is_empty(argv[i + 1])) { // --In
            info.inFile = argv[i + 1];
        } else if (!info.outFile && !strcmp(outArg, argv[i])
                && !is_empty(argv[i + 1])) { // --Out
            info.outFile = argv[i + 1];
        } else { // Otherwise Error!
            usage_error();
        }

        i++;
    }

    return info;
}

/* read_error()
 *
 * This function when called will print the appropriate error message (either
 * format strings readError or writeError) to stderr depending on 'readOrWrite'
 * and exit with status READ_ERROR or WRITE_ERROR.
 *
 * readOrWrite: A boolean type that specifies whether the file was a read or
 *     write file. (Assumed true for read and false for write)
 * fileName: Name of the file that failed to open.
 *
 * Errors: This function when called will always exit with either READ_ERROR or
 *     WRITE_ERROR.
 */
void read_write_error(bool readOrWrite, char* fileName)
{
    if (readOrWrite) { // If read file error
        fprintf(stderr, readError, fileName);
        exit(READ_ERROR);
    }

    if (!readOrWrite) { // If write file error
        fprintf(stderr, writeError, fileName);
        exit(WRITE_ERROR);
    }
}

/* file_check()
 *
 * This function checks if the given input and output files can be opened.
 *
 * info: An instance of the ClientInfo struct.
 *
 * Returns: If an input file was specified then this function will return an
 *     opened FILE* stream for the input file otherwise it will return NULL.
 * Errors: If any of the files specified within the command line cannot be
 *     opened then the function will call read_write_error() to exit with
 *     status READ_ERROR or WRITE_ERROR.
 */
FILE* file_check(ClientInfo info)
{
    char* inputFile = info.inFile;
    char* outputFile = info.outFile;
    FILE* returnFile = NULL;

    // Try reading input and output files
    if (inputFile) { // Input file was specified
        returnFile = fopen(inputFile, "r");
        if (!returnFile) {
            read_write_error(true, inputFile);
        }
    }

    if (outputFile) { // Output file was specified
        int outputFileFd = open(
                outputFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        // If output file cannot be read close input and exit.
        if (outputFileFd < 0) {
            if (returnFile) {
                fclose(returnFile);
            }
            read_write_error(false, outputFile);
        } else {
            close(outputFileFd);
        }
    }

    return returnFile;
}

/* port_error()
 *
 * This function prints the format string portError message to stderr and exits
 * with a status of PORT_ERROR.
 *
 * portno: Failed port number/string
 * ai: A pointer to an addrinfo struct instance
 *
 * Errors: The program will exit with status PORT_ERROR if this function is
 *     called.
 */
void port_error(char* portno, struct addrinfo* ai)
{
    // Clean up, print error message and exit
    freeaddrinfo(ai);
    fprintf(stderr, portError, portno);
    exit(PORT_ERROR);
}

/* attempt_connect()
 *
 * This function attempts to connect to the given port number. If it couldn't
 * connect to the given 'portno' then the program will print the format string
 * portError to stderr and exit with status PORT_ERROR.
 *
 * portno: The port number specified within command line.
 * inputFile: Opened input file stream for the current client program.
 *
 * Returns: Once a connection established this function returns the socket
 *     file descriptor of the connection.
 * Errors: If for some reason the program couldn't connect to the given port
 *     the program will print the appropriate error message to stderr and exit
 *     with status PORT_ERROR.
 *
 * REF: Inspired by net4.c given during week 9 contact
 */
int attempt_connect(char* portno, FILE* inputFile)
{
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    int err;

    if ((err = getaddrinfo("localhost", portno, &hints, &ai))) { // Error!
        if (inputFile != NULL) {
            fclose(inputFile);
        }
        port_error(portno, ai); // Unable to connect to port
    }

    // Create socket and try connect
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(socketFd, ai->ai_addr, sizeof(struct sockaddr))) { // Error!
        if (inputFile != NULL) {
            fclose(inputFile);
        }
        port_error(portno, ai); // Unable to connect to port
    }
    freeaddrinfo(ai);

    return socketFd;
}

/* no_data_error()
 *
 * This function when called prints noDataError to stderr and always exits
 * with status NO_DATA_ERROR.
 *
 * Errors: This function always exits with the NO_DATA_ERROR.
 */
void no_data_error(void)
{
    fprintf(stderr, noDataError);
    exit(NO_DATA_ERROR);
}

/* read_image()
 *
 * This function reads and stores the given inputFile stream. If the given
 * inputFile stream is NULL then it will attempt to read stdin instead.
 *
 * inputFile: Input file stream.
 *
 * Returns: This function returns an instance of an ImageFile struct which is
 *     'filled' by what ever is read from either the specified inputFile stream
 *     or stdin.
 * Errors: If no data exists within the given inputFile stream or stdin closes
 *     without any data being sent in then the program will exit using
 *     no_data_error().
 */
ImageFile read_image(FILE* inputFile)
{
    FILE* imageFile;
    ImageFile image = {NULL, 0};

    // If input file was specified
    if (inputFile != NULL) {
        imageFile = inputFile;
    } else { // If input file wasn't specified read from STDIN
        imageFile = stdin;
    }

    // Read file until EOF
    unsigned char buffer[BUFFER_SIZE];
    size_t readSize;

    while ((readSize = fread(buffer, 1, BUFFER_SIZE, imageFile)) > 0) {
        // Calculate new size and realloc
        size_t newSize = image.count + readSize;
        image.imageData
                = realloc(image.imageData, sizeof(unsigned char) * newSize);

        // Store the read memory and modify count
        memcpy(image.imageData + image.count, buffer, readSize);
        image.count += readSize;

        if (readSize < BUFFER_SIZE - 1) { // Check if EOF
            break;
        }
    }

    // Finished reading so close file
    if (imageFile != stdin) {
        fclose(imageFile);
    }

    // If image file empty error!
    if (image.count == 0) {
        no_data_error();
    }

    return image;
}

/* send_http_request()
 *
 * This fuction creates a HTTP request and sends it to the server. The HTTP
 * request created will depend on the image convert operation specified
 * within the command line during the start of the program.
 *
 * info: An instance of the ClientInfo struct
 * image: An instance of the ImageFile struct
 * socketFd: Socket file descriptor
 */
void send_http_request(ClientInfo info, ImageFile image, int socketFd)
{
    // Open socket for writing
    FILE* socketWrite = fdopen(socketFd, "w");

    // Create request address
    char addr[BUFFER_SIZE];
    if (!info.convert) {
        snprintf(addr, sizeof(addr), "/%s,%s", "rotate", "0");
    } else if (!strcmp(info.convert, "scale")) {
        snprintf(addr, sizeof(addr), "/%s,%s,%s", info.convert, info.width,
                info.height);
    } else if (!strcmp(info.convert, "flip")) {
        snprintf(addr, sizeof(addr), "/%s,%s", info.convert, info.direction);
    } else if (!strcmp(info.convert, "rotate")) {
        snprintf(addr, sizeof(addr), "/%s,%s", info.convert, info.degrees);
    }

    // Create full HTTP request
    char request[BUFFER_SIZE];
    int requestLen = snprintf(request, sizeof(request),
            "POST %s HTTP/1.1\r\n"
            "Content-Length: %ld\r\n\r\n",
            addr, image.count);

    // Write HTTP request without body
    fwrite(request, 1, requestLen, socketWrite);

    // Write body (image data) and clean up
    fwrite(image.imageData, 1, image.count, socketWrite);
    fflush(socketWrite);
    fclose(socketWrite);
    free(image.imageData);
}

/* successful_response()
 *
 * This function is to only be called when a successful HTTP response is
 * received. When this function is called the program will write to either
 * stdout or specified output file the body given.
 *
 * info: An instance of the ClientInfo struct.
 * body: Body of successful HTTP response.
 * len: Length of the HTTP body.
 *
 * Errors: If for some reason any part of the given body cannot be written
 * to stdout or the specified output file then the program will exit with
 *     status CANNOT_WRITE_ERROR.
 */
void successful_response(
        ClientInfo info, unsigned char* body, unsigned long len)
{
    FILE* output;

    // Open outfile file for writing if it specified
    if (info.outFile) {
        output = fopen(info.outFile, "w");
    } else {
        output = stdout;
    }

    // Try writing to output file / stdout
    size_t wrote = fwrite(body, 1, len, output);
    fflush(output);

    // Check if error occured
    if (wrote < len) {
        if (output != stdout) {
            fclose(output);
        }
        free(body);
        fprintf(stderr, cannotWriteError);
        fflush(stderr);
        exit(CANNOT_WRITE_ERROR);
    }

    // Close output
    if (output != stdout) {
        fclose(output);
    }

    free(body);
}

/* error_response()
 *
 * This function is only to be called when an error HTTP response is
 * received by the client. Once called this function will print the given
 * body to stderr and exit with status INVALID_RESPONSE_ERROR.
 *
 * body: Body of HTTP response to be printed to stderr
 * len: Length of the body of HTTP response
 *
 * Errors: This function when called always exits with status
 *     INVALID_RESPONSE_ERROR
 */
void error_response(unsigned char* body, unsigned long len)
{
    // Write each character of body to stderr
    for (unsigned long i = 0; i < len; i++) {
        fputc(body[i], stderr);
    }
    fflush(stderr);

    // Exit
    free(body);
    exit(INVALID_RESPONSE_ERROR);
}

/* wait_http_response()
 *
 * This function waits for a HTTP response from the server. Once a HTTP
 * response is received it will handle it in the following way:
 * 1. If a valid response (200) is received then the function will store the
 *    body of the HTTP response into either the stdout or specified output
 * file.
 * 2. If an invalid response (not 200) is received then the function will
 * call error_response() and print the body of the HTTP response to stderr.
 *
 * info: An Instance of the ClientInfo struct
 * socketFd: Socket File descriptor
 *
 * Errors: If for some reason the server closes the socket this function
 *     will exit with status NETWORK_CLOSE_ERROR by calling the
 *     network_closed_error() function.
 */
void wait_http_response(ClientInfo info, int socketFd)
{
    FILE* stream = fdopen(socketFd, "r");

    // HTTP response variables
    int status;
    char* statusExplanation;
    unsigned char* body;
    unsigned long len;
    HttpHeader** headers;

    // Wait for HTTP response.
    if (get_HTTP_response(
                stream, &status, &statusExplanation, &headers, &body, &len)) {

        // Free stuff that's not necessary
        fclose(stream);
        free(statusExplanation);
        free_array_of_headers(headers);

        if (status == VALID_RESPONSE) { // If response of status 200
            successful_response(info, body, len);
        } else { // If error response
            error_response(body, len);
        }
    } else { // If connection to server was terminated by the server.
        fclose(stream);
        network_closed_error();
    }
}

/* handle_sigpipe()
 *
 * This function is a signal handler and is only to be used with sigaction.
 * This is an empty function to catch SIGPIPE and do nothing.
 *
 * sig: Signal
 */
void handle_sigpipe(int sig __attribute__((unused)))
{
}

int main(int argc, char** argv)
{
    // Ignore SIGPIPE
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigpipe;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sa, NULL);

    // Process command line
    ClientInfo info = process_command_line(argc, argv);

    // Check that files are readable
    FILE* inputFile = file_check(info);
    int socketFd = attempt_connect(info.portno, inputFile);

    // Read image
    ImageFile image = read_image(inputFile);

    // Create and sent http request
    int socketFd2 = dup(socketFd);
    send_http_request(info, image, socketFd2);
    wait_http_response(info, socketFd);

    // Close socket when finished
    close(socketFd);

    return 0;
}
