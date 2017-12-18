#include "ProgramOptions.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>
#include <codecvt>
#include <regex>
#include "HelpException.h"

namespace po = boost::program_options;

Config helpers::parseCommandLine(int argc, const char* const argv[])
{
    Config config;
    std::string chars;
    std::string charsFile;
    std::string color;
    std::string backgroundColor;
    std::string dataFormat;

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message" )
        ("font-file,F", po::value<std::string>(&config.fontFile)->required(), "path to ttf file, required")
        ("chars", po::value<std::string>(&chars), "required characters, for example: 32-64,92,120-126\ndefault value is 32-127 if chars-file not defined")
        ("chars-file", po::value<std::string>(&charsFile), "optional path to UTF-8 text file with required characters (will be combined with chars)")
        ("color", po::value<std::string>(&color)->default_value("255,255,255"), "foreground RGB color, for example: 32,255,255, default value is 255,255,255")
        ("background-color", po::value<std::string>(&backgroundColor), "background color RGB color, for example: 0,0,128, transparent, if not exists")
        ("font-size,S", po::value<uint16_t>(&config.fontSize)->default_value(32), "font size, default value is 32")
        ("padding-up", po::value<int>(&config.padding.up)->default_value(0), "padding up, default valie is 0")
        ("padding-right", po::value<int>(&config.padding.right)->default_value(0), "padding right, default valie is 0")
        ("padding-down", po::value<int>(&config.padding.down)->default_value(0), "padding down, default valie is 0")
        ("padding-left", po::value<int>(&config.padding.left)->default_value(0), "padding left, default valie is 0")
        ("spacing-vert", po::value<int>(&config.spacing.ver)->default_value(0), "spacing vert, default valie is 0")
        ("spacing-horiz", po::value<int>(&config.spacing.hor)->default_value(0), "spacing horiz, default valie is 0")
        ("texture-width", po::value<uint32_t>(&config.textureSize.w)->default_value(256), "texture width, default valie is 256")
        ("texture-height", po::value<uint32_t>(&config.textureSize.h)->default_value(256), "texture height, default valie is 256")
        ("output,O", po::value<std::string>(&config.output)->required(), "output files name without extension, required")
        ("data-format", po::value<std::string>(&dataFormat)->default_value("txt"), "output data file format, \"xml\" or \"txt\", default \"xml\"")
        ("include-kerning-pairs", "include kerning pairs to output file");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        throw HelpException();
    }

    po::notify(vm);

    if (chars.empty() && charsFile.empty())
        chars = "32-127";
    config.chars = parseCharsString(chars);
    if (!charsFile.empty())
    {
        auto c = getCharsFromFile(charsFile);
        config.chars.insert(c.begin(), c.end());
    }

    config.color = parseColor(color);
    if (backgroundColor.empty())
        config.backgroundColor = boost::none;
    else
        config.backgroundColor = parseColor(backgroundColor);

    boost::algorithm::to_lower(dataFormat);
    if (dataFormat == "txt")
        config.dataFormat = Config::DataFormat::Text;
    else if (dataFormat == "xml")
        config.dataFormat = Config::DataFormat::Xml;
    else if (dataFormat == "bin")
        config.dataFormat = Config::DataFormat::Bin;
    else if (dataFormat == "json")
        config.dataFormat = Config::DataFormat::Json;
    else
        throw std::runtime_error("invalid data format");

    config.includeKerningPairs = (vm.count("include-kerning-pairs") > 0);

    //TODO: check values range

    return config;
}

std::set<uint32_t> helpers::parseCharsString(std::string str)
{
    // remove whitespace characters
    str.erase(std::remove_if(str.begin(), str.end(), std::bind( std::isspace<char>, std::placeholders::_1, std::locale::classic() )), str.end());

    if (str.empty())
        return std::set<uint32_t>();

    const std::regex re("^\\d{1,5}(-\\d{1,5})?(,\\d{1,5}(-\\d{1,5})?)*$");
    if (!std::regex_match(str, re))
        throw std::logic_error("invalid chars value");

    std::vector<std::string> ranges;
    boost::split(ranges, str, boost::is_any_of(","));

    std::vector<std::pair<uint32_t, uint32_t>> charList;
    for (auto range: ranges)
    {
        std::vector<std::string> minMaxStr;
        boost::split(minMaxStr, range, boost::is_any_of("-"));
        if (minMaxStr.size() == 1)
            minMaxStr.push_back(minMaxStr[0]);

        try
        {
            charList.emplace_back(boost::lexical_cast<uint16_t>(minMaxStr[0]),
                                boost::lexical_cast<uint16_t>(minMaxStr[1]));
        }
        catch(boost::bad_lexical_cast &)
        {
            throw std::logic_error("incorrect chars value (out of range)");
        }
    }

    std::set<uint32_t> result;
    for (auto range: charList)
    {
        //TODO: check too big result
        for (uint32_t v = range.first; v < range.second; ++v)
            result.insert(v);
        result.insert(range.second);
    }

    return result;
}

std::set<uint32_t> helpers::getCharsFromFile(const std::string& f)
{
    std::ifstream fs(f, std::ifstream::binary);
    if (!fs)
        throw std::runtime_error("can`t open characters file");
    std::string str((std::istreambuf_iterator<char>(fs)),
                    std::istreambuf_iterator<char>());

    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> cvt;
    std::u32string utf32str = cvt.from_bytes(str);

    std::set<uint32_t> result;
    for (auto c: utf32str)
        result.insert(static_cast<uint32_t>(c));
    return result;
}

Config::Color helpers::parseColor(const std::string& str)
{
    const std::regex e("^\\s*\\d{1,3}\\s*,\\s*\\d{1,3}\\s*,\\s*\\d{1,3}\\s*$");
    if (!std::regex_match(str, e))
        throw std::logic_error("invalid color");

    std::vector<std::string> rgbStr;
    boost::split(rgbStr, str, boost::is_any_of(","));

    auto colorToUint8 = [](const std::string& s)
    {
        int v = boost::lexical_cast<int>(boost::algorithm::trim_copy(s));
        if ((v < 0) || (v > 255))
            throw std::logic_error("invalid color");
        return static_cast<uint8_t>(v);
    };

    Config::Color color;
    color.r = colorToUint8(rgbStr[0]);
    color.g = colorToUint8(rgbStr[1]);
    color.b = colorToUint8(rgbStr[2]);

    return color;
}
