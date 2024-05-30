# NOTE: Make sure you have your project files in the ./project directory
# Will run the autograder and place the results in ./results/results.json
run:
	rm -f project/server project/client project/test*
	docker run --rm -v ./project:/autograder/submission -v ./results:/autograder/results security-is-key /autograder/run_autograder && cat results/results.json

# In case you want to run the autograder manually, use interactive
interactive:
	docker run --rm -it -v ./project:/autograder/submission -v ./results:/autograder/results security-is-key bash

# Don't worry about this one
build:
	docker build -t security-is-key --progress=plain -f autograder/Dockerfile .