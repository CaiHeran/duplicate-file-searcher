/**
 * @author Cai Heran (Tsai.love.dev@outlook.com)
 * @brief Search the given directory for duplicate regular files.
 *
 * The standard output will look like:
开始搜索
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
 *
 * The log.txt will look like:
20221216T150405.8106698+0800
010846.162812300
Found (xxxB) xxx/xxx/xxx
Found (xxB) xxx/xxx/xxx
nonempty/total
0000.102965500 go to check files of xxxB.
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx : xxx/xxx/xxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx : xxx/xxx/xxx
...
print #1
print #2
......
0000.140347400
20221216T150405.9094823+0800
Done.
 */

#include "headers.hpp"

using int32 = std::int32_t;
using int64 = std::int64_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using size_t = std::size_t;

using std::endl;
using std::format;

namespace fs = std::filesystem;
namespace ranges = std::ranges;
namespace views = std::views;

#ifdef __WINDOWS__
# define myout std::wcout
# define _T(x) L ## x
std::wofstream mylog;
#else
# define myout std::cout
# define _T(x) x
std::ofstream mylog;
#endif

const auto StartTime = std::chrono::steady_clock::now();

auto nowtime()
{
    using namespace std::chrono;
    zoned_time t(current_zone(), system_clock::now());
    return std::format(_T("{:%Y%m%dT%H%M%S%z}"), t);
}
auto myclock()
{
    return std::chrono::steady_clock::now() - StartTime;
}

/**
 * @brief Calculates the 64-bit hash of @p file using XXH3_64bits.
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
 * @return a pair of the numbers of non-empty files ans all files.
 */
template <class Container>
auto search(const fs::path dir, Container& size_map)
{
    myout << _T("开始搜索\n");
    size_t tot=0, empty=0;

    myout << _T("以下是空文件：\n");
    for (const auto& entry: fs::recursive_directory_iterator(dir))
    if (entry.is_regular_file())
    {
        tot++;
        auto& path_str = entry.path().native();
        mylog << format(_T("Found ({}B) {}\n"), entry.file_size(), path_str);
        if (!entry.file_size()) {
            empty++; myout << path_str << endl;
        }
        else {
            size_map[entry.file_size()].emplace_back(path_str);
        }
    }

    mylog << format(_T("{}/{}\n"), tot-empty, tot);
    myout << format(_T("\n空文件：{}\n总文件：{}\n"), empty, tot) << endl;

    return std::make_pair(tot-empty, tot);
}

/**
 * @brief Hash all files of @p paths and sort them by hash value.
 */
template<ranges::input_range Iterable, class Container>
void hash_check(Iterable&& paths, Container& xxh_map)
{
    for (auto&& path: paths) {
        auto hash = xxhash3_128_file(path);
        mylog << format(_T("{:016x}{:016x} : {}\n"), hash.high64, hash.low64, path);
        xxh_map[hash].emplace_back(std::move(path));
    }
}

/**
 * @brief Search @p dirpath for duplicate files.
 */
void duplicate_file_search(const fs::path dirpath)
{
    mylog << format(_T("{}\n{:%H%M%S}\n"), nowtime(), StartTime.time_since_epoch());

    using path_string = fs::path::string_type;

    std::map<uint64, std::vector<path_string>> size_map;
    search(dirpath, size_map);
    size_t num=0;

    for (auto&& pair: size_map)
    {
        const auto filesize = pair.first;
        auto paths = std::move(pair.second);
        if (paths.size() <= 1)
            continue;

        mylog << format(_T("{:%M%S} go to check files of {}B.\n"), myclock(), filesize);
        std::map<xxh::hash128_t, std::vector<path_string>> xxh_map;
        hash_check(std::move(paths), xxh_map);
        for (auto&& paths: xxh_map | views::values)
        if (paths.size()>=2)
        {
            num++;
            mylog << format(_T("Print #{}\n"), num);
            ranges::sort(paths);
            myout << format(_T(" #{} ({}) {}B\n"), num, paths.size(), filesize);
            for (const auto& p: paths)
                (myout << p).put('\n');
            myout << endl;
        }
    }

    myout << _T("Done.\n");
    mylog << format(_T("{:%M:%S}\n{}\nDone."), myclock(), nowtime());
}

int main(int argc, char *argv[])
{
    std::locale::global(std::locale("zh_CN"));
    myout.imbue(std::locale("zh_CN"));
    mylog.imbue(std::locale("zh_CN"));

    std::ios_base::sync_with_stdio(false);

    fs::path dir;

    if (argc <= 1) {
        dir = ".";
    }
    else {
        dir = argv[1];
        if (!fs::exists(dir) || !fs::is_directory(dir))
        {
            myout << _T("No such a directory.\n没有这个目录。\n");
            return 0;
        }
    }

    fs::path logpath = fs::temp_directory_path()/"log.txt";
    mylog.open(logpath);

    try { duplicate_file_search(dir); }
    catch (const fs::filesystem_error& fs_err) {
        std::cerr << "Filesystem Exception: " << fs_err.what() << endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << endl;
    }

    myout << format(_T("Log file : {}\n"), logpath.native());
}