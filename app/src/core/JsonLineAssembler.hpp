#ifndef BUDDY_JSON_LINE_ASSEMBLER_HPP
#define BUDDY_JSON_LINE_ASSEMBLER_HPP

#include <stddef.h>
#include <stdint.h>

namespace buddy {

class JsonLineAssembler final {
public:
    static constexpr uint16_t kMaxLineLength = 4096;

    typedef void (*LineCallback)(const char *line, uint16_t len, void *context);

    struct FeedStats {
        uint16_t lines = 0;
        uint16_t overflowed = 0;
    };

    void reset();
    FeedStats feed(const uint8_t *data, uint16_t len, LineCallback callback, void *context);

    uint16_t pending_length() const;
    uint32_t total_lines() const;
    uint32_t total_overflowed() const;
    bool discarding() const;

private:
    void finish_line(LineCallback callback, void *context, FeedStats &stats);
    void mark_overflow(FeedStats &stats);

    char line_[kMaxLineLength + 1] = {};
    uint16_t len_ = 0;
    uint32_t total_lines_ = 0;
    uint32_t total_overflowed_ = 0;
    bool discarding_ = false;
};

}  // namespace buddy

#endif
