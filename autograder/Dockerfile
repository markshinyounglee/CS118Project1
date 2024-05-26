ARG BASE_REPO=gradescope/autograder-base
ARG TAG=latest

FROM ${BASE_REPO}:${TAG}

ADD source /autograder/source

COPY . /autograder/source
COPY /autograder/source/run_autograder /autograder/source
RUN chmod +x /autograder/run_autograder

RUN apt install 