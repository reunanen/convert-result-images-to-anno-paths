#include <dlib/dir_nav/dir_nav_extensions.h>

#include "cxxopts/include/cxxopts.hpp"

#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>

class compare_color
{
public:
    bool operator()(const cv::Vec4b& color1, const cv::Vec4b& color2) const
    {
        const auto color_tuple = [](const cv::Vec4b& color) {
            return std::make_tuple(color[0], color[1], color[2], color[3]);
        };

        return color_tuple(color1) < color_tuple(color2);
    }
};

std::map<cv::Vec4b, std::vector<std::vector<cv::Point>>, compare_color> read_existing_contours(const std::string& path_filename)
{
    std::map<cv::Vec4b, std::vector<std::vector<cv::Point>>, compare_color> contours_by_color;

    FILE* existingFile = fopen(path_filename.c_str(), "rb");
    if (existingFile) {
        try {
            std::cout << "Found previous file - appending..." << std::endl;
            char buffer[65536];
            rapidjson::FileReadStream stream(existingFile, buffer, sizeof(buffer));
            rapidjson::Document document;
            document.ParseStream<0, rapidjson::UTF8<>, rapidjson::FileReadStream>(stream);
            size_t counter = 0;
            if (document.IsArray()) {
                for (rapidjson::SizeType i = 0, end = document.Size(); i < end; ++i) {
                    const auto& item = document[i];
                    const auto color_member = item.FindMember("color");
                    const auto color_paths_member = item.FindMember("color_paths");
                    if (color_member != item.MemberEnd() && color_paths_member != item.MemberEnd()) {
                        const auto red_member = color_member->value.FindMember("r");
                        const auto green_member = color_member->value.FindMember("g");
                        const auto blue_member = color_member->value.FindMember("b");
                        const auto alpha_member = color_member->value.FindMember("a");
                        const auto end = color_member->value.MemberEnd();

                        if (red_member != end && green_member != end && blue_member != end) {
                            cv::Vec4b color;
                            const auto get_value = [](const auto& member) {
                                return member->value.IsInt()
                                    ? member->value.GetInt()
                                    : 255;
                            };
                            color[0] = get_value(blue_member);
                            color[1] = get_value(green_member);
                            color[2] = get_value(red_member);
                            color[3] = alpha_member != end ? get_value(alpha_member) : 255;

                            if (color_paths_member->value.IsArray()) {
                                for (rapidjson::SizeType j = 0, end = color_paths_member->value.Size(); j < end; ++j) {
                                    const auto& contour = color_paths_member->value[j];
                                    if (contour.IsArray()) {
                                        std::vector<cv::Point> result_contour;
                                        result_contour.reserve(contour.Size());
                                        for (rapidjson::SizeType k = 0, end = contour.Size(); k < end; ++k) {
                                            const auto& point = contour[k];
                                            const auto x_member = point.FindMember("x");
                                            const auto y_member = point.FindMember("y");
                                            if (x_member != point.MemberEnd() && y_member != point.MemberEnd()) {
                                                if (x_member->value.IsNumber() && y_member->value.IsNumber()) {
                                                    result_contour.emplace_back(
                                                        x_member->value.GetInt(),
                                                        y_member->value.GetInt()
                                                    );
                                                }
                                            }
                                        }
                                        if (!result_contour.empty()) {
                                            contours_by_color[color].push_back(result_contour);
                                            ++counter;
                                        }
                                    }
                                }
                            }

                            ++counter;
                        }
                    }
                }
            }
            std::cout << "Appending to " << counter << " previous contour" << (counter == 1 ? "" : "s") << std::endl;
        }
        catch (std::exception& e) {
            std::cerr << "Error reading existing file (" + path_filename + "): " << e.what() << std::endl;
        }
        fclose(existingFile);
    }

    return contours_by_color;
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        std::cout << "Usage: " << std::endl;
        std::cout << "> convert-result-images-to-anno-paths /path/to/result/data" << std::endl;
        return 1;
    }

    cxxopts::Options options("convert-result-images-to-anno-paths", "Convert result images to paths that can be loaded and shown in anno (see https://github.com/reunanen/anno)");

    options.add_options()
        ("i,input-directory", "Input image directory", cxxopts::value<std::string>())
        ("b,blur-amount", "Set the amount of blur", cxxopts::value<int>()->default_value("0"))
        ("a,append", "Append to existing files (if any)")
        ("v,verbose", "Be verbose")
        ;

    try {
        options.parse_positional("input-directory");
        options.parse(argc, argv);

        cxxopts::check_required(options, { "input-directory" });

        std::cout << "Input directory = " << options["input-directory"].as<std::string>() << std::endl;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << std::endl;
        std::cerr << options.help() << std::endl;
        return 2;
    }

    const std::string input_directory = options["input-directory"].as<std::string>();
    const std::string result_image_suffix = "_result.png";
    const std::string result_path_suffix = "_result_path.json";

    const int blur_amount = options["blur-amount"].as<int>();    

    std::cout << "Searching for result images..." << std::endl;

    const std::vector<dlib::file> files = dlib::get_files_in_directory_tree(input_directory, dlib::match_ending(result_image_suffix));

    std::cout << "Converting " << files.size() << " result images..." << std::endl;

    cv::Vec4b clean_color(0, 255, 0, 64);

    const bool verbose = options.count("verbose") > 0;

    size_t counter = 0;

    for (const auto& file : files) {

        const std::string& full_name = file.full_name();

        ++counter;
        if (verbose) {
            std::cout << "Converting " << full_name;
        }
        else {
            std::cout << "\r" << std::to_string(counter);
        }

        const cv::Mat result_image = cv::imread(full_name, cv::IMREAD_UNCHANGED);

        if (verbose) {
            std::cout
                << ", width = " << result_image.cols
                << ", height = " << result_image.rows
                << ", channels = " << result_image.channels()
                << ", type = 0x" << std::hex << result_image.type()
                << std::dec;
        }

        if (result_image.channels() != 4) {
            std::cerr << std::endl << " - Expecting 4 channels: skipping..." << std::endl;
            continue;
        }

        // Find distinct colors in the image
        std::set<cv::Vec4b, compare_color> colors;
        for (int y = 0; y < result_image.rows; ++y) {
            const cv::Vec4b* row = result_image.ptr<cv::Vec4b>(y);
            for (int x = 0; x < result_image.cols; ++x) {
                const cv::Vec4b& color = row[x];
                if (color != clean_color) {
                    colors.insert(color);
                }
            }
        }

        if (verbose) {
            std::cout << ", " << colors.size() << " distinct classes" << std::endl;
        }

        std::map<cv::Vec4b, std::vector<std::vector<cv::Point>>, compare_color> contours_by_color;

        const size_t base_pos = full_name.length() - result_image_suffix.length();
        assert(base_pos < full_name.length()); // check that we didn't just wrap
        assert(full_name.substr(base_pos) == result_image_suffix);
        const std::string path_filename = full_name.substr(0, base_pos) + result_path_suffix;

        if (options.count("append") > 0) {
            // Try to read contours from previously existing file (if any)
            contours_by_color = read_existing_contours(path_filename);
        }

        // Find contours in the image
        cv::Mat color_result;
        for (const cv::Vec4b& color : colors) {
            if (verbose) {
                std::cout << " - 0x" << std::hex
                    << std::setw(2) << std::setfill('0') << static_cast<int>(color[2])
                    << std::setw(2) << std::setfill('0') << static_cast<int>(color[1])
                    << std::setw(2) << std::setfill('0') << static_cast<int>(color[0])
                    << std::setw(2) << std::setfill('0') << static_cast<int>(color[3])
                    << ": " << std::dec;
            }

            cv::inRange(result_image, color, color, color_result);

            if (blur_amount > 0) {
                cv::blur(color_result, color_result, cv::Size(2 * blur_amount + 1, 2 * blur_amount + 1));
                color_result = color_result > 127.5;
            }

            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(color_result, contours, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

            if (verbose) {
                std::cout << contours.size() << " paths" << std::endl;
            }

            contours_by_color[color] = std::move(contours);
        }

        // Write JSON output
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

        writer.StartArray();

        for (const auto& item : contours_by_color) {
            const cv::Vec4b& color = item.first;
            const auto& contours = item.second;

            writer.StartObject();
            writer.String("color");
            writer.StartObject();
            {
                writer.String("r"); writer.Int(color[2]);
                writer.String("g"); writer.Int(color[1]);
                writer.String("b"); writer.Int(color[0]);
                writer.String("a"); writer.Int(color[3]);
            }
            writer.EndObject();

            writer.String("color_paths");
            writer.StartArray();
            
            for (const std::vector<cv::Point>& contour : contours) {
                writer.StartArray();
                for (const cv::Point& point : contour) {
                    writer.StartObject();
                    writer.String("x"); writer.Int(point.x);
                    writer.String("y"); writer.Int(point.y);
                    writer.EndObject();
                }
                writer.EndArray();
            }

            writer.EndArray();
            writer.EndObject();
        }

        writer.EndArray();

        std::ofstream out(path_filename);
        out << buffer.GetString();
    }
    
    std::cout << "\rDone                 " << std::endl;
}
