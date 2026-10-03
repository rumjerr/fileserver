#include "httplib.h"
#include "mainfunctions.h"

#include <cstdlib>
#include <string>

// time libraries and stuff

#include <thread>
#include <chrono>

int main()
{
    httplib::Server svr;

    // GET /api/list
    // lists the contents of the directory given by the "path" query param
    // (relative to the base directory); returns the entries as a JSON array
    svr.Get("/api/list", [](const httplib::Request &req, httplib::Response &res)
            {
        fs::path dir = safePath(req.get_param_value("path"));
        if (dir.empty())
        {
            res.status = 403;
            res.set_content("invalid path", "text/plain");
            return;
        }

        json result = toJson(listDir(dir));
        res.set_content(result.dump(), "application/json"); });

    // POST /api/mkdir
    // creates a new directory name by the name thing
    svr.Post("/api/mkdir", [](const httplib::Request &req, httplib::Response &res)
             {
        fs::path dir = safePath(req.get_param_value("name"));
        if (dir.empty())
        {
            res.status = 403;
            res.set_content("invalid path", "text/plain");
            return;
        }

        if (createDir(dir))
            res.set_content("created", "text/plain");
        else
            res.set_content("failed", "text/plain"); });

    // GET /api/download
    // reads a file named by the name query param and returns its raw bytes
    svr.Get("/api/download", [](const httplib::Request &req, httplib::Response &res)
            {
        fs::path filePath = safePath(req.get_param_value("name"));
        if (filePath.empty())
        {
            res.status = 403;
            res.set_content("invalid path", "text/plain");
            return;
        }

        std::string content;
        if (downloadFile(filePath, content))
            res.set_content(content, getMimeType(filePath));
        else
        {
            res.status = 404;
            res.set_content("not found", "text/plain");
        } });

    // POST /api/upload
    // accepts a multipart file upload under the file field and writes it into
    // the subfolder given by the "path" query param (relative to the base directory)
    svr.Post("/api/upload", [](const httplib::Request &req, httplib::Response &res)
             {
        if (!req.form.has_file("file"))
        {
            res.status = 400;
            res.set_content("no file provided", "text/plain");
            return;
        }

        const auto &file = req.form.get_file("file");
        std::string filename = fs::path(file.filename).filename().string();
        std::string destDir = req.get_param_value("path");

        fs::path filePath = safePath((fs::path(destDir) / filename).string());
        if (filePath.empty())
        {
            res.status = 403;
            res.set_content("invalid path", "text/plain");
            return;
        }

        if (uploadFile(filePath, file.content))
            res.set_content("uploaded", "text/plain");
        else
        {
            res.status = 500;
            res.set_content("failed to write", "text/plain");
        } });

    // DELETE /api/delete
    // moves a file or directory (name query param) to the trash
    svr.Delete("/api/delete", [](const httplib::Request &req, httplib::Response &res)
               {
        fs::path target = safePath(req.get_param_value("name"));
        if (target.empty() || target == baseDir())
        {
            res.status = 403;
            res.set_content("invalid path", "text/plain");
            return;
        }

        if (!fs::exists(target))
        {
            res.status = 404;
            res.set_content("not found", "text/plain");
            return;
        }

        if (moveToTrash(target))
            res.set_content("deleted", "text/plain");
        else
        {
            res.status = 500;
            res.set_content("failed", "text/plain");
        } });

    // GET /api/trash/list
    svr.Get("/api/trash/list", [](const httplib::Request &, httplib::Response &res)
            { res.set_content(trashList().dump(), "application/json"); });

    // POST /api/trash/restore?id=...
    svr.Post("/api/trash/restore", [](const httplib::Request &req, httplib::Response &res)
             {
        if (restoreFromTrash(req.get_param_value("id")))
            res.set_content("restored", "text/plain");
        else
        {
            res.status = 404;
            res.set_content("not found", "text/plain");
        } });

    // DELETE /api/trash/purge?id=...   (one item, forever)
    svr.Delete("/api/trash/purge", [](const httplib::Request &req, httplib::Response &res)
               {
        if (purgeFromTrash(req.get_param_value("id")))
            res.set_content("purged", "text/plain");
        else
        {
            res.status = 404;
            res.set_content("not found", "text/plain");
        } });

    // DELETE /api/trash/empty   (everything, forever)
    svr.Delete("/api/trash/empty", [](const httplib::Request &, httplib::Response &res)
               {
        if (emptyTrash())
            res.set_content("emptied", "text/plain");
        else
        {
            res.status = 500;
            res.set_content("failed", "text/plain");
        } });

    // GET /api/storage
    // returns disk usage stats: total/used/free, folder sizes, top files
    svr.Get("/api/storage", [](const httplib::Request &, httplib::Response &res)
            { res.set_content(storageInfo().dump(), "application/json"); });

    // background cleaner: every 10 minutes remove trash older than 48h
    std::thread([]
                {
        while (true)
        {
            purgeExpired();
            std::this_thread::sleep_for(std::chrono::minutes(10));
        } })
        .detach();

    // (OLD) listen stuff,etc

    const char *www = std::getenv("FILES_WWW");
    const char *host = std::getenv("FILES_HOST");
    const char *port = std::getenv("FILES_PORT");

    svr.set_mount_point("/", www ? www : "./www");
    svr.listen(host ? host : "0.0.0.0", port ? std::stoi(port) : 8080);

}