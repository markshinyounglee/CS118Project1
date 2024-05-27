FROM gradescope/autograder-base:latest

COPY src /autograder/source/src
COPY tutorial/keys /autograder/source/keys

RUN apt update \
    && apt install -y libssl-dev \
    && apt install -y seccomp \
    && cd /autograder/source/src \
    && make \
    && adduser student --no-create-home --disabled-password --gecos "" \
    && chmod o= /autograder/source/src

COPY autograder/run_autograder /autograder
COPY autograder /autograder/source
RUN pip install -r /autograder/source/requirements.txt