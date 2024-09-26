/**
 * @file apm32f4xx_crypto_hash.c
 * @brief APM32F4 hash hardware accelerator
 *
 * @section License
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2010-2024 Oryx Embedded SARL. All rights reserved.
 *
 * This file is part of CycloneCRYPTO Open.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @author Oryx Embedded SARL (www.oryx-embedded.com)
 * @version 2.4.0
 **/

//Switch to the appropriate trace level
#define TRACE_LEVEL CRYPTO_TRACE_LEVEL

//Dependencies
#include "apm32f4xx.h"
#include "apm32f4xx_rcm.h"
#include "core/crypto.h"
#include "hardware/apm32f4xx/apm32f4xx_crypto.h"
#include "hardware/apm32f4xx/apm32f4xx_crypto_hash.h"
#include "hash/hash_algorithms.h"
#include "debug.h"

//Check crypto library configuration
#if (APM32F4XX_CRYPTO_HASH_SUPPORT == ENABLED)


/**
 * @brief HASH module initialization
 * @return Error code
 **/

error_t hashInit(void)
{
   //Enable HASH peripheral clock
   RCM_EnableAHB2PeriphClock(RCM_AHB2_PERIPH_HASH);

   //Successful processing
   return NO_ERROR;
}


/**
 * @brief Update hash value
 * @param[in] algo Hash algorithm
 * @param[in] data Pointer to the input buffer
 * @param[in] length Length of the input buffer
 * @param[in,out] h Intermediate hash value
 * @param[in] hLen Length of the intermediate hash value, in words
 **/

void hashProcessData(uint32_t algo, const uint8_t *data, size_t length,
   uint32_t *h, size_t hLen)
{
   uint_t i;

   //Acquire exclusive access to the HASH module
   osAcquireMutex(&apm32f4xxCryptoMutex);

   //Select the relevant hash algorithm
   HASH->CTRL = HASH_CTRL_DTYPE_8B | algo;
   //Initialize message digest calculation
   HASH->CTRL |= HASH_CTRL_INITCAL;

   //Restore initial hash value
   for(i = 0; i < hLen; i++)
   {
      HASH->CTSWAP[6 + i] = h[i];
      HASH->CTSWAP[14 + i] = h[i];
   }

   //Input data are processed in a block-by-block fashion
   while(length >= 64)
   {
      //Write the first byte of the block
      HASH->INDATA = __UNALIGNED_UINT32_READ(data);

      //Wait for the BUSY bit to be cleared
      while((HASH->STS & HASH_STS_BUSY) != 0)
      {
      }

      //Write the rest of the block
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 4);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 8);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 12);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 16);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 20);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 24);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 28);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 32);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 36);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 40);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 44);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 48);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 52);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 56);
      HASH->INDATA = __UNALIGNED_UINT32_READ(data + 60);

      //Advance data pointer
      data += 64;
      length -= 64;
   }

   //Partial digest computation are triggered each time the application
   //writes the first word of the next block
   HASH->INDATA = 0;

   //Wait for the BUSY bit to be cleared
   while((HASH->STS & HASH_STS_BUSY) != 0)
   {
   }

   //Save intermediate hash value
   for(i = 0; i < hLen; i++)
   {
      h[i] = HASH->CTSWAP[14 + i];
   }

   //Release exclusive access to the HASH module
   osReleaseMutex(&apm32f4xxCryptoMutex);
}


#if (MD5_SUPPORT == ENABLED)

/**
 * @brief Update the MD5 context with a portion of the message being hashed
 * @param[in] context Pointer to the MD5 context
 * @param[in] data Pointer to the buffer being hashed
 * @param[in] length Length of the buffer
 **/

void md5Update(Md5Context *context, const void *data, size_t length)
{
   size_t n;

   //Process the incoming data
   while(length > 0)
   {
      //Check whether some data is pending in the buffer
      if(context->size == 0 && length >= 64)
      {
         //The length must be a multiple of 64 bytes
         n = length - (length % 64);

         //Update hash value
         hashProcessData(HASH_CTRL_ALGSEL_MD5, data, n, context->h,
            MD5_DIGEST_SIZE / 4);

         //Update the MD5 context
         context->totalSize += n;
         //Advance the data pointer
         data = (uint8_t *) data + n;
         //Remaining bytes to process
         length -= n;
      }
      else
      {
         //The buffer can hold at most 64 bytes
         n = MIN(length, 64 - context->size);

         //Copy the data to the buffer
         osMemcpy(context->buffer + context->size, data, n);

         //Update the MD5 context
         context->size += n;
         context->totalSize += n;
         //Advance the data pointer
         data = (uint8_t *) data + n;
         //Remaining bytes to process
         length -= n;

         //Check whether the buffer is full
         if(context->size == 64)
         {
            //Update hash value
            hashProcessData(HASH_CTRL_ALGSEL_MD5, context->buffer, context->size,
               context->h, MD5_DIGEST_SIZE / 4);

            //Empty the buffer
            context->size = 0;
         }
      }
   }
}


/**
 * @brief Process message in 16-word blocks
 * @param[in] context Pointer to the MD5 context
 **/

void md5ProcessBlock(Md5Context *context)
{
   //Update hash value
   hashProcessData(HASH_CTRL_ALGSEL_MD5, context->buffer, 64, context->h,
      MD5_DIGEST_SIZE / 4);
}

#endif
#if (SHA1_SUPPORT == ENABLED)

/**
 * @brief Update the SHA-1 context with a portion of the message being hashed
 * @param[in] context Pointer to the SHA-1 context
 * @param[in] data Pointer to the buffer being hashed
 * @param[in] length Length of the buffer
 **/

void sha1Update(Sha1Context *context, const void *data, size_t length)
{
   size_t n;

   //Process the incoming data
   while(length > 0)
   {
      //Check whether some data is pending in the buffer
      if(context->size == 0 && length >= 64)
      {
         //The length must be a multiple of 64 bytes
         n = length - (length % 64);

         //Update hash value
         hashProcessData(HASH_CTRL_ALGSEL_SHA1, data, n, context->h,
            SHA1_DIGEST_SIZE / 4);

         //Update the SHA-1 context
         context->totalSize += n;
         //Advance the data pointer
         data = (uint8_t *) data + n;
         //Remaining bytes to process
         length -= n;
      }
      else
      {
         //The buffer can hold at most 64 bytes
         n = MIN(length, 64 - context->size);

         //Copy the data to the buffer
         osMemcpy(context->buffer + context->size, data, n);

         //Update the SHA-1 context
         context->size += n;
         context->totalSize += n;
         //Advance the data pointer
         data = (uint8_t *) data + n;
         //Remaining bytes to process
         length -= n;

         //Check whether the buffer is full
         if(context->size == 64)
         {
            //Update hash value
            hashProcessData(HASH_CTRL_ALGSEL_SHA1, context->buffer, context->size,
               context->h, SHA1_DIGEST_SIZE / 4);

            //Empty the buffer
            context->size = 0;
         }
      }
   }
}


/**
 * @brief Process message in 16-word blocks
 * @param[in] context Pointer to the SHA-1 context
 **/

void sha1ProcessBlock(Sha1Context *context)
{
   //Update hash value
   hashProcessData(HASH_CTRL_ALGSEL_SHA1, context->buffer, 64, context->h,
      SHA1_DIGEST_SIZE / 4);
}

#endif
#endif
