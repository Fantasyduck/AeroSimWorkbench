#pragma once

#include <iostream>

namespace aerosim_test {

class TestContext {
public:
    void check(bool condition, const char *expression, const char *file, int line)
    {
        if (condition)
            return;
        ++failures_;
        std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
    }

    int result() const { return failures_ == 0 ? 0 : 1; }

private:
    int failures_ = 0;
};

} // namespace aerosim_test

#define AEROSIM_CHECK(context, expression) \
    (context).check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
