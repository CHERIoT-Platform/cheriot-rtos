FROM ubuntu:20.04
ARG USERNAME=cheriot
ARG LLVM_PATH=.
ARG SAIL_PATH=.

RUN apt update \
    && apt upgrade -y \
    && apt install -y software-properties-common \
    && add-apt-repository ppa:xmake-io/xmake \
    && apt update \
    && apt install -y xmake git clang

# Create the user
RUN useradd -m $USERNAME \
    # Add sudo support. Omit if you don't need to install software after connecting.
    && apt-get install -y sudo \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

RUN mkdir -p /cheriot-tools/licenses
COPY ${LLVM_PATH}/LLVM-LICENSE.TXT /cheriot-tools/licenses/
COPY ${SAIL_PATH}/LICENCE-cheriot-sail.txt ${SAIL_PATH}/LICENCE-sail-riscv.txt /cheriot-tools/licenses/
RUN mkdir -p /cheriot-tools/bin
# FIXME: We should be installing objdump as well
COPY "${LLVM_PATH}/bin/clang" "${LLVM_PATH}/bin/lld" "${LLVM_PATH}/bin/llvm-objdump" "${LLVM_PATH}/bin/clangd" "${LLVM_PATH}/bin/clang-format" "${LLVM_PATH}/bin/clang-tidy" /cheriot-tools/bin/
COPY "${SAIL_PATH}/cheriot_sim" /cheriot-tools/bin/
RUN cd /cheriot-tools/bin && ln -s clang clang++ && ln -s lld ld.lld && chmod +x *
USER $USERNAME
ENV SHELL /bin/bash
CMD bash
