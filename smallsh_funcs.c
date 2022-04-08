#define _XOPEN_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define DEBUGFUNCS 0

/*  Similar to strtok_r (see strtok_r), except that DELIM specifies a string
    (not including the null-terminating character), rather than a set of bytes.
 */
char *strtok_r_custom(char *str, const char *delim, char **saveptr)
{
    if (str != NULL)
    {
        // On first call, point saveptr to str
        *saveptr = str;
    }

    if (*saveptr == NULL)
    {
        // The last call set saveptr to NULL, so return NULL to indicate that
        // parsing is complete.
        return *saveptr;
    }

    // Get a pointer to the location of the next delimeter
    char *delim_loc = strstr(*saveptr, delim);

    // Point the output token to the location currently referenced by saveptr
    char *token = *saveptr;

    if (delim_loc == NULL)
    {
        // There are no more delimiters. Set saveptr to null to indicate on
        // next call that parsing is complete.
        *saveptr = NULL;
    }
    else
    {
        // The delimeter was found. Set the delimiter chars to null bytes.
        for (int i = 0; i < strlen(delim); i++)
        {
            *delim_loc = '\0';
            delim_loc++;
        }
        // Point saveptr to the location after the delimiter.
        *saveptr = delim_loc;
    }

    return token;
}

/*  Copies the contents of SRC to DST, expanding any substrings of "$$"
    to the current process's pid, in ASCII. DST must have enough memory
    allocated to avoid segmentation faults.
 */
void copy_and_expand_dollar(char *dst, char *src)
{
    // Setup vars for strtok_r_custom
    char *saveptr = NULL;
    char *token = strtok_r_custom(src, "$$", &saveptr);
    strcat(dst, token);

    // Get current process ID as a string
    pid_t this_pid = getpid();
    char this_pid_str[6] = {'\0'};
    sprintf(this_pid_str, "%d", this_pid);

    // Until in_str is completely parsed, cat this_pid_string and token to
    // the end of out_str.
    while ((token = strtok_r_custom(NULL, "$$", &saveptr)))
    {
        strcat(dst, this_pid_str);
        strcat(dst, token);
    }
}

/*  Appends the value VAL to the array ARR that has size ARR_SIZE and number of
    elements NUM_ELEMS. Increments NUM_ELEMS.
    If (NUM_ELEMS is > 3/4 * ARR_SIZE) before the operation, then ARR is
    reallocated to double ARR_SIZE, and ARR_SIZE is updated.
 */
int arr_add(int **arr, int *arr_size, int val, int *num_elems)
{
    if (*num_elems > (*arr_size * 3 / 4))
    {
        int *arr_ptr = realloc(*arr, (*arr_size) * sizeof(int) * 2);
        if (arr_ptr == NULL)
        {
            perror("arr_add(): realloc()");
            exit(1);
        }
        *arr_size *= 2;
    }
    (*arr)[*num_elems] = val;
    (*num_elems)++;

    if (DEBUGFUNCS)
    {
        printf("arr[%d]: %d, arr_size: %d, num_elems: %d\n", *num_elems-1, (*arr)[*num_elems-1], *arr_size, *num_elems);
    }

    return EXIT_SUCCESS;
}

/*  Removes the element located at index IDX of array ARR that has size
    ARR_SIZE and number of elements NUM_ELEMS. Decrements NUM_ELEMS.
    If (NUM_ELEMS < ARR_SIZE / 4) before the operation, then ARR is reallocated
    to half of ARR_SIZE, and ARR_SIZE is updated.
 */
int arr_del(int **arr, int *arr_size, int idx, int *num_elems)
{
    // Move the block of the array starting at arr[idx+1] to arr[idx]
    memmove(&(*arr)[idx], &(*arr)[(idx+1)], sizeof(int) * (*num_elems - idx));

    if (DEBUGFUNCS)
        printf("arr_del: checking to see if realloc necessary...\n");

    if ((*num_elems < (*arr_size / 4)) && (*arr_size > 8))
    {
        int *arr_ptr = realloc(*arr, (*arr_size) * sizeof(int) / 2);
        if (arr_ptr == NULL)
        {
            perror("arr_del(): realloc()");
            exit(1);
        }
        *arr_size *= 0.5;
    }
    (*num_elems)--;

    if (DEBUGFUNCS)
    {
        printf("arr_size: %d, num_elems: %d\n", *arr_size, *num_elems);
    }

    return EXIT_SUCCESS;
}
