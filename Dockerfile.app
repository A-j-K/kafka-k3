
FROM k3:dev 
 
COPY . /buildspace
RUN	cd /buildspace && mkdir build && cd build \
	&& cmake .. && make VERBOSE=1 && cp main/k3 /usr/bin/k3 \
	&& cd / && rm -rf buildspace
WORKDIR /tmp
CMD ["/bin/bash"]


