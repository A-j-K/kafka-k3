#!/bin/bash

set -e

AWS_SDK_CPP_VER_MAJOR="1"
AWS_SDK_CPP_VER_MINOR="4"
AWS_SDK_CPP_VER_PATCH="62"

VER=`head -1 CHANGELOG.txt`

# Start by building a container that holds a static
# library for SASL. We have to do this as it uses libreSSL
# rather than OpenSSL and Alpine:3.7 appears to have 
# a package manager conflict between these two SSL libraries
# Builds k3:sasl
#
# Build the dev builder env that uses COPY --from k3:sasl to bring \
# in the static SASL library to link against. This dev stage build \
# librdkafa, libcurl (again Alpine packake manager issues) and the \
# AWS C++ SDK (all static builds). Finally it builds the k3 app. \
# Builds k3:dev
#
# The last step is to create an image with a COPY --from from the \
# above to produce a tiny Docker image we can push to the repo. \
# Builds k3:rel-${VER}

if [[ ! -z $BUILD_WORLD && $BUILD_WORLD == "yes" ]]; then
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
	. 
else
echo "Short build..."
fi
docker build -f Dockerfile.app --tag k3:app . \
&& docker build -f Dockerfile.rel --tag k3:rel-${VER} . \
&& echo "docker tag k3:rel-${VER} andykirkham/kafka-k3:${VER}" \
&& echo "docker push andykirkham/kafka-k3:${VER}" 


