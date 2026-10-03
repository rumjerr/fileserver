#include <iostream>
#include <string>
#include <vector>

#include "mainfunctions.h" // -------

#include <fstream>

#include <mutex>
#include <chrono>
#include <cctype>
#include <system_error>

#include <cstdlib>

// list directory

std::vector<FileEntry> listDir(const fs::path &dir)
{
    std::vector<FileEntry> results;

    for (const auto &entry : fs::directory_iterator(dir))
    {
        FileEntry fe;
        fe.name = entry.path().filename().string();
        fe.isDirectory = entry.is_directory();
        results.push_back(fe);
    }

    return results;
}

// json shit

json toJson(const std::vector<FileEntry> &entries)
{
    json result = json::array();

    for (const auto &fe : entries)
    {
        json item;
        item["name"] = fe.name;
        item["isDirectory"] = fe.isDirectory;
        result.push_back(item);
    }

    return result;
}

// base direcotry (against hardcoded stuff)

fs::path baseDir()
{
    static const fs::path base = []
    {
        const char *env = std::getenv("FILES_ROOT");
        return fs::weakly_canonical(env ? env : "/srv/files");
    }();
    return base;
}

// safe path !!!

fs::path safePath(const std::string &name)
{
    fs::path base = baseDir();
    fs::path requested = fs::weakly_canonical(base / name);

    std::string baseStr = base.string();
    std::string reqStr = requested.string();

    if (reqStr.size() < baseStr.size() || reqStr.compare(0, baseStr.size(), baseStr) != 0)
        return {};

    if (reqStr.size() > baseStr.size() && reqStr[baseStr.size()] != '/')
        return {};

    return requested;
}

// create directory

bool createDir(const fs::path &dir)
{
    return fs::create_directory(dir);
}

// chage directory

// NOT IN USE !!!!!!!       THESE FUNCTIONS ARE COMMENTED OUT     (just for reference if i have to debug something if i cant access the server)

/* 

bool changeDir(fs::path &currentDir, const std::string &folderName)
{
    if (folderName == "..")
    {
        if (currentDir.has_parent_path() && currentDir != currentDir.root_path())
        {
            currentDir = currentDir.parent_path();
            return true;
        }
        std::cout << "already at root\n";
        return false;
    }

    fs::path target = currentDir / folderName;

    if (!fs::exists(target) || !fs::is_directory(target))
    {
        std::cout << "no such directory: " << target << "\n";
        return false;
    }

    currentDir = target;
    return true;
}

// delete file or directory

bool deleteFile(const fs::path &path)
{
    if (!fs::exists(path))
        return false;

    if (fs::is_directory(path))
        return fs::remove_all(path) > 0;

    return fs::remove(path);
}

*/

// -- DOWNLOAD / UPLOAD --

// upload files

bool uploadFile(const fs::path &filePath, const std::string &content)
{
    std::ofstream out(filePath, std::ios::binary);
    if (!out)
        return false;

    out << content;
    return true;
}

// download files

bool downloadFile(const fs::path &filePath, std::string &outContent)
{
    std::ifstream in(filePath, std::ios::binary);
    if (!in)
        return false;

    outContent.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}

// New

// mime detection stuff

std::string getMimeType(const fs::path &filePath)
{
    std::string ext = filePath.extension().string();

    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".png")
        return "image/png";
    if (ext == ".gif")
        return "image/gif";
    if (ext == ".mp4")
        return "video/mp4";
    if (ext == ".webm")
        return "video/webm";
    if (ext == ".pdf")
        return "application/pdf";
    if (ext == ".txt")
        return "text/plain";

    return "application/octet-stream";
}


// vibcoded trash function(s) below


// i was too lazy to actually code the trash functions and just wanted to be done with this project
// i only ever needed it to share files from a pc to another via tailscale and have those files safe
// i havent touched any of the code below only explained what functions it should make and what they should do and how. thats why its different from the code above
// the code is much more complex compared to what ive wrote
// this project was mostly to fufill my need, and to learn a new library ill probably use in the future


// -- TRASH --
// deleted items are moved to trashDir()/<id> with a note trashDir()/<id>.json.
// items older than 48 hours are removed for good by purgeExpired().

static const long long TRASH_TTL_SECONDS = 48LL * 3600;
static std::mutex trashMutex; // httplib is multithreaded, and the cleaner thread runs too

fs::path trashDir()
{
    // sits next to the base folder, so /api/list can never browse it

    return fs::path(baseDir().string() + "-trash");
}

static long long nowSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

static long long nowMillis()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// ids are digits only, so a request put a random ../ into a path

static bool validId(const std::string &id)
{
    if (id.empty() || id.size() > 20)
        return false;
    for (unsigned char c : id)
        if (!std::isdigit(c))
            return false;
    return true;
}

static bool readNote(const fs::path &p, json &out)
{
    std::ifstream in(p);
    if (!in)
        return false;
    try
    {
        in >> out;
    }
    catch (...)
    {
        return false;
    }
    return out.is_object();
}

static void removeEntry(const std::string &id)
{
    std::error_code ec;
    fs::remove_all(trashDir() / id, ec);
    fs::remove(trashDir() / (id + ".json"), ec);
}

// move a file/folder into the trash

bool moveToTrash(const fs::path &path)
{
    std::lock_guard<std::mutex> lock(trashMutex);
    std::error_code ec;

    if (!fs::exists(path, ec))
        return false;

    fs::create_directories(trashDir(), ec);
    if (ec)
        return false;

    long long id = nowMillis();
    while (fs::exists(trashDir() / std::to_string(id)))
        id++;
    std::string idStr = std::to_string(id);

    json note;
    note["name"] = path.filename().string();
    note["originalPath"] = fs::relative(path, baseDir()).generic_string();
    note["isDirectory"] = fs::is_directory(path);
    note["deletedAt"] = nowSeconds();

    fs::path notePath = trashDir() / (idStr + ".json");
    {
        std::ofstream out(notePath);
        if (!out)
            return false;
        out << note.dump();
    }

    fs::rename(path, trashDir() / idStr, ec);
    if (ec)
    {
        fs::remove(notePath, ec);
        return false;
    }
    return true;
}

// list what's in the trash (shape matches what the web page expects)

json trashList()
{
    std::lock_guard<std::mutex> lock(trashMutex);
    json result = json::array();
    std::error_code ec;

    if (!fs::exists(trashDir(), ec))
        return result;

    for (const auto &entry : fs::directory_iterator(trashDir(), ec))
    {
        if (entry.path().extension() != ".json")
            continue;

        std::string id = entry.path().stem().string();
        json note;
        if (!validId(id) || !readNote(entry.path(), note))
            continue;

        long long deletedAt = note.value("deletedAt", 0LL);
        json item;
        item["id"] = id;
        item["name"] = note.value("name", "");
        item["originalPath"] = note.value("originalPath", "");
        item["isDirectory"] = note.value("isDirectory", false);
        item["deletedAt"] = deletedAt;
        item["expiresAt"] = deletedAt + TRASH_TTL_SECONDS;
        result.push_back(item);
    }
    return result;
}

// put an item back where it came from

bool restoreFromTrash(const std::string &id)
{
    if (!validId(id))
        return false;

    std::lock_guard<std::mutex> lock(trashMutex);
    std::error_code ec;

    fs::path item = trashDir() / id;
    fs::path notePath = trashDir() / (id + ".json");
    json note;

    if (!readNote(notePath, note) || !fs::exists(item, ec))
        return false;

    fs::path target = safePath(note.value("originalPath", ""));
    if (target.empty() || target == baseDir())
        return false;

    fs::create_directories(target.parent_path(), ec); // original folder may be gone
    if (ec)
        return false;

    if (fs::exists(target, ec)) // name taken -> "photo (restored).jpg"
    {
        bool isDir = note.value("isDirectory", false);
        fs::path parent = target.parent_path();
        std::string stem = isDir ? target.filename().string() : target.stem().string();
        std::string ext = isDir ? "" : target.extension().string();
        int n = 1;
        do
        {
            std::string tag = n == 1 ? " (restored)" : " (restored " + std::to_string(n) + ")";
            target = parent / (stem + tag + ext);
            n++;
        } while (fs::exists(target, ec));
    }

    fs::rename(item, target, ec);
    if (ec)
        return false;

    fs::remove(notePath, ec);
    return true;
}

// delete one item forever

bool purgeFromTrash(const std::string &id)
{
    if (!validId(id))
        return false;

    std::lock_guard<std::mutex> lock(trashMutex);
    std::error_code ec;

    if (!fs::exists(trashDir() / (id + ".json"), ec))
        return false;

    removeEntry(id);
    return true;
}

// delete everytihng in the trash forever

bool emptyTrash()
{
    std::lock_guard<std::mutex> lock(trashMutex);
    std::error_code ec;

    if (!fs::exists(trashDir(), ec))
        return true;

    std::vector<fs::path> all;
    for (const auto &entry : fs::directory_iterator(trashDir(), ec))
        all.push_back(entry.path());

    for (const auto &p : all)
        fs::remove_all(p, ec);
    return true;
}

// remove everything older than 48h

void purgeExpired()
{
    std::lock_guard<std::mutex> lock(trashMutex);
    std::error_code ec;

    if (!fs::exists(trashDir(), ec))
        return;

    long long now = nowSeconds();
    std::vector<std::string> expired;

    for (const auto &entry : fs::directory_iterator(trashDir(), ec))
    {
        if (entry.path().extension() != ".json")
            continue;

        std::string id = entry.path().stem().string();
        json note;
        if (!validId(id) || !readNote(entry.path(), note))
            continue;

        if (now - note.value("deletedAt", 0LL) >= TRASH_TTL_SECONDS)
            expired.push_back(id);
    }

    for (const auto &id : expired)
        removeEntry(id);
}

// storage info is vibecoded aswell

// fileserver v1.2 addition

// -- STORAGE INFO --
// returns disk usage stats: total/used/free from statvfs,
// top-level folder sizes, subfolder breakdown for the biggest ones,
// and the largest individual files across the whole tree.

static uintmax_t dirSize(const fs::path &dir)
{
    uintmax_t total = 0;
    std::error_code ec;
    for (auto &e : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec))
    {
        if (e.is_regular_file(ec))
            total += e.file_size(ec);
    }
    return total;
}

json storageInfo()
{
    json result;
    fs::path base = baseDir();

    // disk-level stats via statvfs
    struct statvfs st;
    if (statvfs(base.c_str(), &st) == 0)
    {
        uintmax_t blockSize = st.f_frsize;
        result["totalBytes"]     = (uintmax_t)st.f_blocks * blockSize;
        result["freeBytes"]      = (uintmax_t)st.f_bavail * blockSize;
        result["usedBytes"]      = ((uintmax_t)st.f_blocks - (uintmax_t)st.f_bfree) * blockSize;
    }
    else
    {
        result["totalBytes"] = 0;
        result["freeBytes"]  = 0;
        result["usedBytes"]  = 0;
    }

    // per-folder sizes (top-level children of base)
    struct FolderInfo { std::string name; uintmax_t size; fs::path path; };
    std::vector<FolderInfo> folders;
    uintmax_t rootFiles = 0; // loose files directly in base

    std::error_code ec;
    for (auto &entry : fs::directory_iterator(base, ec))
    {
        if (entry.is_directory(ec))
        {
            uintmax_t sz = dirSize(entry.path());
            folders.push_back({entry.path().filename().string(), sz, entry.path()});
        }
        else if (entry.is_regular_file(ec))
        {
            rootFiles += entry.file_size(ec);
        }
    }

    std::sort(folders.begin(), folders.end(), [](auto &a, auto &b){ return a.size > b.size; });

    json jfolders = json::array();
    // for the top 5 biggest folders, also list their subfolders
    for (size_t i = 0; i < folders.size(); i++)
    {
        json jf;
        jf["name"] = folders[i].name;
        jf["bytes"] = folders[i].size;

        if (i < 5)
        {
            std::vector<FolderInfo> subs;
            uintmax_t subFiles = 0;
            for (auto &sub : fs::directory_iterator(folders[i].path, ec))
            {
                if (sub.is_directory(ec))
                    subs.push_back({sub.path().filename().string(), dirSize(sub.path()), sub.path()});
                else if (sub.is_regular_file(ec))
                    subFiles += sub.file_size(ec);
            }
            std::sort(subs.begin(), subs.end(), [](auto &a, auto &b){ return a.size > b.size; });

            json jsubs = json::array();
            for (size_t s = 0; s < subs.size() && s < 8; s++)
            {
                json js;
                js["name"] = subs[s].name;
                js["bytes"] = subs[s].size;
                jsubs.push_back(js);
            }
            if (subFiles > 0)
            {
                json js;
                js["name"] = "(files in root)";
                js["bytes"] = subFiles;
                jsubs.push_back(js);
            }
            jf["subfolders"] = jsubs;
        }

        jfolders.push_back(jf);
    }
    if (rootFiles > 0)
    {
        json jf;
        jf["name"] = "(files in root)";
        jf["bytes"] = rootFiles;
        jfolders.push_back(jf);
    }
    result["folders"] = jfolders;

    // top 20 largest files anywhere under base
    struct BigFile { std::string path; uintmax_t size; };
    std::vector<BigFile> allFiles;
    for (auto &e : fs::recursive_directory_iterator(base, fs::directory_options::skip_permission_denied, ec))
    {
        if (e.is_regular_file(ec))
            allFiles.push_back({fs::relative(e.path(), base).generic_string(), e.file_size(ec)});
    }
    std::sort(allFiles.begin(), allFiles.end(), [](auto &a, auto &b){ return a.size > b.size; });

    json jbig = json::array();
    uintmax_t bigTotal = 0;
    size_t topN = std::min(allFiles.size(), (size_t)20);
    for (size_t i = 0; i < topN; i++)
    {
        json jf;
        jf["path"] = allFiles[i].path;
        jf["bytes"] = allFiles[i].size;
        jbig.push_back(jf);
        bigTotal += allFiles[i].size;
    }
    result["topFiles"] = jbig;
    result["topFilesBytes"] = bigTotal;

    return result;
}

// this is commented out just like changeDir, just for me, ignore

/*

int main()
{
    std::string input;
    fs::path currentDir = "/home/rumjer/debianexamplefs";

    while (true)
    {
        std::cout << "enter command: ";
        std::getline(std::cin, input);

        if (input == "create")
        {
            std::string name;
            std::cout << "enter directory name: ";
            std::getline(std::cin, name);

            fs::path dir = "/home/rumjer/debianexamplefs/" + name;

            if (createDir(dir))
                std::cout << "created: " << dir << "\n";
            else
                std::cout << "failed to create (already exists?): " << dir << "\n";
        }
        else if (input == "ls")
        {
            json result = toJson(listDir(currentDir));
            std::cout << result.dump(4) << "\n";
        }
        else if (input == "exit" || input == "quit")
        {
            break;
        }
        else if (input == "upload")
        {
            std::string sourcePath;
            std::cout << "file path: ";
            std::getline(std::cin, sourcePath);

            fs::path src(sourcePath);
            std::ifstream in(src, std::ios::binary);
            if (!in)
            {
                std::cout << "failed to open: " << src << "\n";
                continue;
            }

            std::string content((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());

            fs::path filePath = currentDir / src.filename();

            if (uploadFile(filePath, content))
                std::cout << "wrote: " << filePath << "\n";
            else
                std::cout << "failed to write: " << filePath << "\n";
        }
        else if (input == "cd")
        {
            std::string folderName;
            std::cout << "folder name: ";
            std::getline(std::cin, folderName);

            changeDir(currentDir, folderName);
        }
        else if (input == "pwd")
        {
            std::cout << currentDir << "\n";
        }
        else if (input == "download")
        {
            std::string name;
            std::cout << "file name: ";
            std::getline(std::cin, name);

            fs::path filePath = currentDir / name;
            std::string content;

            if (downloadFile(filePath, content))
            {
                fs::path outPath = "/home/rumjer/Downloads/deb-serv-download_" + name;
                uploadFile(outPath, content);
                std::cout << "wrote copy to: " << outPath << "\n";
            }
            else
                std::cout << "failed to read: " << filePath << "\n";
        }
        else if (input == "delete")
        {
            std::string name;
            std::cout << "name to delete: ";
            std::getline(std::cin, name);

            fs::path target = currentDir / name;

            if (deleteFile(target))
                std::cout << "deleted: " << target << "\n";
            else
                std::cout << "failed to delete (doesn't exist?): " << target << "\n";
        }

        // final else for unknown commands

        else
        {
            std::cout << "unknown command: " << input << "\n";
        }
    }
}

*/