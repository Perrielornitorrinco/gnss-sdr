/*!
 * \file geojson_printer.cc
 * \brief Implementation of a class that prints PVT solutions in GeoJSON format
 * \author Carles Fernandez-Prades, 2015. cfernandez(at)cttc.es
 *
 *
 * -----------------------------------------------------------------------------
 *
 * GNSS-SDR is a Global Navigation Satellite System software-defined receiver.
 * This file is part of GNSS-SDR.
 *
 * Copyright (C) 2010-2020  (see AUTHORS file for a list of contributors)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * -----------------------------------------------------------------------------
 */


#include "geojson_printer.h"
#include "pvt_solution.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <glog/logging.h>
#include <ctime>      // for tm
#include <exception>  // for exception
#include <iomanip>    // for operator<<
#include <iostream>   // for cout, cerr
#include <sstream>    // for stringstream

// clang-format off
#if HAS_STD_FILESYSTEM
#include <system_error>
namespace errorlib = std;
#if HAS_STD_FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#else
#include <boost/filesystem/operations.hpp>   // for create_directories, exists
#include <boost/filesystem/path.hpp>         // for path, operator<<
#include <boost/filesystem/path_traits.hpp>  // for filesystem
#include <boost/system/error_code.hpp>       // for error_code
namespace fs = boost::filesystem;
namespace errorlib = boost::system;
#endif
// clang-format on

GeoJSON_Printer::GeoJSON_Printer(const std::string& base_path)
{
    first_pos = true;
    geojson_base_path = base_path;
    fs::path full_path(fs::current_path());
    const fs::path p(geojson_base_path);
    if (!fs::exists(p))
        {
            std::string new_folder;
            for (auto& folder : fs::path(geojson_base_path))
                {
                    new_folder += folder.string();
                    errorlib::error_code ec;
                    if (!fs::exists(new_folder))
                        {
                            if (!fs::create_directory(new_folder, ec))
                                {
                                    std::cout << "Could not create the " << new_folder << " folder.\n";
                                    geojson_base_path = full_path.string();
                                }
                        }
                    new_folder += fs::path::preferred_separator;
                }
        }
    else
        {
            geojson_base_path = p.string();
        }
    if (geojson_base_path != ".")
        {
            std::cout << "GeoJSON files will be stored at " << geojson_base_path << '\n';
        }

    geojson_base_path = geojson_base_path + fs::path::preferred_separator;
}


GeoJSON_Printer::~GeoJSON_Printer()
{
    DLOG(INFO) << "GeoJSON printer destructor called.";
    try
        {
            GeoJSON_Printer::close_file();
        }
    catch (const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
}


bool GeoJSON_Printer::set_headers(const std::string& filename, bool time_tag_name)
{
    const boost::posix_time::ptime pt = boost::posix_time::second_clock::local_time();
    const tm timeinfo = boost::posix_time::to_tm(pt);

    if (time_tag_name)
        {
            std::stringstream strm0;
            const int year = timeinfo.tm_year - 100;
            strm0 << year;
            const int month = timeinfo.tm_mon + 1;
            if (month < 10)
                {
                    strm0 << "0";
                }
            strm0 << month;
            const int day = timeinfo.tm_mday;
            if (day < 10)
                {
                    strm0 << "0";
                }
            strm0 << day << "_";
            const int hour = timeinfo.tm_hour;
            if (hour < 10)
                {
                    strm0 << "0";
                }
            strm0 << hour;
            const int min = timeinfo.tm_min;
            if (min < 10)
                {
                    strm0 << "0";
                }
            strm0 << min;
            const int sec = timeinfo.tm_sec;
            if (sec < 10)
                {
                    strm0 << "0";
                }
            strm0 << sec;

            filename_ = filename + "_" + strm0.str() + ".geojson";
        }
    else
        {
            filename_ = filename + ".geojson";
        }
    filename_ = geojson_base_path + filename_;

    geojson_file.open(filename_.c_str());

    first_pos = true;
    if (geojson_file.is_open())
        {
            DLOG(INFO) << "GeoJSON printer writing on " << filename.c_str();

            // Set iostream numeric format and precision
            geojson_file.setf(geojson_file.std::ofstream::fixed, geojson_file.std::ofstream::floatfield);
            geojson_file << std::setprecision(14);

            // Writing the header
            geojson_file << "{\n";
            geojson_file << R"(  "type":  "Feature",)" << '\n';
            geojson_file << "  \"properties\": {\n";
            geojson_file << R"(       "name": "Locations generated by GNSS-SDR" )" << '\n';
            geojson_file << "   },\n";
            geojson_file << "  \"geometry\": {\n";
            geojson_file << R"(      "type": "MultiPoint",)" << '\n';
            geojson_file << "      \"coordinates\": [\n";

            return true;
        }

    std::cout << "File " << filename_ << " cannot be saved. Wrong permissions?\n";
    return false;
}


bool GeoJSON_Printer::print_position(const Pvt_Solution* const position, bool print_average_values)
{
    double latitude;
    double longitude;
    double height;

    if (print_average_values == false)
        {
            latitude = position->get_latitude();
            longitude = position->get_longitude();
            height = position->get_height();
        }
    else
        {
            latitude = position->get_avg_latitude();
            longitude = position->get_avg_longitude();
            height = position->get_avg_height();
        }

    if (geojson_file.is_open())
        {
            if (first_pos == true)
                {
                    geojson_file << "       [" << longitude << ", " << latitude << ", " << height << "]";
                    first_pos = false;
                }
            else
                {
                    geojson_file << ",\n";
                    geojson_file << "       [" << longitude << ", " << latitude << ", " << height << "]";
                }
            return true;
        }
    return false;
}


bool GeoJSON_Printer::close_file()
{
    if (geojson_file.is_open())
        {
            geojson_file << '\n';
            geojson_file << "       ]\n";
            geojson_file << "   }\n";
            geojson_file << "}\n";
            geojson_file.close();

            // if nothing is written, erase the file
            if (first_pos == true)
                {
                    errorlib::error_code ec;
                    if (!fs::remove(fs::path(filename_), ec))
                        {
                            LOG(INFO) << "Error deleting temporary file";
                        }
                }
            return true;
        }
    return false;
}
