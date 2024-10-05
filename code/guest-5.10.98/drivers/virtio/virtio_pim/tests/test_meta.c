#include <linux/kernel.h>
#include "../dpu.h"
#include "../dpu_rank.h"
#include "phys_address_test.h"
#include "CI_xfer_test.h"
#include "test_meta.h"

void run_tests(struct dpu_rank_t *rank){
    if(!RUN_TESTS) return;
    printk(KERN_INFO "########### Start testing\n");
    if(TEST_CI) read_from_ci_test(rank);
    if(TEST_CI) write_to_ci_test(rank);
    if(TEST_ADDRESS) send_address_test(rank);
    printk(KERN_INFO "########### Tests finished\n");
}