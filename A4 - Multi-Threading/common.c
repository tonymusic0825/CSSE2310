#include "common.h"

// Minimum and Maximum values for image manipulation arguments
typedef enum {
    MAX_DEG = 359,
    MIN_DEG = -359,
    MAX_SCALE = 10000,
    MIN_SCALE = 1
} Values;

/* is_empty()
 *
 * This function checks if a given string is empty/equals to ""
 *
 * str: The string to be checked
 *
 * Returns: If str is empty returns true otherwise returns false
 */
bool is_empty(char* str)
{
    if (!strcmp(str, "")) {
        return true;
    }

    return false;
}

/* is_number()
 *
 * This function checks if the given 'str' is a valid number. This function
 * accepts numbers leading with '-' and '+' however, declines numbers with
 * leading zeros.
 *
 * str: The string to be checked
 *
 * Returns: If the string is a number returns true otherwise returns false.
 */
bool is_number(const char* str)
{
    int i = 1;

    // Check that number is not leading '0'
    if (str[0] == '0' && strlen(str) != 1) {
        return false;
    }

    // Check that the first character is either a '-', '+', or a number.
    if ((str[0] != '-' && str[0] != '+') && (!isdigit(str[0]))) {
        return false;
    }

    // Check if the first character is '-' or '+' the next char is not '\0'
    if ((str[0] == '-' || str[0] == '+') && str[1] == '\0') {
        return false;
    }

    // Check if the first character is '-' or '+' and isn't a number lead by 0
    if ((str[0] == '-' || str[0] == '+')
            && (str[1] == '0' && strlen(str) > 2)) {
        return false;
    }

    // Loop over each character and check that it's a digit
    while (str[i]) {
        if (!isdigit(str[i])) {
            return false;
        }
        i++;
    }

    return true;
}

/* check_rotate_arg()
 *
 * This function checks that the given degrees string is a number and is
 * non-empty. It will check that the number representation of degrees string
 * is no less than -359 and no greather than 359. If all requirements are
 * satisfied the integer representation of the degrees string will be stored
 * within the 'store' parameter.
 *
 * degrees: The string to be checked
 *
 * Returns: If the given 'degrees' is either not a valid number or a number in
 *     between -359 and 359, false is returned. Otherwise True.
 */
bool check_rotate_arg(char* degrees)
{
    // If degrees argument is empty or is non integer error
    if (is_empty(degrees) || !is_number(degrees)) {
        return false;
    }

    // Convert to integer and check argument is appropriate size
    int degreesInt = atoi(degrees);
    if (degreesInt < MIN_DEG || degreesInt > MAX_DEG) {
        return false;
    }

    return true;
}

/* check_flip_arg()
 *
 * This function checks that the given direction string is either "h" or "v".
 * It makes sure that the given direction string is non-empty aswell. If all
 * requirements are satisfied then the direction string will be stored within
 * the 'store' parameter.
 *
 * direction: The string to be checked
 *
 * Returns: If the 'direction' is neither "h" or "v" exactly false is returned.
 *     Otherwise True.
 */
bool check_flip_arg(char* direction)
{
    if (strcmp(direction, "h") && strcmp(direction, "v")) {
        return false;
    }

    return true;
}

/* check_scale_arg()
 *
 * This function checks that the given widthStr and heightStr strings are
 * numbers and non-empty. It will also check that the number representation of
 * both the widthStr and heightStr are postive integers that is no greater than
 * 10000.
 *
 * widthStr: Width string to be checked
 * heightStr: Height string to be checked
 *
 * Returns: If any of 'widthStr' or 'heightStr' is either not a number or a
 *     number not within the range of 1 ~ 10000 false is returned. Otherwise
 *     true.
 */
bool check_scale_arg(char* widthStr, char* heightStr)
{
    // Check that widthStr and heightStr are numbers and non-empty
    if (is_empty(widthStr) || is_empty(heightStr) || !is_number(widthStr)
            || !is_number(heightStr)) {
        return false;
    }

    // Convert strings to numbers
    int width = atoi(widthStr);
    int height = atoi(heightStr);

    // Check that the width and height values are within the limits
    if (width > MAX_SCALE || height > MAX_SCALE || width < MIN_SCALE
            || height < MIN_SCALE) {
        return false;
    }

    return true;
}
