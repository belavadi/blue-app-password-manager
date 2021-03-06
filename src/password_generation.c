/*******************************************************************************
*   Password Manager application
*   (c) 2017 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include <string.h>
#include "os.h"
#include "cx.h"
#include "password_generation.h"

static const uint8_t *SETS[] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                                "abcdefghijklmnopqrstuvwxyz",
                                "0123456789",
                                "-",
                                "_",
                                " ",
                                "\"#$%&'*+,./:;=?!@\\^`|~",
                                "[]{}()<>",
                                NULL};

uint8_t rng_u8_modulo(mbedtls_ctr_drbg_context *drbg, uint8_t modulo) {
    uint32_t rng_max = 256 % modulo;
    uint32_t rng_limit = 256 - rng_max;
    uint8_t candidate;
    do {
        if (mbedtls_ctr_drbg_random(drbg, &candidate, 1) != 0) {
            THROW(EXCEPTION);
        }
    } while (candidate > rng_limit);
    return (candidate % modulo);
}

void shuffle_array(mbedtls_ctr_drbg_context *drbg, uint8_t *buffer,
                   uint32_t size) {
    uint32_t i;
    for (i = size - 1; i > 0; i--) {
        uint32_t index = rng_u8_modulo(drbg, i + 1);
        uint8_t tmp = buffer[i];
        buffer[i] = buffer[index];
        buffer[index] = tmp;
    }
}

/* Sample from set with replacement */
void sample(mbedtls_ctr_drbg_context *drbg, const uint8_t *set,
            uint32_t setSize, uint8_t *out, uint32_t size) {
    uint32_t i;
    for (i = 0; i < size; i++) {
        uint32_t index = rng_u8_modulo(drbg, setSize);
        out[i] = set[index];
    }
}

uint32_t generate_password(mbedtls_ctr_drbg_context *drbg, setmask_t setMask,
                           const uint8_t *minFromSet, uint8_t *out,
                           uint32_t size) {
    uint8_t setChars[100];
    uint32_t setCharsOffset = 0;
    uint32_t outOffset = 0;
    uint32_t i;

    for (i = 0; setMask && i < NUM_SETS; i++, setMask >>= 1) {
        if (setMask & 1) {
            const uint8_t *set = (const uint8_t *)PIC(SETS[i]);
            uint32_t setSize = strlen((const char *)set);
            os_memcpy(setChars + setCharsOffset, set, setSize);
            setCharsOffset += setSize;
            if (minFromSet[i] > 0) {
                if (outOffset + minFromSet[i] > size) {
                    THROW(EXCEPTION);
                }
                sample(drbg, set, setSize, out + outOffset, minFromSet[i]);
                outOffset += minFromSet[i];
            }
        }
    }

    if (setMask || setCharsOffset == 0) {
        THROW(EXCEPTION);
    }

    sample(drbg, setChars, setCharsOffset, out + outOffset, size - outOffset);
    shuffle_array(drbg, out, size);
    out[size] = '\0';
    return size;
}
