#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <csse2310a4.h>
#include <FreeImage.h>
#include <semaphore.h>
#include <csse2310_freeimage.h>
#include <signal.h>
#include "common.h"

/* Information of a single server - Contains all necessary variables including
 * port number and max connection number
 */
typedef struct {
    char* port;
    int maxConns;
} ServerInfo;

/* Server statistics - Constains all necessary variables for server statistics
 * counting and maximum connection limiting including the necessary semaphores.
 */
typedef struct {
    sem_t maxConnsLock;
    sem_t statsLock;
    unsigned int currentClients;
    unsigned int totalClients;
    unsigned int successRequests;
    unsigned int failRequests;
    unsigned int completedOperations;
    int maxConns;
} ServerStats;

/* Information for a single SIGHUP signal handling thread */
typedef struct {
    sigset_t set;
    ServerStats* stats;
} SignalThreadInfo;

/* Information for a single connection between the server and a specific client
 */
typedef struct {
    int clientFd;
    ServerStats* serverStats;
} ClientData;

// Server Program Values
typedef enum {
    MAX_CONNS = 10000,
    MIN_CONNS = 0,
    MAX_IMAGE_SIZE = 8388608,
    MAX_CMD_ARG = 5,
    OP_ERROR_MSG_DEFAULT = 30,
    SIZE_ERROR_MSG_DEFAULT = 28
} ServerValues;

// HTTP response statuses
typedef enum {
    SUCCESS = 200,
    BAD_METHOD = 405,
    BAD_GET = 404,
    BAD_POST = 400,
    IMAGE_TOO_LARGE = 413,
    BAD_IMAGE = 422,
    OPERATION_ERROR = 501
} HttpStatus;

// Server statistics values
typedef enum {
    CONNECT = 0,
    DISCONNECT = 1,
    HTTP_SUCCESS = 2,
    HTTP_FAIL = 3,
    OPERATE_IMAGE = 4
} StatChange;

// Program/Server exit codes
typedef enum { USAGE_ERROR = 5, PORT_ERROR = 17 } ExitStatus;

// SIGHUP statistic messages
const char* const currentClientsMsg = "Currently connected clients: %u\n";
const char* const finishedClientsMsg = "Num completed clients: %u\n";
const char* const successHttpMsg = "Successfully processed HTTP requests: %u\n";
const char* const failHttpMsg = "HTTP requests unsuccessful: %u\n";
const char* const imageOperationMsg = "Operations on images completed: %u\n";

// Command line option arguments
const char* const portArg = "--port";
const char* const connsArg = "--maxConns";

// Error message
const char* const usageError
        = "Usage: uqimageproc [--port portnum] [--maxConns num]\n";
const char* const portError = "uqimageproc: unable to listen on port \"%s\"\n";

/* usage_error()
 *
 * This function prints the usageError message and exits with a status of
 * ERROR_USAGE.
 *
 * Errors: The program will exit with status USAGE_ERROR if this function is
 *     called.
 */
void usage_error(void)
{
    fprintf(stderr, usageError);
    exit(USAGE_ERROR);
}

/* port_error()
 *
 * This function prints the format string portError message to stderr and exits
 * with a status of PORT_ERROR.
 *
 * port: Failed port number/string
 * ai: A pointer to an addrinfo struct instance
 * listenFd: Socket file descriptor. (Assumed to be < 0 if socket wasn't opened)
 *
 * Errors: The program will exit with status PORT_ERROR if this function is
 *     called.
 */
void port_error(char* port, struct addrinfo* ai, int listenFd)
{
    // Free addrinfo
    if (ai) {
        freeaddrinfo(ai);
    }

    // Close socket if it was created
    if (listenFd > 0) {
        close(listenFd);
    }

    // Exit
    fprintf(stderr, portError, port);
    exit(PORT_ERROR);
}

/* change_stats
 *
 * This function changes the server statistics and is multi-thread safe using
 * semaphores.
 *
 * stats: A pointer to an instance of the ServerStats struct
 * type: One of the StatChange enums. This describes the stat to be incremented
 *     or decremented.
 */
void change_stats(ServerStats* stats, StatChange type)
{
    // Wait for semaphore
    sem_wait(&stats->statsLock);

    // Increment / Decrement appropriate stats counter
    switch (type) {
    case CONNECT: // When new client connects
        stats->currentClients++;
        break;
    case DISCONNECT: // When a client disconnects
        stats->totalClients++;
        stats->currentClients--;
        break;
    case HTTP_SUCCESS: // When a successful http response is sent
        stats->successRequests++;
        break;
    case HTTP_FAIL: // When a fail http rseponse is sent
        stats->failRequests++;
        break;
    case OPERATE_IMAGE: // Each time the server operates on an image
        stats->completedOperations++;
    }

    // Post semaphore
    sem_post(&stats->statsLock);
}

/* process_command_line()
 *
 * This function processes and checks the command line arguments. Here are the
 * checks that happen:
 * 1. The command line specifiers are either --port or --maxConns.
 * 2. The command line specifiers are followed by a non-empty value.
 * 3. The following value for the --maxConns specifier is a non-negative
 *    integer value.
 * 4. There are no duplicate specifiers
 *
 * argc: Number of command line arguments
 * argv: Array of command line arguments
 *
 * Returns: A 'filled' out instance of the ServerInfo struct.
 * Errors: If any of the 4 requirements above aren't met then the program exits
 *     using by calling the usage_error() function.
 */
ServerInfo process_command_line(int argc, char** argv)
{
    // Check argc < 6
    if (argc > MAX_CMD_ARG) {
        usage_error();
    }

    // Create serverinfo struct instance
    ServerInfo server = {NULL, -1};

    // Loop over each command line argument
    for (int i = 1; i < argc; i++) {
        // Port Argument
        if (!server.port && !strcmp(argv[i], portArg) && (i + 1 < argc)
                && !is_empty(argv[i + 1])) {
            server.port = argv[i + 1];
            // MaxConns Argument
        } else if (server.maxConns == -1 && !strcmp(argv[i], connsArg)
                && (i + 1 < argc) && !is_empty(argv[i + 1])) {
            int conns = atoi(argv[i + 1]);
            if (conns > MAX_CONNS || conns < MIN_CONNS) {
                usage_error();
            } else { // Error!
                server.maxConns = conns;
            }
        } else { // Error!
            usage_error();
        }
        i++;
    }

    return server;
}

/* get_port_num()
 *
 * This function gets the port number that the server is listening on and prints
 * it to stderr.
 *
 * listenFd: Server socket file descriptor.
 */
void get_port_num(int listenFd)
{
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(listenFd, (struct sockaddr*)&ad, &len)) {
        perror("Sockname");
    }
    fprintf(stderr, "%u\n", ntohs(ad.sin_port));
    fflush(stderr);
}

/* check_port()
 *
 * When this function is called the server will attempt to listen to the 'port'
 * number specified within the given ServerInfo struct.
 *
 * server: An instance of the ServerInfo struct.
 *
 * Returns: When port is successfully opened for listening the server socket
 *     file descriptor will be returned
 * Errors: If for some reason the socket cannot be created, the given port
 *     within the command line is invalid, a socket could not be bound to
 *     an address or a socket cannot be listened on, the server will shut down
 *     and exit using the port_error() function.
 *
 * REF: This function is inspired by server-multithreaded.c given during week 10
 * REF: lectures.
 */
int check_port(ServerInfo server)
{
    char* port;

    if (server.port == NULL) { // Ephemeral port
        port = "0";
    } else { // Provided port
        port = server.port;
    }

    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err;
    if ((err = getaddrinfo("localhost", port, &hints, &ai))) {
        port_error(port, ai, -1);
    }

    // Try creating socket -> Binding to a port -> Listen to a port
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        port_error(port, ai, listenfd);
    }

    int optVal = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int))
            < 0) {
        port_error(port, ai, listenfd);
    }
    if (bind(listenfd, ai->ai_addr, sizeof(struct sockaddr)) < 0) {
        port_error(port, ai, listenfd);
    }

    if (listen(listenfd, 0) < 0) { // Listen
        port_error(port, ai, listenfd);
    }

    // Print port number
    get_port_num(listenfd);
    freeaddrinfo(ai);

    return listenfd;
}

/* read_home_page()
 *
 * This function reads the home page body that needs to be returned for a
 * valid GET HTTP request.
 *
 * Returns: The body of HTML file.
 */
char* read_home_page(void)
{
    char* homePage = NULL;
    FILE* file = fopen("/local/courses/csse2310/resources/a4/home.html", "r");
    char nextChar;
    int count = 0;

    while ((nextChar = fgetc(file)) != EOF) {
        homePage = realloc(homePage, sizeof(char) * (count + 1));
        homePage[count++] = nextChar;
    }

    // Add null character
    homePage = realloc(homePage, sizeof(char) * (count + 1));
    homePage[count] = '\0';

    // Close file stream
    fclose(file);

    return homePage;
}

/* send_http_response()
 *
 * This function builds a http response and sends it to a client.
 *
 * fd: Socket file descriptor for an accepted connection.
 * status: Status for HTTP response
 * statusExplanation: Explanation for HTTP response
 * headers: Headers for the HTTP response.
 * body: Body for HTTP response.
 * bodySize: Size of Body for the HTTP response
 */
void send_http_response(int fd, int status, const char* statusExplanation,
        HttpHeader** headers, const unsigned char* body, unsigned long bodySize)
{
    // Create HTTP response
    unsigned long responseLen;
    unsigned char* response = construct_HTTP_response(
            status, statusExplanation, headers, body, bodySize, &responseLen);

    // Send HTTP response
    write(fd, response, responseLen);
    free(response);
    free_array_of_headers(headers);
}

/* create_header()
 *
 * This function creates the headers array which includes only one
 * "Content-Type" header.
 *
 * typeValue: The value for the Content-Type header.
 *
 * Returns: An array of HttpHeader pointers. (Only one instance of HttpHeader
 *     will be included which is "Content-Type").
 */
HttpHeader** create_header(char* typeValue)
{
    char* typeName = "Content-Type";

    // Create single HttpHeader instance
    HttpHeader* type = malloc(1 * sizeof(HttpHeader));
    type->name = strdup(typeName);
    type->value = strdup(typeValue);

    // Add single instance to array of instances
    HttpHeader** headers = malloc(sizeof(HttpHeader*) * 2);
    headers[0] = type;
    headers[1] = NULL;

    return headers;
}

/* check_method()
 *
 * This function checks whether the given HTTP request's method is either a
 * POST or GET. If 'method' is neither then an error HTTP response will be sent
 * to the client.
 *
 * fd: Socket file descriptor for an accepted connection.
 * method: Method of HTTP request.
 * stats: A pointer to an instance of the ServerStats struct.
 *
 * Returns: If the given HTTP request's method was valid then 0 is returned.
 *     Otherwise 1.
 */
int check_method(int fd, char* method, ServerStats* stats)
{
    if (strcmp(method, "POST") && strcmp(method, "GET")) {
        // Construct HTTP response
        char* message = "Invalid method on request list\n";
        int messageLen = strlen(message);
        const char* explanation = "Method Not Allowed";
        HttpHeader** headers = create_header("text/plain");

        // Send HTTP response and change stats
        send_http_response(fd, BAD_METHOD, explanation, headers,
                (unsigned char*)message, messageLen);
        change_stats(stats, HTTP_FAIL);

        return 1;
    }

    return 0;
}

/* check_get_request()
 *
 * If the HTTP request received is a GET method then this function will check
 * if the given address is correct ('/' and nothing else). If a the GET HTTP
 * request is valid then a success HTTP response will be sent to the client
 * with the body being the home page HTML. Otherwise an error HTTP response will
 * be sent to the client.
 *
 * fd: Socket file descriptor for an accepted connection.
 * method: Method of HTTP request.
 * address: Address of the HTTP request.
 * stats: A pointer to an instance of the ServerStats struct.
 *
 * Returns: If the given HTTP request was a valid GET HTTP request then 0 is
 *     returned. Otherwise 1.
 */
int check_get_request(int fd, char* method, char* address, ServerStats* stats)
{
    // Construct headers
    HttpHeader** headers;

    // Invalid home page request
    if (!strcmp(method, "GET") && strcmp(address, "/")) {
        // Construct HTTP request
        char* message = "Invalid address\n";
        int messageLen = strlen(message);
        const char* explanation = "Not Found";
        headers = create_header("text/plain");

        // Send http response and change stats
        send_http_response(fd, BAD_GET, explanation, headers,
                (unsigned char*)message, messageLen);
        change_stats(stats, HTTP_FAIL);

        return 1;
    }

    // Valid home page request
    if (!strcmp(method, "GET") && !strcmp(address, "/")) {
        // Construct HTTP request
        char* message = read_home_page();
        int messageLen = strlen(message);
        const char* explanation = "OK";
        headers = create_header("text/html");

        // Send http response and change stats
        send_http_response(fd, SUCCESS, explanation, headers,
                (unsigned char*)message, messageLen);
        free(message);
        change_stats(stats, HTTP_SUCCESS);

        return 1;
    }

    return 0;
}

/* check_operation_arg_number()
 *
 * This function checks the amount of arguments present for each image
 * operation. For rotate and flip there must be only one argument present, no
 * more, no less. For scale there must be only two arguments present no more,
 * no less.
 *
 * split: Array of strings in the format of [operations, arg, arg2, ...].
 *        (This parameter is assumed to be a char** type created by using
 *        split_by_char() function)
 *
 * Returns: If the correct number of arguments are present then 1 is returned
 *     otherwise 0.
 */
int check_operation_arg_number(char** split)
{
    int argCount = 0;
    char* type = split[0];

    // Check that the first 'word' is either rotate, flip or scale
    if (!strcmp(type, "rotate") || !strcmp(type, "flip")) {
        argCount = 1;
    } else if (!strcmp(type, "scale")) {
        argCount = 2;
    } else {
        return 0;
    }

    // Check correct amount of args for scale, flip and rotate
    for (int i = 1; i < (argCount + 2); i++) {
        if (i < argCount + 1 && split[i] == NULL) {
            return 0;
        }
        if (i == argCount + 1 && split[i] != NULL) {
            return 0;
        }
    }

    return 1;
}

/* check_operation_arg_valid()
 *
 * This function checks that the arguments of each operations are valid.
 *
 * split: Array of strings in the format of [operations, arg, arg2, ...].
 *        (This parameter is assumed to be a char** created by using
 *        split_by_char() function).
 *
 * Returns: If the operation arguments are valid 1 is returned, otherwise 0.
 */
int check_operation_arg_valid(char** split)
{
    char* type = split[0];

    if (!strcmp(type, "rotate") && !check_rotate_arg(split[1])) {
        return 0;
    }
    if (!strcmp(type, "flip") && !check_flip_arg(split[1])) {
        return 0;
    }
    if (!strcmp(type, "scale") && !check_scale_arg(split[1], split[2])) {
        return 0;
    }

    return 1;
}

/* check_image_operation()
 *
 * This function checks that the image 'operation' (and its arguments) given
 * are valid.
 *
 * operation: An operation and it's argument in string format separated by
 *     commas.
 *
 * Returns: If the given 'operation' is invalid then 0 will return otherwise 1.
 */
int check_image_operation(char* operation)
{
    // Split given operation string by a comma.
    char* opCopy = strdup(operation);
    char** split = split_by_char(opCopy, ',', 0);
    int returnValue = 1;

    if (split[0] == NULL) {
        returnValue = 1;
    }

    // Check correct number of arguments
    if (returnValue && !check_operation_arg_number(split)) {
        returnValue = 0;
    }

    // Check that arguments are valid
    if (returnValue && !check_operation_arg_valid(split)) {
        returnValue = 0;
    }

    free(opCopy);
    free(split);

    return returnValue;
}

/* check_post_request()
 *
 * This function checks that a given POST request is valid. It's validity will
 * depend on if the address is in a valid format
 *
 * fd: Socket file descriptor for an accepted connection
 * method: Method of HTTP request
 * address: Address of HTTP request
 * stats: A pointer to an instance of the ServerStats struct.
 *
 * Returns: If the POST request is valid then an array of strings
 *     (NULL terminated) will be returned.
 */
char** check_post_request(
        int fd, char* method, char* address, ServerStats* stats)
{
    // Check method is POST
    if (strcmp(method, "POST")) {
        return 0;
    }

    bool invalidResp = false;

    // Split address by '/'
    char** operations = split_by_char(address, '/', 0);
    int i = 1;
    // If request address contains nothing or doesn't start with '/'
    if (strcmp(operations[0], "") || operations[0] == NULL) {
        invalidResp = true;
    }

    // Loop over each operation request
    while (operations[i] != NULL && !invalidResp) {
        // Check each operation is in the right format
        if (!check_image_operation(operations[i])) {
            invalidResp = true;
        }
        i++;
    }

    if (invalidResp) { // If invalid POST request sent fail HTTP response
        // Construct http response
        HttpHeader** headers = create_header("text/plain");
        char* message = "Invalid operation requested\n";
        int messageLen = strlen(message);
        const char* explanation = "Bad Request";

        // Send http resposne
        send_http_response(fd, BAD_POST, explanation, headers,
                (unsigned char*)message, messageLen);
        change_stats(stats, HTTP_FAIL);
        free(operations);
        return NULL;
    }

    return operations;
}

/* check_image_size()
 *
 * This function checks if the image from the HTTP response is too large.
 *
 * fd: Socket file descriptor to an accepted connection.
 * imageSize: Size of image in bytes.
 * stats: A pointer to instance of the ServerStats struct.
 *
 * Returns: If the image size is valid then the function 0 is returned.
 *     Otherwise 1.
 */
int check_image_size(int fd, unsigned long imageSize, ServerStats* stats)
{
    // Check if image size too large
    if (imageSize > MAX_IMAGE_SIZE) {
        HttpHeader** headers = create_header("text/plain");

        // Create HTTP response message
        size_t messageSize = SIZE_ERROR_MSG_DEFAULT + sizeof(imageSize);
        char* message = malloc(sizeof(char) * messageSize);
        snprintf(message, messageSize, "Image is too large: %lu bytes\n",
                imageSize);
        int messageLen = strlen(message);
        const char* explanation = "Payload Too Large";

        // Send http response
        send_http_response(fd, IMAGE_TOO_LARGE, explanation, headers,
                (unsigned char*)message, messageLen);
        change_stats(stats, HTTP_FAIL);
        free(message);

        return 1;
    }

    return 0;
}

/* invalid_image_response()
 *
 * This function creates and sends an invalid image HTTP response to a specified
 * client.
 *
 * fd: Socket file descriptor of an accepted connection.
 */
void invalid_image_response(int fd)
{
    char* message = "Invalid image received\n";
    int messageLen = strlen(message);
    const char* explanation = "Unprocessable Content";
    HttpHeader** headers = create_header("text/plain");

    // Send http response
    send_http_response(fd, BAD_IMAGE, explanation, headers,
            (unsigned char*)message, messageLen);
}

/* operation_failed_response()
 *
 * This function creates and sends an operation failed HTTP response to a
 * specified client.
 *
 * fd: Socket file descriptor of an accepted connection.
 * failedOperation: The operation type which is one of 'rotate', 'flip' or
 *     'scale' that the program failed at.
 */
void operation_error_response(int fd, char* failedOperation)
{
    // Create HTTP resposne
    HttpHeader** headers = create_header("text/plain");
    size_t messageSize = OP_ERROR_MSG_DEFAULT + sizeof(failedOperation);
    char* message = malloc(sizeof(char) * messageSize);
    snprintf(message, messageSize, "Operation did not complete: %s\n",
            failedOperation);
    int messageLen = strlen(message);
    const char* explanation = "Not Implemented";

    // Send HTTP response
    send_http_response(fd, OPERATION_ERROR, explanation, headers,
            (unsigned char*)message, messageLen);
    free(message);
}

/* operation_success_response()
 *
 * This function creates and sends a success HTTP response to a specified
 * client. This function should only be called when all image operations
 * have succeeded.
 *
 * fd: Socket file descriptor of an accepted connection.
 * imageMap: A pointer to a FIBITMAP struct instance.
 */
void operation_success_response(int fd, FIBITMAP* imageMap)
{
    // Convert image from BITMAP to raw binary data
    unsigned long imageSize;
    unsigned char* image = fi_save_png_image_to_buffer(imageMap, &imageSize);

    // Create HTTP response
    HttpHeader** headers = create_header("image/png");
    const char* explanation = "OK";

    // Send HTTP response
    send_http_response(fd, SUCCESS, explanation, headers, image, imageSize);
    free(image);
    FreeImage_Unload(imageMap);
}

/* operate_on_image()
 *
 * This function performs all the types of image manipulation specified within
 * 'operations' on a given 'imageMap'.
 *
 * fd: Socket file descriptor of an accepted connection.
 * imageMap: A pointer to a FIBITMAP struct instance.
 * operations: An array of strings in the format of [operation,arg,...].
 *     (Assumed to a char** type created by using the split_by_char() function).
 * stats: A pointer to a ServerStats struct instance.
 *
 * Returns: If any operations 'rotate', 'flip' or 'scale' was unsuccessful for
 *     some reason the function returns NULL. Otherwise a new modified pointer
 *     to instance of FIBITMAP is returned.
 */
FIBITMAP* operate_on_image(
        int fd, FIBITMAP* imageMap, char** operations, ServerStats* stats)
{
    int32_t flipStatus = -1;
    int i = 1;
    FIBITMAP* tempMap = NULL;
    FIBITMAP* returnMap = imageMap;

    // Loop over each operation and split
    while (operations[i] != NULL) {
        tempMap = returnMap;
        char** singleOp = split_by_char(operations[i], ',', 0);

        if (!strcmp(singleOp[0], "rotate")) { // Rotate operation
            double degrees = atoi(singleOp[1]);
            returnMap = FreeImage_Rotate(returnMap, degrees, NULL);
            FreeImage_Unload(tempMap);
        } else if (!strcmp(singleOp[0], "scale")) { // Scale operation
            int width = atoi(singleOp[1]);
            int height = atoi(singleOp[2]);
            returnMap = FreeImage_Rescale(
                    returnMap, width, height, FILTER_BILINEAR);
            FreeImage_Unload(tempMap);
        } else if (!strcmp(singleOp[0], "flip")) { // Flip operation
            if (!strcmp(singleOp[1], "h")) {
                flipStatus = FreeImage_FlipHorizontal(returnMap);
            } else {
                flipStatus = FreeImage_FlipVertical(returnMap);
            }
        }
        // Check if operation failed
        if (returnMap == NULL || !flipStatus) {
            operation_error_response(fd, singleOp[0]); // Send fail response
            change_stats(stats, HTTP_FAIL);
            free(singleOp);
            return NULL;
        }
        change_stats(stats, OPERATE_IMAGE);
        free(singleOp);
        i++;
    }
    return returnMap;
}

/* process_image()
 *
 * This function 'processes' the given 'image' firstly by trying to load it
 * into a FIBITMAP and then sending it off to be manipulated. If loading the
 * image to a FIBITMAP fails (meaning that it is an invalid image) the function
 * will send a fail HTTP response to the specified client. If the loading and
 * image manipulations were all successful a success HTTP request with the body
 * as the new image will be sent to the specified client.
 *
 * fd: Socket file descriptor of an accepted connection.
 * image: The image to be manipulated.
 * imageSize: The size of the given 'image'.
 * operations: An array of strings in the format of [operation,arg,...].
 *     (Assumed to a char** type created by using the split_by_char() function).
 * stats: A pointer to an instance of the ServerStats struct.
 *
 * Returns: If the loading of image to a FIBITMAP fails or any operations on an
 *     image fails for some reason 0 is returned. Otherwise 1.
 */
int process_image(int fd, unsigned char* image, unsigned long imageSize,
        char** operations, ServerStats* stats)
{
    // Try loading image into BITMAP
    FIBITMAP* imageMap = fi_load_image_from_buffer(image, imageSize);
    if (imageMap == NULL) { // Loading image failed
        invalid_image_response(fd);
        change_stats(stats, HTTP_FAIL);
        free(operations);
        return 0;
    }

    // Do all image operation requests
    imageMap = operate_on_image(fd, imageMap, operations, stats);

    // Check if operations on the image failed.
    if (imageMap == NULL) {
        free(operations);
        return 0;
    }

    // If everything was successful send it to the client.
    operation_success_response(fd, imageMap);
    change_stats(stats, HTTP_SUCCESS);
    free(operations);
    return 1;
}

/* process_request()
 *
 * This function processes a received HTTP request from a client in the
 * following order:
 * 1. Check that the request method is either a GET or POST.
 * 2. If a GET request is received check that the address of the HTTP request is
 *    valid.
 * 3. If a POST request is received check that the address of the HTTP request
 *    is valid.
 * 4. Check that the image received (body) within the HTTP request is of valid
 *    size.
 *
 * fd: Socket file descriptor of an accepted connection.
 * method: Method of HTTP request.
 * address: Address of HTTP request.
 * len: Length of the body of the HTTP request.
 * stats: A pointer to an instance of the ServerStats struct.
 *
 * Returns: If the received HTTP request was a POST request and includes a
 *     valid POST address then the function will return a char** type created
 *     from split_by_char() function which includes all the image operations
 *     requested. Otherwise NULL will be returned.
 */
char** process_request(int fd, char* method, char* address, unsigned long len,
        ServerStats* stats)
{
    // Validate the request's method and GET requests
    if (check_method(fd, method, stats)
            || check_get_request(fd, method, address, stats)) {
        return NULL;
    }

    // Check POST request
    char** operations = check_post_request(fd, method, address, stats);

    // Check if POST request was valid
    if (operations == NULL) {
        return NULL;
    }

    // Check if image size is valid
    if (check_image_size(fd, len, stats)) {
        free(operations);
        return NULL;
    }

    return operations;
}

/* free_http_request()
 *
 * This function frees all the necessary dynamically allocated memory for a
 * HTTP request.
 *
 * method: Method of HTTP request
 * address: Address of HTTP request
 * body: Body of HTTP request
 * headers: Headers of HTTP request
 */
void free_http_request(
        char* method, char* address, unsigned char* body, HttpHeader** headers)
{
    free(method);
    free(address);
    free(body);
    free_array_of_headers(headers);
}

/* client_thread()
 *
 * This is a server thread function for each client connected to the server.
 * This function repeatedly waits for a HTTP request and once upon receiving one
 * it will process it.
 *
 * arg: Expected to a pointer to an instance of ClientData struct.
 *
 * Returns: This function only returns NULL.
 */
void* client_thread(void* arg)
{
    ClientData* data = (ClientData*)arg;
    ServerStats* stats = data->serverStats;
    int fd = data->clientFd;
    FILE* stream = fdopen(fd, "r");
    change_stats(stats, CONNECT);

    // Request info
    char *method, *address;
    unsigned char* body;
    unsigned long len;
    HttpHeader** headers;

    // Loop to continuosly handle HTTP requests
    while (1) {
        if (get_HTTP_request(
                    stream, &method, &address, &headers, &body, &len)) {

            // Check for invalid requests
            char** operations
                    = process_request(fd, method, address, len, stats);

            if (!operations) { // If invalid request
                free_http_request(method, address, body, headers);
                continue;
            }

            // Now process image
            process_image(fd, body, len, operations, stats);

            // Free necessary information
            free_http_request(method, address, body, headers);
        } else { // Client disconnected
            break;
        }
    }

    // Clean up and exit
    change_stats(stats, DISCONNECT);
    if (stats->maxConns > 0) {
        sem_post(&(stats->maxConnsLock));
    }
    fclose(stream);
    free(data);
    return NULL;
}

/* process_connections()
 *
 * This function is responsible for handling incoming client connections on the
 * server. It continuously loops to accept incoming connection requests from
 * clients. If maxConns is specified (an integer larger than 0) then it will
 * make sure to limit the amount of clients connected to the server at a time
 * using semaphores. Once a client is accepted it will be put in a thread and
 * sent off.
 *
 * fdServer: Socket file descriptor representing the endpoint for communication
 * maxConns: An integer representing the maximum connections possible at a given
 *     time for the server.
 * stats: A pointer to an instance of the ServerStats struct
 *
 * REF: This function is inspired by server-multithreaded.c given during week 10
 * REF: lectures.
 */
void process_connections(int fdServer, int maxConns, ServerStats* stats)
{
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    // Repeatedly accept connections
    while (1) {
        // Check max connections
        if (maxConns > 0) {
            sem_wait(&stats->maxConnsLock);
        }

        fromAddrSize = sizeof(struct sockaddr_in);
        // Block, waiting for a new connection.
        fd = accept(fdServer, (struct sockaddr*)&fromAddr, &fromAddrSize);
        if (fd < 0) { // If connection could NOT be accepted
            if (maxConns > 0) {
                sem_post(&stats->maxConnsLock);
            }
            continue;
        }

        // Turn our client address
        char hostname[NI_MAXHOST];
        getnameinfo((struct sockaddr*)&fromAddr, fromAddrSize, hostname,
                NI_MAXHOST, NULL, 0, 0);

        // Create thread for a single client
        ClientData* clientData = malloc(sizeof(ClientData));
        clientData->clientFd = fd;
        clientData->serverStats = stats;
        pthread_t threadID;
        pthread_create(&threadID, NULL, client_thread, clientData);
        pthread_detach(threadID);
    }
}

/* signal_handler()
 *
 * This is a thread function specifically designed to catch SIGHUP signals.
 * When a SIGHUP signal is caught it will print out the current statistics of
 * the server.
 *
 * arg: Expected to be pointer to an instance of the sigInfo struct.
 *
 * REF: Inspired by the man pages (3) pthread_sigmask
 */
void* signal_handler(void* arg)
{
    SignalThreadInfo* sigInfo = (SignalThreadInfo*)arg;
    sigset_t set = sigInfo->set;
    ServerStats* stats = sigInfo->stats;

    // Now wait for signal to happen
    int signal;

    // Wait for signal
    while (1) {
        sigwait(&set, &signal);
        // If signal is SIGHUP print statistics
        if (signal == SIGHUP) {
            fprintf(stderr, currentClientsMsg, stats->currentClients);
            fprintf(stderr, finishedClientsMsg, stats->totalClients);
            fprintf(stderr, successHttpMsg, stats->successRequests);
            fprintf(stderr, failHttpMsg, stats->failRequests);
            fprintf(stderr, imageOperationMsg, stats->completedOperations);
            fflush(stderr);
        }
    }
}

/* create_signal_thread()
 *
 * This function creates a signal handling thread and sends it off.
 *
 * sigInfo: A pointer to an instance of the sigInfo struct.
 */
void create_signal_thread(SignalThreadInfo* sigInfo)
{
    pthread_t signalThread;
    pthread_create(&signalThread, NULL, signal_handler, sigInfo);
    pthread_detach(signalThread);
}

/* setup_server_stats()
 *
 * This function initializes a ServerStats struct. This includes all statistics
 * values (to 0), the semaphore for connection limiting and a binary semaphore
 * for statistics update.
 *
 * maxConns: A value representing the maximum connections possible for the
 *     server at a given moment.
 *
 * Returns: Returns a pointer to a "filled" instance of the ServerStats struct.
 */
ServerStats* setup_server_stats(int maxConns)
{
    ServerStats* serverStats = malloc(sizeof(ServerStats));

    if (maxConns > 0) {
        sem_init(&serverStats->maxConnsLock, 0, maxConns);
    }
    sem_init(&serverStats->statsLock, 0, 1);
    serverStats->currentClients = 0;
    serverStats->totalClients = 0;
    serverStats->successRequests = 0;
    serverStats->failRequests = 0;
    serverStats->completedOperations = 0;
    serverStats->maxConns = maxConns;

    return serverStats;
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

/* setupSignalMask()
 *
 * This function masks the SIGHUP signal for all threads created within the
 * program.
 *
 * stats: A pointer to an instance of the ServerStats struct.
 *
 * REF: Inspired by the man pages (3) pthread_sigmask
 */
void setup_signal_mask(ServerStats* stats)
{
    // Signal Mask for SIGHUP
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Create signal thread
    SignalThreadInfo* sigInfo = malloc(sizeof(SignalThreadInfo));
    sigInfo->set = set;
    sigInfo->stats = stats;
    create_signal_thread(sigInfo);
}

int main(int argc, char* argv[])
{
    // Ignore SIGPIPE
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigpipe;
    sigaction(SIGPIPE, &sa, NULL);

    // Process command line arguments
    ServerInfo server = process_command_line(argc, argv);

    // Check port
    int fdServer = check_port(server);

    // Set up server statistics
    ServerStats* serverStats = setup_server_stats(server.maxConns);

    // Set up SIGHUP handling thread
    setup_signal_mask(serverStats);

    // Starting receiving connections from clients
    process_connections(fdServer, server.maxConns, serverStats);

    return 0;
}
