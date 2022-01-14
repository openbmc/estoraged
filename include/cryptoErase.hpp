#pragma once

#include "erase.hpp"

#include <libcryptsetup.h>

class CryptoErase : public Erase
{
  public:
    CryptoErase(struct crypt_device* cd, int keyslot);
};
