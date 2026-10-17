#pragma once

#include <cstddef>
#include <ratio> // ratio<分子，分母>记录的是比例，分子 : 分母

namespace logger {

// Rep是数值类型Representation，比如说size_t，Period是一个ratio类型，记录比例
template <typename Rep, typename Period = std::ratio<1>>
class space;

// ToSpace是目标容量类型，比如bytes, Rep和Period是源容量的数值类型和比例
template <typename ToSpace, typename Rep, typename Period>
constexpr ToSpace space_cast(const space<Rep, Period>& value) {
    // factor也是一个ratio，记录着源单位到目标单位的比例；比如，kilobytes转到bytes就是1024 : 1
    using factor = std::ratio_divide<Period, typename ToSpace::period>;
    const auto count = static_cast<typename ToSpace::rep>(value.count() * factor::num / factor::den);
    return ToSpace(count);
}

template <typename Rep, typename Period>
class space {
public:
    using rep = Rep;
    using period = Period;

    constexpr space() : rep_() {}

    // megabytes m{4}调用下面模板函数，其中Rep is size_t, Rep2 is int, count = 4;
    template <typename Rep2>
    explicit constexpr space(const Rep2& count) : rep_(count) {}

    // logger::kilobytes k{4};
    // logger::bytes b = k;
    // b调用下面模板函数，b.count() = 4096
    template <typename Rep2, typename Period2>
    constexpr space(const space<Rep2, Period2>& other) : rep_(space_cast<space>(other).count()) {}

    constexpr Rep count() const { return rep_; }

private:
    // rep_都是以模板参数Rep为单位的；一般Rep都是size_t，即4个字节
    Rep rep_;
};

using kilo = std::ratio<1024L, 1>;
using mega = std::ratio<1024L * 1024, 1>;
using giga = std::ratio<1024L * 1024 * 1024, 1>;
using tera = std::ratio<1024L * 1024 * 1024 * 1024, 1>;

using bytes = space<size_t>;
using kilobytes = space<size_t, kilo>;
using megabytes = space<size_t, mega>;
using gigabytes = space<size_t, giga>;
using terabytes = space<size_t, tera>;


// bytes b{1024}; 相当于space<size_t> b{1024};
// megabytes m{4}; 相当于space<size_t, mega> m{4};

}