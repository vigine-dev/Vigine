#pragma once

#include <type_traits>

// Concept for checking if a type is abstract
template <typename T>
concept IsAbstract = std::is_abstract_v<T>;

// Concept for checking destructor
template <typename T>
concept HasMethod_destructor = requires(T t)
{
    { t.~T() } -> std::same_as<void>;
};

// Concept for checking operator()
template <typename T, typename R>
concept HasMethod_operator = requires(T t)
{
    { t() } -> std::same_as<R>;
};

// Concept for checking update method
template <typename T, typename R>
concept HasMethod_update = requires(T t)
{
    { t.update() } -> std::same_as<R>;
};

// Concept for checking execute method
template <typename T, typename R>
concept HasMethod_execute = requires(T t)
{
    { t.execute() } -> std::same_as<R>;
};

// Concept for checking operator== method
template <typename T, typename R, typename Other>
concept HasMethod_operatorEqual = requires(T t, const Other& other)
{
    { t.operator==(other) } -> std::same_as<R>;
};

// Concept for checking operator!= method
template <typename T, typename R, typename Other>
concept HasMethod_operatorNotEqual = requires(T t, const Other& other)
{
    { t.operator!=(other) } -> std::same_as<R>;
};

// Concept for checking enter method
template <typename T, typename R>
concept HasMethod_enter = requires(T t)
{
    { t.enter() } -> std::same_as<R>;
};

// Concept for checking exit method
template <typename T, typename R>
concept HasMethod_exit = requires(T t)
{
    { t.exit() } -> std::same_as<R>;
};

// Concept for checking isActive method
template <typename T, typename R>
concept HasMethod_isActive = requires(T t)
{
    { t.isActive() } -> std::same_as<R>;
};

// Concept for checking getExitStatus method
template <typename T, typename R>
concept HasMethod_getExitStatus = requires(T t)
{
    { t.getExitStatus() } -> std::same_as<R>;
}; 
