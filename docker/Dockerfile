FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-13 g++-13 \
        cmake \
        ninja-build \
        git \
        python3 \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-13 CXX=g++-13

WORKDIR /src
COPY . /src

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --target notification_service -j

RUN python3 scripts/gen_dataset.py --count 100000 --out /src/data/notifications.jsonl

FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 \
        ca-certificates \
        curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/notification_service /app/notification_service
COPY --from=build /src/data/ /app/data/

EXPOSE 8080
ENTRYPOINT ["/app/notification_service"]
