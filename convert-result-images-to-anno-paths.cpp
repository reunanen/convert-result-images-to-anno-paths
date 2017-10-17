#include <dlib/dir_nav/dir_nav_extensions.h>
#include <opencv2/imgcodecs/imgcodecs.hpp>

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

    const std::vector<dlib::file> files = dlib::get_files_in_directory_tree(argv[1], dlib::match_ending(result_image_suffix));

    std::cout << "Searching for result images..." << std::endl;
    std::cout << "Converting " << files.size() << " result images..." << std::endl;

    cv::Vec3b background_color(0, 0, 0);

    for (const auto& file : files) {
        std::cout << "Converting " << file.full_name();

        const cv::Mat result_image = cv::imread(file.full_name());

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

        std::cout << ": " << colors.size() << " distinct classes";


        std::cout << " - Done!" << std::endl;
    }
}
