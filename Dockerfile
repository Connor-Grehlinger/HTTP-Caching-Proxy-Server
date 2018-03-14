FROM gcc

RUN mkdir /src
WORKDIR /src

RUN mkdir /config
ADD ./config /config/

RUN apt-get update && apt-get install -y libpoco-dev

ADD ./src /src


RUN chmod +x /config/entrypoint.sh
RUN chmod +rx proxy_server.cpp
