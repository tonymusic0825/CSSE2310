#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <csse2310a3.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/wait.h>
#include <unistd.h>
#include <locale.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

/* Settings of a run of uqfindexec - Contains all necessary variables
 * and options for the current run
 */
typedef struct Settings {
    char* dir;
    bool dirDefault;
    bool parallel;
    bool statistics;
    bool allfiles;
    char* cmd;
    bool processFailed;
    bool sigInt;
} Settings;

/* A list of file names - Contains a list of file names for a certain
 * directory
 */
typedef struct FileList {
    char** files;
    int count;
} FileList;

// Program/Process exit codes
typedef enum ExitStatus {
    USAGE_ERROR = 18,
    DIR_ERROR = 6,
    CMD_ERROR = 9,
    PROCESS_ERROR = 16,
    CHILD_PROCESS_ERROR = 99,
    SIGINT_REC = 17
} ExitStatus;

// Magic number Values
typedef enum Values {
    MAX_CMD_LEN = 8,
    READ = 0,
    WRITE = 1,
    SUCCESS = 0,
    FAIL = 1,
    SIGNAL = 2,
    NOTEXEC = 3,
    TOTAL_STATS = 4
} Values;

// Signal Global Variable
bool sigintReceived = false;

// Error messages (Results in program termination)
const char* const usageError
        = "Usage: uqfindexec [--dir dir] [--parallel] "
          "[--statistics] [--allfiles] [--descend] [cmd]\n";
const char* const dirError
        = "uqfindexec: directory \"%s\" can not be accessed\n";
const char* const cmdError = "uqfindexec: command is not valid\n";
const char* const execError
        = "uqfindexec: unable to execute \"%s\" when processing \"%s\"\n";
const char* const readError = "uqfindexec: unable to open \"%s\" for "
                              "reading when processing \"%s\"\n";
const char* const writeError = "uqfindexec: unable to write \"%s\" "
                               "while processing \"%s\"\n";

// --statistics message
const char* const openingMsg = "Attempted to process a total of %d files\n";
const char* const successMsg = " - operations succeeded for %d files\n";
const char* const failedMsg = " - processing may have failed for %d files\n";
const char* const signalMsg
        = " - processing was terminated by signal for %d files\n";
const char* const notexecMsg = " - pipeline not executed for %d files\n";

// Command line arguments
const char* const dirArg = "--dir";
const char* const parallelArg = "--parallel";
const char* const statArg = "--statistics";
const char* const allFilesArg = "--allfiles";
const char* const descendArg = "--descend";

/* usage_error()
 * This function prints the usageError message and exits with status of enum
 * USAGE_ERROR.
 *
 * Errors: This program always exits with status USAGE_ERROR.
 */
void usage_error(void)
{
    fprintf(stderr, usageError);
    exit(USAGE_ERROR);
}

/* check_dd()
 *
 * This function checks if a given string starts with a double dash "--"
 *
 * str: String to check if it starts with "--"
 *
 * Returns: True if the given string starts with "--". If the given string is
 *     less than the length of 2 or the string doesn't start with "--" the
 *     function returns false.
 */
bool check_dd(char* str)
{
    if (strlen(str) < 2) { // If string too short
        return false;
    }

    if (str[0] == '-' && str[1] == '-') { // If string starts with "--"
        return true;
    }

    return false;
}

/* process_command_line()
 *
 * This function checks if the 'optional' arguments within the command line
 * are exactly one of --dir, --parallel, --statistics, --allfiles, --descend.
 * For the --dir argument the function will check that it's followed by an
 * associated value argument. The function will also check that there are no
 * duplicate arguments within the command line. (Any string starting with '--'
 * will be assumed as an attempt to start an 'optional' argument).
 *
 * argc: Number of command-line arguments
 * argv: Array of command line arguments
 *
 * Returns: Returns a pointer to an instance of a "filled-out" Settings struct.
 * Errors: Too many arguments (argc > 8) and the program will exit with status
 *     of USAGE_ERROR. If any arguments are not exactly one of --dir,
 *     --statistics, --allfiles, --descend, --parallel or the --dir argument
 *     is not followed by an associated value then the program will also exit
 *     with status USAGE_ERROR. This function calls the usage_error() function
 *     to exit.
 */
Settings process_command_line(int argc, char** argv)
{
    bool descend = false;

    if (argc > MAX_CMD_LEN) { // Check argc > 8
        usage_error(); // Otherwise Error!
    }

    // Create instance of Settings
    Settings config = {NULL, 0, 0, 0, 0, NULL, 0, 0};

    // Loop to check each argv argument
    for (int i = 1; i < argc; i++) {
        // Check --dic
        if (!strcmp(argv[i], dirArg) && !config.dir && argc > i + 1) {
            // Check directory value is not empty
            if (!strcmp(argv[i + 1], "")) {
                usage_error();
            }
            config.dir = argv[i + 1]; // Store directory value
            i++;
            // Check --parallel
        } else if (!strcmp(argv[i], parallelArg) && !config.parallel) {
            config.parallel = 1;
            // Check --statistics
        } else if (!strcmp(argv[i], statArg) && !config.statistics) {
            config.statistics = 1;
            // Check --allfiles
        } else if (!strcmp(argv[i], allFilesArg) && !config.allfiles) {
            config.allfiles = 1;
            // Check --descend
        } else if (!strcmp(argv[i], descendArg) && !descend) {
            descend = true;
            // Store cmd
        } else if (i == argc - 1 && strcmp(argv[i], dirArg)
                && strlen(argv[i]) > 0 && !check_dd(argv[i])) {
            config.cmd = argv[i];
        } else { // Otherwise Error!
            usage_error();
        }
    }
    return config;
}

/* dir_error()
 *
 * This function is called when a given directory doens't exist or cannot be
 * read. When this function is called the program prints dirError to stderr
 * and exits with status DIR_ERROR.
 *
 * dirName: Name of directory that the program tried to read.
 *
 * Errors: This program when called always exits with status DIR_ERROR.
 */
void dir_error(char* dirName)
{
    fprintf(stderr, dirError, dirName);
    exit(DIR_ERROR);
}

/* cmd_error()
 *
 * This function is called when a given command is invalid. When this function
 * is called the program prints cmdError to stderr and exits with status
 * CMD_ERROR.
 *
 * Errors: This program when called always exits with status CMD_ERROR.
 */
void cmd_error(void)
{
    fprintf(stderr, cmdError);
    exit(CMD_ERROR);
}

/* validate_args()
 *
 * This function validates the cmd argument and the provided directory.
 * It firstly checks that the directory exists and is readable.
 * Then it checks that the cmd argument provided is valid.
 * If the directory or cmd arguments were not set, then the default values
 * "." and "echo {}" will be set respectively.
 *
 * config: Pointer to an instance of the Settings struct. (Assumes that the
 *     Settings struct is "filled" using the process_command_line function)
 *
 * Returns: A pointer to an instance of the CommandPipeline struct
 * Errors: If the directory does not exist or isn't readable then the program
 *     will exit with status DIR_ERROR (calls dir_error()). If the cmd argument
 *     provided isn't valid then the program will exit with status CMD_ERROR
 *     (calls cmd_error()).
 */
CommandPipeline* validate_args(Settings* config)
{
    CommandPipeline* pipeline;

    // If directory was provided try opening dir
    if (config->dir != NULL) {
        DIR* dirTest = opendir(config->dir);
        // If opening failed, error
        if (!dirTest) {
            dir_error(config->dir);
        } else { // Otherwise close
            closedir(dirTest);
        }
    } else { // If dir not provided set default value
        config->dir = ".";
        config->dirDefault = 1;
    }

    // Check cmd was provided
    if (config->cmd != NULL) {
        // If commands are invalid, error
        pipeline = parse_pipeline_string(config->cmd);
        if (!pipeline) {
            cmd_error();
        }
    } else { // If cmd not provided set default value
        config->cmd = "echo {}";
        pipeline = parse_pipeline_string(config->cmd);
    }

    return pipeline;
}

/* is_hidden()
 *
 * This function checks that the given filename is hidden (starts with '.')
 *
 * fileName: File name to be checked
 *
 * Returns: true if file is hidden otherwise returns false
 */
bool is_hidden(const char* fileName)
{
    if (fileName[0] == '.') {
        return true;
    }

    return false;
}

/* check_dir_format()
 *
 * This function checks that the directory ends with a '/' and creates a new
 * directory ending with '/' if needed.
 *
 * dir: The directory string to be checked
 *
 * Returns: Returns duplicate of the given directory and returns it with
 *     '/' added if needed otherwise the duplicate of the original string is
 *     returned (duplicate is created with strdup()).
 */
char* check_dir_format(char* dir)
{
    int strLen = strlen(dir);
    char* fullDir = strdup(dir); // Duplicate directory string

    // Check if the last character is '/'
    if (dir[strLen - 1] != '/') {
        // Reallocate space for '/' and a NULL character.
        fullDir = realloc(fullDir, (strLen + 2) * sizeof(char));

        // Add '/' and '\0' at the end of string
        fullDir[strLen] = '/';
        fullDir[strLen + 1] = '\0';
    }

    return fullDir;
}

/* if_valid_file()
 *
 * This function checks that the given dirent* is one of three things.
 * 1. The directory entry is a hidden file
 * 2. The directory entry is a regular file
 * 3. The directory entry is a symbolic link. If so it will also check that
 *    the symbolic link points to a regular file.
 *
 * entry: A pointer to a dirent struct. Assumes this is not NULL.
 * dir: The directory of the 'entry' parameter.
 * config: Settings struct instance of the current program run.
 *
 * Returns: 0 is returned if the entry is not a regular file or it's a hidden
 *     file but the current config's allfiles settings are set to false. 0 is
 *     also returned if the entry is a symbolic link however it does not point
 *     to a regular file. Otherwise return 1.
 */
int if_valid_file(struct dirent* entry, char* dir, Settings config)
{
    struct stat sb;

    // Check hidden file
    if (is_hidden(entry->d_name) && !config.allfiles) {
        return 0;
    }

    // Create file directory
    char* linkPath = strdup(dir);
    // Create fullpath
    linkPath = realloc(
            linkPath, sizeof(char) * (strlen(entry->d_name) + strlen(dir) + 1));
    strcat(linkPath, entry->d_name);

    // Try stat() to get file information
    if (stat(linkPath, &sb) == 0) {
        if (S_ISREG(sb.st_mode)) { // If file is regular
            free(linkPath);
            return 1;
        }
    }

    free(linkPath);
    return 0;
}

/* read_dir()
 *
 * This function scans all files within a given directory. The files are sorted
 * and stored within a FileList struct. It calls the if_valid_file() to check
 * that each file within the directory is valid for the current program config.
 *
 * config: Settings struct instance fo the current program run.
 *
 * Returns: Returns a newly created and populated FileList instance.
 *     If no files are valid within the given directory it will still return
 *     the struct instance without it being populated.
 */
FileList read_dir(Settings config)
{
    // Create FileList instance and Open directory
    FileList fileList = {NULL, 0};
    char* dirpath = config.dir;
    char* fullDir = check_dir_format(config.dir);
    struct dirent** list;
    char* fileName;
    int numFiles;

    // Scan directory
    numFiles = scandir(dirpath, &list, NULL, alphasort);

    if (numFiles > 0) { // If 1 or more files resize fileList
        fileList.files = malloc(sizeof(char*) * numFiles);
    }

    // Store filepaths
    for (int i = 0; i < numFiles; i++) {
        fileName = strdup(list[i]->d_name);
        // Check if file is valid
        if (if_valid_file(list[i], fullDir, config)) {
            if (config.dirDefault) { // If default directory
                fileList.files[fileList.count++] = fileName;
            } else { // If directory specified by user
                char* fullFileName
                        = malloc(strlen(fileName) + strlen(fullDir) + 1);
                strcpy(fullFileName, fullDir);
                strcat(fullFileName, fileName);
                fileList.files[fileList.count++] = fullFileName;
                free(fileName); // Free filename since we don't need it here
            }
        } else {
            free(fileName);
        }
    }
    free(fullDir);

    // Free scandir
    // REF: The following 'free' code is from the examples within
    // REF: man (3) scandir
    while (numFiles--) {
        free(list[numFiles]);
    }
    free(list);

    return fileList;
}

/* count_placeholders()
 *
 * This function counts all occurrences of placeholders "{}" within a given
 * string.
 *
 * checkStr: String to check the amount of placeholders
 *
 * Returns: The integer value of the count of the amount of placeholders within
 *     the given string.
 */
int count_placeholders(char* checkStr)
{
    int count = 0;

    // Create a loop using strstr() to find placeholders
    while ((checkStr = strstr(checkStr, "{}")) != NULL) {
        count++; // Count occurence
        checkStr += 2; // Skip over the checked placeholder
    }

    return count;
}

/* handle_placeholders()
 *
 * This function changes all occurrences of placeholders "{}" within a given
 * string with the fill string.
 *
 * checkStr: String to change all placeholders
 * fill: The string to fill all the placeholders
 *
 * Returns: If there doesn't exist any placeholders it'll return the original
 *     strings (dynamic). Otherwise it'll return a new dynamically stored
 *     string.
 * REF: The placeholder substitution logic is inspired by the code at:
 * REF: https://stackoverflow.com/questions/44688310/
 * REF: The use of memcpy & strcpy is also inspired by the 'memcpy vs strcpy' at
 * REF: https://sternumiot.com/iot-blog
 *     /memcpy-c-function-examples-and-best-practices/
 */
char* handle_placeholders(char* checkStr, char* fill)
{
    int count = count_placeholders(checkStr);

    if (!count) { // If placeholders don't exist
        return strdup(checkStr);
    }

    // Calculate size of new string DON'T FORGET NULL TERMINATOR!
    size_t size = strlen(checkStr) - (count * 2) + (count * strlen(fill)) + 1;
    char* newStr = malloc(size);

    // Create temporary pointers
    char* nextPlaceholder;
    size_t lenToPlaceHolder;
    char* newStrPtr = newStr;
    char* tempPtr = checkStr;

    while (1) {
        // Find placeholder position
        nextPlaceholder = strstr(tempPtr, "{}");
        if (nextPlaceholder) {
            lenToPlaceHolder = nextPlaceholder - tempPtr;

            // Copy string up to placeholder and move newStr to next char space
            memcpy(newStrPtr, tempPtr, lenToPlaceHolder);
            newStrPtr += lenToPlaceHolder;

            // Now insert filename and move newStr to next char space
            memcpy(newStrPtr, fill, strlen(fill));
            newStrPtr += strlen(fill);

            // Move the checkStr past the current placeholder
            tempPtr = (nextPlaceholder + 2);
        } else {
            // No more placeholders, copy remaining string
            strcpy(newStrPtr, tempPtr);
            break;
        }
    }
    return newStr;
}

/* try_open_file()
 *
 * This function tries to open the file given in the argument.
 *
 * openFileName: The string of the name of the file trying to be opened.
 * fileName: The fileName that is being current processed
 * readOrWrite: A boolean value representing whether the openFileName is for
 *     reading or writing. (Assumes True is for read and False is for write)
 *
 * Returns: Returns -1 if the file could not be opened. Otherwise it returns
 *     the open file descriptor.
 * Errors: If the file could not be opened than the apprioriate message
 *     (differs depending on readOrWrite) will be printed to stderr.
 */
int try_open_file(char* openFileName, char* fileName, bool readOrWrite)
{
    char* newReadFileName = handle_placeholders(openFileName, fileName);
    int file;

    if (readOrWrite) { // If read file
        file = open(newReadFileName, O_RDONLY); // Try open
        if (file < 0) { // Error!
            fprintf(stderr, readError, newReadFileName, fileName);
            fflush(stderr);
            free(newReadFileName);
            return -1;
        }
    } else { // If write file
        file = open(newReadFileName, O_WRONLY | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR); // Try open
        if (file < 0) { // Error!
            fprintf(stderr, writeError, newReadFileName, fileName);
            fflush(stderr);
            free(newReadFileName);
            return -1;
        }
    }
    free(newReadFileName);

    return file;
}

/* cmd_copy()
 *
 * This function makes a deep copy of a command line. It deals with any
 * placeholders '{}' by replacing with the given filename (duplicated)
 * and then returns the new command line.
 *
 * oldCmd: The command to copy and process
 * fileName: File name to replace the placeholders within oldCmd
 *
 * Returns: A new dynamically allocated command line which is an array of
 *     strings.
 */
char** cmd_copy(char** oldCmd, char* fileName)
{
    // New command line
    char** newCmd = NULL;
    int count = 0;

    // Loop over commandline
    while (*oldCmd != NULL) {
        newCmd = realloc(newCmd, (count + 1) * sizeof(char*));

        // If placeholder replace with filename
        newCmd[count++] = handle_placeholders(*oldCmd, fileName);
        oldCmd++;
    }

    // Terminate with null pointer
    newCmd = realloc(newCmd, (count + 1) * sizeof(char*));
    newCmd[count] = NULL;

    return newCmd;
}

/* redirect_from_file()
 *
 * This function redirects the STDIN of a process to the given file.
 *
 * readFileName: Name of file to change STDIN to.
 * fileName: Name of file that the process is processing
 */
void redirect_from_file(char* readFileName, char* fileName)
{
    // Open file for reading
    int inputFile = try_open_file(readFileName, fileName, true);

    // Redirect stdin
    dup2(inputFile, STDIN_FILENO);
    close(inputFile);
}

/* redirect_to_file()
 *
 * This function redirects the STDOUT of a process to go to the given file.
 *
 * writeFileName: Name of file to change STDOUT to.
 * fileName: Name of file that the process is processing
 */
void redirect_to_file(char* writeFileName, char* fileName)
{
    // Open file for writing
    int outputFile = try_open_file(writeFileName, fileName, false);

    // Redirect STDOUT
    dup2(outputFile, STDOUT_FILENO);
    close(outputFile);
}

/* redirect_from_pipe()
 *
 * This function redirects the STDIN of a process to the given pipe file
 * descriptor.
 *
 * pipe: File descriptor of the read end of a pipe
 */
void redirect_from_pipe(int pipe)
{
    // Redirect STDIN
    dup2(pipe, STDIN_FILENO);
    close(pipe);
}

/* redirect_to_pipe()
 *
 * This function redirects the STDOUT of a process to the write end of a given
 * pipe.
 *
 * pipe: File descriptor of the write end of a pipe
 */
void redirect_to_pipe(int* pipe)
{
    // Close Read end of pipe
    close(pipe[READ]);

    // Redirect STDOUT
    dup2(pipe[WRITE], STDOUT_FILENO);
    close(pipe[WRITE]);
}

/* cmd_array_count()
 *
 * This function counts the number of strings within a command line array
 * excluding the NULL terminator (E.g. ["ls", "-li", NULL] returns 2).
 *
 * cmdArray: Array of strings which represent a command and its arguments
 *
 * Returns: The integer value for the count of the number of strings within
 *     cmdArray argument.
 */
int cmd_array_count(char** cmdArray)
{
    int count = 0;
    while (*cmdArray != NULL) {
        count++;
        cmdArray++;
    }

    return count;
}

/* spawn_child()
 *
 * This function spawns a child process. Redirects the STDIN of the first
 * process child or the STDOUT of the last process child to redirection files
 * if they are specified within CommandPipeLine struct. These child processes
 * will execute the given command line.
 *
 * pipeline: A pointer to a CommandPipeLine struct instance.
 * cmd: Command for the child process to execute.
 * cmdIdx: The index of the command to be executed in the cmdArray within
 *     CommandPipeLine struct instance.
 * fileName: File currently being processed
 * prevPipe: A pointer to the read end of the pipe used in the previous
 *     command's child process.
 * nextPipe: Pipe used in this child process and next child process as well.
 *
 * Returns: This function returns the process id of the child created.
 * Errors: Child Process exits with status CHILD_PROCESS_ERROR (99)
 *     when execution fails.
 */
pid_t spawn_child(CommandPipeline* pipeline, char** cmd, int cmdIdx,
        char* fileName, int* prevPipe, int nextPipe[])
{
    // Open pipe for output
    if (nextPipe != NULL && pipeline->numCmds > 1) {
        pipe(nextPipe);
    }

    pid_t pid = fork(); // Fork!

    if (!pid) { // child
        // Redirect input to pipe or inputfile
        if (pipeline->stdinFileName && cmdIdx == 0) { // STDIN
            redirect_from_file(pipeline->stdinFileName, fileName);
        } else if (cmdIdx > 0 && pipeline->numCmds > 1) {
            redirect_from_pipe(*prevPipe);
        }

        // Redirect output to pipe or outputfile
        if (pipeline->stdoutFileName && cmdIdx == (pipeline->numCmds) - 1) {
            redirect_to_file(pipeline->stdoutFileName, fileName);
        } else if (cmdIdx < pipeline->numCmds - 1 && pipeline->numCmds > 1) {
            redirect_to_pipe(nextPipe);
        }

        // Try execute program
        execvp(cmd[0], cmd);
        fprintf(stderr, execError, cmd[0], fileName); // Process failed!
        fflush(stderr);
        _exit(CHILD_PROCESS_ERROR);
    } else { // Parent
        if (nextPipe) { // Next child process doesn't need write end of pipe.
            close(nextPipe[WRITE]);
        }
        if (cmdIdx > 0) { // Close fully used pipe
            close(*prevPipe);
        }

        // Current pipe read end becomes prevPipe for next process
        if (nextPipe && cmdIdx < pipeline->numCmds - 1) {
            *prevPipe = nextPipe[READ];
        }
    }
    return pid;
}

/* exec_cmd()
 *
 * This function executes a set of commands within a pipeline for each and
 * every file within the directory.
 *
 * pipeline: Pointer to an instance of the CommandPipeline struct
 * fileName: Name of file to be processed
 * pids: An array to store pid_ts
 */
void exec_cmd(CommandPipeline* pipeline, char* fileName, pid_t* pids)
{
    // Pipeline commands, PID array and --statistics array
    int numCmds = pipeline->numCmds;

    // Initialize input and output pipes
    int prevPipe;
    int nextPipe[2];
    // Create a process for each command in pipeline
    for (int i = 0; i < numCmds; i++) {
        // Deal with placeholders in command line
        char** newCmd = cmd_copy(pipeline->cmdArray[i], fileName);

        // Spawn Child
        if (i < numCmds - 1) {
            pids[i] = spawn_child(
                    pipeline, newCmd, i, fileName, &prevPipe, nextPipe);
        } else { // Last command in pipeline
            pids[i] = spawn_child(
                    pipeline, newCmd, i, fileName, &prevPipe, NULL);
        }

        // Free newCmd
        for (int j = 0; j < cmd_array_count(pipeline->cmdArray[i]); j++) {
            free(newCmd[j]);
        }
        free(newCmd);
    }
}

/* validate_files()
 *
 * This function validates the stdin and stdout redirection files within the
 * CommandPipeline struct instance if specified. It first checks the stdin
 * file then the stdout file and immediately closes them after checking as
 * not all processes will need them.
 *
 * inputFile: Name of stdin redirection file.
 * outputFile: Name of stdout redirection file.
 * fileName: Name of the file being processed.
 *
 * Returns: True if both the stdin and stdout redirection files are readable
 *     (unless not specified) false if any of the two files aren't readable.
 */
bool validate_files(char* inputFile, char* outputFile, char* fileName)
{
    int testopen;

    // Validate Input file
    if (inputFile) {
        testopen = try_open_file(inputFile, fileName, true);
        if (testopen == -1) {
            return false;
        }
        // Close if open successful
        close(testopen);
    }

    // Validate Output file
    if (outputFile) {
        testopen = try_open_file(outputFile, fileName, false);
        if (testopen == -1) {
            return false;
        }
        // Close if open successful
        close(testopen);
    }

    return true;
}

/* count_stats()
 *
 * This function increments the appropriate value within the stats array
 * depending on how the child process terminated.
 *
 * stats: The --statistics array that needs to store the count
 * status: A child processes' exit status
 * statusCount: The size of statuses array
 */
void count_stats(int* stats, const int* statuses, int statusCount)
{
    bool fail = false, signal = false, notexec = false;
    int status;

    // Loop over each status
    for (int i = 0; i < statusCount; i++) {
        status = statuses[i];
        if (WIFEXITED(statuses[i])) { // If exited
            if (WEXITSTATUS(status) == CHILD_PROCESS_ERROR) {
                notexec = true;
            } else if (WEXITSTATUS(status) != 0) {
                fail = true;
            }
        } else if (WIFSIGNALED(statuses[i])) { // If signaled
            signal = true;
        }
    }

    // Increment correct statistics array index
    if (notexec) {
        stats[NOTEXEC]++;
    } else if (signal) {
        stats[SIGNAL]++;
    } else if (fail) {
        stats[FAIL]++;
    } else {
        stats[SUCCESS]++;
    }

    // Count total
    stats[TOTAL_STATS] += 1;
}

/* reap_children()
 *
 * Reaps the child processes given within the pids array given within the
 * parameters. It will make sure to reap these child processes from left to
 * right meaning first in first out. Once reaping is finished count_stats()
 * is called to analyze the --statistics.
 *
 * pids: Array of pid_ts.
 * stats: Array for statistics counting.
 * numCmds: Size of pids array.
 */
void reap_children(pid_t* pids, int* stats, int numCmds)
{
    int statuses[numCmds];
    int status;

    // Reap all processes within pids in order
    for (int i = 0; i < numCmds; i++) {
        waitpid(pids[i], &status, 0);
        statuses[i] = status; // Store status
    }

    // Count --statistics
    count_stats(stats, statuses, numCmds);
}

/* sequential_process()
 *
 * This function is for non-parallel mode processing. For each file the pipeline
 * is run by creating a child process for each and every command within the
 * pipeline then the child processes are reaped immediately.
 *
 * pipeline: A pointer to a CommandPipeline instance.
 * fileName: Name of the file being processed.
 * stats: Array for statistics counting.
 * pids: An array of arrays of pid_ts.
 */
void sequential_process(
        CommandPipeline* pipeline, char* fileName, int* stats, pid_t** pids)
{
    // Allocate memory for an array of pid_ts
    pids[0] = malloc(pipeline->numCmds * sizeof(pid_t));

    // Execute and reap child processes immediately
    exec_cmd(pipeline, fileName, pids[0]);
    reap_children(pids[0], stats, pipeline->numCmds);
    free(pids[0]);
}

/* parallel_process()
 *
 * This function is for parallel mode processing. For each file the pipeline
 * is run by creating a child process for each and every command within the
 * pipeline. The child processes are not to be reaped until the creation of
 * all child processes are created. (The reaping is done within run_pipeline)
 *
 * pipeline: A pointer to a CommnadPipeline instance.
 * fileName: Name of the file being processed.
 * pids: An array of arrays of pid_ts.
 * fileNo: The number/index of the file being processed.
 */
void parallel_process(
        CommandPipeline* pipeline, char* fileName, pid_t** pids, int fileNo)
{
    // Allocate memory for an array of pid_ts
    pids[fileNo] = malloc(pipeline->numCmds * sizeof(pid_t));

    // Execute and DON'T reap child processes
    exec_cmd(pipeline, fileName, pids[fileNo]);
}

/* run_pipeline()
 *
 * This function runs the pipeline for each and every file within the
 * fileList parameter. If parallel mode is specified then the program will run
 * parallel_process() otherwise it'll run sequential_process().
 *
 * pipeline: A pointer to an instance of the CommandPipeline struct.
 * config: A pointer to an instance of the Settings struct.
 * stats: Array for statistics counting
 * FileList: An instance of the FileList struct (Can be empty).
 *
 * Errors: If stdin and stdout files within the CommandPipeline struct instance
 *     is not readable/openable then the appropriate error message will be
 *     printed to stderr using validate_files().
 */
void run_pipeline(CommandPipeline* pipeline, Settings* config, int* stats,
        FileList fileList)
{
    char** files = fileList.files;
    char* inputFile = pipeline->stdinFileName;
    char* outputFile = pipeline->stdoutFileName;
    pid_t** pids;

    // Initialize pids list
    if (config->parallel) { // If parallel mode
        pids = malloc(fileList.count * sizeof(pid_t*));
        memset(pids, 0, fileList.count * sizeof(pid_t*));
    } else { // If not parallel mode
        pids = malloc(1 * sizeof(pid_t*));
    }

    // Execute pipeline for each file
    for (int i = 0; i < fileList.count; i++) {
        // Firstly check that input and output files are valid
        if (!validate_files(inputFile, outputFile, files[i])
                && !sigintReceived) {
            stats[NOTEXEC]++;
            stats[TOTAL_STATS]++;
            continue;
        }
        // Create and run processes
        if (!config->parallel && !sigintReceived) { // Sequential
            sequential_process(pipeline, files[i], stats, pids);
            if (sigintReceived && i < fileList.count - 1) { // If SIGINT
                config->sigInt = true;
                break;
            }
        } else if (config->parallel) { // Parallel
            parallel_process(pipeline, files[i], pids, i);
        }
    }

    // Parallel mode reaping
    if (config->parallel) {
        for (int i = 0; i < fileList.count; i++) {
            if (pids[i] != NULL) {
                reap_children(pids[i], stats, pipeline->numCmds);
                free(pids[i]);
            }
        }
    }
    free(pids);
}

/* handle_sigint()
 *
 * This function is a signal handler and is only to be used with sigaction.
 * It flags the 'sigintReceived' global variable to notify the program that
 * a SIGINT has been received.
 *
 * sig: SIGINT signal
 *
 * Global variables modified: sigintReceived
 */
void handle_sigint(int sig __attribute__((unused)))
{
    sigintReceived = true;
}

/* print_exit()
 *
 * This function prints the statistics to stderr and is to be called
 * when the program finishes all pipeline processes.
 *
 * stats: Array for statistics counting
 */
void print_exit(int* stats)
{
    fprintf(stderr, openingMsg, stats[TOTAL_STATS]);
    fprintf(stderr, successMsg, stats[SUCCESS]);
    fprintf(stderr, failedMsg, stats[FAIL]);
    fprintf(stderr, signalMsg, stats[SIGNAL]);
    fprintf(stderr, notexecMsg, stats[NOTEXEC]);
    fflush(stderr);
}

/* free_all()
 *
 * This function frees the FileList and CommandPipeline struct instances given
 * within the parameters
 *
 * fileList: An Instance of the FileList struct
 * pipeline: A pointer to an instance of the CommandPipeline struct
 */
void free_all(FileList fileList, CommandPipeline* pipeline)
{
    // Free fileList
    for (int i = 0; i < fileList.count; i++) {
        free(fileList.files[i]);
    }
    free(fileList.files);

    // Free pipeline
    free_pipeline(pipeline);
}

int main(int argc, char** argv)
{
    // Set locale
    setlocale(LC_COLLATE, "en_AU");

    // Process command line
    Settings config = process_command_line(argc, argv);

    // Validate args
    CommandPipeline* pipeline = validate_args(&config);

    // Read directory and sort list of files
    FileList fileList = read_dir(config);

    // Setup signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    // Execute pipeline for each file
    int stats[] = {0, 0, 0, 0, 0};
    run_pipeline(pipeline, &config, stats, fileList);
    free_all(fileList, pipeline);

    // Print stats
    if (config.statistics) {
        print_exit(stats);
    }

    // Exit commands
    if (stats[NOTEXEC]) {
        exit(PROCESS_ERROR);
    } else if (config.sigInt) {
        exit(SIGINT_REC);
    }

    return 0;
}
