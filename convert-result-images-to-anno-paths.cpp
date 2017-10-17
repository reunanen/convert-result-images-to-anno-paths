#include <dlib/dir_nav/dir_nav_extensions.h>

#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

class compare_color
{
public:
    bool operator()(const cv::Vec3b& color1, const cv::Vec3b& color2)
    {
        const auto color_tuple = [](const cv::Vec3b& color) {
            return std::make_tuple(color[0], color[1], color[2]);
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

    const std::vector<dlib::file> files = dlib::get_files_in_directory_tree(argv[1], dlib::match_ending(result_image_suffix));

    std::cout << "Searching for result images..." << std::endl;
    std::cout << "Converting " << files.size() << " result images..." << std::endl;

    cv::Vec3b background_color(0, 0, 0);

    for (const auto& file : files) {

        const std::string& full_name = file.full_name();

        std::cout << "Converting " << full_name;

        const cv::Mat result_image = cv::imread(full_name);

        std::cout
            << ", width = " << result_image.cols
            << ", height = " << result_image.rows
            << ", channels = " << result_image.channels()
            << ", type = 0x" << std::hex << result_image.type();

        std::set<cv::Vec3b, compare_color> colors;

        for (int y = 0; y < result_image.rows; ++y) {
            const cv::Vec3b* row = result_image.ptr<cv::Vec3b>(y);
            for (int x = 0; x < result_image.cols; ++x) {
                const cv::Vec3b& color = row[x];
                if (color != background_color) {
                    colors.insert(color);
                }
            }
        }

        std::cout << ", " << colors.size() << " distinct classes:" << std::endl;

        const size_t base_pos = full_name.length() - result_image_suffix.length();
        assert(base_pos < full_name.length()); // check that we didn't just wrap
        assert(full_name.substr(base_pos) == result_image_suffix);
        const std::string path_filename = full_name.substr(0, base_pos) + result_path_suffix;

        std::ofstream out(path_filename);

        cv::Mat color_result;
        for (const cv::Vec3b& color : colors) {

            std::cout << " - 0x" << std::hex 
                << std::setw(2) << std::setfill('0') << static_cast<int>(color[2])
                << std::setw(2) << std::setfill('0') << static_cast<int>(color[1])
                << std::setw(2) << std::setfill('0') << static_cast<int>(color[0])
                << ": " << std::dec;

            cv::inRange(result_image, color, color, color_result);

            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(color_result, contours, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

            std::cout << contours.size() << " paths" << std::endl;
            
        }
    }
}
