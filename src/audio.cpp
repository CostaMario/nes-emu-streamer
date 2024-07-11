#include "audio.h"
#include <cstdio>
#include <stdlib.h>
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <errno.h>

namespace {
constexpr int nominal_frequency = 32000;
constexpr float max_frequency_difference = 0.02f;
constexpr int target_queue_size = nominal_frequency / 60 * 3;
}  // namespace

bool Audio::init() {

  audio_socket = socket(AF_INET, SOCK_DGRAM, 0); 

  sockaddr_in server_address;
  server_address.sin_family = AF_INET; 
  server_address.sin_port = htons(24242); 
  server_address.sin_addr.s_addr =  htonl(INADDR_ANY);

  audio_address.sin_family = AF_INET; 
  audio_address.sin_port = htons(24243); 
  audio_address.sin_addr.s_addr =  htonl(INADDR_ANY);
  
  int result = bind(audio_socket, (struct sockaddr*)&server_address, sizeof(server_address));
  if(result < 0)
  {
    fprintf(stderr, "socket: %s\n", errno);
    exit(2);
  }

  /*if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }

  SDL_AudioSpec audio_spec;
  SDL_zero(audio_spec);
  audio_spec.freq = nominal_frequency;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = NULL;
  if ((audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0)) < 0) {
    fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    return false;
  }
  SDL_PauseAudioDevice(audio_device, 0);*/

  return true;
}

void Audio::destroy() {
  /*SDL_CloseAudioDevice(audio_device);
  SDL_Quit();*/
  close(audio_socket);
}

void Audio::output() {
  //Stream audio
  sendto(audio_socket, nes.apu.output_buffer, nes.apu.sample_count, 0, (struct sockaddr*)&audio_address, sizeof(audio_address));

  /*// Exponential moving average of audio queue size
  int queue_size = SDL_GetQueuedAudioSize(audio_device) / sizeof(int16_t);
  constexpr float alpha = 0.1f;
  average_queue_size =
      (int)(queue_size * alpha + average_queue_size * (1.0f - alpha));

  // Adjust sample frequency to try and maintain a constant queue size
  float diff =
      (float)(average_queue_size - target_queue_size) / target_queue_size;
  diff = std::min(std::max(diff, -1.0f), 1.0f);
  int sample_rate =
      (int)(nominal_frequency * (1.0f - diff * max_frequency_difference));
  
  nes.apu.set_sample_rate(sample_rate);

  if (queue_size > target_queue_size * 2) {
    // Queue is too large, just skip this frame's audio to catch up faster
  } 
  else 
  {
    SDL_QueueAudio(audio_device, nes.apu.output_buffer,
                    nes.apu.sample_count * sizeof(int16_t));
  }*/
  nes.apu.set_sample_rate(nominal_frequency);
  nes.apu.clear_output_buffer();
}