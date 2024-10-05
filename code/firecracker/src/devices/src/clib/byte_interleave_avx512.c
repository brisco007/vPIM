#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <numa.h>
#include <numaif.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <x86intrin.h>
#include <immintrin.h>
#include <sys/sysinfo.h>

#define min(a,b) (a<=b?a:b)
#define NB_REAL_CIS 8
#define BANK_CHUNK_SIZE 0x20000
#define BANK_NEXT_CHUNK_OFFSET 0x100000
typedef struct _xfer_pt {
    uint64_t nb_pages;
    uint32_t off_first_page;
    uint8_t** pages;
} xfer_page_table;


static uint32_t
apply_address_translation_on_mram_offset(uint32_t byte_offset)
{
    /* We have observed that, within the 26 address bits of the MRAM address, we need to apply an address translation:
     *
     * virtual[13: 0] = physical[13: 0]
     * virtual[20:14] = physical[21:15]
     * virtual[   21] = physical[   14]
     * virtual[25:22] = physical[25:22]
     *
     * This function computes the "virtual" mram address based on the given "physical" mram address.
     */

    uint32_t mask_21_to_15 = ((1 << (21 - 15 + 1)) - 1) << 15;
    uint32_t mask_21_to_14 = ((1 << (21 - 14 + 1)) - 1) << 14;
    uint32_t bits_21_to_15 = (byte_offset & mask_21_to_15) >> 15;
    uint32_t bit_14 = (byte_offset >> 14) & 1;
    uint32_t unchanged_bits = byte_offset & ~mask_21_to_14;

    return unchanged_bits | (bits_21_to_15 << 14) | (bit_14 << 21);
}


void byte_interleave_avx512(uint64_t *input, uint64_t *output, int use_stream)
{
    __m512i mask;
    mask = _mm512_set_epi64(0x0f0b07030e0a0602ULL,
        0x0d0905010c080400ULL,

        0x0f0b07030e0a0602ULL,
        0x0d0905010c080400ULL,

        0x0f0b07030e0a0602ULL,
        0x0d0905010c080400ULL,

        0x0f0b07030e0a0602ULL,
        0x0d0905010c080400ULL);

    __m512i vindex = _mm512_setr_epi32(0, 8, 16, 24, 32, 40, 48, 56, 4, 12, 20, 28, 36, 44, 52, 60);
    __m512i perm = _mm512_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15);

    __m512i load = _mm512_i32gather_epi32(vindex, input, 1);
    __m512i transpose = _mm512_shuffle_epi8(load, mask);
    __m512i final = _mm512_permutexvar_epi32(perm, transpose);

    if (use_stream) {
        _mm512_stream_si512((void *)output, final);
        return;
    }

    _mm512_storeu_si512((void *)output, final);
}


uint8_t
c_write_to_dpus(uint8_t* ptr_dest, xfer_page_table* matrix, uint32_t size_transfer, uint32_t offset_in_mram, uint8_t idx)
{
    uint64_t cache_line[NB_REAL_CIS] = {0};
    uint8_t* cur_pages[NB_REAL_CIS] = {NULL};
    uint32_t offset_in_page[NB_REAL_CIS] = {0};
    uint32_t len_xfer_in_page[NB_REAL_CIS] = {0};
    uint32_t page[NB_REAL_CIS] = {0};
    uint32_t len_xfer_done_in_page[NB_REAL_CIS] = {0};
    xfer_page_table* xferp;
    bool do_dpu_transfer = false;
    size_t len_xfer_remaining;

    for (int ci_id = 0; ci_id < NB_REAL_CIS; ci_id++) {
        // Here is the xfer_pages for the dpu idx of the CI of index ci_id
        xferp = matrix + (idx + ci_id);
        if (xferp->nb_pages == 0) {
            continue;
        }
        /*if (xferp->nb_pages != (uint64_t)xferp->pages.len()) {
            panic("VPIMDevice panicked because of inconsistent number of pages");
        }*/
        do_dpu_transfer = true;
        page[ci_id] = 0;
        // Read the page content into the memory
        // let _dur = memory_read_u8(mem, xferp.pages[page[ci_id]], &mut cur_pages[ci_id]);
        cur_pages[ci_id] =  *((uint8_t **)(xferp->pages) + page[ci_id]);
        len_xfer_in_page[ci_id] = (uint32_t)min(4096 - xferp->off_first_page, size_transfer);
        offset_in_page[ci_id] = xferp->off_first_page;
        len_xfer_done_in_page[ci_id] = 0;
    }

    if (!do_dpu_transfer) {
        idx += NB_REAL_CIS;
        return 4; //continue;
    }

    len_xfer_remaining = size_transfer;
    for (size_t len_xfer_done = 0; len_xfer_done < size_transfer; len_xfer_done += NB_REAL_CIS) {

        size_t mram_64_bit_word_offset = apply_address_translation_on_mram_offset((len_xfer_done + offset_in_mram)) / 8;
        size_t next_data = mram_64_bit_word_offset * sizeof(uint64_t) * 16;
        size_t offset = (next_data % (BANK_CHUNK_SIZE)) + (next_data / (BANK_CHUNK_SIZE)) * (BANK_NEXT_CHUNK_OFFSET);

        for (size_t ci_id = 0; ci_id < NB_REAL_CIS; ci_id++) {
            xferp = matrix + (idx + ci_id);
            if (xferp->nb_pages != 0) {
                cache_line[ci_id] = *(uint64_t*)(cur_pages[ci_id] + 
               ( offset_in_page[ci_id] + len_xfer_done_in_page[ci_id]));
            }
        }

        byte_interleave_avx512((uint64_t*)cache_line, (uint64_t*)(ptr_dest + offset), true);

        len_xfer_remaining -= NB_REAL_CIS;

        for (size_t ci_id = 0; ci_id < NB_REAL_CIS; ci_id++) {
            xferp = matrix + (idx + ci_id);
            if (xferp->nb_pages == 0) {
                continue;
            }

            len_xfer_done_in_page[ci_id] += NB_REAL_CIS;
            
            if (page[ci_id] < (xferp->nb_pages -1) && len_xfer_done_in_page[ci_id] >= len_xfer_in_page[ci_id]) {
                page[ci_id] += 1;
                cur_pages[ci_id] = *((uint8_t **)(xferp->pages) + page[ci_id]);

                len_xfer_in_page[ci_id] = min(4096, len_xfer_remaining);
                len_xfer_done_in_page[ci_id] = 0;
                offset_in_page[ci_id] = 0;
            }
        }
    }

return 3;

}



uint8_t
c_read_from_dpus(uint8_t* ptr_dest, xfer_page_table* matrix, uint32_t size_transfer, uint32_t offset_in_mram, uint8_t idx)
{
    uint64_t cache_line[NB_REAL_CIS] = {0};
    uint64_t cache_line_interleave[NB_REAL_CIS] = {0};
    uint8_t* cur_pages[NB_REAL_CIS] = {NULL};
    uint32_t offset_in_page[NB_REAL_CIS] = {0};
    uint32_t len_xfer_in_page[NB_REAL_CIS] = {0};
    uint32_t page[NB_REAL_CIS] = {0};
    uint32_t len_xfer_done_in_page[NB_REAL_CIS] = {0};
    xfer_page_table* xferp;
    bool do_dpu_transfer = false;
    size_t len_xfer_remaining;

    for (int ci_id = 0; ci_id < NB_REAL_CIS; ci_id++) {
        // Here is the xfer_pages for the dpu idx of the CI of index ci_id
        xferp = matrix + (idx + ci_id);
        if (xferp->nb_pages == 0) {
            continue;
        }
        do_dpu_transfer = true;
        page[ci_id] = 0;
        cur_pages[ci_id] =  *((uint8_t **)(xferp->pages) + page[ci_id]);
        len_xfer_in_page[ci_id] = (uint32_t)min(4096 - xferp->off_first_page, size_transfer);
        offset_in_page[ci_id] = xferp->off_first_page;
        len_xfer_done_in_page[ci_id] = 0;
    }

    if (!do_dpu_transfer) {
        idx += NB_REAL_CIS;
        return 4; //continue;
    }
    __builtin_ia32_mfence();
    for (int i = 0; i < size_transfer / sizeof(uint64_t); ++i) {
            size_t mram_64_bit_word_offset = apply_address_translation_on_mram_offset((i*8 + offset_in_mram)) / 8;
            size_t next_data = mram_64_bit_word_offset * sizeof(uint64_t) * 16;
            size_t offset = (next_data % (BANK_CHUNK_SIZE)) + (next_data / (BANK_CHUNK_SIZE)) * (BANK_NEXT_CHUNK_OFFSET);

            /* Invalidates possible prefetched cache line or old cache line */
            __builtin_ia32_clflushopt((uint8_t *)ptr_dest + offset);
    }

    __builtin_ia32_mfence();


    len_xfer_remaining = size_transfer;
    for (size_t len_xfer_done = 0; len_xfer_done < size_transfer; len_xfer_done += NB_REAL_CIS) {

        size_t mram_64_bit_word_offset = apply_address_translation_on_mram_offset((len_xfer_done + offset_in_mram)) / 8;
        size_t next_data = mram_64_bit_word_offset * sizeof(uint64_t) * 16;
        size_t offset = (next_data % (BANK_CHUNK_SIZE)) + (next_data / (BANK_CHUNK_SIZE)) * (BANK_NEXT_CHUNK_OFFSET);

        cache_line[0] = *((volatile uint64_t *)((uint8_t *)ptr_dest + offset + 0 * sizeof(uint64_t)));
        cache_line[1] = *((volatile uint64_t *)((uint8_t *)ptr_dest + offset + 1 * sizeof(uint64_t)));
        cache_line[2] = *((volatile uint64_t *)((uint8_t *)ptr_dest + offset + 2 * sizeof(uint64_t)));
        cache_line[3] = *((volatile uint64_t *)((uint8_t *)ptr_dest + offset + 3 * sizeof(uint64_t)));
        cache_line[4] = *((volatile uint64_t *)((uint8_t *)ptr_dest + offset + 4 * sizeof(uint64_t)));
        cache_line[5] = *((volatile uint64_t *)((uint8_t *)ptr_dest + offset + 5 * sizeof(uint64_t)));
        cache_line[6] = *((volatile uint64_t *)((uint8_t *)ptr_dest + offset + 6 * sizeof(uint64_t)));
        cache_line[7] = *((volatile uint64_t *)((uint8_t *)ptr_dest + offset + 7 * sizeof(uint64_t)));


        byte_interleave_avx512(cache_line, cache_line_interleave, false);

        for (int ci_id = 0; ci_id < NB_REAL_CIS; ++ci_id) {
            xferp = matrix + (idx + ci_id);
            if (xferp->nb_pages != 0) {
                 *((uint64_t *)(cur_pages[ci_id] + offset_in_page[ci_id] + len_xfer_done_in_page[ci_id])) = cache_line_interleave[ci_id];
            }
        }

        len_xfer_remaining -= NB_REAL_CIS;

        for (size_t ci_id = 0; ci_id < NB_REAL_CIS; ci_id++) {
            xferp = matrix + (idx + ci_id);
            if (xferp->nb_pages == 0) {
                continue;
            }

            len_xfer_done_in_page[ci_id] += NB_REAL_CIS;
            
            if (page[ci_id] < (xferp->nb_pages -1) && len_xfer_done_in_page[ci_id] >= len_xfer_in_page[ci_id]) {
                page[ci_id] += 1;
                cur_pages[ci_id] = *((uint8_t **)(xferp->pages) + page[ci_id]);

                len_xfer_in_page[ci_id] = min(4096, len_xfer_remaining);
                len_xfer_done_in_page[ci_id] = 0;
                offset_in_page[ci_id] = 0;
            }
        }
    }

return 3;

}
