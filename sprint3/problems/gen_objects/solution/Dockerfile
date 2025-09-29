FROM gcc:11.3 AS build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan \
    && \
    conan profile detect

# Запуск conan как раньше
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11

# Папка data больше не нужна
COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 AS run

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www

# Копируем билд артефакт и данные
COPY --from=build /app/build/game_server /app/game_server
COPY ./data /app/data
COPY ./static /app/static

# Устанавливаем владельца для скопированных файлов
RUN chown -R www:www /app

USER www


# Запускаем игровой сервер
ENTRYPOINT ["/app/game_server"]
CMD ["--config-file", "/app/data/config.json", "--www-root", "/app/static"]