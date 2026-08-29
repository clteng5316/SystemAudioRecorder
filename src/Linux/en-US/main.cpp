/*
 * SystemAudioRecorder - Linux reference port
 * English (United States) terminal interface.
 */
#include "audio.h"
#include "queue.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

static void help() {
    std::cout <<
    "SystemAudioRecorder Linux Reference Port\n"
    "Commands:\n"
    "  start                         Start system-audio capture\n"
    "  stop                          Stop capture\n"
    "  spectrum                      Show the current spectrum\n"
    "  record <file.mp3> [gain]      Start MP3 recording (default gain: 6 dB)\n"
    "  stop-record                   Stop recording and convert to MP3\n"
    "  add-audio <url>               Add an audio download to the queue\n"
    "  add-video <url>               Add a video download to the queue\n"
    "  queue                         Show the download queue\n"
    "  status                        Show capture status\n"
    "  help                          Show command help\n"
    "  quit                          Exit\n";
}

static void show_spectrum(AudioEngine &engine) {
    SpectrumSnapshot s; SpectrumGetSnapshot(&engine.spectrum,&s); const auto &centers=SpectrumGetBandCenters();
    std::cout << "RMS " << std::fixed << std::setprecision(1) << s.rmsDb << " dBFS | Peak " << std::setprecision(0) << s.dominantHz << " Hz\n";
    for(int i=0;i<SPECTRUM_BAND_COUNT;++i){ int bars=(int)((s.bandDb[i]+90.0f)/4.5f); if(bars<0)bars=0;if(bars>20)bars=20;
        std::cout<<std::setw(6)<<centers[i]<<" Hz | "<<std::string((size_t)bars,'#')<<"\n"; }
}

int main(int argc,char **argv) {
    if(argc>1 && std::string(argv[1])=="--help"){help();return 0;}
    std::cout << "SystemAudioRecorder " << SYSTEM_AUDIO_RECORDER_VERSION << "\n";
    std::cout << "Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.\n\n";
    AudioEngine engine; if(!AudioEngineInitialize(&engine)){std::cerr<<"Audio engine initialization failed\n";return 1;}
    DownloadQueue queue("downloads"); help();
    std::string line;
    while(std::cout<<"\n> " && std::getline(std::cin,line)){
        std::istringstream in(line);std::string cmd;in>>cmd;if(cmd.empty())continue;
        if(cmd=="start"){AudioEngineStart(&engine);std::this_thread::sleep_for(std::chrono::milliseconds(200));std::cout<<AudioEngineGetStatus(&engine).message<<"\n";}
        else if(cmd=="stop"){AudioEngineStop(&engine);std::cout<<"Stopped\n";}
        else if(cmd=="spectrum")show_spectrum(engine);
        else if(cmd=="record"){std::string path;int gain=6;in>>path>>gain;if(path.empty())std::cout<<"Please specify an MP3 file name\n";else std::cout<<(AudioEngineStartMp3(&engine,path,192,gain)?"Recording started":"Unable to start recording")<<"\n";}
        else if(cmd=="stop-record"){AudioEngineStopMp3(&engine);std::cout<<FfmpegMp3Message(&engine.recorder)<<"\n";}
        else if(cmd=="add-audio"||cmd=="add-video"){std::string url;in>>url;if(url.empty())std::cout<<"Please enter a URL\n";else{int id=queue.add(url,cmd=="add-audio"?DownloadMode::Audio:DownloadMode::Video);std::cout<<"Added to queue #"<<id<<"\n";}}
        else if(cmd=="queue"){for(const auto&i:queue.snapshot())std::cout<<"#"<<i.id<<" "<<(i.mode==DownloadMode::Audio?"Audio":"Video")<<" "<<i.state<<" "<<i.url<<"\n";}
        else if(cmd=="status"){auto s=AudioEngineGetStatus(&engine);std::cout<<(s.running?"Capturing":"Not capturing")<<" | monitor: "<<s.monitorSource<<" | "<<s.message<<"\n";}
        else if(cmd=="help")help();
        else if(cmd=="quit"||cmd=="exit")break;
        else std::cout<<"Unknown command; enter help to show available commands\n";
    }
    AudioEngineStop(&engine); return 0;
}
