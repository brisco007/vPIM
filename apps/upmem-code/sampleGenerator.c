#include <stdio.h>

#define DLEN (1 << 16)

int main() {
  FILE *f = fopen("sample.bin", "wb");
  int i, checksum = 0;
  i = DLEN;
  fwrite(&i, 4, 1, f);
  for (i = 0; i < DLEN; i++) {
    unsigned char ii = (unsigned char)i;
    fwrite(&ii, 1, 1, f);
    checksum += ii;
  }
  fclose(f);
  printf("checksum = %d = 0x%x\n", checksum, checksum);
  return 0;
}
