/*
 * SystemAudioRecorder - Linux reference port
 * Traditional Chinese terminal interface.
 */
#include "audio.h"
#include "queue.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

static void help() {
    std::cout <<
    "SystemAudioRecorder Linux 參考移植版\n"
    "指令：\n"
    "  start                         開始擷取系統音訊\n"
    "  stop                          停止擷取\n"
    "  spectrum                      顯示目前頻譜\n"
    "  record <file.mp3> [gain]      開始 MP3 錄音（gain 預設 6 dB）\n"
    "  stop-record                   停止錄音並轉換 MP3\n"
    "  add-audio <url>               加入音訊下載佇列\n"
    "  add-video <url>               加入影片下載佇列\n"
    "  queue                         顯示下載佇列\n"
    "  status                        顯示擷取狀態\n"
    "  help                          顯示說明\n"
    "  quit                          結束\n";
}

static void show_spectrum(AudioEngine &engine) {
    SpectrumSnapshot s; SpectrumGetSnapshot(&engine.spectrum,&s); const auto &centers=SpectrumGetBandCenters();
    std::cout << "RMS " << std::fixed << std::setprecision(1) << s.rmsDb << " dBFS | 主峰 " << std::setprecision(0) << s.dominantHz << " Hz\n";
    for(int i=0;i<SPECTRUM_BAND_COUNT;++i){ int bars=(int)((s.bandDb[i]+90.0f)/4.5f); if(bars<0)bars=0;if(bars>20)bars=20;
        std::cout<<std::setw(6)<<centers[i]<<" Hz | "<<std::string((size_t)bars,'#')<<"\n"; }
}

int main(int argc,char **argv) {
    if(argc>1 && std::string(argv[1])=="--help"){help();return 0;}
    std::cout << "SystemAudioRecorder " << SYSTEM_AUDIO_RECORDER_VERSION << "\n";
    std::cout << "Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.\n\n";
    AudioEngine engine; if(!AudioEngineInitialize(&engine)){std::cerr<<"音訊引擎初始化失敗\n";return 1;}
    DownloadQueue queue("downloads"); help();
    std::string line;
    while(std::cout<<"\n> " && std::getline(std::cin,line)){
        std::istringstream in(line);std::string cmd;in>>cmd;if(cmd.empty())continue;
        if(cmd=="start"){AudioEngineStart(&engine);std::this_thread::sleep_for(std::chrono::milliseconds(200));std::cout<<AudioEngineGetStatus(&engine).message<<"\n";}
        else if(cmd=="stop"){AudioEngineStop(&engine);std::cout<<"已停止\n";}
        else if(cmd=="spectrum")show_spectrum(engine);
        else if(cmd=="record"){std::string path;int gain=6;in>>path>>gain;if(path.empty())std::cout<<"請指定 MP3 檔名\n";else std::cout<<(AudioEngineStartMp3(&engine,path,192,gain)?"開始錄音":"無法開始錄音")<<"\n";}
        else if(cmd=="stop-record"){AudioEngineStopMp3(&engine);std::cout<<FfmpegMp3Message(&engine.recorder)<<"\n";}
        else if(cmd=="add-audio"||cmd=="add-video"){std::string url;in>>url;if(url.empty())std::cout<<"請輸入 URL\n";else{int id=queue.add(url,cmd=="add-audio"?DownloadMode::Audio:DownloadMode::Video);std::cout<<"已加入佇列 #"<<id<<"\n";}}
        else if(cmd=="queue"){for(const auto&i:queue.snapshot())std::cout<<"#"<<i.id<<" "<<(i.mode==DownloadMode::Audio?"音訊":"影片")<<" "<<i.state<<" "<<i.url<<"\n";}
        else if(cmd=="status"){auto s=AudioEngineGetStatus(&engine);std::cout<<(s.running?"擷取中":"未擷取")<<" | monitor: "<<s.monitorSource<<" | "<<s.message<<"\n";}
        else if(cmd=="help")help();
        else if(cmd=="quit"||cmd=="exit")break;
        else std::cout<<"未知指令；輸入 help 查看說明\n";
    }
    AudioEngineStop(&engine); return 0;
}
