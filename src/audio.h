#pragma once
#include "nes/nes.h"
#include <netinet/in.h> 
#include <sys/socket.h> 

class Audio {
 public:
  Audio(NES& nes) : nes(nes) {}
  bool init();
  void destroy();
  void output();
  int audio_socket;
  sockaddr_in audio_address;

 private:
  NES& nes;

  //SDL_AudioDeviceID audio_device;
  int average_queue_size = 0;
};