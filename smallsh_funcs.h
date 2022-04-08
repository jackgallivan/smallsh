char *strtok_r_custom(char *str, const char *delim, char **saveptr);
void copy_and_expand_dollar(char *dst, char *src);
int arr_add(int **arr, int *arr_size, int val, int *num_elems);
int arr_del(int **arr, int *arr_size, int idx, int *num_elems);
