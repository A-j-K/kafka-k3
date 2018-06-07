# Developing on K3

## Requirements

A local dev machine running Docker (best use Ubuntu)

## Step 1

Clone the repo.

Build the entire package using the ```build.sh``` script. This ensures all the containers are now built out.

## Step 2

Create a test version of ```/etc/k3conf.json``` configuration with appropiate values (see README.md)

In a seperate bash shell start a k3:dev container and build a local k3 instance:

```
$ docker run --cap-add=SYS_PTRACE --security-opt=seccomp:unconfined --privileged --rm -v /etc/k3conf.json:/etc/k3conf.json -v `pwd`:/ws -it k3:dev
bash-4.4# cd /tmp
bash-4.4# cmake /ws
bash-4.4# make
```

You should now have main/k3 program built.

## Step 3

Run a test (note the /etc/k3conf.json was mounted from your host at the previous step)

```
bash-4.4# main/k3
```

## Step 4

You can now swap back to the other bash shell window and work on the code. To build changes you swap bash shells and just rerun ```make``` and test again.

