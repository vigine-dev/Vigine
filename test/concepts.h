#pragma once

#include <type_traits>

template <typename T>
concept IsAbstract = std::is_abstract_v<T>;

template <typename T>
concept HasMethod_destructor = std::is_destructible_v<T>;

template <typename T>
concept HasMethod_operatorFunctor = requires(T t)
{
  requires std::is_member_function_pointer_v<decltype(&T::operator())>;
  { t() } -> std::convertible_to<void>;
}; 