#ifndef CKT_BIT_H
#define CKT_BIT_H


#include "headers.h"


const uint64_t TABLE_LEN = (1 << 22);


static int Ckt_CountOneNum2(uint64_t n);


class Ckt_Bit_Cnt_t
{
public:
    uint8_t                 table[TABLE_LEN];

public:
    explicit                Ckt_Bit_Cnt_t     (void)                        {for (uint64_t i = 0; i < TABLE_LEN; ++i) table[i] = static_cast <uint8_t> (Ckt_CountOneNum2(i));}

    inline uint8_t          GetOneNum         (uint64_t n)                  {return table[n >> 44] + table[(n >> 22) & 0x3fffff] + table[n & 0x3fffff];}
};


static inline void          Ckt_SetBit        (uint64_t & x, uint64_t f)    {x |= (static_cast<uint64_t>(1) << (f & static_cast<uint64_t>(63)));}
static inline void          Ckt_ResetBit      (uint64_t & x, uint64_t f)    {x &= ~(static_cast<uint64_t>(1) << (f & static_cast<uint64_t>(63)));}
static inline bool          Ckt_GetBit        (uint64_t x, uint64_t f)      {return static_cast<bool>((x >> f) & static_cast<uint64_t>(1));}
static inline int           Ckt_CountOneNum   (uint64_t n)                  {static Ckt_Bit_Cnt_t t; return static_cast <int> (t.GetOneNum(n));}
static inline int           Ckt_CountOneNum2  (uint64_t n)                  {int ret = 0; while (n) { n = n & (n - 1); ++ret; } return ret;}


#endif
