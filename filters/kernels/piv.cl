/*
 * Copyright (C) 2013-2016 Karlsruhe Institute of Technology
 *
 * This file is part of Ufo.
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

void
compare_and_exchange (local float *keyA, local float *keyB, unsigned dir)
{
    float t;

    if ((*keyA > *keyB) == dir) {
        t = *keyA;
        *keyA = *keyB;
        *keyB = t;
    }
}

int
get_position (int position, int size)
{
    int newPos = 0;

    if (position < 0) {
        ++position;
        position *= (-1);
    }

    int divisor = (position / size);
    return divisor % 2 ? position % size : size - 1 - (position % size);
}

void
get_center (const unsigned width, const unsigned height, int2 *center)
{
    center->x = width % 2 == 0 ? width / 2 - 1 : width / 2;
    center->y = height % 2 == 0 ? height / 2 - 1  : height / 2;
}

int
get_pel_position (int2 imageDim, int2 filterPos, int2 pel, int2 center)
{
    int2 posInImage = (int2)(filterPos.x - center.x + pel.x, filterPos.y - center.y + pel.y);

    if (posInImage.x < 0 || posInImage.x >= imageDim.x) {
        posInImage.x = get_position (posInImage.x, imageDim.x);
    }

    if (posInImage.y < 0 || posInImage.y >= imageDim.y) {
        posInImage.y = get_position (posInImage.y, imageDim.y);
    }

    return (posInImage.x + posInImage.y * imageDim.x);
}
