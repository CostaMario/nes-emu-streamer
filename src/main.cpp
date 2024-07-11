#include <chrono>
#include <cstdio>
#include <stdlib.h>
#include <stdexcept>
#include "audio.h"
#include "nes/nes.h"
#include "renderer.h"
#include <fstream>
#include <netinet/in.h> 
#include <sys/types.h>
#include <sys/socket.h> 
#include <errno.h>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

NES nes;
Renderer renderer(nes);
Audio audio(nes);
double last_time = 0.0f;
double accumulator = 0.0f;
double connection_accumulator = 0.0f;
bool connected = false;
bool try_connecting = false;

int input_socket;
int client_input_socket;
sockaddr_in input_address;

bool send_input = false;
bool wait_input = false;
bool received_input = false;

std::vector<std::string> rom_dirs;
uint16_t rom_count;

int checkSocket(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1 && errno != 0) {
        //perror("fcntl() failed"); // Handle error in your application
        return 0;
    } else {
        return 1;
    }
}

void advertiseRoms()
{
  for(int i = 0; i < rom_count; i++)
  {
    send(client_input_socket, rom_dirs[i].c_str(), rom_dirs[i].length(), 0);
    uint8_t buffer[5] = {255};
    send(client_input_socket, buffer, sizeof(buffer), 0);
  }

  uint8_t buffer[5] = {255};
  send(client_input_socket, buffer, sizeof(buffer), 0);
}

void loop() {

  if(checkSocket(client_input_socket))
  {
    if(connection_accumulator > 1.0 * 1000)
    {
      fd_set read_fds;
      struct timeval timeout;

      FD_ZERO(&read_fds);
      FD_SET(client_input_socket, &read_fds);

      timeout.tv_sec = 0;
      timeout.tv_usec = 0;

      int result = select(client_input_socket + 1, &read_fds, NULL, NULL, &timeout);
      connection_accumulator = 0.0;
      connection_accumulator = 0;
      printf("Test connection %d\n", result);
      if(result == -1)
      {
        printf("ERROR");
      }
      else if(result == 0)
      {
        connected = true;
      }
      else
      {
        char buffer;
        result = recv(client_input_socket, &buffer, 1, MSG_PEEK);
        if (result == 0) {
            // Connection has been closed
            connected = false;
        } else if (result == -1) {
            // Error occurred
            connected = false;
        } else {
            // Data is available, socket is still connected
            connected = true;
        }
      }
    }
  }
  else
  {
    connected = false;
  }

  if(connected)
  {
    try_connecting = false;
    if(!send_input)
    {
      uint8_t buffer[16] = {0};
      socklen_t slen = sizeof(input_address);
      int res = recv(client_input_socket, buffer, sizeof(buffer), 0);
      
      if(res == -1)
      {int s = send(client_input_socket, buffer, sizeof(buffer), 0);
        fprintf(stderr, "socket: %s\n", strerror(errno));
      }
      else if(res == 0)
      {
        //client_input_socket = accept(input_socket, nullptr, nullptr);
      }
      else
      {
        connection_accumulator = 0.0;
        send_input = true;
        for(int i = 0; i < 9; i++)
        {
          nes.joypad.set_button_state(0, (Button)(i - 1), buffer[i]);
        }
        wait_input = buffer[15] > 0;
        if(buffer[14] != 0 && buffer[14] < rom_count + 1)
        {
          printf("ATTEMPTING TO LOAD %s\n", rom_dirs[buffer[14] - 1].c_str());
          nes.load(rom_dirs[buffer[14] - 1].c_str());
        }
        received_input = true;
      }
    }
    else
    {
      uint8_t buffer[1] = {255};
      int s = send(client_input_socket, buffer, sizeof(buffer), 0);
      if(s == 1)
      {
        send_input = false;
      }
    }

    //printf("Received %d bytes\n", res);
  }
  else
  {
    if(!try_connecting)
    {
      printf("Trying connection...\n");
      int flags = fcntl(input_socket, F_GETFL);
      flags &= ~O_NONBLOCK;
      fcntl(input_socket, F_SETFL, flags);
      client_input_socket = accept(input_socket, nullptr, nullptr);
      advertiseRoms();
      flags = fcntl(input_socket, F_GETFL);
      flags &= ~O_NONBLOCK;
      fcntl(input_socket, F_SETFL, flags | O_NONBLOCK);
      try_connecting = true;
    }
    for(int i = 0; i < 16; i++)
    {
      nes.joypad.set_button_state(0, (Button)(i - 1), false);
    }
  }

  double time = renderer.time();
  double delta = time - last_time;
  last_time = time;
  constexpr double target_frame_time = 1.0 / 60.0 * 1000;
  // Set delta to exactly 1 / 60 if within 3%
  if (std::abs(delta - target_frame_time) / target_frame_time <= 0.03) {
    delta = target_frame_time;
  }
  accumulator = std::min(accumulator + delta, target_frame_time * 3);
  connection_accumulator += delta;
  while ((accumulator >= target_frame_time && wait_input == false) || (wait_input && received_input)) {
    nes.run_frame();
    audio.output();
    accumulator -= target_frame_time;
    received_input = false;
  }
  renderer.render();
}

void handle_close(int s){
  printf("Caught signal %d\n",s);
  close(input_socket);
  close(client_input_socket);
  renderer.destroy();
  audio.destroy();
  exit(1);
}

int main(int argc, char* agv[]) {

  struct sigaction sigIntHandler;

  sigIntHandler.sa_handler = handle_close;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);

  if (!renderer.init()) {
    return -1;
  }
  if (!audio.init()) {
    return -1;
  }

  // Setup input socket
  input_socket = socket(AF_INET, SOCK_STREAM, 0);

  int flags = fcntl(input_socket, F_GETFL);
  fcntl(input_socket, F_SETFL, flags | O_NONBLOCK);
  
  input_address.sin_family = AF_INET;
  input_address.sin_port = htons(24240);
  input_address.sin_addr.s_addr = htonl(INADDR_ANY);

  int result = bind(input_socket, (struct sockaddr*)&input_address, sizeof(input_address));
  
  if(result < 0)
  {
    fprintf(stderr, "socket: %s\n", strerror(errno));
    exit(1);
  }

  result = listen(input_socket, 5);

  if(result < 0)
  {
    fprintf(stderr, "socket: %s\n", strerror(errno));
    exit(1);
  }

  client_input_socket = accept(input_socket, nullptr, nullptr);

  printf("Connected to client!\n");

  if(argc > 1)
  {
    if(std::filesystem::is_directory(agv[1]))
    {
      std::filesystem::directory_iterator dir(agv[1]);
      std::filesystem::directory_iterator end;
      rom_count = 1;
      rom_dirs.push_back("assets/nestest.nes");
      while(dir != end)
      {
        rom_count++;
        rom_dirs.push_back(dir->path().string());
        dir++;
      }
      printf("iterated over path\n");
    }
    else
    {
      std::ifstream file(agv[1], std::ios::in);
      if(!file)
      {
          fprintf(stderr, "Could not load cartridge file %s\n", agv[1]);
          return 0;
      }
      else
      {
        nes.load(agv[1]);
      }
    }
  }
  else
  {
    nes.load("assets/nestest.nes");
  }

  printf("Starting emulation...\n");
  while (!renderer.done()) {
    loop();
  }

  renderer.destroy();
  audio.destroy();
  return 0;
}