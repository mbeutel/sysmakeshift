
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_TYPE_TRAITS_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_TYPE_TRAITS_HPP_


#include <gsl-lite/gsl-lite.hpp> // for void_t<>

#include <type_traits> // for integral_constant<>


namespace sysmakeshift {


namespace gsl = ::gsl_lite;


namespace detail {


template <template <typename...> class, typename, typename...> struct can_instantiate_ : std::false_type { };
template <template <typename...> class Z, typename... Ts> struct can_instantiate_<Z, gsl::void_t<Z<Ts...>>, Ts...> : std::true_type { };
template <template <typename...> class Z, typename... Ts> struct can_instantiate : can_instantiate_<Z, void, Ts...> { };
template <template <typename...> class Z, typename... Ts> constexpr bool can_instantiate_v = can_instantiate<Z, Ts...>::value;


template <typename T> constexpr T static_const{ };


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_TYPE_TRAITS_HPP_
