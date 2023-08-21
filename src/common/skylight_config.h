#pragma once
#include <tomlcpp.hpp>
#include <fmt/core.h>
#include "spdlog/spdlog.h"
#include <filesystem>

namespace skylight {

    std::shared_ptr<toml::Table> GetConfig(std::string filename,
                                           std::vector<std::filesystem::path> configSearchPaths = {"./", "../", "/usr/local/etc/"})
    {


        for (std::filesystem::path &searchPath : configSearchPaths){
            std::filesystem::path filePath = searchPath / filename;
            toml::Result result = toml::parseFile(filePath.string());
            if (result.table){
                spdlog::info("loaded {}", filePath.string());
                return result.table;
            }
        }

        return std::shared_ptr<toml::Table>();
    }
}

