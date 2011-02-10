
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"


EdfFile *ufo_read_edf(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    char *buffer = (char *) malloc(4096);

    size_t read = fread(buffer, 1, 1024, fp);
    if (read != 1024) {
        fclose(fp);
        return NULL;
    }

    char *token = strtok(buffer, "\n");
    int pointer = 0;
    while (token != NULL) {
        printf("token: %s\n", token);
        pointer += strlen(token);
        token = strtok(buffer+pointer, "\n");
    }

    EdfFile *edf = (EdfFile *) malloc(sizeof(EdfFile));

    fclose(fp);
    free(buffer);

    return edf;
}

void ufo_close_edf(EdfFile *edf)
{
    assert((edf != NULL) && (edf->data != NULL));
    free(edf->data);
    free(edf);
}
