#include <mram.h>
#include <seqread.h>
#include <stdint.h>

#define DLEN (1 << 16)

__mram_noinit uint8_t buffer[DLEN];

int main() {
  unsigned int bytes_read = 0, buffer_size, checksum = 0;

  /* Cache for the sequential reader */
  seqreader_buffer_t local_cache = seqread_alloc();
  /* The reader */
  seqreader_t sr;
  /* The pointer where we will access the cached data */
  uint8_t *current_char = seqread_init(local_cache, buffer, &sr);

  buffer_size = *(uint32_t *)current_char;
  current_char = seqread_get(current_char, sizeof(uint32_t), &sr);

  while (bytes_read != buffer_size) {
    checksum += *current_char;
    bytes_read++;
    current_char = seqread_get(current_char, sizeof(*current_char), &sr);
  }

  return checksum;
}
