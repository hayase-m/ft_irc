FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

RUN apt-get update \
	&& DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
		build-essential \
		clang \
		clang-format \
		gdb \
		less \
		man-db \
		manpages \
		manpages-dev \
		netcat-openbsd \
		tzdata \
		valgrind \
	&& update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100 \
	&& update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 100 \
	&& ln -sfn /usr/share/zoneinfo/Asia/Tokyo /etc/localtime \
	&& apt-get clean \
	&& rm -rf /var/lib/apt/lists/*

ENV TZ=Asia/Tokyo
