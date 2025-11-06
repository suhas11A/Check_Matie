import subprocess

def run_test_case(input_file, output_file, program_file):
    with open(input_file, 'r') as f:
        input_data = f.read()

    # Run the program as a subprocess
    # print("---------------------------------------")
    # print("Started Running")
    result = subprocess.run(['/usr/bin/python3', program_file], input=input_data, text=True, capture_output=True)
    # print("Finished Runnign")
    actual_output = result.stdout.strip()
    expected_output = open(output_file, 'r').read().strip()
    return actual_output == expected_output

def run_all_test_cases(program_file):
    for i in range(11):
        input_file = f'testcases/input{i}.txt'
        output_file = f'testcases/output{i}.txt'
        if run_test_case(input_file, output_file, program_file):
            print(f'Test case {i}: PASSED')
        else:
            print(f'Test case {i}: FAILED')

if __name__ == "__main__":
    program_file = 'Greedy_Or_Not.py'  
    run_all_test_cases(program_file)
