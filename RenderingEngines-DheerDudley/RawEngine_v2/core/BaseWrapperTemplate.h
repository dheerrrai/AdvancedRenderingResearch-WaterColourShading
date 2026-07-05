//
// Created by dheer on 13-03-2026.
//

#ifndef RAWENGINE_BASEWRAPPERTEMPLATE_H
#define RAWENGINE_BASEWRAPPERTEMPLATE_H

#pragma once
#include <map>
#include <string>

template<typename T>
class BaseWrapperTemplate
{
public:
    void add(const std::string& name, const T& resource)
    {
        resources[name] = resource;
    }

    const T& get(const std::string& name) const
    {
        return resources.at(name);
    }

private:
    std::map<std::string, T> resources;
};
#endif //RAWENGINE_BASEWRAPPERTEMPLATE_H