/*
 * ModuleResource.h
 *
 * Copyright (C) 2020 by VISUS (Universitaet Stuttgart).
 * Alle Rechte vorbehalten.
 */

#pragma once

#include <string>
#include <any>
#include <functional>
#include <optional>

namespace megamol {
namespace render_api {


class ModuleResource {
public:
    ModuleResource()
		: identifier{""}
		, resource{}
	{}

    template <typename T>
    ModuleResource(const char* identifier, const T& resource) : ModuleResource(std::string{identifier}, resource) {}

    template <typename T>
    ModuleResource(const std::string& identifier, const T& resource)
        : identifier{identifier}, resource{std::reference_wrapper<const T>(resource)} {}

    const std::string& getIdentifier() const { return identifier; }

    template <typename T> void setResource(const T& resource) {
        this->resource = std::reference_wrapper<const T>(resource);
    }

    template <typename T> T const& getResource() const {
        //try {
            return std::any_cast<std::reference_wrapper<const T>>(resource).get();
        //} catch (const std::bad_any_cast& e) {
        //    return std::nullopt;
        //}
    }

private:
    std::string identifier;
    std::any resource;
};


} /* end namespace render_api */

} /* end namespace megamol */

