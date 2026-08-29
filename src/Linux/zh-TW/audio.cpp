/*
 * SystemAudioRecorder - Linux system-audio capture
 * Uses FFmpeg's PulseAudio input against the default sink monitor source.
 * This works with PulseAudio and with PipeWire's PulseAudio compatibility server.
 */
#include "audio.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static std::string trim(std::string s) {
    while (!s.empty() && (s.back()=='\n'||s.back()=='\r'||s.back()==' '||s.back()=='\t')) s.pop_back();
    size_t p=0; while (p<s.size() && (s[p]==' '||s[p]=='\t')) ++p; return s.substr(p);
}

static std::string run_capture_text(const char *cmd) {
    std::string out; std::array<char,512> buf{}; FILE *f=popen(cmd,"r"); if (!f) return out;
    while (fgets(buf.data(), (int)buf.size(), f))
        out += buf.data();
    pclose(f);
    return trim(out);
}

static std::string find_monitor_source() {
    std::string sink=run_capture_text("pactl get-default-sink 2>/dev/null");
    if (sink.empty()) {
        std::string info=run_capture_text("pactl info 2>/dev/null");
        std::istringstream in(info); std::string line;
        while (std::getline(in,line)) if (line.rfind("Default Sink:",0)==0) { sink=trim(line.substr(13)); break; }
    }
    std::string wanted=sink.empty()?"":sink+".monitor";
    std::string sources=run_capture_text("pactl list short sources 2>/dev/null");
    std::istringstream in(sources); std::string line, firstMonitor;
    while (std::getline(in,line)) {
        std::istringstream cols(line); std::string index,name; cols>>index>>name;
        if (name.empty()) continue;
        if (name==wanted) return name;
        if (firstMonitor.empty() && name.size()>8 && name.rfind(".monitor")==name.size()-8) firstMonitor=name;
    }
    return !wanted.empty()?wanted:firstMonitor;
}

static void capture_loop(AudioEngine *engine) {
    const std::string source=find_monitor_source();
    if (source.empty()) { std::lock_guard<std::mutex> g(engine->lock); engine->status.message="找不到 PulseAudio/PipeWire monitor source；請確認 pactl 可用"; return; }
    int pipefd[2];
    if (pipe(pipefd)!=0) { std::lock_guard<std::mutex> g(engine->lock); engine->status.message="無法建立 FFmpeg 音訊管線"; return; }
    pid_t pid=fork();
    if (pid==0) {
        dup2(pipefd[1],STDOUT_FILENO); close(pipefd[0]); close(pipefd[1]);
        execlp("ffmpeg","ffmpeg","-hide_banner","-loglevel","error","-nostdin","-f","pulse","-i",source.c_str(),"-ac","2","-ar","48000","-f","s16le","pipe:1",(char*)nullptr);
        _exit(127);
    }
    close(pipefd[1]);
    if (pid<0) { close(pipefd[0]); std::lock_guard<std::mutex> g(engine->lock); engine->status.message="無法啟動 ffmpeg"; return; }
    engine->ffmpegPid=pid;
    {
        std::lock_guard<std::mutex> g(engine->lock); engine->status.running=true; engine->status.monitorSource=source; engine->status.message="正在擷取系統音訊";
    }
    std::vector<int16_t> pcm(4096*2); std::vector<float> mono(4096);
    while (!engine->stopRequested.load()) {
        const ssize_t n=read(pipefd[0],pcm.data(),pcm.size()*sizeof(int16_t));
        if (n>0) {
            const size_t frames=(size_t)n/(sizeof(int16_t)*2);
            for (size_t i=0;i<frames;++i) mono[i]=(pcm[i*2]+pcm[i*2+1])/(2.0f*32768.0f);
            SpectrumPushMono(&engine->spectrum,mono.data(),frames);
            FfmpegMp3Write(&engine->recorder,pcm.data(),frames,2);
        } else if (n==0) break; else if (errno!=EINTR) break;
    }
    close(pipefd[0]);
    if (engine->stopRequested.load()) kill(pid,SIGTERM);
    int st=0; while (waitpid(pid,&st,0)<0 && errno==EINTR) {}
    engine->ffmpegPid=-1;
    std::lock_guard<std::mutex> g(engine->lock); engine->status.running=false;
    if (!engine->stopRequested.load()) engine->status.message="音訊擷取程序已結束；請檢查 ffmpeg 的 PulseAudio 支援與 monitor source";
    else engine->status.message="已停止";
}

bool AudioEngineInitialize(AudioEngine *engine)
{
    if (!engine)
        return false;
    return SpectrumInitialize(&engine->spectrum, 48000);
}

bool AudioEngineStart(AudioEngine *engine) {
    if (!engine) return false;
    if (engine->worker.joinable()) {
        AudioStatus s=AudioEngineGetStatus(engine);
        if (s.running) return true;
        engine->worker.join();
    }
    engine->stopRequested=false; engine->worker=std::thread(capture_loop,engine); return true;
}

void AudioEngineStop(AudioEngine *engine)
{
    if (!engine)
        return;

    engine->stopRequested = true;
    const pid_t pid = engine->ffmpegPid.load();
    if (pid > 0)
        kill(pid, SIGTERM);
    if (engine->worker.joinable())
        engine->worker.join();
    if (engine->recorder.recording)
        FfmpegMp3Stop(&engine->recorder);
    SpectrumReset(&engine->spectrum, 48000);
}

AudioStatus AudioEngineGetStatus(AudioEngine *engine)
{
    if (!engine)
        return {};
    std::lock_guard<std::mutex> guard(engine->lock);
    return engine->status;
}

bool AudioEngineStartMp3(AudioEngine *engine, const std::string &path, int bitrateKbps, int gainDb)
{
    if (!engine)
        return false;
    const AudioStatus status = AudioEngineGetStatus(engine);
    if (!status.running)
        return false;
    return FfmpegMp3Start(&engine->recorder, path, status.sampleRate, bitrateKbps, gainDb);
}

bool AudioEngineStopMp3(AudioEngine *engine) { return engine && FfmpegMp3Stop(&engine->recorder); }
