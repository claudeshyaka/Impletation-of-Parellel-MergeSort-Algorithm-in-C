#!/usr/bin/python -u

import os, csv, subprocess, time

JOB_NAME = 'clw_sort'

def ChangeThreshold( in_path, out_path, threshold ):

    in_file = open( in_path, 'r' )
    out_file = open( out_path, 'w' )

    for line in in_file:

        if('#define THRESHOLD ' in line):
            line = '#define THRESHOLD ' + str(threshold) + '\n'

        out_file.write(line)

    in_file.close()
    out_file.close()


def ChangeQSub( in_path, proc_count, threshold ):

    in_file = open( in_path, 'r' )

    out_path = 'qsub_' + str(proc_count) + '.sh'
    out_file = open( out_path, 'w' )

    for line in in_file:

        if('#$ -o sort.out' in line):
            line = '#$ -o sort_' + str(proc_count) + '_' + str(threshold) + '.out\n'
        elif('./sort 10000000' in line):
            line = 'taskset -c 0-' + str(proc_count-1) + ' ./sort 10000000 ' + str(proc_count) + '\n'

        out_file.write(line)

    in_file.close()
    out_file.close()

    return out_path


def waitForJobCompletion( job_name ):

    proc = subprocess.Popen(["qstat"], stdout=subprocess.PIPE)
    out = proc.communicate()[0]

    cleaned = out.replace('\n','')
    cleaned = cleaned.replace('\r','')
    cleaned = cleaned.replace('\t','')

    while( job_name in cleaned ):

        proc = subprocess.Popen(["qstat"], stdout=subprocess.PIPE)
        out = proc.communicate()[0]

        cleaned = out.replace('\n','')
        cleaned = cleaned.replace('\r','')
        cleaned = cleaned.replace('\t','')

        time.sleep(5)


def PostProcess( in_path ):

    results = {}
    mode = ''
    in_file = open(in_path, 'r')

    for line in in_file:

        if('cilk_sort sorting successful' in line):
            mode = 'cilk'
        if('pthread_sort sorting successful' in line):
            mode = 'pthread'
        elif('cilk_sort sorting FAILURE' in line):
            print("Cilk Sorting Failed!!!")
            exit()
        elif('pthread_sort sorting FAILURE' in line):
            print("Pthread Sorting Failed!!!")
            exit()
        elif('Running time average' in line):
            splits = line.split(':')
            sub_splits = splits[1].strip().split(' ')
            if('cilk' in mode):
                results['cilk_avg'] = sub_splits[0]
            elif('pthread' in mode):
                results['pthread_avg'] = sub_splits[0]
        elif('Std. dev' in line):
            splits = line.split(':')
            s_splits = splits[1].split('s')
            if('cilk' in mode):
                results['cilk_std_dev_time'] = s_splits[0].strip()
                results['cilk_std_dev_percentage'] = s_splits[1].strip()
            elif('pthread' in mode):
                results['pthread_std_dev_time'] = s_splits[0].strip()
                results['pthread_std_dev_percentage'] = s_splits[1].strip()
            mode = ''
    
    return results


# Contains the list of possible threshold values to try
# in order to find the ideal grain size for the cilk
# implementation
thresholds = [ 4, 8, 16, 32, 64, 128, 256, 512 ]

# contains the list of all processor core amounts needed
processor_values = [ 1, 2, 4, 8, 16 ]

result_file = open('results.csv', 'w')
writer = csv.writer(result_file, dialect='excel', lineterminator='\n')

data = []
data.append('threshold')
data.append('processor_count')
data.append('number_elements')
data.append('cilk_avg_time')
data.append('cilk_std_dev_time')
data.append('cilk_std_dev_percentage')
data.append('pthread_avg_time')
data.append('pthread_std_dev')
data.append('pthread_std_dev_percentage')
writer.writerow(data)

# iterate over each threshold in order to build the software
# and capture the results
for threshold in thresholds:

    # iterate over each of the sorting algorithms and alter
    # the amount of coursening that takes place
    for item in ['cilk_sort.c', 'pthread_sort.c']:

        if(not os.path.exists('template_' + item)):
            os.system('cp ' + item + ' template_' + item)

        in_file  = 'template_' + item
        out_file = item

        # change the cilk_sort.c file with new threshold
        print("\nGenerating " + item + " ... " + str(threshold))
        ChangeThreshold( in_file, out_file, threshold )

    # initiate software clean and build
    print("Building ... " + str(threshold))
    os.system('make clean')
    os.system('make')

    for proc in processor_values:

        # generate a submission script for this configuration
        qsub_name = ChangeQSub( 'qsub.sh', proc, threshold )

        # submit job to the queue for completion
        cmd = 'qsub -q cse539s.q ' + qsub_name
        os.system(cmd)

        # wait for the job to be completed
        print("Wait for threshold " + str(threshold) + " on " + str(proc) + " processors")
        waitForJobCompletion( JOB_NAME )

        # post process results
        out_path = 'sort_' + str(proc) + '_' + str(threshold) + '.out'
        results = PostProcess(out_path)

        # add to spreadsheet
        data = []
        data.append(str(threshold))
        data.append(str(proc))
        data.append('10000000')
        data.append(results['cilk_avg'])
        data.append(results['cilk_std_dev_time'])
        data.append(results['cilk_std_dev_percentage'])
        data.append(results['pthread_avg'])
        data.append(results['pthread_std_dev_time'])
        data.append(results['pthread_std_dev_percentage'])
        writer.writerow(data)

        print("Completed threshold " + str(threshold) + " on " + str(proc) + " processors")


result_file.close()

if(not os.path.exists('output')):
    os.makedirs('output')

# Move all the output files from the run into an output directory
os.system('mv *.out output')

if(not os.path.exists('scripts')):
    os.makedirs('scripts')

os.system('mv qsub_* scripts')


