#include "JsonLineAssembler.hpp"

namespace buddy {

void JsonLineAssembler::reset()
{
    len_ = 0;
    discarding_ = false;
    line_[0] = '\0';
}

JsonLineAssembler::FeedStats JsonLineAssembler::feed(const uint8_t *data,
                                                     uint16_t len,
                                                     LineCallback callback,
                                                     void *context)
{
    FeedStats stats;

    if (data == nullptr || len == 0)
    {
        return stats;
    }

    for (uint16_t i = 0; i < len; ++i)
    {
        const char ch = static_cast<char>(data[i]);

        if (discarding_)
        {
            if (ch == '\n')
            {
                reset();
            }
            continue;
        }

        if (ch == '\n')
        {
            finish_line(callback, context, stats);
            continue;
        }

        if (len_ >= kMaxLineLength)
        {
            mark_overflow(stats);
            continue;
        }

        line_[len_] = ch;
        ++len_;
    }

    return stats;
}

uint16_t JsonLineAssembler::pending_length() const
{
    return len_;
}

uint32_t JsonLineAssembler::total_lines() const
{
    return total_lines_;
}

uint32_t JsonLineAssembler::total_overflowed() const
{
    return total_overflowed_;
}

bool JsonLineAssembler::discarding() const
{
    return discarding_;
}

void JsonLineAssembler::finish_line(LineCallback callback, void *context, FeedStats &stats)
{
    uint16_t line_len = len_;

    if (line_len > 0 && line_[line_len - 1] == '\r')
    {
        --line_len;
    }

    line_[line_len] = '\0';

    if (callback != nullptr)
    {
        callback(line_, line_len, context);
    }

    ++stats.lines;
    ++total_lines_;
    reset();
}

void JsonLineAssembler::mark_overflow(FeedStats &stats)
{
    len_ = 0;
    line_[0] = '\0';
    discarding_ = true;
    ++stats.overflowed;
    ++total_overflowed_;
}

}  // namespace buddy
