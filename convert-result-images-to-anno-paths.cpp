#include <dlib/dir_nav/dir_nav_extensions.h>

#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <rapidjson/prettywriter.h>

class compare_color
{
public:
    bool operator()(const cv::Vec4b& color1, const cv::Vec4b& color2)
    {
        const auto color_tuple = [](const cv::Vec4b& color) {
            return std::make_tuple(color[0], color[1], color[2], color[3]);
        };

        return color_tuple(color1) < color_tuple(color2);
    }
};

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << std::endl;
        std::cout << "> convert-result-images-to-anno-paths /path/to/result/data" << std::endl;
        return 1;
    }

    const std::string result_image_suffix = "_result.png";
    const std::string result_path_suffix = "_result_path.json";

    std::cout << "Searching for result images..." << std::endl;

    const std::vector<dlib::file> files = dlib::get_files_in_directory_tree(argv[1], dlib::match_ending(result_image_suffix));

    std::cout << "Converting " << files.size() << " result images..." << std::endl;

    cv::Vec4b clean_color(0, 255, 0, 64);

    for (const auto& file : files) {

        const std::string& full_name = file.full_name();

        std::cout << "Converting " << full_name;

        const cv::Mat result_image = cv::imread(full_name, cv::IMREAD_UNCHANGED);

        std::cout
            << ", width = " << result_image.cols
            << ", height = " << result_image.rows
            << ", channels = " << result_image.channels()
            << ", type = 0x" << std::hex << result_image.type();

        if (result_image.channels() != 4) {
            std::cerr << std::endl << " - Expecting 4 channels: skipping..." << std::endl;
            continue;
        }

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

        std::cout << ", " << colors.size() << " distinct classes:" << std::endl;

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

        writer.StartArray();

        cv::Mat color_result;
        for (const cv::Vec4b& color : colors) {
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

            std::cout << " - 0x" << std::hex 
                << std::setw(2) << std::setfill('0') << static_cast<int>(color[2])
                << std::setw(2) << std::setfill('0') << static_cast<int>(color[1])
                << std::setw(2) << std::setfill('0') << static_cast<int>(color[0])
                << std::setw(2) << std::setfill('0') << static_cast<int>(color[3])
                << ": " << std::dec;

            cv::inRange(result_image, color, color, color_result);

            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(color_result, contours, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

            std::cout << contours.size() << " paths" << std::endl;
            
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

        const size_t base_pos = full_name.length() - result_image_suffix.length();
        assert(base_pos < full_name.length()); // check that we didn't just wrap
        assert(full_name.substr(base_pos) == result_image_suffix);
        const std::string path_filename = full_name.substr(0, base_pos) + result_path_suffix;

        std::ofstream out(path_filename);
        out << buffer.GetString();
    }
}
