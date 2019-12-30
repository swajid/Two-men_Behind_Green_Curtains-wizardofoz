#include "unit.h"
#include "fff.h"
#include "logging.h"
#include "hpack/utils.h"

extern uint32_t hpack_utils_read_bits_from_bytes(uint16_t current_bit_pointer, uint8_t number_of_bits_to_read, const uint8_t *buffer);
extern uint32_t hpack_utils_log128(uint32_t x);
extern hpack_preamble_t hpack_utils_get_preamble(uint8_t preamble);
extern uint32_t hpack_utils_encoded_integer_size(uint32_t num, uint8_t prefix);

DEFINE_FFF_GLOBALS;
/*
   FAKE_VALUE_FUNC(int8_t, hpack_huffman_encode, huffman_encoded_word_t *, uint8_t);
   FAKE_VALUE_FUNC(int8_t, hpack_huffman_decode, huffman_encoded_word_t *, uint8_t *);
   FAKE_VALUE_FUNC(int,  headers_add, header_list_t * , const char * , const char * );
 */

void setUp(void)
{
    /* Register resets */
    //FFF_FAKES_LIST(RESET_FAKE);

    /* reset common FFF internal structures */
    FFF_RESET_HISTORY();
}


void test_hpack_utils_read_bits_from_bytes(void)
{
    uint8_t buffer[] = { 0xD1, 0xC5, 0x6E };
    uint32_t code = 0;

    //Test if it reads 1 byte correctly
    code = hpack_utils_read_bits_from_bytes(0, 8, buffer);
    TEST_ASSERT_EQUAL(0xD1, code);

    code = hpack_utils_read_bits_from_bytes(8, 8, buffer);
    TEST_ASSERT_EQUAL(0xC5, code);

    code = hpack_utils_read_bits_from_bytes(16, 8, buffer);
    TEST_ASSERT_EQUAL(0x6E, code);

    //Test reading different number of bits
    code = hpack_utils_read_bits_from_bytes(0, 1, buffer);
    TEST_ASSERT_EQUAL(0x1, code);

    code = hpack_utils_read_bits_from_bytes(0, 3, buffer);
    TEST_ASSERT_EQUAL(0x6, code);

    code = hpack_utils_read_bits_from_bytes(0, 5, buffer);
    TEST_ASSERT_EQUAL(0x1A, code);

    //Test reading between bytes
    code = hpack_utils_read_bits_from_bytes(4, 8, buffer);
    TEST_ASSERT_EQUAL(0x1C, code);

    code = hpack_utils_read_bits_from_bytes(12, 8, buffer);
    TEST_ASSERT_EQUAL(0x56, code);

    code = hpack_utils_read_bits_from_bytes(4, 16, buffer);
    TEST_ASSERT_EQUAL(0x1C56, code);

    /**/
    uint8_t buffer2[] = {0x9d};
    code = hpack_utils_read_bits_from_bytes(2, 5, buffer2);
    TEST_ASSERT_EQUAL(0xe, code);

    code = hpack_utils_read_bits_from_bytes(2, 6, buffer2);
    TEST_ASSERT_EQUAL(0x1d, code);

    /*Test reading eos*/
    uint8_t eos[] = {0xff,0xff,0xff,0xfc};

    code = hpack_utils_read_bits_from_bytes(0, 30, eos);
    TEST_ASSERT_EQUAL(0x3fffffff, code);

    uint8_t eos2[] = {0x7f,0xff,0xff,0xfe};

    code = hpack_utils_read_bits_from_bytes(1, 30, eos2);
    TEST_ASSERT_EQUAL(0x3fffffff, code);

    uint8_t eos3[] = {0x3f,0xff,0xff,0xff};

    code = hpack_utils_read_bits_from_bytes(2, 30, eos3);
    TEST_ASSERT_EQUAL(0x3fffffff, code);

    uint8_t eos4[] = {0x3,0xff,0xff,0xff,0xf0};

    code = hpack_utils_read_bits_from_bytes(6, 30, eos4);
    TEST_ASSERT_EQUAL(0x3fffffff, code);
}
void test_hpack_utils_log128(void)
{
    TEST_ASSERT_EQUAL(0, hpack_utils_log128(1));
    TEST_ASSERT_EQUAL(0, hpack_utils_log128(2));
    TEST_ASSERT_EQUAL(0, hpack_utils_log128(3));
    TEST_ASSERT_EQUAL(0, hpack_utils_log128(4));
    TEST_ASSERT_EQUAL(0, hpack_utils_log128(5));
    TEST_ASSERT_EQUAL(0, hpack_utils_log128(25));
    TEST_ASSERT_EQUAL(0, hpack_utils_log128(127));
    TEST_ASSERT_EQUAL(1, hpack_utils_log128(128));
    TEST_ASSERT_EQUAL(1, hpack_utils_log128(387));
    TEST_ASSERT_EQUAL(1, hpack_utils_log128(4095));
    TEST_ASSERT_EQUAL(1, hpack_utils_log128(16383));
    TEST_ASSERT_EQUAL(2, hpack_utils_log128(16384));
    TEST_ASSERT_EQUAL(2, hpack_utils_log128(1187752));
    TEST_ASSERT_EQUAL(3, hpack_utils_log128(2097152));
}

void test_hpack_utils_get_preamble(void)
{
    uint8_t preamble_arr[] = { (uint8_t)INDEXED_HEADER_FIELD,
                               (uint8_t)LITERAL_HEADER_FIELD_WITH_INCREMENTAL_INDEXING,
                               (uint8_t)DYNAMIC_TABLE_SIZE_UPDATE,
                               (uint8_t)LITERAL_HEADER_FIELD_NEVER_INDEXED,
                               (uint8_t)LITERAL_HEADER_FIELD_WITHOUT_INDEXING };

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL((hpack_preamble_t)preamble_arr[i], hpack_utils_get_preamble(preamble_arr[i]));
    }

}
void test_hpack_utils_find_prefix_size(void)
{
    hpack_preamble_t octet = LITERAL_HEADER_FIELD_WITHOUT_INDEXING;//0000 0000

    uint8_t rc = hpack_utils_find_prefix_size(octet);

    TEST_ASSERT_EQUAL(4, rc);

    octet = INDEXED_HEADER_FIELD;//1 0000000
    rc = hpack_utils_find_prefix_size(octet);
    TEST_ASSERT_EQUAL(7, rc);

    octet = LITERAL_HEADER_FIELD_WITH_INCREMENTAL_INDEXING;//01 000000
    rc = hpack_utils_find_prefix_size(octet);
    TEST_ASSERT_EQUAL(6, rc);

    octet = DYNAMIC_TABLE_SIZE_UPDATE;//001 00000
    rc = hpack_utils_find_prefix_size(octet);
    TEST_ASSERT_EQUAL(5, rc);
}

void test_hpack_utils_encoded_integer_size(void)
{
    uint32_t integer = 10;
    uint8_t prefix = 5;

    INFO("encoded: %u with prefix %u", integer, prefix);
    int rc = hpack_utils_encoded_integer_size(integer, prefix);
    TEST_ASSERT_EQUAL(1, rc);

    integer = 30;
    prefix = 5;
    INFO("encoded: %u with prefix %u", integer, prefix);
    rc = hpack_utils_encoded_integer_size(integer, prefix);
    TEST_ASSERT_EQUAL(1, rc);

    integer = 31;
    prefix = 5;
    INFO("encoded: %u with prefix %u", integer, prefix);
    rc = hpack_utils_encoded_integer_size(integer, prefix);
    TEST_ASSERT_EQUAL(2, rc);

    integer = 31;
    prefix = 6;
    INFO("encoded: %u with prefix %u", integer, prefix);
    rc = hpack_utils_encoded_integer_size(integer, prefix);
    TEST_ASSERT_EQUAL(1, rc);

    integer = 1337;
    prefix = 5;
    INFO("encoded: %u with prefix %u", integer, prefix);
    rc = hpack_utils_encoded_integer_size(integer, prefix);
    TEST_ASSERT_EQUAL(3, rc);
}

int main(void)
{
    UNIT_TESTS_BEGIN();
    UNIT_TEST(test_hpack_utils_read_bits_from_bytes);
    UNIT_TEST(test_hpack_utils_log128);
    UNIT_TEST(test_hpack_utils_get_preamble);
    UNIT_TEST(test_hpack_utils_find_prefix_size);
    UNIT_TEST(test_hpack_utils_encoded_integer_size);
    return UNIT_TESTS_END();
}