#!/bin/sh
# This script uses buildah to build the CHERIoT simulator from Sail.
# It will create two container images for caching:
#  - A FreeBSD base system image (freebsd-${VERSION})
#  - An image with Sail installed from opam and configured (sail)
# The final build runs in an ephemeral container and the simulator is copied
# out before the container is destroyed.
#
# Newer versions of the simulator rarely depend on a newer version of Sail.  If
# the build fails, try deleting the sail container image (buildah rmi sail) and
# retrying.

if which -s buildah ; then
	echo Buildah found 
else
	echo Buildah not found, please install with pkg ins buildah
	exit 1
fi

# The FreeBSD project does not yet publish container base images.  This script
# fetches a release tarball and uses it to initialise a container.

# If no arguments are specified, create a FreeBSD 13.1 release, otherwise use
# the provided argument.
VERSION=13.1
if [ -n "$1" ] ; then
	VERSION=$1
fi

# If we have an existing sail container image, use it.
c=$(buildah from sail 2>/dev/null)
if [ $? -ne 0 ] ; then
	# If we have an existing FreeBSD base container, use it.
	c=$(buildah from freebsd-${VERSION} 2>/dev/null)
	if [ $? -ne 0 ] ; then
		echo FreeBSD ${VERSION} container does not exist, building...
		c=$(buildah from scratch)
		BASE=$(mktemp)
		echo Fetching base tarball...
		fetch -o ${BASE} https://download.freebsd.org/ftp/releases/$(uname -p)/${VERSION}-RELEASE/base.txz
		buildah copy ${c} ${BASE} /base.txz
		rm ${BASE}
		echo Copying tar into container...
		buildah copy ${c} /rescue/tar /tar
		echo Extracting...
		buildah run ${c} /tar -xpf base.txz
		echo Deleting temporary files in container
		buildah run ${c} rm /tar /base.txz
		echo Committing freebsd-${VERSION} image...
		buildah commit ${c} freebsd-${VERSION}
	fi
	echo Sail container does not exist, building...
	# Install dependencies
	buildah run --env IGNORE_OSVERSION=yes ${c} -- pkg ins --yes ocaml-opam z3 gmp gmake pkgconf
	# Set up opam
	buildah run --terminal ${c} -- opam init --yes
	buildah run --terminal ${c} -- opam install --yes sail
	buildah commit ${c} sail
fi

# Clone the CHERIoT Sail repo
buildah run ${c} -- git clone --recurse \
   https://github.com/microsoft/cheriot-sail

# Create a shell script that runs the build with the correct environment
# variables set, we'll run that as a single build step.
BUILDSH=$(mktemp)
# The patch step turns each patch into a git commit, so needs an owner set.
echo  'git config --global user.email "root@localhost"' > ${BUILDSH}
echo  'git config --global user.name "Charlie Root"' >> ${BUILDSH}
# Patch the RISC-V repo
echo 'gmake patch_sail_riscv' >> ${BUILDSH}
# Set the ocaml environment variables.
echo 'eval $(opam env)' >> ${BUILDSH}
# Build cheriot_sim
echo "gmake csim C_OPT_FLAGS='-O4 -flto=full'" >> ${BUILDSH}
# Copy the build script into the container.
echo build.sh
cat $BUILDSH
buildah copy --chmod 555 ${c} ${BUILDSH} /cheriot-sail/build.sh
rm ${BUILDSH}
# Run the build
buildah run --env OPAMROOT=/.opam --workingdir cheriot-sail ${c} -- sh ./build.sh
# Copy the built simulator out
m=$(buildah mount ${c})
cp ${m}/cheriot-sail/c_emulator/cheriot_sim .
buildah umount ${c}
buildah rm ${c}
# Make the simulator executable
chmod +x cheriot_sim
