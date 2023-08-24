# FROM ubuntu:20.04

# COPY . /vpp
# WORKDIR /vpp
# RUN apt-get update && apt-get install -y make sudo tzdata iproute2
# RUN make install-dep

FROM nikitaxored/vpp-deps:3

COPY . /vpp
WORKDIR /vpp
# RUN make pkg-deb
# RUN sudo dpkg -i build-root/*.deb

ENTRYPOINT [ "tail", "-f", "/dev/null" ]