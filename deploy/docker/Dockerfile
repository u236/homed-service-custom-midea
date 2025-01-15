FROM    127.0.0.1:5000/homed-service:latest
ARG     TARGETARCH
RUN     mkdir /etc/homed
RUN     ln -s /data/homed-custom-midea.conf /etc/homed/homed-custom-midea.conf
RUN     ln -s /data/homed.log /var/log/homed.log
COPY    homed-custom-midea-$TARGETARCH /usr/bin/homed-custom-midea
CMD     ["/usr/bin/homed-custom-midea"]
