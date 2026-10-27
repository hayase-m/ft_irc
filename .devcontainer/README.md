# Dev Container

## VS Codeで開く

1. Docker DesktopやOrbStackなど、Dockerを実行できる環境を起動する。
2. VS CodeにDev Containers拡張機能をインストールする。
3. ft_ircリポジトリをVS Codeで開く。
4. コマンドパレットから`Dev Containers: Reopen in Container`を実行する。

コンテナが起動したら、ターミナルでビルドする。

```sh
make
./ircserv 6667 password
```

## Dockerからビルドする

```sh
docker build -f .devcontainer/Dockerfile -t ft-irc-dev .
docker run --rm -v "$PWD:/workspace" -w /workspace ft-irc-dev make
```

生成物を削除する。

```sh
make fclean
```
