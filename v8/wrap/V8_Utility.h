/*
 *
 *  Created on: 2016年1月23日
 *      Author: zhangyalei
 */

#ifndef V8_UTILITY_H_
#define V8_UTILITY_H_

#include <tuple>
#include <functional>

namespace v8_wrap { namespace v8_detail {

template<typename T>
struct tuple_tail;

template<typename Head, typename... Tail>
struct tuple_tail<std::tuple<Head, Tail...>>
{
	using type = std::tuple<Tail... >;
};

/////////////////////////////////////////////////////////////////////////////
//
// Function traits
//
template<typename F>
struct function_traits;

template<typename R, typename ...Args>
struct function_traits<R (Args...)>
{
	using return_type = R;
	using arguments = std::tuple<Args...>;
};

// function pointer
template<typename R, typename ...Args>
struct function_traits<R (*)(Args...)> : function_traits<R (Args...)> {};

// member function pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::*)(Args...)> : function_traits<R (C&, Args...)>
{
};

// const member function pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R (C::*)(Args...) const> : function_traits<R (C const&, Args...)>
{
};

// volatile member function pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R(C::*)(Args...) volatile> : function_traits<R(C volatile&, Args...)>
{
};

// const volatile member function pointer
template<typename C, typename R, typename ...Args>
struct function_traits<R(C::*)(Args...) const volatile> : function_traits<R(C const volatile&, Args...)>
{
};

// member object pointer
template<typename C, typename R>
struct function_traits<R (C::*)> : function_traits<R (C&)>
{
};

// function object, std::function, lambda
template<typename F>
struct function_traits
{
	static_assert(!std::is_bind_expression<F>::value, "std::bind result is not supported yet");
private:
	using callable_traits = function_traits<decltype(&F::operator())>;
public:
	using return_type = typename callable_traits::return_type;
	using arguments = typename tuple_tail<typename callable_traits::arguments>::type;
};

template<typename F>
struct function_traits<F&> : function_traits<F> {};

template<typename F>
struct function_traits<F&&> : function_traits<F> {};

/////////////////////////////////////////////////////////////////////////////
//
// integer_sequence
//
template<typename T, T... I>
struct integer_sequence
{
	using type = T;
	static size_t size() { return sizeof...(I); }

	template<T N>
	using append = integer_sequence<T, I..., N>;

	using next = append<sizeof...(I)>;
};

template<typename T, T Index, size_t N>
struct sequence_generator
{
	using type = typename sequence_generator<T, Index - 1, N - 1>::type::next;
};

template<typename T, T Index>
struct sequence_generator<T, Index, 0ul>
{
	using type = integer_sequence<T>;
};

template<size_t... I>
using index_sequence = integer_sequence<size_t, I...>;

template<typename T, T N>
using make_integer_sequence = typename sequence_generator<T, N, N>::type;

template<size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;

/////////////////////////////////////////////////////////////////////////////
//
// apply_tuple
//
template<typename F, typename Tuple, size_t... Indices>
typename function_traits<F>::return_type apply_impl(F&& f, Tuple&& t, index_sequence<Indices...>)
{
	return std::forward<F>(f)(std::get<Indices>(std::forward<Tuple>(t))...);
}

template<typename F, typename Tuple>
typename function_traits<F>::return_type apply_tuple(F&& f, Tuple&& t)
{
	using Indices = make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>;
	return apply_impl(std::forward<F>(f), std::forward<Tuple>(t), Indices{});
}

template<typename F, typename ...Args>
typename function_traits<F>::return_type apply(F&& f, Args&&... args)
{
	return std::forward<F>(f)(std::forward<Args>(args)...);
}

}} // namespace v8_wrap::v8_detail

#endif // V8_UTILITY_H_
