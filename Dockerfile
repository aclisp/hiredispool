FROM debian:stretch
LABEL maintainer="Alan Vieyra <alan.vieyra376@gmail.com>"

ENV DEBIAN_FRONTEND noninteractive

ADD $PWD /usr/src/hiredispool

WORKDIR /usr/src/hiredispool

RUN set -ex; \
	\
	savedAptMark="$(apt-mark showmanual)"; \
	\
	apt-get update; \
	apt-get install -y --no-install-recommends \
		build-essential \
		ca-certificates \
		cmake \
		libev-dev \
	; \
	\
	make install; \
	\
	apt-mark auto '.*' > /dev/null; \
	apt-mark manual $savedAptMark; \
	apt-get purge -y --auto-remove -o APT::AutoRemove::RecommendsImportant=false; \
	rm -rf /var/lib/apt/lists/*