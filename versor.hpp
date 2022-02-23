#ifndef INCLUDE_VERSOR_HPP_E4DD574F_5FD6_4161_B921_D2A8BB3A6106
#define INCLUDE_VERSOR_HPP_E4DD574F_5FD6_4161_B921_D2A8BB3A6106

#include <iosfwd>
#include <iomanip>
#include <algorithm>
#include <cstdint>

inline namespace versor
{

template <class T> constexpr T min = std::numeric_limits<T>::min();
template <class T> constexpr T max = std::numeric_limits<T>::max();

constexpr auto clamp(auto x, auto a, auto b) {
    return std::min<decltype (x)>(std::max<decltype (x)>(x, a), b);
}
template <class T>
constexpr T clamp(auto x) {
    return clamp(x, min<T>, max<T>);
}

template <class T, size_t N>
struct versor : versor<T, N-1> {
public:
    using CAR = T;
    using CDR = versor<T, N-1>;

public:
    constexpr versor() noexcept : CDR{CAR{}}, car{CAR{}}
    {
    }
    constexpr versor(CAR car) noexcept : CDR{car}, car{car}
    {
    }
    constexpr versor(CAR car, auto... cdr) noexcept : CDR{cdr...}, car{car}
    {
    }
    constexpr versor(auto car) noexcept : CDR{car}, car{clamp<CAR>(car)}
    {
    }
    constexpr versor(auto car, auto... cdr) noexcept : CDR{cdr...}, car{clamp<CAR>(car)}
    {
    }

public:
    auto& rest() noexcept { return *static_cast<CDR*>(this); }

public:
    constexpr auto& operator += (versor rhs) noexcept {
        this->rest() += rhs.rest();
        // NOTE: Assumes a temporary promotion to a signed integer.
        this->car = clamp<CAR>(this->car + rhs.car);
        return *this;
    }
    constexpr auto& operator -= (versor rhs) noexcept {
        this->rest() -= rhs.rest();
        // NOTE: Assumes a temporary promotion to a signed integer.
        this->car = clamp<CAR>(this->car - rhs.car);
        return *this;
    }
    friend constexpr auto operator + (versor lhs, versor rhs) noexcept {
        return versor(lhs) += rhs;
    }
    friend constexpr auto operator - (versor lhs, versor rhs) noexcept {
        return versor(lhs) -= rhs;
    }

private:
    CAR car;
};
template <class T>
struct versor<T, 0> {
    constexpr versor(auto) noexcept { }
    constexpr auto& operator += (versor) noexcept { return *this; }
    constexpr auto& operator -= (versor) noexcept { return *this; }
};

using color = versor<uint8_t, 4>;

template <class Ch, class T, size_t N>
auto& operator << (std::basic_ostream<Ch>& output, versor<T, N> v) {
    return output;
}
template <class Ch>
auto& operator << (std::basic_ostream<Ch>& output, color v) {
    auto prevfill = output.fill('0');
    auto prevflag = output.setf(std::ios_base::hex, std::ios_base::basefield);
    output << std::setw(8) << reinterpret_cast<uint32_t&>(v);
    output.setf(prevflag);
    output.fill(prevfill);
    return output;
}

} // end of namespace versor

#endif/*INCLUDE_VERSOR_HPP_E4DD574F_5FD6_4161_B921_D2A8BB3A6106*/
