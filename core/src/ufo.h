/**
 * \file ufo.h
 * \brief A brief comment
 *
 * Detailed comment.
 */
#ifndef UFO_H
#define UFO_H

typedef struct {
    int x;  /**< description of x */
} ufo;

/**
 * \brief Initialize an UFO context.
 *
 * \return A pointer to an ufo struct. NULL in case something went wrong.
 */
ufo *ufo_init(void);
void ufo_destroy(ufo *);

#endif
