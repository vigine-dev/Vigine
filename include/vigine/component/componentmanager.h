#pragma once

#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include <any>

namespace vigine {

template<typename T>
using ComponentSPtr = std::shared_ptr<T>;
template<typename T>
using ComponentContainer = std::vector<ComponentSPtr<T>>;

class ComponentManager 
{
public:
    ComponentManager();

    template <typename T, typename... Args>
    ComponentSPtr<T> createComponent(Args&&... args)
    {
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        auto typeIndex = std::type_index(typeid(T));

        if (_components.find(typeIndex) == _components.end())
            _components[typeIndex] = std::make_any<ComponentContainer<T>>();

        auto& container = std::any_cast<ComponentContainer<T>&>(_components[typeIndex]);
        container.push_back(std::move(component));

        return container.back();
    }
    
    template <typename T>
    auto cbegin()
    {
        auto typeIndex = std::type_index(typeid(T));

        auto it = _components.find(typeIndex);
        if (it != _components.end())
            return std::any_cast<ComponentContainer<T>&>(it->second).cbegin();

        return getOrCreateEmptyContainer<T>().cbegin();
    }

    template <typename T>
    auto cend()
    {
        auto typeIndex = std::type_index(typeid(T));

        auto it = _components.find(typeIndex);
        if (it != _components.end())
            return std::any_cast<ComponentContainer<T>&>(it->second).cend();

        return getOrCreateEmptyContainer<T>().cend();
    }

    template <typename T>
    void removeComponents()
    {
        _components.erase(typeid(T));
    }

    void clear()
    {
        _components.clear();
        _empty.clear();
    }

private:
    template <typename T>
    ComponentContainer<T>& getOrCreateEmptyContainer()
    {
        auto typeIndex = std::type_index(typeid(T));

        auto itEmpty = _empty.find(typeIndex);
        if (itEmpty != _empty.end())
            return std::any_cast<ComponentContainer<T>&>(itEmpty->second);

        _empty[typeIndex] = std::move(std::make_any<ComponentContainer<T>>());

        return std::any_cast<ComponentContainer<T>&>(_empty[typeIndex]);
    }

private:
    std::unordered_map<std::type_index, std::any> _components;
    std::unordered_map<std::type_index, std::any> _empty;
};

} // namespace vigine
