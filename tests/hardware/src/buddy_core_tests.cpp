#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "BuddyProtocol.hpp"
#include "JsonLineAssembler.hpp"

extern "C" void setUp(void)
{
}

extern "C" void tearDown(void)
{
}

namespace {

struct Lines {
    uint8_t count;
    uint16_t len[3];
    char value[3][96];
};

void reset_lines(Lines *lines)
{
    memset(lines, 0, sizeof(*lines));
}

void collect_line(const char *line, uint16_t len, void *context)
{
    Lines *lines = static_cast<Lines *>(context);
    const uint8_t index = lines->count;
    uint16_t copy_len = len;

    if (index >= 3)
    {
        return;
    }

    if (copy_len >= sizeof(lines->value[index]))
    {
        copy_len = sizeof(lines->value[index]) - 1U;
    }

    memcpy(lines->value[index], line, copy_len);
    lines->value[index][copy_len] = '\0';
    lines->len[index] = len;
    lines->count++;
}

void feed_text(buddy::JsonLineAssembler *assembler, const char *text, Lines *lines)
{
    assembler->feed(reinterpret_cast<const uint8_t *>(text),
                    static_cast<uint16_t>(strlen(text)),
                    collect_line,
                    lines);
}

void test_json_line_assembler_splits_fragmented_line(void)
{
    buddy::JsonLineAssembler assembler;
    Lines lines;

    reset_lines(&lines);
    feed_text(&assembler, "{\"cmd\":\"sta", &lines);
    TEST_ASSERT_EQUAL_UINT8(0, lines.count);

    feed_text(&assembler, "tus\"}\n", &lines);
    TEST_ASSERT_EQUAL_UINT8(1, lines.count);
    TEST_ASSERT_EQUAL_STRING("{\"cmd\":\"status\"}", lines.value[0]);
    TEST_ASSERT_EQUAL_UINT16(0, assembler.pending_length());
}

void test_json_line_assembler_handles_packed_crlf_lines(void)
{
    buddy::JsonLineAssembler assembler;
    Lines lines;

    reset_lines(&lines);
    feed_text(&assembler, "{\"cmd\":\"status\"}\r\n{\"cmd\":\"name\"}\n", &lines);

    TEST_ASSERT_EQUAL_UINT8(2, lines.count);
    TEST_ASSERT_EQUAL_STRING("{\"cmd\":\"status\"}", lines.value[0]);
    TEST_ASSERT_EQUAL_STRING("{\"cmd\":\"name\"}", lines.value[1]);
    TEST_ASSERT_EQUAL_UINT32(2, assembler.total_lines());
}

void test_json_line_assembler_drops_oversized_line_and_recovers(void)
{
    static uint8_t too_long[buddy::JsonLineAssembler::kMaxLineLength + 11U];
    buddy::JsonLineAssembler assembler;
    Lines lines;

    memset(too_long, 'x', sizeof(too_long));
    too_long[sizeof(too_long) - 1U] = '\n';

    reset_lines(&lines);
    buddy::JsonLineAssembler::FeedStats stats =
        assembler.feed(too_long, sizeof(too_long), collect_line, &lines);
    TEST_ASSERT_EQUAL_UINT16(1, stats.overflowed);
    TEST_ASSERT_EQUAL_UINT8(0, lines.count);

    feed_text(&assembler, "{\"cmd\":\"status\"}\n", &lines);
    TEST_ASSERT_EQUAL_UINT8(1, lines.count);
    TEST_ASSERT_EQUAL_STRING("{\"cmd\":\"status\"}", lines.value[0]);
    TEST_ASSERT_EQUAL_UINT32(1, assembler.total_overflowed());
}

struct ProtocolHarness {
    uint8_t sent_count;
    int unpair_calls;
    char sent[4][512];
};

int send_line(const char *line, uint16_t len, void *context)
{
    ProtocolHarness *harness = static_cast<ProtocolHarness *>(context);
    uint16_t copy_len = len;

    if (harness->sent_count >= 4)
    {
        return len;
    }

    if (copy_len >= sizeof(harness->sent[harness->sent_count]))
    {
        copy_len = sizeof(harness->sent[harness->sent_count]) - 1U;
    }

    memcpy(harness->sent[harness->sent_count], line, copy_len);
    harness->sent[harness->sent_count][copy_len] = '\0';
    harness->sent_count++;

    return len;
}

int unpair(void *context)
{
    ProtocolHarness *harness = static_cast<ProtocolHarness *>(context);
    harness->unpair_calls++;
    return 0;
}

buddy::BuddyProtocol::RuntimeStatus runtime_status(void)
{
    buddy::BuddyProtocol::RuntimeStatus runtime;

    runtime.connected = true;
    runtime.encrypted = true;
    runtime.uptime_ms = 1234;
    runtime.tick_count = 7;
    runtime.rx_lines = 3;
    runtime.rx_overflowed = 1;

    return runtime;
}

bool contains(const char *text, const char *needle)
{
    return strstr(text, needle) != nullptr;
}

void handle_line(buddy::BuddyProtocol *protocol, const char *line)
{
    protocol->handle_line(line, static_cast<uint16_t>(strlen(line)), runtime_status());
}

void test_buddy_protocol_status_ack_includes_runtime_fields(void)
{
    buddy::BuddyProtocol protocol;
    buddy::BuddyProtocol::Hooks hooks;
    ProtocolHarness harness = {};

    hooks.send = send_line;
    hooks.unpair = unpair;
    hooks.context = &harness;
    protocol.set_hooks(hooks);

    handle_line(&protocol, "{\"cmd\":\"status\"}");

    TEST_ASSERT_EQUAL_UINT8(1, harness.sent_count);
    TEST_ASSERT_TRUE(contains(harness.sent[0], "\"ack\":\"status\""));
    TEST_ASSERT_TRUE(contains(harness.sent[0], "\"ok\":true"));
    TEST_ASSERT_TRUE(contains(harness.sent[0], "\"sec\":true"));
    TEST_ASSERT_TRUE(contains(harness.sent[0], "\"species_count\":18"));
}

void test_buddy_protocol_unknown_command_is_rejected(void)
{
    buddy::BuddyProtocol protocol;
    buddy::BuddyProtocol::Hooks hooks;
    ProtocolHarness harness = {};

    hooks.send = send_line;
    hooks.context = &harness;
    protocol.set_hooks(hooks);

    handle_line(&protocol, "{\"cmd\":\"not_a_command\"}");

    TEST_ASSERT_EQUAL_UINT8(1, harness.sent_count);
    TEST_ASSERT_TRUE(contains(harness.sent[0], "\"ack\":\"not_a_command\""));
    TEST_ASSERT_TRUE(contains(harness.sent[0], "\"ok\":false"));
    TEST_ASSERT_TRUE(contains(harness.sent[0], "\"error\":\"unsupported\""));
}

}  // namespace

extern "C" int buddy_hardware_tests_run_all(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_json_line_assembler_splits_fragmented_line);
    RUN_TEST(test_json_line_assembler_handles_packed_crlf_lines);
    RUN_TEST(test_json_line_assembler_drops_oversized_line_and_recovers);
    RUN_TEST(test_buddy_protocol_status_ack_includes_runtime_fields);
    RUN_TEST(test_buddy_protocol_unknown_command_is_rejected);
    return UNITY_END();
}
