#include <stdint.h>
#include <stdio.h>

void
diag_vdump_buf_with_offset(
      uint8_t    *p,
      uint32_t   s,
      uint8_t    *base
      )
{
    int i, c;
    if ((uint32_t)s > (uint32_t)p) {
        s = (uint32_t)s - (uint32_t)p;
    }
    while ((int)s > 0) {
        if (base) {
            printf("%08X: ", (int)((uint32_t)p - (uint32_t)base));
        } else {
            printf("%08X: ", (int)p);
        }
        for (i = 0;  i < 16;  i++) {
            if (i < (int)s) {
                printf("%02X ", p[i] & 0xFF);
            } else {
                printf("   ");
            }
        if (i == 7) printf(" ");
        }
        printf(" |");
        for (i = 0;  i < 16;  i++) {
            if (i < (int)s) {
                c = p[i] & 0xFF;
                if ((c < 0x20) || (c >= 0x7F)) c = '.';
            } else {
                c = ' ';
            }
            printf("%c", c);
        }
        printf("|\n\r");
        s -= 16;
        p += 16;
    }
}

void
diag_dump_buf_with_offset(
      uint8_t    *p,
      uint32_t   s,
      uint8_t    *base
      )
{
    diag_vdump_buf_with_offset(p, s, base);
}

void diag_dump_buf(void *p, uint32_t s)
{
   diag_dump_buf_with_offset((uint8_t *)p, s, 0);
}
