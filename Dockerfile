# FROM ubuntu:20.04

# COPY . /vpp
# WORKDIR /vpp
# RUN apt-get update && apt-get install -y make sudo tzdata
# RUN make install-dep
# RUN make pkg-deb

FROM nikitaxored/vpp-deps:4

COPY . /vpp
WORKDIR /vpp

# RUN dpkg -i build-root/*.deb

ENTRYPOINT [ "tail", "-f", "/dev/null" ]