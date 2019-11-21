
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_ENUM_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_ENUM_HPP_


#include <type_traits> // for underlying_type<>

#include <gsl/gsl-lite.hpp> // for gsl_NODISCARD


#define SYSMAKESHIFT_DEFINE_ENUM_BITMASK_OPERATORS(ENUM)                                    \
    gsl_NODISCARD constexpr ENUM                                                            \
    operator ~(ENUM val) noexcept                                                           \
    {                                                                                       \
        return ENUM(~std::underlying_type_t<ENUM>(val));                                    \
    }                                                                                       \
    gsl_NODISCARD constexpr ENUM                                                            \
    operator |(ENUM lhs, ENUM rhs) noexcept                                                 \
    {                                                                                       \
        return ENUM(std::underlying_type_t<ENUM>(lhs) | std::underlying_type_t<ENUM>(rhs)); \
    }                                                                                       \
    gsl_NODISCARD constexpr ENUM                                                            \
    operator &(ENUM lhs, ENUM rhs) noexcept                                                 \
    {                                                                                       \
        return ENUM(std::underlying_type_t<ENUM>(lhs) & std::underlying_type_t<ENUM>(rhs)); \
    }                                                                                       \
    gsl_NODISCARD constexpr ENUM                                                            \
    operator ^(ENUM lhs, ENUM rhs) noexcept                                                 \
    {                                                                                       \
        return ENUM(std::underlying_type_t<ENUM>(lhs) ^ std::underlying_type_t<ENUM>(rhs)); \
    }                                                                                       \
    constexpr ENUM&                                                                         \
    operator |=(ENUM& lhs, ENUM rhs) noexcept                                               \
    {                                                                                       \
        lhs = lhs | rhs;                                                                    \
        return lhs;                                                                         \
    }                                                                                       \
    constexpr ENUM&                                                                         \
    operator &=(ENUM& lhs, ENUM rhs) noexcept                                               \
    {                                                                                       \
        lhs = lhs & rhs;                                                                    \
        return lhs;                                                                         \
    }                                                                                       \
    constexpr ENUM&                                                                         \
    operator ^=(ENUM& lhs, ENUM rhs) noexcept                                               \
    {                                                                                       \
        lhs = lhs ^ rhs;                                                                    \
        return lhs;                                                                         \
    }


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_ENUM_HPP_
