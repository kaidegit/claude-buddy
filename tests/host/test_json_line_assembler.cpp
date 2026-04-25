#include "JsonLineAssembler.hpp"

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Lines {
    std::vector<std::string> values;
};

void collect_line(const char *line, uint16_t len, void *context)
{
    auto *lines = static_cast<Lines *>(context);
    lines->values.emplace_back(line, len);
}

void feed(buddy::JsonLineAssembler &assembler, const char *text, Lines &lines)
{
    assembler.feed(reinterpret_cast<const uint8_t *>(text), static_cast<uint16_t>(std::strlen(text)),
                   collect_line, &lines);
}

void test_split_line()
{
    buddy::JsonLineAssembler assembler;
    Lines lines;

    feed(assembler, "{\"cmd\":\"sta", lines);
    assert(lines.values.empty());
    feed(assembler, "tus\"}\n", lines);

    assert(lines.values.size() == 1);
    assert(lines.values[0] == "{\"cmd\":\"status\"}");
    assert(assembler.pending_length() == 0);
}

void test_packed_lines()
{
    buddy::JsonLineAssembler assembler;
    Lines lines;

    feed(assembler, "{\"cmd\":\"status\"}\n{\"cmd\":\"name\"}\n", lines);

    assert(lines.values.size() == 2);
    assert(lines.values[0] == "{\"cmd\":\"status\"}");
    assert(lines.values[1] == "{\"cmd\":\"name\"}");
    assert(assembler.total_lines() == 2);
}

void test_crlf()
{
    buddy::JsonLineAssembler assembler;
    Lines lines;

    feed(assembler, "{\"cmd\":\"status\"}\r\n", lines);

    assert(lines.values.size() == 1);
    assert(lines.values[0] == "{\"cmd\":\"status\"}");
}

void test_oversized_line_is_dropped()
{
    buddy::JsonLineAssembler assembler;
    Lines lines;
    std::string too_long(buddy::JsonLineAssembler::kMaxLineLength + 10, 'x');
    too_long.push_back('\n');
    too_long += "{\"cmd\":\"status\"}\n";

    auto stats = assembler.feed(reinterpret_cast<const uint8_t *>(too_long.data()),
                                static_cast<uint16_t>(too_long.size()), collect_line, &lines);

    assert(stats.overflowed == 1);
    assert(stats.lines == 1);
    assert(lines.values.size() == 1);
    assert(lines.values[0] == "{\"cmd\":\"status\"}");
    assert(assembler.total_overflowed() == 1);
}

void test_max_length_line_is_allowed()
{
    buddy::JsonLineAssembler assembler;
    Lines lines;
    std::string max_line(buddy::JsonLineAssembler::kMaxLineLength, 'a');
    max_line.push_back('\n');

    auto stats = assembler.feed(reinterpret_cast<const uint8_t *>(max_line.data()),
                                static_cast<uint16_t>(max_line.size()), collect_line, &lines);

    assert(stats.overflowed == 0);
    assert(stats.lines == 1);
    assert(lines.values.size() == 1);
    assert(lines.values[0].size() == buddy::JsonLineAssembler::kMaxLineLength);
}

}  // namespace

int main()
{
    test_split_line();
    test_packed_lines();
    test_crlf();
    test_oversized_line_is_dropped();
    test_max_length_line_is_allowed();
    return 0;
}
