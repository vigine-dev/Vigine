#pragma once

/**
 * @file property.h
 * @brief Legacy lookup-mode enum used by registries / repositories.
 */

namespace vigine
{

/**
 * @brief Lookup policy for registry-style accessors.
 *
 * Exist         -- return the item if it already exists, else fail.
 * New           -- create a new item, fail if one already exists.
 * All           -- return all matching items.
 * NewIfNotExist -- return the existing item, or create a new one.
 */
enum class Property
{
    Exist,
    New,
    All,
    NewIfNotExist
};
}
