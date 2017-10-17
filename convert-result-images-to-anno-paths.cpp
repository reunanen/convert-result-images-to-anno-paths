#include <dlib/dir_nav/dir_nav_extensions.h>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << std::endl;
        std::cout << "> convert-result-images-to-anno-paths /path/to/result/data" << std::endl;
        return 1;
    }

    const std::vector<dlib::file> files = dlib::get_files_in_directory_tree(argv[1], dlib::match_ending("_result.png"));

    std::cout << "Searching for result images..." << std::endl;
    std::cout << "Converting " << files.size() << " result images..." << std::endl;
}
