# Dev Container

## VS Codeで開く

1. Docker DesktopやOrbStackなど、Dockerを実行できる環境を起動する。
2. VS CodeにDev Containers拡張機能をインストールする。
3. ft_ircリポジトリをVS Codeで開く。
4. コマンドパレットから`Dev Containers: Reopen in Container`を実行する。

macOSとDocker（Dev Containerを含む）を切り替える場合は、
ビルド前に必ず現在の環境で`make fclean`を実行して生成物を作り直す。
コンテナが起動したら、ターミナルでビルドする。

```sh
make fclean
make
./ircserv 6667 password
```

## Dockerからビルドする

```sh
docker build -f .devcontainer/Dockerfile -t ft-irc-dev .
docker run --rm -v "$PWD:/workspace" -w /workspace ft-irc-dev make fclean
docker run --rm -v "$PWD:/workspace" -w /workspace ft-irc-dev make
```

生成物を削除する。

```sh
make fclean
```
