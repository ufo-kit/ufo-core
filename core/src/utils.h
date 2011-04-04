/**
 * \file utils.h
 * \brief Utility functions
 *
 * Detailed comment.
 */
#ifndef UFO_UTILS_H
#define UFO_UTILS_H

#define EDF_FILE_LOW_BYTE_FIRST     1
#define EDF_FILE_HIGH_BYTE_FIRST    2
#define EDF_FILE_DT_FLOAT           4
#define EDF_FILE_DT_USHORT          8
#define EDF_FILE_DT_UINT            16
#define EDF_FILE_DT_ULONG           32
#define EDF_FILE_READ_BLOCK_SIZE    4096

typedef struct EdfFile_t {
    int flags;              /**< Flags describing data block */
    int dim[3];             /**< Dimensionality of the data */
    size_t element_size;    /**< Size per element in bytes */
    size_t total_size;      /**< Size of data. Should be product over all i dim[i] * sizeof(datatype) */
    char *data;             /**< Flat array of data */
} EdfFile;

EdfFile *ufo_edf_read(const char *filename);
void ufo_edf_close(EdfFile* f);


#endif
