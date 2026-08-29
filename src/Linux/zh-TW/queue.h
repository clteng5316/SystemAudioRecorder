/* SystemAudioRecorder - Linux in-memory FIFO download queue */
#ifndef SYSTEM_AUDIO_RECORDER_LINUX_QUEUE_H
#define SYSTEM_AUDIO_RECORDER_LINUX_QUEUE_H
#include "downloader.h"
#include <condition_variable>
#include <deque>

struct QueueItem { int id{0}; DownloadMode mode{DownloadMode::Video}; std::string url; std::string state{"等待中"}; };
class DownloadQueue {
public:
    explicit DownloadQueue(std::string outputDirectory);
    ~DownloadQueue();
    int add(const std::string &url, DownloadMode mode);
    std::vector<QueueItem> snapshot();
    void setOutputDirectory(const std::string &path);
private:
    void loop();
    std::mutex lock_;
    std::condition_variable cv_;
    std::deque<QueueItem> waiting_;
    std::vector<QueueItem> history_;
    bool stop_{false};
    int nextId_{1};
    std::string outputDirectory_;
    std::thread worker_;
};
#endif
