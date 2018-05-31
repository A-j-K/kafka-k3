#!/bin/bash

AWS_SDK_CPP_VER_MAJOR="1"
AWS_SDK_CPP_VER_MINOR="4"
AWS_SDK_CPP_VER_PATCH="57"

docker build \
	-f Dockerfile.sasl \
	--tag k3:sasl \
	. \
&& docker build \
	-f Dockerfile.dev \
	--build-arg AWS_SDK_CPP_VER_MAJOR=$AWS_SDK_CPP_VER_MAJOR \
	--build-arg AWS_SDK_CPP_VER_MINOR=$AWS_SDK_CPP_VER_MINOR \
	--build-arg AWS_SDK_CPP_VER_PATCH=$AWS_SDK_CPP_VER_PATCH \
	--build-arg BUILD_SHARED_LIBS="OFF" \
	--build-arg ENABLE_UNITY_BUILD="ON" \
	--tag k3:dev \
	. \
&& docker build -f Dockerfile.rel --tag k3:rel .

