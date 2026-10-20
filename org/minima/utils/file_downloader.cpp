#include "org/minima/utils/file_downloader.hpp"

#include <curl/curl.h>

#include <cstdio>
#include <mutex>
#include <system_error>

#include "org/minima/utils/minima_logger.hpp"

namespace org {
namespace minima {
namespace utils {

namespace {

std::once_flag g_curlOnce;

void ensureCurl() {
    std::call_once(g_curlOnce, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

struct Sink {
    std::FILE* file = nullptr;
    long long max = 0;
    long long written = 0;
};

std::size_t writeCb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    Sink* sink = static_cast<Sink*>(userdata);
    const std::size_t n = size * nmemb;
    if (sink->written + static_cast<long long>(n) > sink->max) return 0;  // abort over limit
    const std::size_t w = std::fwrite(ptr, 1, n, sink->file);
    sink->written += static_cast<long long>(w);
    return w;
}

} // namespace

bool downloadFile(const std::string& url,
                  const std::filesystem::path& dest,
                  long long maxBytes) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        MinimaLogger::log("downloadFile: unsupported URL scheme: " + url);
        return false;
    }
    ensureCurl();

    std::error_code ec;
    if (!dest.parent_path().empty()) {
        std::filesystem::create_directories(dest.parent_path(), ec);
    }

    std::filesystem::path part = dest;
    part += ".part";

    Sink sink;
    sink.max = maxBytes;
    sink.file = std::fopen(part.string().c_str(), "wb");
    if (!sink.file) {
        MinimaLogger::log("downloadFile: cannot open " + part.string() + " for writing");
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::fclose(sink.file);
        std::filesystem::remove(part, ec);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 120L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(maxBytes));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "minima-more/0.2");
#if defined(CURLOPT_PROTOCOLS_STR)
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS,
                     static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS,
                     static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif

    MinimaLogger::log("Downloading " + url + " ..");
    const CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    std::fclose(sink.file);

    if (rc != CURLE_OK) {
        MinimaLogger::log(std::string("downloadFile failed: ") + curl_easy_strerror(rc) + " (" + url + ")");
        std::filesystem::remove(part, ec);
        return false;
    }

    std::filesystem::remove(dest, ec);
    std::filesystem::rename(part, dest, ec);
    if (ec) {
        MinimaLogger::log("downloadFile rename failed: " + ec.message());
        std::filesystem::remove(part, ec);
        return false;
    }

    MinimaLogger::log("Downloaded " + dest.string() + " (" +
                      std::to_string(sink.written) + " bytes)");
    return true;
}

} // namespace utils
} // namespace minima
} // namespace org
