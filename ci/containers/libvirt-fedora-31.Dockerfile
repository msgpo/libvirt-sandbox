FROM fedora:31

RUN dnf update -y && \
    dnf install -y \
        autoconf \
        automake \
        bash \
        bash-completion \
        ca-certificates \
        ccache \
        chrony \
        cppi \
        gcc \
        gdb \
        gettext \
        gettext-devel \
        git \
        glib2-devel \
        glibc-devel \
        glibc-langpack-en \
        glibc-static \
        gobject-introspection-devel \
        gtk-doc \
        libcap-ng-devel \
        libselinux-devel \
        libtirpc-devel \
        libtool \
        libvirt-devel \
        libvirt-gobject-devel \
        libxml2-devel \
        lsof \
        make \
        net-tools \
        ninja-build \
        patch \
        perl \
        perl-App-cpanminus \
        pkgconfig \
        python3 \
        python3-pip \
        python3-setuptools \
        python3-wheel \
        rpcgen \
        rpm-build \
        screen \
        strace \
        sudo \
        vim \
        xz \
        xz-devel \
        xz-static \
        zlib-devel \
        zlib-static && \
    dnf autoremove -y && \
    dnf clean all -y && \
    mkdir -p /usr/libexec/ccache-wrappers && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/cc && \
    ln -s /usr/bin/ccache /usr/libexec/ccache-wrappers/$(basename /usr/bin/gcc)

RUN pip3 install \
         meson==0.54.0

ENV LANG "en_US.UTF-8"

ENV MAKE "/usr/bin/make"
ENV NINJA "/usr/bin/ninja"
ENV PYTHON "/usr/bin/python3"

ENV CCACHE_WRAPPERSDIR "/usr/libexec/ccache-wrappers"
