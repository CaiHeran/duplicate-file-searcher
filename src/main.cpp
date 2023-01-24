/**
 * @author Cai Heran (Tsai.love.dev@outlook.com)
 * @brief Search the given directory for duplicate regular files.
 *
 * The standard output will look like:
Empty file list:
...

Empty: xx
Total: xxx

 #1 (n1) xxx B
...
...

 #2 (n2) x xxx B
...
......

Done in x.xxxs.
 */

#include <iostream>
#include <fstream>
#include <format>
#include <filesystem>

#include <ranges>
#include <map>
#include <vector>

#include "xxhash.hpp"

namespace fs = std::filesystem;
namespace ranges = std::ranges;
namespace views = std::views;

using std::format;

#ifdef __WINDOWS__
# define myout std::wcout
# define _T(x) L ## x
#else
# define myout std::cout
# define _T(x) x
#endif


/**
 * @brief Group the same files in @p filelist into @p res.
 *
 * Algorithm:
 * In case of small files, directly hash the whole files.
 * Or else, hash the first 512 B and the last 512 B firstly,
 * and group files by hash value.
 * Then for each multiple group, hash the entire files and
 * group them by hash value.
 * Every ultimate multiple group is a result.
 */
template<ranges::input_range Iterable, class Container>
void hash_check(Iterable&& filelist, Container &res)
{
    constexpr std::size_t buffersize{1<<15}; // 32 KiB
    std::map<xxh::hash128_t, typename Container::value_type> map1, map2;
    char buffer[buffersize];

    if (fs::file_size(*filelist.cbegin()) <= buffersize)
    {
        for (auto&& file: filelist) {
            std::ifstream fin(file, std::ios_base::binary);
            fin.read(buffer, buffersize);
            auto hash = xxh::xxhash3<128>(buffer, fin.gcount());
            map1[hash].emplace_back(std::forward_like<Iterable>(file));
        }
        for (auto& files: map1 | views::values)
            if (files.size() > 1)
                res.emplace_back(std::move_if_noexcept(files));
        return;
    }

    for (auto&& file: filelist) {
        constexpr auto buffersize{1<<10}; // 1 KiB
        constexpr auto halfsize{buffersize/2};
        std::ifstream fin(file, std::ios_base::binary);
        fin.read(buffer, buffersize-halfsize);
        fin.seekg(-halfsize, std::ios_base::end);
        fin.read(buffer+halfsize, halfsize);
        auto hash = xxh::xxhash3<128>(buffer, buffersize);
        map1[hash].emplace_back(std::forward_like<Iterable>(file));
    }

    xxh::hash3_state128_t state;

    for (auto& files1: map1 | views::values)
    if (files1.size() > 1) {
        for (auto& file: files1) {
            std::ifstream fin(file, std::ios_base::binary);
            state.reset();
            while (fin.read(buffer, buffersize).gcount())
                state.update(buffer, fin.gcount());
            map2[state.digest()].emplace_back(std::move(file));
        }
        for (auto& files2: map2 | views::values)
            if (files2.size() > 1)
                res.emplace_back(std::move_if_noexcept(files2));
        map2.clear();
    }
}

/**
 * @brief Search @p dir recursively for all regular files, sorted by size.
 *
 * @return a pair of the numbers of non-empty files ans all regular files.
 */
template <class Container>
auto search(const fs::path& dir, Container& size_map)
{
    std::size_t tot=0, empty=0;

    myout << _T("Empty file list:") << std::endl;
    for (const auto& entry: fs::recursive_directory_iterator{dir})
    if (entry.is_regular_file())
    {
        tot++;
        auto& path_str = entry.path().native();
        if (entry.file_size()) {
            size_map[entry.file_size()].emplace_back(path_str);
        }
        else {
            empty++;
            myout << path_str << std::endl;
        }
    }

    myout << format(_T("\nEmpty: {}\nTotal: {}\n"), empty, tot) << std::endl;

    return std::make_pair(tot-empty, tot);
}

/**
 * @brief Search @p dirpath for duplicate files.
 *
 * Algorithm:
 * 1. Search @p dirpath recursively for all regular files.
 * 2. Group files by size, with empty files directly output.
 * 3. For each group of multiple files, group them by hashing.
 * 4. If an ultimate group is multiple, output it.
 */
void duplicate_file_search(const fs::path dirpath)
{
    using path_string = fs::path::string_type;

    std::map<std::uint64_t, std::vector<path_string>> size_map;
    search(dirpath, size_map);
    std::size_t num=0;
    std::uint_fast64_t rdsize=0; // redundant data size

    for (auto&& pair: size_map)
    {
        const auto filesize = pair.first;
        auto paths = std::move(pair.second);
        if (paths.size() <= 1)
            continue;

        std::vector<std::vector<path_string>> res;
        hash_check(std::move(paths), res);
        for (auto&& paths: res) {
            num++;
            rdsize += filesize * (paths.size()-1);
            ranges::sort(paths);
            myout << format(_T(" #{} ({}) {:L} B\n"), num, paths.size(), filesize);
            for (const auto& p: paths)
                (myout << p).put('\n');
            myout << std::endl;
        }
    }

    myout << format(_T("Redundant data size: {:L} B\n\nDone in {:.3f}s.\n"),
                    rdsize, (double)clock()/CLOCKS_PER_SEC);
}

int main(int argc, char *argv[])
{
    std::locale::global(std::locale{""});
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
        std::cerr << "Filesystem Exception: " << fs_err.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}