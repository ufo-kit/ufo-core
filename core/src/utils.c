
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"


/**
 * \brief Reads key or value from an EDF header line
 *
 * The function reads so many ASCII characters until encountering a
 * '=', a space, a comment character or until the end of the line.
 *
 * \param[out] dst Memory area where key/value is stored
 * \param[in] src Memory are to read from
 * \param[in] src_idx Parameter to begin reading from src[src_idx]
 */
static void ufo_edf_scan(char* dst, const char* src, int *src_idx)
{
    int idx = 0, sidx = *src_idx;
    const int n = strlen(src);
    while ((sidx < n) && (src[sidx] != '=') && (src[sidx] != ';')) {
        if (src[sidx] != ' ')
            dst[idx++] = src[sidx];
        sidx++;
    }
    dst[idx] = '\0';
    *src_idx = sidx;
}

/**
 * \brief Handles a single key-value-pair from an EDF file.
 * \warning Loading might crash when handling a non-standard, hand-crafted EDF
 * file.
 */
static void ufo_edf_handle_token(EdfFile *edf, const char *token)
{
    if (token[0] == '{')
        return;

    char key[128];
    char value[128];

    const int n = strlen(token);

    /* find '=' if exists and copy key */
    int idx = 0;
    ufo_edf_scan(key, token, &idx);
    idx++;
    if (idx < n) {
        ufo_edf_scan(value, token, &idx);
        if (!strcmp(key, "ByteOrder")) {
            if (!strcmp(value, "HighByteFirst"))
                edf->flags |= EDF_FILE_HIGH_BYTE_FIRST;
            else if (!strcmp(value, "LowByteFirst"))
                edf->flags |= EDF_FILE_LOW_BYTE_FIRST;
        }
        else if (!strcmp(key, "DataType")) {
            if (!strcmp(value, "Float")) {
                edf->flags |= EDF_FILE_DT_FLOAT;
                edf->element_size = sizeof(float);
            }
        }
        else if (!strcmp(key, "Dim_1"))
            edf->dim[0] = atoi(value);
        else if (!strcmp(key, "Dim_2"))
            edf->dim[1] = atoi(value);
        else if (!strcmp(key, "Dim_3"))
            edf->dim[2] = atoi(value);
        else if (!strcmp(key, "Size"))
            edf->total_size = atoi(value);
    }
}

/**
 * \brief Open and read an EDF file
 *
 * \return A pointer to an EdfFile structure
 */
EdfFile *ufo_edf_read(const char *filename)
{
    /* TODO: Evaluate different block size */
    const size_t BUFFER_SIZE = 4096;

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
        goto error_all;

    char *buffer = (char *) malloc(4096);
    if (buffer == NULL)
        goto error_file;

    size_t read = fread(buffer, 1, 1024, fp);
    if (read != 1024)
        goto error_file;

    EdfFile *edf = (EdfFile *) malloc(sizeof(EdfFile));
    edf->flags = 0;
    edf->dim[0] = edf->dim[1] = edf->dim[2] = 1;
    edf->total_size = edf->element_size = 0;
    edf->data = NULL;

    /* Read header */
    char *token = strtok(buffer, "\n");
    int pointer = 0;
    while (token != NULL) {
        ufo_edf_handle_token(edf, token);
        pointer += strlen(token) + 1;
        token = strtok(buffer+pointer, "\n");
    }

    /* Read data */
    if (edf->total_size != (edf->dim[0]*edf->dim[1]*edf->dim[2]*edf->element_size))
        goto error_read;

    if ((edf->data = (char *) malloc(edf->total_size)) == NULL)
        goto error_read;

    size_t total_read = 0;
    char *bp = edf->data;
    while (!feof(fp)) {
        read = fread(bp, 1, BUFFER_SIZE, fp);
        bp += read;
        total_read += read;
    }

    if (edf->total_size != total_read)
        goto error_read;

    fclose(fp);
    free(buffer);
    return edf;

error_read:
    free(edf->data);
    free(edf);

error_file:
    free(buffer);
    fclose(fp);

error_all:
    return NULL;
}

/**
 * \brief Free memory of the EdfFile structure
 */
void ufo_edf_close(EdfFile *edf)
{
    free(edf->data);
    free(edf);
}
