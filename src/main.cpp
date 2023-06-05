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

#include <print>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <filesystem>

#include <bit>
#include <memory>
#include <ranges>
#include <map>
#include <vector>

#include "xxhash.hpp"

namespace fs = std::filesystem;
namespace ranges = std::ranges;
namespace views = std::views;


std::string prettify_bytes(std::size_t size)
{
    using namespace std::literals;
    constexpr std::string_view unit[]{"B"sv, "KiB"sv, "MiB"sv, "GiB"sv, "TiB"sv};
    int base = std::countr_zero(std::bit_floor(size)) / 10;
    std::uint16_t len=0, s[7];
    std::ostringstream o;
    o.precision(4);
    o << (double)size/(1<<(base*10)) << ' ' << unit[base] << " (";
    while (size) {
        s[len++] = size%1000;
        size /= 1000;
    }
    o << s[--len];
    o.fill('0');
    while (len)
        o << ' ' << std::setw(3) << s[--len];
    o << " B)";
    return o.str();
};

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
template<class Iterable, class Container>
void hash_check(Iterable&& filelist, Container &res)
{
    constexpr std::size_t buffersize {1<<15}; // 32 KiB
    thread_local auto buffer = std::make_unique_for_overwrite<char[]>(buffersize);
    std::map<xxh::hash128_t, typename Container::value_type> map1, map2;

    if (fs::file_size(*filelist.cbegin()) <= buffersize)
    {
        for (auto&& file: filelist) {
            std::ifstream fin(file, std::ios_base::binary);
            fin.read(buffer.get(), buffersize);
            auto hash = xxh::xxhash3<128>(buffer.get(), fin.gcount());
            map1[hash].emplace_back(std::forward_like<Iterable>(file));
        }
        for (auto& files: map1 | views::values)
            if (files.size() > 1)
                res.emplace_back(std::move_if_noexcept(files));
        return;
    }

    for (auto&& file: filelist)
    {
        constexpr auto buffersize {1<<10}; // 1 KiB
        constexpr auto halfsize {buffersize/2};

        std::ifstream fin(file, std::ios_base::binary);
        fin.read(buffer.get(), buffersize - halfsize);
        fin.seekg(-halfsize, std::ios_base::end);
        fin.read(buffer.get() + halfsize, halfsize);

        auto hash = xxh::xxhash3<128>(buffer.get(), buffersize);
        map1[hash].emplace_back(std::forward_like<Iterable>(file));
    }

    xxh::hash3_state128_t state;

    for (auto& files1: map1 | views::values)
      if (files1.size() > 1)
      {
        for (auto& file: files1) {
            std::ifstream fin(file, std::ios_base::binary);
            state.reset();
            while (fin.read(buffer.get(), buffersize).gcount())
                state.update(buffer.get(), fin.gcount());
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
    std::size_t tot_size=0;

    std::println("Empty file list:");

    for (const auto& entry: fs::recursive_directory_iterator{dir})
      if (entry.is_regular_file())
      {
        tot++;
        auto path_str = entry.path().generic_string();
        if (entry.file_size()) {
            tot_size += entry.file_size();
            size_map[entry.file_size()].emplace_back(path_str);
        }
        else {
            empty++;
            std::println("{}", path_str);
        }
      }

    std::println("\nEmpty: {}\nTotal: {}\nSize:  {}\n", empty, tot, prettify_bytes(tot_size));

    return std::make_tuple(tot_size, tot, tot-empty);
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
    std::map<std::uint_least64_t, std::vector<std::string>> size_map;
    search(dirpath, size_map);
    std::size_t num=0;
    std::uintmax_t rdsize=0; // redundant data size

    for (auto&& pair: size_map)
    {
        const auto filesize = pair.first;
        auto paths = std::move(pair.second);
        if (paths.size() <= 1)
            continue;

        std::vector<std::vector<std::string>> res;
        hash_check(std::move(paths), res);
        for (auto&& paths: res) {
            num++;
            rdsize += filesize * (paths.size()-1);
            ranges::sort(paths);
            std::println(" #{} [{}]  {}", num, paths.size(), prettify_bytes(filesize));
            for (const auto& p: paths)
                // This needs to be enforced on Windows.
                std::vprint_nonunicode("{}\n", std::make_format_args(p));
            std::println("");
        }
    }

    std::println("Redundant data size: {}\n\nDone in {:.3f}s.",
                    prettify_bytes(rdsize) , (double)clock()/CLOCKS_PER_SEC);
}

int main(int argc, char *argv[])
{
    fs::path dir;

    if (argc <= 1) {
        dir = ".";
    }
    else {
        dir = argv[1];
        if (!fs::exists(dir) || !fs::is_directory(dir))
        {
            std::println("No such directory.");
            return 0;
        }
    }

    try { duplicate_file_search(dir); }
    catch (const fs::filesystem_error& fs_err) {
        std::println(stderr, "Filesystem Exception: {}", fs_err.what());
    }
    catch (const std::exception& e) {
        std::println(stderr, "Exception: ", e.what());
    }
}