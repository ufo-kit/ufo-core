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

typedef struct EdfFile_t {
    int flags; 
    int dim[3];
    char *data;
} EdfFile;

/**
 * \brief Open and read an EDF file
 *
 * \return A pointer to an EdfFile structure
 */
EdfFile *ufo_read_edf(const char *filename);

/**
 * \brief Free memory of the EdfFile structure
 */
void ufo_close_edf(EdfFile* f);


#endif
