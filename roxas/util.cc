/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <chrono>   // for system_clock
#include <ctime>    // for localtime, time_t, tm
#include <fstream>  // for basic_ifstream
#include <iomanip>  // for operator<<, put_time
#include <iostream> // for cout
#include <roxas/util.h>
struct tm;

namespace roxas {

namespace util {

/**
 * @brief read source file
 *
 * @param path path to file
 * @return std::string
 */
std::string read_file_from_path(fs::path path)
{
    std::ifstream f(path, std::ios::in | std::ios::binary);
    const auto sz = fs::file_size(path);

    std::string result(sz, '\0');
    f.read(result.data(), sz);

    return result;
}

/**
 * @brief log function
 *
 * @param level Logging log level
 * @param message log message
 */
void log(Logging level, std::string_view message)
{
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

    std::tm* localTime = std::localtime(&currentTime);
    std::cout << "[" << std::put_time(localTime, "%Y-%m-%d %H:%M:%S") << "] ";

    switch (level) {
        case Logging::INFO:
#ifndef DEBUG
            return;
#endif
            std::cout << "[INFO] " << message << std::endl;
            break;
        case Logging::WARNING:
            std::cout << "[WARNING] " << message << std::endl;
            break;
        case Logging::ERROR:
            std::cout << "[ERROR] " << message << std::endl;
            break;
    }
}

} // namespace util

} // namespace roxas
