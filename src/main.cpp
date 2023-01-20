/**
 * @author Cai Heran (Tsai.love.dev@outlook.com)
 * @brief Search the given directory for duplicate regular files.
 *
 * The standard output will look like:
以下是空文件：
...

空文件：xx
总文件：xxx

 #1 (n1) xxxB
...
...

 #2 (n2) xxxxB
...
......

Done.
 */

#include "headers.hpp"

using std::endl;
using std::format;

namespace fs = std::filesystem;
namespace ranges = std::ranges;
namespace views = std::views;

#ifdef __WINDOWS__
# define myout std::wcout
# define _T(x) L ## x
#else
# define myout std::cout
# define _T(x) x
#endif


/**
 * @brief Calculates the 128-bit hash of @p file using XXH3_128bits.
 * 
 * If the file is not larger than 4 MiB, digest the whole file;
 * otherwise, digest the first 2 MiB and the last 2 MiB.
 *
 * xxHash is an extremely fast non-cryptographic hash algorithm.
 * The latest variant, XXH3, offers improved performance across the board,
 * especially on small data.
 */
template <class File>
xxh::hash128_t xxhash3_128_file(File&& file)
{
    constexpr std::streamsize buffersize{1<<22}; // 4 MiB
    thread_local auto buffer = std::make_unique_for_overwrite<char[]>(buffersize);

    std::ifstream fin(file, std::ios_base::binary);

    if (fs::file_size(file)<=buffersize)
    {
        fin.read(buffer.get(), buffersize);
        return xxh::xxhash3<128>(buffer.get(), fin.gcount());
    }
    else {
        constexpr auto halfsize{buffersize/2};
        fin.read(buffer.get(), halfsize);
        fin.seekg(-halfsize, std::ios_base::end);
        fin.read(buffer.get()+halfsize, halfsize);
        return xxh::xxhash3<128>(buffer.get(), buffersize);
    }
}

/**
 * @brief Search @p dir recursively for all regular files, sorted by size.
 *
 * @return a pair of the numbers of non-empty files ans all regular files.
 */
template <class Container>
auto search(const fs::path dir, Container& size_map)
{
    std::size_t tot=0, empty=0;

    myout << _T("以下是空文件：\n");
    for (const auto& entry: fs::recursive_directory_iterator(dir))
    if (entry.is_regular_file())
    {
        tot++;
        auto& path_str = entry.path().native();
        if (!entry.file_size()) {
            empty++;
            myout << path_str << endl;
        }
        else {
            size_map[entry.file_size()].emplace_back(path_str);
        }
    }

    myout << format(_T("\n空文件：{}\n总文件：{}\n"), empty, tot) << endl;

    return std::make_pair(tot-empty, tot);
}

/**
 * @brief Hash all files in @p paths and sort them by hash value.
 */
template<ranges::input_range Iterable, class Container>
void hash_check(Iterable&& paths, Container& xxh_map)
{
    for (auto&& path: paths) {
        auto hash = xxhash3_128_file(path);
        xxh_map[hash].emplace_back(std::move(path));
    }
}

/**
 * @brief Search @p dirpath for duplicate files.
 * 
 * Algorithm:
 * 1. Search @p dipath recursively for all regular files.
 * 2. Group files by size, with empty files directly output.
 * 3. For each group of multiple files, group them by hash value.
 * 4. If an ultimate group is multiple, output it.
 */
void duplicate_file_search(const fs::path dirpath)
{
    using path_string = fs::path::string_type;

    std::map<std::uint64_t, std::vector<path_string>> size_map;
    search(dirpath, size_map);
    std::size_t num=0;

    for (auto&& pair: size_map)
    {
        const auto filesize = pair.first;
        auto paths = std::move(pair.second);
        if (paths.size() <= 1)
            continue;

        std::map<xxh::hash128_t, std::vector<path_string>> xxh_map;
        hash_check(std::move(paths), xxh_map);
        for (auto&& paths: xxh_map | views::values)
        if (paths.size()>=2)
        {
            num++;
            ranges::sort(paths);
            myout << format(_T(" #{} ({}) {}B\n"), num, paths.size(), filesize);
            for (const auto& p: paths)
                (myout << p).put('\n');
            myout << endl;
        }
    }

    myout << _T("Done.\n");
}

int main(int argc, char *argv[])
{
    std::locale::global(std::locale("zh_CN"));
    myout.imbue(std::locale("zh_CN"));

    std::ios_base::sync_with_stdio(false);

    fs::path dir;

    if (argc <= 1) {
        dir = ".";
    }
    else {
        dir = argv[1];
        if (!fs::exists(dir) || !fs::is_directory(dir))
        {
            myout << _T("No such directory.\n");
            return 0;
        }
    }

    try { duplicate_file_search(dir); }
    catch (const fs::filesystem_error& fs_err) {
        std::cerr << "Filesystem Exception: " << fs_err.what() << endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << endl;
    }
}