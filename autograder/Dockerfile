FROM gradescope/autograder-base:latest

RUN apt update \
    && apt install -y libssl-dev \
    && adduser student --no-create-home --disabled-password --gecos ""

COPY src /autograder/source/src
COPY tutorial/keys /autograder/source/src/keys

RUN cd /autograder/source/src \
    && make \
    && rm -rf Makefile *.c *.h

COPY autograder/run_autograder /autograder
COPY autograder /autograder/source
RUN pip install -r /autograder/source/requirements.txt