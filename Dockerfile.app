
FROM k3:dev 
 
COPY . /buildspace
RUN	cd /buildspace && mkdir build && cd build \
	&& cmake .. -DBUILD_DEBUG="yes" && make VERBOSE=1 \
	&& cp main/k3 /usr/bin/k3 \
	&& cp k3backup/k3backup /usr/bin/k3backup \
	&& cp k3replay/k3replay /usr/bin/k3replay \
	&& cd / && rm -rf buildspace
WORKDIR /tmp
CMD ["/bin/bash"]


