#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <csse2310a1.h>
#include <stdbool.h>

#define LETTER_ARG "--letters"
#define LENGTH_MIN_ARG "--length-min"
#define DICTIONARY_ARG "--dictionary"
#define NOT_SET "None"

#define MAX_LEN_CHK 6
#define MIN_LEN_CHK 3
#define DEFAULT_LENGTH 4
#define DEFAULT_LETTERS_LEN 8
#define MAX_LETTERS_LEN 13

#define USAGE_ERROR 7
#define VALID_LENGTH_ERROR 10
#define LETTERS_ERROR 4
#define LETTERS_LONG_ERROR 17
#define LETTERS_SHORT_ERROR 1
#define FILENAME_ERROR 15
#define NO_GUESS_EXIT 16

#define DIC_BUFFER_SIZE 51
#define ALPHA_SIZE 26
#define SPECIAL_SCORE 10
#define USAGE_ERROR_STR                                                        \
    "Usage: uqunscramble [--dictionary filename] [--length-min numletters] "   \
    "[--letters chars]\n"

/* Function prototypes */
void free_letters(int freeLetters, char* letters);
bool word_in_array(char** array, int arrayLen, char* word);

/* List of words within a game - Array of words and its count */
typedef struct WordList {
    char** words;
    int count;
} WordList;

/* Settings of a particular game - Contains all necessary variables to run the
 * game along with a list of valid words
 */
typedef struct GameSettings {
    char* letters;
    char* dicStr;
    int freeLetters;
    int minLen;
    int maxLen;
    FILE* dic;
    WordList* validWords;
} GameSettings;

/* check_arg_count()
 * −−−−−−−−−−−−−−−--
 * This functions checks if the number of command line arguments (argc) is valid
 *
 * argc: Number of command-line arguments assumed to be bigger or equal to 1
 *
 * Errors: With the exclusion of the program name (argc - 1)
 *     if the number of arguments is larger than 6 or the arguments
 *     are not specified in pairs 0 (mod2) than exit program with status 7.
 */
void check_arg_count(int argc)
{
    if ((argc - 1) % 2 != 0 || (argc - 1) > MAX_LEN_CHK) {
        fprintf(stderr, USAGE_ERROR_STR);
        exit(USAGE_ERROR);
    }
}

/* is_empty()
 * −−−−−−−−−−−
 * This function checks if a given string is empty/equals to ""
 *
 * cmdArg: The string to be checked
 *
 * Returns: If cmdArg is empty return 0 otherwise returns 1.
 */
int is_empty(char* cmdArg)
{
    if (!strcmp(cmdArg, "")) {
        return 1;
    }
    return 0;
}

/* usage_exit()
 * ------------
 * This function prints usage error message to stderr and exits the program
 * with status 7.
 *
 * Errors: When this function is called the program exits with status 7
 */
void usage_exit(void)
{
    fprintf(stderr, USAGE_ERROR_STR);
    exit(USAGE_ERROR);
}

/* usage_check()
 * -------------
 * This function checks if the optional argument specifiers within the
 * command line is one of --letters, --min-length, --dictionary exactly.
 * It also checks that there are no duplicate argument specifiers given.
 *
 * argc: Number of command-line arguments
 * argv: Array of command line arguments
 * config: A pointer to an instance of the type GameSettings struct.
 *
 * Errors: If any of the arguments are not exactly --length, --min-length
 *     or --dictionary the program will exit with status 7. If any of the
 *     argument specifiers given contain duplicates then the program will also
 *     exit with status 7. To exit this the function usage_exit() is called.
 */
void usage_check(int argc, char** argv, GameSettings* config)
{
    // Check each odd element within argv and check it's a valid specifier
    for (int i = 1; i < argc; i += 2) {
        if (is_empty(argv[i + 1])) { // If arg empty exit
            usage_exit();
        }

        // Compare each argument
        // --length-min
        if (!strcmp(argv[i], LENGTH_MIN_ARG) && config->minLen == -1) {
            // Check that --length-min value is a single digit
            if (isdigit(*(argv[i + 1])) && strlen(argv[i + 1]) == 1) {
                config->minLen = atoi(argv[i + 1]);
            } else {
                usage_exit(); // Exit
            }
        } else if (!strcmp(argv[i], DICTIONARY_ARG) // --dictionary
                && !strcmp(config->dicStr, NOT_SET)) {
            config->dicStr = argv[i + 1];
        } else if (!strcmp(argv[i], LETTER_ARG) // --letters
                && !strcmp(config->letters, NOT_SET)) {
            config->letters = argv[i + 1];
        } else {
            usage_exit(); // Exit
        }
    }
}

/* is_only_alpha()
 * ---------------
 * This function checks that the string provided within the parameter only
 * contains alphabet characterss and nothing else.
 *
 * letters: The string to check
 *
 * Returns: Returns 1 if only alphabet is detected otherwise returns 0.
 */
int is_only_alpha(char* letters)
{
    // While character not '\0'
    while (*letters) {
        if (!isalpha(*letters)) {
            return 0;
        }
        letters++; // Increment pointer for next char
    }
    return 1;
}

/* valid_length()
 * --------------
 * This function checks that the minimum length for the "guess" of the game
 * given within the command line is valid. If the minimum length specifier was
 * not given within the command line then a DEFAULT value of 4 will be given.
 *
 * minLen: The minimum length value to check
 *
 * Returns: Returns minimum length value of 4 if the user did not supply.
 *     Otherwise returns the minLenStr value in integer format
 * Errors: If the minimum length is not between 3 and 6 inclusive then the
 *     program will exit with status 10.
 *
 */
void valid_length(int* minLen)
{
    // If minimum length wasn't given then set to DEFAULT = 4
    if (*minLen == -1) {
        *minLen = DEFAULT_LENGTH;
        // Otherwise if minLen not within correct range exit
    } else if (!(*minLen >= MIN_LEN_CHK && *minLen <= MAX_LEN_CHK)) {
        fprintf(stderr,
                "uqunscramble: minimum length must be between 3 and 6\n");
        exit(VALID_LENGTH_ERROR);
    }
}

/* valid_letters()
 * ---------------
 * This function checks if the given set of letters for the --letters specifier
 * is valid.
 *
 * letters: The set of letters to check
 * minLen: The minimum length value for the game.
 *
 * Errors: If the letters contain any non-alphabet characters the program will
 *     exit with status 4. If the letters are longer than 13 characters
 *     the program will exit with status 17. If the letters are shorter than
 *     the given minLen then the program will exit with status 1.
 */
void valid_letters(char* letters, int minLen)
{
    int lettersLen = strlen(letters);

    if (!is_only_alpha(letters)) { // Check letters is alphabet only
        fprintf(stderr, "uqunscramble: invalid letter set\n");
        exit(LETTERS_ERROR);
    } else if (lettersLen > MAX_LETTERS_LEN) { // Check if too many letters
        fprintf(stderr, "uqunscramble: too many letters - the limit is 13\n");
        exit(LETTERS_LONG_ERROR);
    } else if (lettersLen < minLen) { // Check if too few letters
        fprintf(stderr,
                "uqunscramble: too few letters for the given minimum "
                "length (%d)\n",
                minLen);
        exit(LETTERS_SHORT_ERROR);
    }
}

/* matches_letters()
 * -----------------
 * This function takes in two strings a word and set of letters. Then evaluates
 * whether the word can be made up of the set of letters
 *
 * word: Word to evaluate
 * letters: Set of letters to evaluate the word
 *
 * Returns: Returns true if the word can be made up of the given set of letters
 *      otherwise return false
 */
bool matches_letters(char* word, char* letters)
{
    // Initialize letter mapping array
    int letterCount[ALPHA_SIZE] = {0};
    int lettersLen = strlen(letters);
    int wordLen = strlen(word);

    // Count occurrence of each letter within the letter set
    for (int i = 0; i < lettersLen; i++) {
        // Can minus 'a' as the ASCII Code arithmetic will give alphabet index
        letterCount[tolower(letters[i]) - 'a']++; // Count that char
    }

    // Check if the word can be made
    for (int i = 0; i < wordLen; i++) {
        if (letterCount[tolower(word[i]) - 'a'] == 0) {
            return false;
        }
        // Decrement if char in letter set
        letterCount[tolower(word[i]) - 'a']--;
    }

    return true;
}

/* valid_game_word()
 * -----------------
 * This function makes sure that a word is valid within the current game's
 * settings/rules.
 *
 * word: The word to check
 * minLen: Minimum word length of current game
 * maxLen: Maximum word length of current game
 * letters: Set of letters possible for the current game
 *
 * Returns: Returns true if word is valid otherwise False
 */
bool valid_game_word(char* word, int minLen, int maxLen, char* letters)
{
    bool valid = true;
    int wordLen = strlen(word);

    if (wordLen < minLen || wordLen > maxLen) { // Check length
        valid = false;
    } else if (!is_only_alpha(word)) { // Check that it's only alphabet
        valid = false;
    } else if (!matches_letters(word, letters)) { // Verify with letter set
        valid = false;
    }

    return valid;
}

/* change_to_upper()
 * -----------------
 * Converts each character of given string to uppercase.
 *
 * word: String to be converted to uppercase
 */
void change_to_upper(char* word)
{
    int wordLen = strlen(word);

    for (int i = 0; i < wordLen; i++) {
        word[i] = toupper(word[i]);
    }
}

/* read_dic()
 * ----------
 * This function reads the dictionary file line by line (each word) and adds
 * it to a dynamic array. The word is only added to the array if:
 * 1. The word is not a duplicate
 * 2. The word is valid (Uses valid_game_word() function to run check)
 *
 * config: A pointer to an instance of the type GameSettings struct.
 *         (Assumes the struct that config points to is not NULL).
 */
void read_dic(GameSettings* config)
{
    char buffer[DIC_BUFFER_SIZE];
    int wordLen;
    int wordCount = 0;
    char*** validWords = &(config->validWords->words);

    // Loop while line is not EOF
    while (fgets(buffer, DIC_BUFFER_SIZE, config->dic) != NULL) {
        wordLen = strlen(buffer); // Calculate length of word
        if (buffer[wordLen - 1] == '\n') { // Replace '\n' with '\0'
            buffer[strlen(buffer) - 1] = '\0';
        }

        change_to_upper(buffer);

        // If word is invalid skip
        if (!valid_game_word(
                    buffer, config->minLen, config->maxLen, config->letters)) {
            continue;
        }

        // If word already in array skip
        if (word_in_array(*validWords, wordCount, buffer)) {
            continue;
        }

        // Change word to upper case and store
        *validWords = realloc(*validWords, sizeof(char*) * (wordCount + 1));
        (*validWords)[wordCount++] = strdup(buffer);
    }

    // Store word count
    config->validWords->count = wordCount;
    fclose(config->dic); // Close file
}

/* valid_dic()
 * -----------
 * This function checks if the given filename for --dictionary is valid.
 *
 * dicStr: Filename to check
 * dic: The opened dictionary (if possible) will be saved to this variable
 * freeLetters: Integer value indicating whether letters needs to be freed.
 * letters: Letters to be freed if dictionary filename is invalid
 *
 * Errors: If the dictionary filename given cannot be read or doesn't exist
 *     the program will exit with status 15.
 */
void valid_dic(char* dicStr, FILE** dic, int freeLetters, char* letters)
{
    // If dictionary was not given by user set to DEFAULT = words
    if (strcmp(dicStr, NOT_SET) == 0) {
        dicStr = "/local/courses/csse2310/etc/words";
    }

    // Try opening file
    *dic = fopen(dicStr, "r");

    // If file cannot be opened exit
    if (*dic == NULL) {
        fprintf(stderr,
                "uqunscramble: dictionary file with name \""
                "%s\" cannot be opened\n",
                dicStr);
        free_letters(freeLetters, letters); // Free letters
        exit(FILENAME_ERROR);
    }
}

/*
 * arg_value_check()
 * -----------------
 * This function checks that the argument values provided within the command
 * line that follow the specifiers are valid. If they are valid it'll change
 * the values to the apropriate data type needed when running the game and
 * assign them to the appropriate variables passed within the parameters.
 *
 * config: A pointer to an instance of the type GameSettings struct.
 *         (Assumes the struct that config points to is not NULL).
 */
void arg_value_check(GameSettings* config)
{
    // Length validity
    valid_length(&(config->minLen));

    // Letter set validity
    if (!strcmp(config->letters, NOT_SET)) {
        // Mark that we need to free letters later
        config->freeLetters = 1;
        const char* tempLetters = get_random_letters(DEFAULT_LETTERS_LEN);
        config->letters = strdup(tempLetters);
    } else { // Otherwise check if letters are valid
        valid_letters(config->letters, config->minLen);
    }

    // Dictionary filename validity
    valid_dic(config->dicStr, &(config->dic), config->freeLetters,
            config->letters);
}

/* word_in_array()
 * ---------------
 * This function checks if the word is within the array given in the parameters
 *
 * array: The array used to check if the word is within it
 * arrayLen: Length of the array
 * word: The word to check within the array
 *
 * Returns: Returns true if word is in the array otherwise return false
 */
bool word_in_array(char** array, int arrayLen, char* word)
{
    // Loop the array and use strcmp() to check each word
    for (int i = 0; i < arrayLen; i++) {
        if (strcmp(word, array[i]) == 0) {
            return true;
        }
    }

    return false;
}

/* valid_user_input()
 * ------------------
 * This function checks if a given user input is valid. A valid user input is
 * considered to be the following:
 * 1. Only contains alphabet characters
 * 2. Does not exceed the max length
 * 3. Not shorter than the min length
 * 4. Can be made with the set of letters within the current game
 * 5. Is not a duplicate meaning that the user has never guessed it before
 * 6. The guessed word is within the dictionary of the current game
 * If any one of these conditions aren't met then the program will print the
 * appropriate message to stdout.
 *
 * user_input: The user input string (Assumes user_input is not NULL).
 * guesses: Array of correct guesses by the user in current game
 * guessCount: The amount of correct guesses by user in current game
 * config: A pointer to an instance of the type GameSettings struct.
 *         (Assumes the struct that config points to is not NULL).
 *
 * Returns: true if the user input is valid. Otherwise false.
 */
bool valid_user_input(
        char* userInput, GameSettings* config, char** guesses, int guessCount)
{
    // Needed variables for comparison
    bool valid = true;
    int userInputLen = strlen(userInput);
    int minLen = config->minLen;
    int maxLen = config->maxLen;
    char* letters = config->letters;
    char** validWords = config->validWords->words;
    int validWordsCount = config->validWords->count;

    // Start comparison
    if (!is_only_alpha(userInput)) {
        printf("Your guess must contain only letters\n");
        valid = false;
    } else if (userInputLen < minLen) {
        printf("Word must be at least %d characters long\n", minLen);
        valid = false;
    } else if (userInputLen > maxLen) {
        printf("Word must be no more than %d characters long\n", maxLen);
        valid = false;
    } else if (!matches_letters(userInput, letters)) {
        printf("Word can't be formed from available letters\n");
        valid = false;
    } else if (word_in_array(guesses, guessCount, userInput)) {
        printf("You've guessed that word before\n");
        valid = false;
    } else if (!word_in_array(validWords, validWordsCount, userInput)) {
        printf("Word not found in dictionary file\n");
        valid = false;
    }

    return valid;
}

/* read_line()
 * -----------
 * This function reads a line from stdin
 *
 * Returns: Returns a string formatted input from stdin
 * REF: This function is a slight variation from Ed Lesson 3.2 code that was
 *      personally written
 */
char* read_line(void)
{
    char* word = NULL;
    int chCount = 0;
    char nextChar;

    // Loop while stdin isn't closed
    while ((nextChar = fgetc(stdin)) != EOF) {
        word = realloc(word, (chCount + 1) * sizeof(char));

        if (nextChar == '\n') { // Add null character
            word[chCount] = '\0';
            break;
        }
        // Store char and increment count
        word[chCount] = nextChar;
        chCount++;
    }

    // If user typed something before closing stdin then
    // add null character to word and return as string format for comparison
    if (chCount > 0) {
        word = realloc(word, (chCount + 1) * sizeof(char));
        word[chCount] = '\0';
    }

    return word;
}

/* take_user_input()
 * -----------------
 * This function takes the user input from stdin and also checks its validity
 * by calling the valid_user_input() function.
 *
 * config: A pointer to an instance of the type GameSettings struct.
 *         (Assumes the struct that config points to is not NULL).
 * guesses: A pointer to an instance of the type WordList struct. The array
 *      that this pointer points to should be either empty or filled with
 *      words that the player has guessed.
 * score: A pointer to a score instance.
 *
 * Returns: true if the user ended the game with 'q' (in stdin) otherwise false
 */
bool take_user_input(GameSettings* config, WordList* guesses, int* score)
{
    bool endedq = false;
    int guessCount = 0;
    char*** gameGuesses = &(guesses->words);
    char* word;
    int maxLen = config->maxLen;

    while (1) {
        // Read line from stdin
        word = read_line();

        if (word == NULL) { // Stdin = EOF
            break;
        }

        if (!strcmp(word, "q")) { // User quit with "q"
            endedq = true;
            free(word); // Free word
            break;
        }

        // Change word to uppercase
        change_to_upper(word);

        // If valid word increase memory, store and add appropriate score
        if (valid_user_input(word, config, *gameGuesses, guessCount)) {
            *gameGuesses
                    = realloc(*gameGuesses, (guessCount + 1) * sizeof(char*));
            (*gameGuesses)[guessCount++] = strdup(word); // Add guess to list
            *score += strlen(word); // Update score

            if ((int)strlen(word) == maxLen) {
                *score += SPECIAL_SCORE; // Special score if necessary
            }
            printf("OK! Current score is %d\n", *score);
        }
        free(word); // Free word
    }
    guesses->count = guessCount; // Update user's count guess
    return endedq;
}

/* free_letters()
 * --------------
 * This function frees the letters of the game if they were set using the
 * get_random_letters() function
 *
 * freeLetters: A integer value 0 or 1 that indicates whether letters needs
 *      to be freed (Assumes that the value is either 0 or 1)
 * letters: Pointer to letters
 *
 */
void free_letters(int freeLetters, char* letters)
{
    if (freeLetters) {
        free(letters);
    }
}

/* free_all()
 * ----------
 * This function frees all dynamic memory variables.
 *
 * config: A pointer to an instance of the type GameSettings struct.
 *         (Assumes the struct that config points to is not NULL).
 * guesses: A pointer to an instance of the type WordList struct. The array
 */
void free_all(GameSettings* config, WordList* guesses)
{
    char*** validWords = &(config->validWords->words);
    int validWordsCount = config->validWords->count;
    char*** playerGuesses = &(guesses->words);
    int playerGuessesCount = guesses->count;

    // Free game letters
    free_letters(config->freeLetters, config->letters);

    // Free everything
    if (validWordsCount > 0) {
        for (int i = 0; i < validWordsCount; i++) {
            free((*validWords)[i]);
        }
        free(*validWords);
    }

    if (playerGuessesCount > 0) {
        for (int i = 0; i < playerGuessesCount; i++) {
            free((*playerGuesses)[i]);
        }
        free(*playerGuesses);
    }
}

/* sortCompare()
 * -------------
 * This is a function created to be used within qsort. The function compares
 * two strings first by its length then by alphabetical order.
 *
 * str1: A string to be compared to str 2 assumed to be a type of char**
 * str2: A string to be compared to str 1 assumed to be a type of char**
 *
 * Returns: Returns 1 if the length of str1 is longer than str2 otherwise it'll
 *      return -1. However if the lengths of both strings are equal than it'll
 *      return strcmp(str1, str2)
 * REF: This function is inspired by the example within qsort (3) man page.
 */
int sort_compare(const void* str1, const void* str2)
{
    // Cast into char pointers
    const char* word1 = *(char* const*)str1;
    const char* word2 = *(char* const*)str2;

    // Compare string lengths
    if (strlen(word1) > strlen(word2)) {
        return 1;
    }

    if (strlen(word2) > strlen(word1)) {
        return -1;
    }

    return strcmp(word1, word2);
}

/* end_game()
 * ----------
 * This function prints the appropriate game ending messages. The messaged will
 * depend on the way that the game was ended by the user.
 *
 * config: A pointer to an instance of the type GameSettings struct
 *         (Assumes the struct that config points to is not NULL).
 * score: Score of the current game
 * guesses: A pointer to an instance of the type WordList struct. The array
 *
 * Errors: The program will exit with status 16 if the game ended while the
 *      score was 0.
 */
void end_game(GameSettings* config, int score, WordList* guesses, bool endedq)
{
    int maxScore = 0;
    char*** validWords = &(config->validWords->words);
    int validWordsCount = config->validWords->count;

    // If ended with 'q' sort valid words
    qsort(*validWords, validWordsCount, sizeof(char*), sort_compare);

    // Print words and calculate possible score
    if (endedq) {
        for (int i = 0; i < validWordsCount; i++) {
            printf("%s\n", (*validWords)[i]);
            maxScore += (int)strlen((*validWords)[i]);

            if ((int)strlen((*validWords)[i]) == config->maxLen) {
                maxScore += SPECIAL_SCORE;
            }
        }

        // Print maximum possible score
        printf("Maximum possible score was %d\n", maxScore);
    }

    // Print game ending messages
    if (score == 0) { // If no valid guess
        printf("No valid guesses\n");
        free_all(config, guesses); // Free all dynamic memory
        exit(NO_GUESS_EXIT);
    }

    printf("Game over. Your final score is %d\n", score);

    free_all(config, guesses);
}

/* start_game()
 * ------------
 * This function prints the starting messages when the game starts to stdout.
 *
 * minLen: Min length of game
 * maxLen: Max length of game
 * letters: The set of letters for the current game
 *          (Assumes letters is not NULL)
 */
void start_game(int minLen, int maxLen, char* letters)
{
    // Print starting messages
    printf("Welcome to UQunscramble!\n");
    printf("Enter words of length %d to %d made from the letters \"%s\"\n",
            minLen, maxLen, letters);
}

/* main()
 * ------
 * This is the main function of the program. This function is responsible for
 * coordinating the execution of the entire program (uqunscramble game).
 *
 * argc: The number of command-line arguments passed to the program.
 *       (Assumes argc >= 1)
 * argv: Array of command-line arguments.
 *
 * Returns: Returns 0 if game was exited with at least one valid guess.
 */
int main(int argc, char** argv)
{
    // Initialize game settings
    WordList validWords = {.words = NULL, .count = 0}; // Need to free

    GameSettings config = {
            .letters = NOT_SET, // May need to free
            .dicStr = NOT_SET,
            .freeLetters = 0,
            .minLen = -1,
            .maxLen = 0,
            .dic = NULL,
            .validWords = &validWords // Need to free
    };

    // Check Usage (command line arguments)
    check_arg_count(argc);
    usage_check(argc, argv, &config);
    arg_value_check(&config);
    config.maxLen = strlen(config.letters); // Set game max length
    read_dic(&config);

    // If everything is valid start the game
    start_game(config.minLen, config.maxLen, config.letters);

    // Variables needed for gameplay
    WordList guesses = {.words = NULL, .count = 0}; // Need to free
    int score = 0;

    // Start loop to take input from stdin
    bool endedq = take_user_input(&config, &guesses, &score);

    // End game
    end_game(&config, score, &guesses, endedq);

    return 0;
}
