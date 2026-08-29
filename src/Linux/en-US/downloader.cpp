/* SystemAudioRecorder - Linux URL downloader; launches yt-dlp directly, not through a shell. */
#include "downloader.h"
#include <cerrno>
#include <filesystem>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static bool valid_url(const std::string &url) {
    if (!(url.rfind("https://",0)==0 || url.rfind("http://",0)==0)) return false;
    return url.find_first_of("\r\n\"") == std::string::npos;
}
static bool command_ok(const char *name) {
    pid_t pid=fork(); if (pid==0) { execlp(name,name,"--version",(char*)nullptr); _exit(127); } if (pid<0) return false;
    int st=0; waitpid(pid,&st,0); return WIFEXITED(st)&&WEXITSTATUS(st)!=127;
}
bool DownloaderToolsAvailable(std::string *message)
{
    const bool ytDlpAvailable = command_ok("yt-dlp");
    const bool ffmpegAvailable = command_ok("ffmpeg");
    if (message)
    {
        if (ytDlpAvailable && ffmpegAvailable)
            *message = "yt-dlp / FFmpeg ready";
        else if (!ytDlpAvailable)
            *message = "yt-dlp not found";
        else
            *message = "FFmpeg not found";
    }
    return ytDlpAvailable && ffmpegAvailable;
}
DownloadResult DownloaderRun(const std::string &url, DownloadMode mode, const std::string &outputDirectory) {
    DownloadResult r; if (!valid_url(url)) { r.message="Invalid URL format"; return r; }
    std::error_code ec; std::filesystem::create_directories(outputDirectory,ec); if (ec) { r.message="Unable to create download directory: "+ec.message(); return r; }
    pid_t pid=fork();
    if (pid==0) {
        chdir(outputDirectory.c_str());
        if (mode==DownloadMode::Audio)
            execlp("yt-dlp","yt-dlp","--no-playlist","--newline","-x","--audio-format","mp3","--audio-quality","0",url.c_str(),(char*)nullptr);
        else
            execlp("yt-dlp","yt-dlp","--no-playlist","--newline","-f","bestvideo+bestaudio/best","--merge-output-format","mp4",url.c_str(),(char*)nullptr);
        _exit(127);
    }
    if (pid<0) { r.message="Unable to start yt-dlp"; return r; }
    int st=0; while (waitpid(pid,&st,0)<0 && errno==EINTR) {}
    r.exitCode=WIFEXITED(st)?WEXITSTATUS(st):-1; r.success=(r.exitCode==0);
    r.message=r.success?"Download completed":"Download failed; yt-dlp exit code "+std::to_string(r.exitCode); return r;
}
