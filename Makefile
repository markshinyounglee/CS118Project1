# NOTE: Make sure you have your project files in the ./project directory

# Will run the autograder and place the results in ./results/results.json
run:
	docker run --rm -v ./project:/autograder/submission -v ./results:/autograder/results eado0/security-is-key /autograder/run_autograder && cat results/results.json

# In case you want to run the autograder manually, use interactive
interactive:
	docker run --rm -it -v ./project:/autograder/submission -v ./results:/autograder/results eado0/security-is-key bash