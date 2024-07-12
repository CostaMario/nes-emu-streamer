FROM alpine:3.14

RUN apk update
RUN apk add --no-cache git cmake make gcc g++

RUN git clone https://github.com/CostaMario/nes-emu-streamer.git
WORKDIR nes-emu-streamer
RUN mkdir build
WORKDIR build

RUN cmake ..
RUN make

RUN cp nes-emu /nes-emu

WORKDIR ../..

RUN rm -rf nes-emu-streamer

EXPOSE 24241/udp
EXPOSE 24243/udp

ENTRYPOINT [ "/nes-emu",  "/roms"]
