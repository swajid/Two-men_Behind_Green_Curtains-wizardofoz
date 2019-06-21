#include <string.h>

#include "unit.h"
#include "cbuf.h"

void test_write_after_end_buffer(void)
{
    char buf[8];
    cbuf_t cbuf;

    cbuf_init(&cbuf, buf, 8);

    int bytes = 0;

    bytes = cbuf_write(&cbuf, "abcd", 4);
    TEST_ASSERT_EQUAL_MESSAGE(4, bytes, "Should write 4 bytes");
    TEST_ASSERT_EQUAL_STRING_LEN("abcd", buf, 4);

    bytes = cbuf_write(&cbuf, "efgh", 4);
    TEST_ASSERT_EQUAL_MESSAGE(4, bytes, "Should write 4 bytes");
    TEST_ASSERT_EQUAL_STRING_LEN("abcdefgh", buf, 8);

    bytes = cbuf_write(&cbuf, "ijk", 3);
    TEST_ASSERT_EQUAL_MESSAGE(0, bytes, "Should not keep writing after end of buffer");
    TEST_ASSERT_EQUAL_STRING_LEN("abcdefgh", buf, 8);
}

void test_write_and_read(void)
{
    char buf[8], readbuf[4];
    cbuf_t cbuf;

    cbuf_init(&cbuf, buf, 8);

    int bytes = 0;

    bytes = cbuf_read(&cbuf, readbuf, 10);
    TEST_ASSERT_EQUAL_MESSAGE(0, bytes, "Should read 0 bytes");

    bytes = cbuf_write(&cbuf, "abcd", 4);
    TEST_ASSERT_EQUAL_MESSAGE(4, bytes, "Should write 4 bytes");
    TEST_ASSERT_EQUAL_STRING_LEN("abcd", buf, 4);

    bytes = cbuf_read(&cbuf, readbuf, 3);
    TEST_ASSERT_EQUAL_MESSAGE(3, bytes, "Should read 3 bytes");
    TEST_ASSERT_EQUAL_STRING_LEN("abc", readbuf, 3);
    
    bytes = cbuf_write(&cbuf, "efgh", 4);
    TEST_ASSERT_EQUAL_MESSAGE(4, bytes, "Should write 4 bytes");
    TEST_ASSERT_EQUAL_STRING_LEN("abcdefgh", buf, 8);
    
    bytes = cbuf_read(&cbuf, readbuf, 4);
    TEST_ASSERT_EQUAL_MESSAGE(4, bytes, "Should read 4 bytes");
    TEST_ASSERT_EQUAL_STRING_LEN("defg", readbuf, 4);
    
    bytes = cbuf_write(&cbuf, "ijk", 3);
    TEST_ASSERT_EQUAL_MESSAGE(3, bytes, "Should write 3 bytes");
    TEST_ASSERT_EQUAL_STRING_LEN("ijkdefgh", buf, 8);
    
    bytes = cbuf_read(&cbuf, readbuf, 4);
    TEST_ASSERT_EQUAL_MESSAGE(4, bytes, "Should read 4 bytes");
    TEST_ASSERT_EQUAL_STRING_LEN("hijk", readbuf, 4);

    bytes = cbuf_read(&cbuf, readbuf, 4);
    TEST_ASSERT_EQUAL_MESSAGE(0, bytes, "Should read 0 bytes");
}


int main(void)
{
    UNIT_TESTS_BEGIN();
    UNIT_TEST(test_write_after_end_buffer);
    UNIT_TEST(test_write_and_read);
    UNIT_TESTS_END();
}