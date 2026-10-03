#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <sys/statvfs.h>
#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

struct FileEntry
{
    std::string name;
    bool isDirectory;
};

std::vector<FileEntry> listDir(const fs::path &dir);
json toJson(const std::vector<FileEntry> &entries);
fs::path safePath(const std::string &name);
bool createDir(const fs::path &dir);

// these two were commented out since they arent needed in mainfunctions.cpp but if needed (for troubleshooting/reference,etc) they can be uncommented

// bool changeDir(fs::path &currentDir, const std::string &folderName);
// bool deleteFile(const fs::path &path);

bool uploadFile(const fs::path &filePath, const std::string &content);
bool downloadFile(const fs::path &filePath, std::string &outContent);
std::string getMimeType(const fs::path &filePath);
fs::path baseDir();
fs::path trashDir();
bool moveToTrash(const fs::path &path);
json trashList();
bool restoreFromTrash(const std::string &id);
bool purgeFromTrash(const std::string &id);
bool emptyTrash();
void purgeExpired();
json storageInfo();