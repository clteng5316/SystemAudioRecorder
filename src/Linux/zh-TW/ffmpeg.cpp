/* SystemAudioRecorder - Linux WAV/MP3 recorder */
#include "ffmpeg.h"
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void put_u16(FILE *f, uint16_t v) { unsigned char b[2]={(unsigned char)(v&255),(unsigned char)((v>>8)&255)}; fwrite(b,1,2,f); }
static void put_u32(FILE *f, uint32_t v) { unsigned char b[4]={(unsigned char)(v&255),(unsigned char)((v>>8)&255),(unsigned char)((v>>16)&255),(unsigned char)((v>>24)&255)}; fwrite(b,1,4,f); }

static bool write_header(FILE *f, uint32_t rate, uint16_t channels, uint32_t dataBytes) {
    if (!f) return false;
    rewind(f);
    fwrite("RIFF",1,4,f); put_u32(f,36u+dataBytes); fwrite("WAVEfmt ",1,8,f); put_u32(f,16); put_u16(f,1);
    put_u16(f,channels); put_u32(f,rate); put_u32(f,rate*channels*2); put_u16(f,channels*2); put_u16(f,16);
    fwrite("data",1,4,f); put_u32(f,dataBytes);
    return !ferror(f);
}

static bool command_exists(const char *name) {
    pid_t pid=fork();
    if (pid==0) { execlp(name,name,"-version",(char*)nullptr); _exit(127); }
    if (pid<0) return false;
    int st=0; waitpid(pid,&st,0);
    return WIFEXITED(st) && WEXITSTATUS(st)!=127;
}

bool FfmpegAvailable() { return command_exists("ffmpeg"); }

bool FfmpegMp3Start(Mp3Recorder *rec, const std::string &path, uint32_t sampleRate, int bitrateKbps, int gainDb) {
    if (!rec || path.empty()) return false;
    std::lock_guard<std::mutex> guard(rec->lock);
    if (rec->recording) { rec->message="目前已有錄音正在進行"; return false; }
    rec->targetPath=path; rec->tempPath=path+".recording.wav"; rec->sampleRate=sampleRate; rec->channels=2;
    rec->bitrateKbps=bitrateKbps; rec->gainDb=gainDb; rec->dataBytes=0;
    rec->wave=fopen(rec->tempPath.c_str(),"wb+");
    if (!rec->wave) { rec->message="無法建立暫存 WAV："+std::string(strerror(errno)); return false; }
    if (!write_header(rec->wave,sampleRate,rec->channels,0)) { fclose(rec->wave); rec->wave=nullptr; rec->message="無法寫入 WAV 標頭"; return false; }
    fseek(rec->wave,44,SEEK_SET); rec->recording=true; rec->message="正在錄音到暫存 WAV"; return true;
}

bool FfmpegMp3Write(Mp3Recorder *rec, const int16_t *samples, size_t frames, uint16_t channels) {
    if (!rec || !samples || frames==0) return false;
    std::lock_guard<std::mutex> guard(rec->lock);
    if (!rec->recording || !rec->wave) return true;
    if (channels != rec->channels) { rec->message="錄音聲道格式改變，已停止寫入"; return false; }
    const size_t n=frames*channels;
    if (rec->dataBytes + n*sizeof(int16_t) > 0xFFFFFFD0ull) { rec->message="錄音已達傳統 WAV 4 GB 上限"; return false; }
    const size_t wrote=fwrite(samples,sizeof(int16_t),n,rec->wave);
    rec->dataBytes += wrote*sizeof(int16_t);
    if (wrote!=n) { rec->message="寫入暫存 WAV 失敗"; return false; }
    return true;
}

static int run_ffmpeg(const Mp3Recorder &r) {
    const std::string bitrate = std::to_string(r.bitrateKbps) + "k";
    const std::string rate = std::to_string(r.sampleRate);
    std::string filter;
    if (r.gainDb>0) filter="volume="+std::to_string(r.gainDb)+"dB,alimiter=limit=0.95";
    pid_t pid=fork();
    if (pid==0) {
        if (!filter.empty())
            execlp("ffmpeg","ffmpeg","-hide_banner","-loglevel","error","-nostdin","-y","-i",r.tempPath.c_str(),"-vn","-af",filter.c_str(),"-ar",rate.c_str(),"-codec:a","libmp3lame","-b:a",bitrate.c_str(),r.targetPath.c_str(),(char*)nullptr);
        else
            execlp("ffmpeg","ffmpeg","-hide_banner","-loglevel","error","-nostdin","-y","-i",r.tempPath.c_str(),"-vn","-ar",rate.c_str(),"-codec:a","libmp3lame","-b:a",bitrate.c_str(),r.targetPath.c_str(),(char*)nullptr);
        _exit(127);
    }
    if (pid<0) return -1;
    int st=0; while (waitpid(pid,&st,0)<0 && errno==EINTR) {}
    return WIFEXITED(st)?WEXITSTATUS(st):-1;
}

bool FfmpegMp3Stop(Mp3Recorder *rec) {
    if (!rec) return false;
    Mp3Recorder copy;
    {
        std::lock_guard<std::mutex> guard(rec->lock);
        if (!rec->recording) { rec->message="目前沒有錄音"; return false; }
        rec->recording=false;
        if (!rec->wave) return false;
        if (!write_header(rec->wave,rec->sampleRate,rec->channels,(uint32_t)rec->dataBytes)) { rec->message="無法完成 WAV 標頭"; fclose(rec->wave); rec->wave=nullptr; return false; }
        fclose(rec->wave); rec->wave=nullptr;
        if (rec->dataBytes==0) { rec->message="沒有錄到音訊樣本"; return false; }
        copy.targetPath=rec->targetPath; copy.tempPath=rec->tempPath; copy.sampleRate=rec->sampleRate; copy.bitrateKbps=rec->bitrateKbps; copy.gainDb=rec->gainDb;
        rec->message="錄音已停止，正在轉換 MP3...";
    }
    const int rc=run_ffmpeg(copy);
    std::lock_guard<std::mutex> guard(rec->lock);
    if (rc==0) { unlink(rec->tempPath.c_str()); rec->message="MP3 轉換完成："+rec->targetPath; return true; }
    rec->message="FFmpeg MP3 轉換失敗，暫存 WAV 已保留："+rec->tempPath; return false;
}

std::string FfmpegMp3Message(Mp3Recorder *rec)
{
    if (!rec)
        return "";
    std::lock_guard<std::mutex> guard(rec->lock);
    return rec->message;
}
