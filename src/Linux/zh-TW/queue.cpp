/* SystemAudioRecorder - Linux in-memory FIFO download queue */
#include "queue.h"

DownloadQueue::DownloadQueue(std::string outputDirectory)
    : outputDirectory_(std::move(outputDirectory)),
      worker_(&DownloadQueue::loop, this)
{
}

DownloadQueue::~DownloadQueue()
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();
}

int DownloadQueue::add(const std::string &url, DownloadMode mode)
{
    std::lock_guard<std::mutex> guard(lock_);
    QueueItem item;
    item.id = nextId_++;
    item.url = url;
    item.mode = mode;
    waiting_.push_back(item);
    cv_.notify_one();
    return item.id;
}

std::vector<QueueItem> DownloadQueue::snapshot()
{
    std::lock_guard<std::mutex> guard(lock_);
    auto result = history_;
    result.insert(result.end(), waiting_.begin(), waiting_.end());
    return result;
}

void DownloadQueue::setOutputDirectory(const std::string &path)
{
    std::lock_guard<std::mutex> guard(lock_);
    outputDirectory_ = path;
}

void DownloadQueue::loop()
{
    for (;;)
    {
        QueueItem item;
        std::string outputDirectory;
        {
            std::unique_lock<std::mutex> guard(lock_);
            cv_.wait(guard, [&] { return stop_ || !waiting_.empty(); });
            if (stop_ && waiting_.empty())
                return;

            item = waiting_.front();
            waiting_.pop_front();
            item.state = "下載中";
            history_.push_back(item);
            outputDirectory = outputDirectory_;
        }

        DownloadResult result = DownloaderRun(item.url, item.mode, outputDirectory);
        std::lock_guard<std::mutex> guard(lock_);
        for (auto &historyItem : history_)
        {
            if (historyItem.id == item.id)
            {
                historyItem.state = result.success ? "完成" : "失敗";
                break;
            }
        }
    }
}
