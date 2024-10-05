#include <kunit/test.h>
#include "example.h"

/* Define the test cases. */

static void virtio_example_add_test_basic(struct kunit *test)
{
        KUNIT_EXPECT_EQ(test, 1, virtio_example_add(1, 0));
        KUNIT_EXPECT_EQ(test, 2, virtio_example_add(1, 1));
        KUNIT_EXPECT_EQ(test, 0, virtio_example_add(-1, 1));
        KUNIT_EXPECT_EQ(test, INT_MAX, virtio_example_add(0, INT_MAX));
        KUNIT_EXPECT_EQ(test, -1, virtio_example_add(INT_MAX, INT_MIN));
}

static void virtio_example_test_failure(struct kunit *test)
{
        KUNIT_FAIL(test, "This test never passes.");
}

static struct kunit_case virtio_example_test_cases[] = {
        KUNIT_CASE(virtio_example_add_test_basic),
        KUNIT_CASE(virtio_example_test_failure),
        {}
};

static struct kunit_suite virtio_example_test_suite = {
        .name = "virtio-example",
        .test_cases = virtio_example_test_cases,
};
kunit_test_suite(virtio_example_test_suite);