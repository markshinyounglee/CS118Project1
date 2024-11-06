# CS 118 Fall 24 Project 0

This repository contains a reference solution and autograder for [CS 118's Fall 24 Project
0](https://docs.google.com/document/d/1O6IuX39E4PoMvQ9uP98AWayqCgmnoBUoRfKCUZboKwg).


Since I was implementing this project in Linux, I had to give 
sudo access to my username to be able to run docker without sudo command. I did the following:

```shell
sudo groupadd docker
sudo usermod -aG docker $USER
newgrp docker
newgrp docker
sudo reboot # reboot the Linux machine to update the change
```
which can also be seen at: https://docs.docker.com/engine/install/linux-postinstall/
