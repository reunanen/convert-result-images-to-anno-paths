#include <dlib/dir_nav/dir_nav_extensions.h>
#include <opencv2/imgcodecs/imgcodecs.hpp>

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

    for (const auto& file : files) {
        std::cout << "Converting " << file.full_name();

        cv::Mat image = cv::imread(file.full_name(), cv::IMREAD_ANYDEPTH);

        std::cout << " (" << image.rows << " x " << image.cols << ")";



        std::cout << " - Done!" << std::endl;
    }
}
